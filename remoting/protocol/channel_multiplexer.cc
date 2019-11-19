// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_multiplexer.h"

#include <stddef.h>
#include <string.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace remoting {
namespace protocol {

namespace {
const int kChannelIdUnknown = -1;
const int kMaxPacketSize = 1024;

class PendingPacket {
 public:
  PendingPacket(std::unique_ptr<MultiplexPacket> packet)
      : packet(std::move(packet)) {}
  ~PendingPacket() = default;

  bool is_empty() { return pos >= packet->data().size(); }

  int Read(char* buffer, size_t size) {
    size = std::min(size, packet->data().size() - pos);
    memcpy(buffer, packet->data().data() + pos, size);
    pos += size;
    return size;
  }

 private:
  std::unique_ptr<MultiplexPacket> packet;
  size_t pos = 0U;

  DISALLOW_COPY_AND_ASSIGN(PendingPacket);
};

}  // namespace

const char ChannelMultiplexer::kMuxChannelName[] = "mux";

struct ChannelMultiplexer::PendingChannel {
  PendingChannel(const std::string& name,
                 const ChannelCreatedCallback& callback)
      : name(name), callback(callback) {
  }
  std::string name;
  ChannelCreatedCallback callback;
};

class ChannelMultiplexer::MuxChannel {
 public:
  MuxChannel(ChannelMultiplexer* multiplexer, const std::string& name,
             int send_id);
  ~MuxChannel();

  const std::string& name() { return name_; }
  int receive_id() { return receive_id_; }
  void set_receive_id(int id) { receive_id_ = id; }

  // Called by ChannelMultiplexer.
  std::unique_ptr<P2PStreamSocket> CreateSocket();
  void OnIncomingPacket(std::unique_ptr<MultiplexPacket> packet);
  void OnBaseChannelError(int error);

  // Called by MuxSocket.
  void OnSocketDestroyed();
  void DoWrite(std::unique_ptr<MultiplexPacket> packet,
               base::OnceClosure done_task,
               const net::NetworkTrafficAnnotationTag& traffic_annotation);
  int DoRead(const scoped_refptr<net::IOBuffer>& buffer, int buffer_len);

 private:
  ChannelMultiplexer* multiplexer_;
  std::string name_;
  int send_id_;
  bool id_sent_;
  int receive_id_;
  MuxSocket* socket_;
  std::list<std::unique_ptr<PendingPacket>> pending_packets_;

  DISALLOW_COPY_AND_ASSIGN(MuxChannel);
};

class ChannelMultiplexer::MuxSocket : public P2PStreamSocket {
 public:
  MuxSocket(MuxChannel* channel);
  ~MuxSocket() override;

  void OnWriteComplete();
  void OnBaseChannelError(int error);
  void OnPacketReceived();

  // P2PStreamSocket interface.
  int Read(const scoped_refptr<net::IOBuffer>& buffer,
           int buffer_len,
           net::CompletionOnceCallback callback) override;
  int Write(
      const scoped_refptr<net::IOBuffer>& buffer,
      int buffer_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  MuxChannel* channel_;

  int base_channel_error_ = net::OK;

  net::CompletionOnceCallback read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;

  bool write_pending_;
  int write_result_;
  net::CompletionOnceCallback write_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MuxSocket> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MuxSocket);
};

ChannelMultiplexer::MuxChannel::MuxChannel(ChannelMultiplexer* multiplexer,
                                           const std::string& name,
                                           int send_id)
    : multiplexer_(multiplexer),
      name_(name),
      send_id_(send_id),
      id_sent_(false),
      receive_id_(kChannelIdUnknown),
      socket_(nullptr) {}

ChannelMultiplexer::MuxChannel::~MuxChannel() {
  // Socket must be destroyed before the channel.
  DCHECK(!socket_);
}

std::unique_ptr<P2PStreamSocket>
ChannelMultiplexer::MuxChannel::CreateSocket() {
  DCHECK(!socket_);  // Can't create more than one socket per channel.
  std::unique_ptr<MuxSocket> result(new MuxSocket(this));
  socket_ = result.get();
  return std::move(result);
}

void ChannelMultiplexer::MuxChannel::OnIncomingPacket(
    std::unique_ptr<MultiplexPacket> packet) {
  DCHECK_EQ(packet->channel_id(), receive_id_);
  if (packet->data().size() > 0) {
    pending_packets_.push_back(
        std::make_unique<PendingPacket>(std::move(packet)));
    if (socket_) {
      // Notify the socket that we have more data.
      socket_->OnPacketReceived();
    }
  }
}

void ChannelMultiplexer::MuxChannel::OnBaseChannelError(int error) {
  if (socket_)
    socket_->OnBaseChannelError(error);
}

void ChannelMultiplexer::MuxChannel::OnSocketDestroyed() {
  DCHECK(socket_);
  socket_ = nullptr;
}

void ChannelMultiplexer::MuxChannel::DoWrite(
    std::unique_ptr<MultiplexPacket> packet,
    base::OnceClosure done_task,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  packet->set_channel_id(send_id_);
  if (!id_sent_) {
    packet->set_channel_name(name_);
    id_sent_ = true;
  }
  multiplexer_->DoWrite(std::move(packet), std::move(done_task),
                        traffic_annotation);
}

int ChannelMultiplexer::MuxChannel::DoRead(
    const scoped_refptr<net::IOBuffer>& buffer,
    int buffer_len) {
  int pos = 0;
  while (buffer_len > 0 && !pending_packets_.empty()) {
    DCHECK(!pending_packets_.front()->is_empty());
    int result = pending_packets_.front()->Read(
        buffer->data() + pos, buffer_len);
    DCHECK_LE(result, buffer_len);
    pos += result;
    buffer_len -= pos;
    if (pending_packets_.front()->is_empty())
      pending_packets_.pop_front();
  }
  return pos;
}

ChannelMultiplexer::MuxSocket::MuxSocket(MuxChannel* channel)
    : channel_(channel),
      read_buffer_size_(0),
      write_pending_(false),
      write_result_(0) {}

ChannelMultiplexer::MuxSocket::~MuxSocket() {
  channel_->OnSocketDestroyed();
}

int ChannelMultiplexer::MuxSocket::Read(
    const scoped_refptr<net::IOBuffer>& buffer,
    int buffer_len,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_callback_.is_null());

  if (base_channel_error_ != net::OK)
    return base_channel_error_;

  int result = channel_->DoRead(buffer, buffer_len);
  if (result == 0) {
    read_buffer_ = buffer;
    read_buffer_size_ = buffer_len;
    read_callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }
  return result;
}

int ChannelMultiplexer::MuxSocket::Write(
    const scoped_refptr<net::IOBuffer>& buffer,
    int buffer_len,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(write_callback_.is_null());

  if (base_channel_error_ != net::OK)
    return base_channel_error_;

  std::unique_ptr<MultiplexPacket> packet(new MultiplexPacket());
  size_t size = std::min(kMaxPacketSize, buffer_len);
  packet->mutable_data()->assign(buffer->data(), size);

  write_pending_ = true;
  channel_->DoWrite(
      std::move(packet),
      base::BindOnce(&ChannelMultiplexer::MuxSocket::OnWriteComplete,
                     weak_factory_.GetWeakPtr()),
      traffic_annotation);

  // OnWriteComplete() might be called above synchronously.
  if (write_pending_) {
    DCHECK(write_callback_.is_null());
    write_callback_ = std::move(callback);
    write_result_ = size;
    return net::ERR_IO_PENDING;
  }

  return size;
}

void ChannelMultiplexer::MuxSocket::OnWriteComplete() {
  write_pending_ = false;
  if (!write_callback_.is_null())
    std::move(write_callback_).Run(write_result_);
}

void ChannelMultiplexer::MuxSocket::OnBaseChannelError(int error) {
  base_channel_error_ = error;

  // Here only one of the read and write callbacks is called if both of them are
  // pending. Ideally both of them should be called in that case, but that would
  // require the second one to be called asynchronously which would complicate
  // this code. Channels handle read and write errors the same way (see
  // ChannelDispatcherBase::OnReadWriteFailed) so calling only one of the
  // callbacks is enough.

  if (!read_callback_.is_null()) {
    std::move(read_callback_).Run(error);
    return;
  }

  if (!write_callback_.is_null())
    std::move(write_callback_).Run(error);
}

void ChannelMultiplexer::MuxSocket::OnPacketReceived() {
  if (!read_callback_.is_null()) {
    int result = channel_->DoRead(read_buffer_.get(), read_buffer_size_);
    read_buffer_ = nullptr;
    DCHECK_GT(result, 0);
    std::move(read_callback_).Run(result);
  }
}

ChannelMultiplexer::ChannelMultiplexer(StreamChannelFactory* factory,
                                       const std::string& base_channel_name)
    : base_channel_factory_(factory),
      base_channel_name_(base_channel_name),
      next_channel_id_(0) {}

ChannelMultiplexer::~ChannelMultiplexer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_channels_.empty());

  // Cancel creation of the base channel if it hasn't finished.
  if (base_channel_factory_)
    base_channel_factory_->CancelChannelCreation(base_channel_name_);
}

void ChannelMultiplexer::CreateChannel(const std::string& name,
                                       const ChannelCreatedCallback& callback) {
  if (base_channel_.get()) {
    // Already have |base_channel_|. Create new multiplexed channel
    // synchronously.
    callback.Run(GetOrCreateChannel(name)->CreateSocket());
  } else if (!base_channel_.get() && !base_channel_factory_) {
    // Fail synchronously if we failed to create |base_channel_|.
    callback.Run(nullptr);
  } else {
    // Still waiting for the |base_channel_|.
    pending_channels_.push_back(PendingChannel(name, callback));

    // If this is the first multiplexed channel then create the base channel.
    if (pending_channels_.size() == 1U) {
      base_channel_factory_->CreateChannel(
          base_channel_name_,
          base::Bind(&ChannelMultiplexer::OnBaseChannelReady,
                     base::Unretained(this)));
    }
  }
}

void ChannelMultiplexer::CancelChannelCreation(const std::string& name) {
  for (auto it = pending_channels_.begin(); it != pending_channels_.end();
       ++it) {
    if (it->name == name) {
      pending_channels_.erase(it);
      return;
    }
  }
}

void ChannelMultiplexer::OnBaseChannelReady(
    std::unique_ptr<P2PStreamSocket> socket) {
  base_channel_factory_ = nullptr;
  base_channel_ = std::move(socket);

  if (base_channel_.get()) {
    // Initialize reader and writer.
    reader_.StartReading(base_channel_.get(),
                         base::Bind(&ChannelMultiplexer::OnIncomingPacket,
                                    base::Unretained(this)),
                         base::Bind(&ChannelMultiplexer::OnBaseChannelError,
                                    base::Unretained(this)));
    writer_.Start(base::Bind(&P2PStreamSocket::Write,
                             base::Unretained(base_channel_.get())),
                  base::Bind(&ChannelMultiplexer::OnBaseChannelError,
                             base::Unretained(this)));
  }

  DoCreatePendingChannels();
}

void ChannelMultiplexer::DoCreatePendingChannels() {
  if (pending_channels_.empty())
    return;

  // Every time this function is called it connects a single channel and posts a
  // separate task to connect other channels. This is necessary because the
  // callback may destroy the multiplexer or somehow else modify
  // |pending_channels_| list (e.g. call CancelChannelCreation()).
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ChannelMultiplexer::DoCreatePendingChannels,
                                weak_factory_.GetWeakPtr()));

  PendingChannel c = pending_channels_.front();
  pending_channels_.erase(pending_channels_.begin());
  std::unique_ptr<P2PStreamSocket> socket;
  if (base_channel_.get())
    socket = GetOrCreateChannel(c.name)->CreateSocket();
  c.callback.Run(std::move(socket));
}

ChannelMultiplexer::MuxChannel* ChannelMultiplexer::GetOrCreateChannel(
    const std::string& name) {
  std::unique_ptr<MuxChannel>& channel = channels_[name];
  if (!channel) {
    // Create a new channel if we haven't found existing one.
    channel = std::make_unique<MuxChannel>(this, name, next_channel_id_);
    ++next_channel_id_;
  }

  return channel.get();
}


void ChannelMultiplexer::OnBaseChannelError(int error) {
  for (auto it = channels_.begin(); it != channels_.end(); ++it) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChannelMultiplexer::NotifyBaseChannelError,
                       weak_factory_.GetWeakPtr(), it->second->name(), error));
  }
}

void ChannelMultiplexer::NotifyBaseChannelError(const std::string& name,
                                                int error) {
  auto it = channels_.find(name);
  if (it != channels_.end())
    it->second->OnBaseChannelError(error);
}

void ChannelMultiplexer::OnIncomingPacket(
    std::unique_ptr<CompoundBuffer> buffer) {
  std::unique_ptr<MultiplexPacket> packet =
      ParseMessage<MultiplexPacket>(buffer.get());
  if (!packet)
    return;

  DCHECK(packet->has_channel_id());
  if (!packet->has_channel_id()) {
    LOG(ERROR) << "Received packet without channel_id.";
    return;
  }

  int receive_id = packet->channel_id();
  MuxChannel* channel = nullptr;
  auto it = channels_by_receive_id_.find(receive_id);
  if (it != channels_by_receive_id_.end()) {
    channel = it->second;
  } else {
    // This is a new |channel_id| we haven't seen before. Look it up by name.
    if (!packet->has_channel_name()) {
      LOG(ERROR) << "Received packet with unknown channel_id and "
          "without channel_name.";
      return;
    }
    channel = GetOrCreateChannel(packet->channel_name());
    channel->set_receive_id(receive_id);
    channels_by_receive_id_[receive_id] = channel;
  }

  channel->OnIncomingPacket(std::move(packet));
}

void ChannelMultiplexer::DoWrite(
    std::unique_ptr<MultiplexPacket> packet,
    base::OnceClosure done_task,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  writer_.Write(SerializeAndFrameMessage(*packet), std::move(done_task),
                traffic_annotation);
}

}  // namespace protocol
}  // namespace remoting
