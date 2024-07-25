// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/gcm/engine/connection_handler_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/base/socket_stream.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace io = google::protobuf::io;

namespace gcm {

namespace {

// # of bytes a MCS version packet consumes.
const int kVersionPacketLen = 1;
// # of bytes a tag packet consumes.
const int kTagPacketLen = 1;
// Max # of bytes a length packet consumes. A Varint32 can consume up to 5 bytes
// (the msb in each byte is reserved for denoting whether more bytes follow).
// Although the protocol only allows for 4KiB payloads currently, and the socket
// stream buffer is only of size 8KiB, it's possible for certain applications to
// have larger message sizes. When payload is larger than 4KiB, an temporary
// in-memory buffer is used instead of the normal in-place socket stream buffer.
const int kSizePacketLenMin = 1;
const int kSizePacketLenMax = 5;

// The normal limit for a data packet is 4KiB. Any data packet with a size
// larger than this uses the temporary in-memory buffer,
const int kDefaultDataPacketLimit = 1024 * 4;

// The current MCS protocol version.
const int kMCSVersion = 41;

}  // namespace

ConnectionHandlerImpl::ConnectionHandlerImpl(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    base::TimeDelta read_timeout,
    const ProtoReceivedCallback& read_callback,
    const ProtoSentCallback& write_callback,
    const ConnectionChangedCallback& connection_callback)
    : io_task_runner_(std::move(io_task_runner)),
      read_timeout_timer_(FROM_HERE,
                          read_timeout,
                          base::BindRepeating(&ConnectionHandlerImpl::OnTimeout,
                                              base::Unretained(this))),
      handshake_complete_(false),
      message_tag_(0),
      message_size_(0),
      read_callback_(read_callback),
      write_callback_(write_callback),
      connection_callback_(connection_callback),
      size_packet_so_far_(0) {
  DCHECK(io_task_runner_);
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
}

ConnectionHandlerImpl::~ConnectionHandlerImpl() {
}

void ConnectionHandlerImpl::Init(
    const mcs_proto::LoginRequest& login_request,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(!read_callback_.is_null());
  DCHECK(!write_callback_.is_null());
  DCHECK(!connection_callback_.is_null());

  // Invalidate any previously outstanding reads.
  weak_ptr_factory_.InvalidateWeakPtrs();

  handshake_complete_ = false;
  message_tag_ = 0;
  message_size_ = 0;
  input_stream_ =
      std::make_unique<SocketInputStream>(std::move(receive_stream));
  output_stream_ = std::make_unique<SocketOutputStream>(std::move(send_stream));

  Login(login_request);
}

void ConnectionHandlerImpl::Reset() {
  CloseConnection();
}

bool ConnectionHandlerImpl::CanSendMessage() const {
  return handshake_complete_ && output_stream_.get() &&
      output_stream_->GetState() == SocketOutputStream::EMPTY;
}

void ConnectionHandlerImpl::SendMessage(
    const google::protobuf::MessageLite& message) {
  DCHECK_EQ(output_stream_->GetState(), SocketOutputStream::EMPTY);
  DCHECK(handshake_complete_);

  {
    io::CodedOutputStream coded_output_stream(output_stream_.get());
    DVLOG(1) << "Writing proto of size " << message.ByteSize();
    int tag = GetMCSProtoTag(message);
    DCHECK_NE(tag, -1);
    coded_output_stream.WriteRaw(&tag, 1);
    coded_output_stream.WriteVarint32(message.ByteSize());
    message.SerializeToCodedStream(&coded_output_stream);
  }

  if (output_stream_->Flush(base::BindOnce(
          &ConnectionHandlerImpl::OnMessageSent,
          weak_ptr_factory_.GetWeakPtr())) != net::ERR_IO_PENDING) {
    OnMessageSent();
  }
}

void ConnectionHandlerImpl::Login(
    const google::protobuf::MessageLite& login_request) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(output_stream_->GetState(), SocketOutputStream::EMPTY);

  const char version_byte[1] = {kMCSVersion};
  const char login_request_tag[1] = {kLoginRequestTag};
  {
    io::CodedOutputStream coded_output_stream(output_stream_.get());
    coded_output_stream.WriteRaw(version_byte, 1);
    coded_output_stream.WriteRaw(login_request_tag, 1);
    coded_output_stream.WriteVarint32(login_request.ByteSize());
    login_request.SerializeToCodedStream(&coded_output_stream);
  }

  if (output_stream_->Flush(base::BindOnce(
          &ConnectionHandlerImpl::OnMessageSent,
          weak_ptr_factory_.GetWeakPtr())) != net::ERR_IO_PENDING) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionHandlerImpl::OnMessageSent,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  read_timeout_timer_.Reset();
  WaitForData(MCS_VERSION_TAG_AND_SIZE);
}

void ConnectionHandlerImpl::OnMessageSent() {
  if (!output_stream_.get()) {
    // The connection has already been closed. Just return.
    DCHECK(!input_stream_.get());
    DCHECK(!read_timeout_timer_.IsRunning());
    return;
  }

  if (output_stream_->GetState() != SocketOutputStream::EMPTY) {
    int last_error = output_stream_->last_error();
    CloseConnection();
    // If the socket stream had an error, plumb it up, else plumb up FAILED.
    if (last_error == net::OK)
      last_error = net::ERR_FAILED;
    connection_callback_.Run(last_error);
    return;
  }

  write_callback_.Run();
}

void ConnectionHandlerImpl::GetNextMessage() {
  DCHECK(SocketInputStream::EMPTY == input_stream_->GetState() ||
         SocketInputStream::READY == input_stream_->GetState());
  message_tag_ = 0;
  message_size_ = 0;

  WaitForData(MCS_TAG_AND_SIZE);
}

void ConnectionHandlerImpl::WaitForData(ProcessingState state) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << "Waiting for MCS data: state == " << state;

  if (!input_stream_) {
    // The connection has already been closed. Just return.
    DCHECK(!output_stream_.get());
    DCHECK(!read_timeout_timer_.IsRunning());
    return;
  }

  if (input_stream_->GetState() != SocketInputStream::EMPTY &&
      input_stream_->GetState() != SocketInputStream::READY) {
    // An error occurred.
    int last_error = output_stream_->last_error();
    CloseConnection();
    // If the socket stream had an error, plumb it up, else plumb up FAILED.
    if (last_error == net::OK)
      last_error = net::ERR_FAILED;
    connection_callback_.Run(last_error);
    return;
  }

  // Used to determine whether a Socket::Read is necessary.
  int min_bytes_needed = 0;
  // Used to limit the size of the Socket::Read.
  int max_bytes_needed = 0;

  switch(state) {
    case MCS_VERSION_TAG_AND_SIZE:
      min_bytes_needed = kVersionPacketLen + kTagPacketLen + kSizePacketLenMin;
      max_bytes_needed = kVersionPacketLen + kTagPacketLen + kSizePacketLenMax;
      break;
    case MCS_TAG_AND_SIZE:
      min_bytes_needed = kTagPacketLen + kSizePacketLenMin;
      max_bytes_needed = kTagPacketLen + kSizePacketLenMax;
      break;
    case MCS_SIZE:
      min_bytes_needed = size_packet_so_far_ + 1;
      max_bytes_needed = kSizePacketLenMax;
      break;
    case MCS_PROTO_BYTES:
      read_timeout_timer_.Reset();
      if (message_size_ < kDefaultDataPacketLimit) {
        // No variability in the message size, set both to the same.
        min_bytes_needed = message_size_;
        max_bytes_needed = message_size_;
      } else {
        int bytes_left = message_size_ - payload_input_buffer_.size();
        if (bytes_left > kDefaultDataPacketLimit)
          bytes_left = kDefaultDataPacketLimit;
        min_bytes_needed = bytes_left;
        max_bytes_needed = bytes_left;
      }
      break;
  }
  DCHECK_GE(max_bytes_needed, min_bytes_needed);

  int unread_byte_count = input_stream_->UnreadByteCount();
  if (min_bytes_needed > unread_byte_count &&
      input_stream_->Refresh(
          base::BindOnce(&ConnectionHandlerImpl::WaitForData,
                         weak_ptr_factory_.GetWeakPtr(), state),
          max_bytes_needed - unread_byte_count) == net::ERR_IO_PENDING) {
    return;
  }

  // Check for refresh errors.
  if (input_stream_->GetState() != SocketInputStream::READY) {
    // An error occurred.
    int last_error = input_stream_->last_error();
    CloseConnection();
    // If the socket stream had an error, plumb it up, else plumb up FAILED.
    if (last_error == net::OK)
      last_error = net::ERR_FAILED;
    connection_callback_.Run(last_error);
    return;
  }

  // Check whether read is complete, or needs to be continued (
  // SocketInputStream::Refresh can finish without reading all the data).
  if (input_stream_->UnreadByteCount() < min_bytes_needed) {
    DVLOG(1) << "Socket read finished prematurely. Waiting for "
             << min_bytes_needed - input_stream_->UnreadByteCount()
             << " more bytes.";
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ConnectionHandlerImpl::WaitForData,
                       weak_ptr_factory_.GetWeakPtr(), MCS_PROTO_BYTES));
    return;
  }

  // Received enough bytes, process them.
  DVLOG(1) << "Processing MCS data: state == " << state;
  switch(state) {
    case MCS_VERSION_TAG_AND_SIZE:
      OnGotVersion();
      break;
    case MCS_TAG_AND_SIZE:
      OnGotMessageTag();
      break;
    case MCS_SIZE:
      OnGotMessageSize();
      break;
    case MCS_PROTO_BYTES:
      OnGotMessageBytes();
      break;
  }
}

void ConnectionHandlerImpl::OnGotVersion() {
  uint8_t version = 0;
  {
    io::CodedInputStream coded_input_stream(input_stream_.get());
    coded_input_stream.ReadRaw(&version, 1);
  }
  // TODO(zea): remove this when the server is ready.
  if (version < kMCSVersion && version != 38) {
    LOG(ERROR) << "Invalid GCM version response: " << static_cast<int>(version);
    connection_callback_.Run(net::ERR_FAILED);
    return;
  }

  input_stream_->RebuildBuffer();

  // Process the LoginResponse message tag.
  OnGotMessageTag();
}

void ConnectionHandlerImpl::OnGotMessageTag() {
  if (input_stream_->GetState() != SocketInputStream::READY) {
    LOG(ERROR) << "Failed to receive protobuf tag.";
    read_callback_.Run(std::unique_ptr<google::protobuf::MessageLite>());
    return;
  }

  {
    io::CodedInputStream coded_input_stream(input_stream_.get());
    coded_input_stream.ReadRaw(&message_tag_, 1);
  }

  DVLOG(1) << "Received proto of type "
           << static_cast<unsigned int>(message_tag_);

  if (!read_timeout_timer_.IsRunning())
    read_timeout_timer_.Reset();
  OnGotMessageSize();
}

void ConnectionHandlerImpl::OnGotMessageSize() {
  if (input_stream_->GetState() != SocketInputStream::READY) {
    LOG(ERROR) << "Failed to receive message size.";
    read_callback_.Run(std::unique_ptr<google::protobuf::MessageLite>());
    return;
  }

  int prev_byte_count = input_stream_->UnreadByteCount();
  int result = net::OK;
  bool incomplete_size_packet = false;
  {
    io::CodedInputStream coded_input_stream(input_stream_.get());
    if (!coded_input_stream.ReadVarint32(&message_size_)) {
      DVLOG(1) << "Expecting another message size byte.";
      if (prev_byte_count >= kSizePacketLenMax) {
        // Already had enough bytes, something else went wrong.
        LOG(ERROR) << "Failed to process message size";
        result = net::ERR_FILE_TOO_BIG;
      } else {
        // Back up by the amount read.
        int bytes_read = prev_byte_count - input_stream_->UnreadByteCount();
        input_stream_->BackUp(bytes_read);
        size_packet_so_far_ = bytes_read;
        incomplete_size_packet = true;
      }
    }
  }

  if (result != net::OK) {
    connection_callback_.Run(result);
    return;
  } else if (incomplete_size_packet) {
    WaitForData(MCS_SIZE);
    return;
  }

  DVLOG(1) << "Proto size: " << message_size_;
  size_packet_so_far_ = 0;
  payload_input_buffer_.clear();

  if (message_size_ > 0)
    WaitForData(MCS_PROTO_BYTES);
  else
    OnGotMessageBytes();
}

void ConnectionHandlerImpl::OnGotMessageBytes() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  read_timeout_timer_.Stop();
  std::unique_ptr<google::protobuf::MessageLite> protobuf(
      BuildProtobufFromTag(message_tag_));
  // Messages with no content are valid; just use the default protobuf for
  // that tag.
  if (protobuf.get() && message_size_ == 0) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionHandlerImpl::GetNextMessage,
                                  weak_ptr_factory_.GetWeakPtr()));
    read_callback_.Run(std::move(protobuf));
    return;
  }

  if (input_stream_->GetState() != SocketInputStream::READY) {
    LOG(ERROR) << "Failed to extract protobuf bytes of type "
               << static_cast<unsigned int>(message_tag_);
    // Reset the connection.
    connection_callback_.Run(net::ERR_FAILED);
    return;
  }

  if (!protobuf.get()) {
     LOG(ERROR) << "Received message of invalid type "
                << static_cast<unsigned int>(message_tag_);
     connection_callback_.Run(net::ERR_INVALID_ARGUMENT);
     return;
  }

  int result = net::OK;
  if (message_size_ < kDefaultDataPacketLimit) {
    io::CodedInputStream coded_input_stream(input_stream_.get());
    if (!protobuf->ParsePartialFromCodedStream(&coded_input_stream)) {
      LOG(ERROR) << "Unable to parse GCM message of type "
                 << static_cast<unsigned int>(message_tag_);
      result = net::ERR_FAILED;
    }
  } else {
    // Copy any data in the input stream onto the end of the buffer.
    const void* data_ptr = nullptr;
    int size = 0;
    input_stream_->Next(&data_ptr, &size);
    payload_input_buffer_.insert(payload_input_buffer_.end(),
                                 static_cast<const uint8_t*>(data_ptr),
                                 static_cast<const uint8_t*>(data_ptr) + size);
    DCHECK_LE(payload_input_buffer_.size(), message_size_);

    if (payload_input_buffer_.size() == message_size_) {
      io::ArrayInputStream buffer_input_stream(payload_input_buffer_.data(),
                                               payload_input_buffer_.size());
      io::CodedInputStream coded_input_stream(&buffer_input_stream);
      if (!protobuf->ParsePartialFromCodedStream(&coded_input_stream)) {
        LOG(ERROR) << "Unable to parse GCM message of type "
                   << static_cast<unsigned int>(message_tag_);
        result = net::ERR_FAILED;
      }
    } else {
      // Continue reading data.
      DVLOG(1) << "Continuing data read. Buffer size is "
                << payload_input_buffer_.size()
                << ", expecting " << message_size_;
      input_stream_->RebuildBuffer();

      read_timeout_timer_.Reset();
      WaitForData(MCS_PROTO_BYTES);
      return;
    }
  }

  if (result != net::OK) {
    // Reset the connection.
    connection_callback_.Run(result);
    return;
  }

  input_stream_->RebuildBuffer();
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConnectionHandlerImpl::GetNextMessage,
                                weak_ptr_factory_.GetWeakPtr()));
  if (message_tag_ == kLoginResponseTag) {
    if (handshake_complete_) {
      LOG(ERROR) << "Unexpected login response.";
    } else {
      handshake_complete_ = true;
      DVLOG(1) << "GCM Handshake complete.";
      connection_callback_.Run(net::OK);
    }
  }
  read_callback_.Run(std::move(protobuf));
}

void ConnectionHandlerImpl::OnTimeout() {
  LOG(ERROR) << "Timed out waiting for GCM Protocol buffer.";
  CloseConnection();
  connection_callback_.Run(net::ERR_TIMED_OUT);
}

void ConnectionHandlerImpl::CloseConnection() {
  DVLOG(1) << "Closing connection.";
  read_timeout_timer_.Stop();
  handshake_complete_ = false;
  message_tag_ = 0;
  message_size_ = 0;
  size_packet_so_far_ = 0;
  payload_input_buffer_.clear();
  input_stream_.reset();
  output_stream_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace gcm
