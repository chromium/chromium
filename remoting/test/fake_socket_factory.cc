// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_socket_factory.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/numerics/math_constants.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "remoting/base/leaky_bucket.h"
#include "third_party/webrtc/media/base/rtp_utils.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/socket.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace remoting {

namespace {

const int kPortRangeStart = 1024;
const int kPortRangeEnd = 65535;

double RandDouble() {
  return static_cast<double>(std::rand()) / RAND_MAX;
}

double GetNormalRandom(double average, double stddev) {
  // Based on Box-Muller transform, see
  // http://en.wikipedia.org/wiki/Box_Muller_transform .
  return average + stddev * sqrt(-2.0 * log(1.0 - RandDouble())) *
                       cos(RandDouble() * 2.0 * base::kPiDouble);
}

class FakeUdpSocket : public rtc::AsyncPacketSocket {
 public:
  FakeUdpSocket(FakePacketSocketFactory* factory,
                scoped_refptr<FakeNetworkDispatcher> dispatcher,
                const rtc::SocketAddress& local_address);
  ~FakeUdpSocket() override;

  void ReceivePacket(const rtc::SocketAddress& from,
                     const rtc::SocketAddress& to,
                     const scoped_refptr<net::IOBuffer>& data,
                     int data_size);

  // rtc::AsyncPacketSocket interface.
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Send(const void* data,
           size_t data_size,
           const rtc::PacketOptions& options) override;
  int SendTo(const void* data,
             size_t data_size,
             const rtc::SocketAddress& address,
             const rtc::PacketOptions& options) override;
  int Close() override;
  State GetState() const override;
  int GetOption(rtc::Socket::Option option, int* value) override;
  int SetOption(rtc::Socket::Option option, int value) override;
  int GetError() const override;
  void SetError(int error) override;

 private:
  FakePacketSocketFactory* factory_;
  scoped_refptr<FakeNetworkDispatcher> dispatcher_;
  rtc::SocketAddress local_address_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(FakeUdpSocket);
};

FakeUdpSocket::FakeUdpSocket(FakePacketSocketFactory* factory,
                             scoped_refptr<FakeNetworkDispatcher> dispatcher,
                             const rtc::SocketAddress& local_address)
    : factory_(factory),
      dispatcher_(dispatcher),
      local_address_(local_address),
      state_(STATE_BOUND) {
}

FakeUdpSocket::~FakeUdpSocket() {
  factory_->OnSocketDestroyed(local_address_.port());
}

void FakeUdpSocket::ReceivePacket(const rtc::SocketAddress& from,
                                  const rtc::SocketAddress& to,
                                  const scoped_refptr<net::IOBuffer>& data,
                                  int data_size) {
  SignalReadPacket(this, data->data(), data_size, from, rtc::TimeMicros());
}

rtc::SocketAddress FakeUdpSocket::GetLocalAddress() const {
  return local_address_;
}

rtc::SocketAddress FakeUdpSocket::GetRemoteAddress() const {
  NOTREACHED();
  return rtc::SocketAddress();
}

int FakeUdpSocket::Send(const void* data, size_t data_size,
                        const rtc::PacketOptions& options) {
  NOTREACHED();
  return EINVAL;
}

int FakeUdpSocket::SendTo(const void* data, size_t data_size,
                          const rtc::SocketAddress& address,
                          const rtc::PacketOptions& options) {
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(data_size);
  memcpy(buffer->data(), data, data_size);
  base::TimeTicks now = base::TimeTicks::Now();
  cricket::ApplyPacketOptions(reinterpret_cast<uint8_t*>(buffer->data()),
                              data_size, options.packet_time_params,
                              (now - base::TimeTicks()).InMicroseconds());
  SignalSentPacket(this, rtc::SentPacket(options.packet_id, rtc::TimeMillis()));
  dispatcher_->DeliverPacket(local_address_, address, buffer, data_size);
  return data_size;
}

int FakeUdpSocket::Close() {
  state_ = STATE_CLOSED;
  return 0;
}

rtc::AsyncPacketSocket::State FakeUdpSocket::GetState() const {
  return state_;
}

int FakeUdpSocket::GetOption(rtc::Socket::Option option, int* value) {
  NOTIMPLEMENTED();
  return -1;
}

int FakeUdpSocket::SetOption(rtc::Socket::Option option, int value) {
  // All options are currently ignored.
  return 0;
}

int FakeUdpSocket::GetError() const {
  return 0;
}

void FakeUdpSocket::SetError(int error) {
  NOTREACHED();
}

}  // namespace

FakePacketSocketFactory::PendingPacket::PendingPacket()
    : data_size(0) {
}

FakePacketSocketFactory::PendingPacket::PendingPacket(
    const rtc::SocketAddress& from,
    const rtc::SocketAddress& to,
    const scoped_refptr<net::IOBuffer>& data,
    int data_size)
    : from(from), to(to), data(data), data_size(data_size) {
}

FakePacketSocketFactory::PendingPacket::PendingPacket(
    const PendingPacket& other) = default;

FakePacketSocketFactory::PendingPacket::~PendingPacket() = default;

FakePacketSocketFactory::FakePacketSocketFactory(
    FakeNetworkDispatcher* dispatcher)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      dispatcher_(dispatcher),
      address_(dispatcher_->AllocateAddress()),
      out_of_order_rate_(0.0),
      next_port_(kPortRangeStart) {
  dispatcher_->AddNode(this);
}

FakePacketSocketFactory::~FakePacketSocketFactory() {
  CHECK(udp_sockets_.empty());
  dispatcher_->RemoveNode(this);
}

void FakePacketSocketFactory::OnSocketDestroyed(int port) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  udp_sockets_.erase(port);
}

void FakePacketSocketFactory::SetBandwidth(int bandwidth, int max_buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (bandwidth <= 0) {
    leaky_bucket_.reset();
  } else {
    leaky_bucket_.reset(new LeakyBucket(max_buffer, bandwidth));
  }
}

void FakePacketSocketFactory::SetLatency(base::TimeDelta average,
                                         base::TimeDelta stddev) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  latency_average_ = average;
  latency_stddev_ = stddev;
}

rtc::AsyncPacketSocket* FakePacketSocketFactory::CreateUdpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  int port = -1;
  if (min_port > 0 && max_port > 0) {
    for (uint16_t i = min_port; i <= max_port; ++i) {
      if (udp_sockets_.find(i) == udp_sockets_.end()) {
        port = i;
        break;
      }
    }
    if (port < 0)
      return nullptr;
  } else {
    do {
      port = next_port_;
      next_port_ =
          (next_port_ >= kPortRangeEnd) ? kPortRangeStart : (next_port_ + 1);
    } while (udp_sockets_.find(port) != udp_sockets_.end());
  }

  CHECK(local_address.ipaddr() == address_);

  FakeUdpSocket* result =
      new FakeUdpSocket(this, dispatcher_,
                        rtc::SocketAddress(local_address.ipaddr(), port));

  udp_sockets_[port] = base::BindRepeating(&FakeUdpSocket::ReceivePacket,
                                           base::Unretained(result));

  return result;
}

rtc::AsyncPacketSocket* FakePacketSocketFactory::CreateServerTcpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port,
    int opts) {
  return nullptr;
}

rtc::AsyncPacketSocket* FakePacketSocketFactory::CreateClientTcpSocket(
    const rtc::SocketAddress& local_address,
    const rtc::SocketAddress& remote_address,
    const rtc::ProxyInfo& proxy_info,
    const std::string& user_agent,
    const rtc::PacketSocketTcpOptions& opts) {
  return nullptr;
}

rtc::AsyncResolverInterface*
FakePacketSocketFactory::CreateAsyncResolver() {
  return nullptr;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
FakePacketSocketFactory::GetThread() const {
  return task_runner_;
}

const rtc::IPAddress& FakePacketSocketFactory::GetAddress() const {
  return address_;
}

void FakePacketSocketFactory::ReceivePacket(
    const rtc::SocketAddress& from,
    const rtc::SocketAddress& to,
    const scoped_refptr<net::IOBuffer>& data,
    int data_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(to.ipaddr() == address_);

  base::TimeDelta delay;

  if (leaky_bucket_) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (!leaky_bucket_->RefillOrSpill(data_size, now)) {
      ++total_packets_dropped_;
      // Drop the packet.
      return;
    }
    delay = std::max(base::TimeDelta(), leaky_bucket_->GetEmptyTime() - now);
  }

  total_buffer_delay_ += delay;
  if (delay > max_buffer_delay_)
    max_buffer_delay_ = delay;
  ++total_packets_received_;

  if (latency_average_ > base::TimeDelta()) {
    delay += base::TimeDelta::FromMillisecondsD(
        GetNormalRandom(latency_average_.InMillisecondsF(),
                        latency_stddev_.InMillisecondsF()));
  }
  if (delay < base::TimeDelta())
    delay = base::TimeDelta();

  // Put the packet to the |pending_packets_| and post a task for
  // DoReceivePackets(). Note that the DoReceivePackets() task posted here may
  // deliver a different packet, not the one added to the queue here. This
  // would happen if another task gets posted with a shorted delay or when
  // |out_of_order_rate_| is greater than 0. It's implemented this way to
  // decouple latency variability from out-of-order delivery.
  PendingPacket packet(from, to, data, data_size);
  pending_packets_.push_back(packet);
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakePacketSocketFactory::DoReceivePacket,
                     weak_factory_.GetWeakPtr()),
      delay);
}

void FakePacketSocketFactory::DoReceivePacket() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  PendingPacket packet;
  if (pending_packets_.size() > 1 && RandDouble() < out_of_order_rate_) {
    auto it = pending_packets_.begin();
    ++it;
    packet = *it;
    pending_packets_.erase(it);
  } else {
    packet = pending_packets_.front();
    pending_packets_.pop_front();
  }

  auto iter = udp_sockets_.find(packet.to.port());
  if (iter == udp_sockets_.end()) {
    // Invalid port number.
    return;
  }

  iter->second.Run(packet.from, packet.to, packet.data, packet.data_size);
}

void FakePacketSocketFactory::ResetStats() {
  total_packets_dropped_ = 0;
  total_packets_received_ = 0;
  total_buffer_delay_ = base::TimeDelta();
  max_buffer_delay_ = base::TimeDelta();
}

}  // namespace remoting
