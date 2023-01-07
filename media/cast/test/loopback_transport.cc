// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/loopback_transport.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "media/cast/test/utility/udp_proxy.h"

namespace media {
namespace cast {
namespace {

// Shim that turns forwards packets from a test::PacketPipe to a
// PacketReceiverCallback.
class LoopBackPacketPipe final : public test::PacketPipe {
 public:
  explicit LoopBackPacketPipe(const PacketReceiverCallback& packet_receiver)
      : packet_receiver_(packet_receiver) {}

  LoopBackPacketPipe(const LoopBackPacketPipe&) = delete;
  LoopBackPacketPipe& operator=(const LoopBackPacketPipe&) = delete;

  ~LoopBackPacketPipe() final = default;

  // PacketPipe implementations.
  void Send(std::unique_ptr<Packet> packet) final {
    packet_receiver_.Run(std::move(packet));
  }

 private:
  PacketReceiverCallback packet_receiver_;
};

}  // namespace

LoopBackTransport::LoopBackTransport(
    scoped_refptr<CastEnvironment> cast_environment)
    : cast_environment_(cast_environment), bytes_sent_(0) {}

LoopBackTransport::~LoopBackTransport() = default;

bool LoopBackTransport::SendPacket(PacketRef packet, base::OnceClosure cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  std::unique_ptr<Packet> packet_copy(new Packet(packet->data));
  packet_pipe_->Send(std::move(packet_copy));
  bytes_sent_ += packet->data.size();
  return true;
}

int64_t LoopBackTransport::GetBytesSent() {
  return bytes_sent_;
}

void LoopBackTransport::Initialize(
    std::unique_ptr<test::PacketPipe> pipe,
    const PacketReceiverCallback& packet_receiver,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::TickClock* clock) {
  std::unique_ptr<test::PacketPipe> loopback_pipe(
      new LoopBackPacketPipe(packet_receiver));
  if (pipe) {
    // Append the loopback pipe to the end.
    pipe->AppendToPipe(std::move(loopback_pipe));
    packet_pipe_ = std::move(pipe);
  } else {
    packet_pipe_ = std::move(loopback_pipe);
  }
  packet_pipe_->InitOnIOThread(task_runner, clock);
}

}  // namespace cast
}  // namespace media
