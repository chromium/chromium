// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_packet_reader.h"

#include "base/bind.h"
#ifdef TEMP_INSTRUMENTATION_1014092
#include "base/debug/alias.h"
#endif
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/quic/address_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_clock.h"

namespace net {

QuicChromiumPacketReader::QuicChromiumPacketReader(
    DatagramClientSocket* socket,
    const quic::QuicClock* clock,
    Visitor* visitor,
    int yield_after_packets,
    quic::QuicTime::Delta yield_after_duration,
    const NetLogWithSource& net_log)
    : socket_(socket),
      should_stop_reading_(false),
      visitor_(visitor),
      read_pending_(false),
      num_packets_read_(0),
      clock_(clock),
      yield_after_packets_(yield_after_packets),
      yield_after_duration_(yield_after_duration),
      yield_after_(quic::QuicTime::Infinite()),
      read_buffer_(base::MakeRefCounted<IOBufferWithSize>(
          static_cast<size_t>(quic::kMaxIncomingPacketSize))),
      net_log_(net_log) {}

QuicChromiumPacketReader::~QuicChromiumPacketReader() {
#ifdef TEMP_INSTRUMENTATION_1014092
  liveness_ = DEAD;
  stack_trace_ = base::debug::StackTrace();
  // Probably not necessary, but just in case compiler tries to optimize out the
  // writes to liveness_ and stack_trace_.
  base::debug::Alias(&liveness_);
  base::debug::Alias(&stack_trace_);
#endif
}

void QuicChromiumPacketReader::StartReading() {
  CHECK(!should_stop_reading_);

  for (;;) {
    if (read_pending_)
      return;

    if (num_packets_read_ == 0)
      yield_after_ = clock_->Now() + yield_after_duration_;

    read_pending_ = true;
    CrashIfInvalid();
    CHECK(socket_);
    int rv =
        socket_->Read(read_buffer_.get(), read_buffer_->size(),
                      base::BindOnce(&QuicChromiumPacketReader::OnReadComplete,
                                     weak_factory_.GetWeakPtr()));
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.AsyncRead", rv == ERR_IO_PENDING);
    if (rv == ERR_IO_PENDING) {
      num_packets_read_ = 0;
      return;
    }

    if (++num_packets_read_ > yield_after_packets_ ||
        clock_->Now() > yield_after_) {
      num_packets_read_ = 0;
      // Data was read, process it.
      // Schedule the work through the message loop to 1) prevent infinite
      // recursion and 2) avoid blocking the thread for too long.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&QuicChromiumPacketReader::OnReadComplete,
                                    weak_factory_.GetWeakPtr(), rv));
    } else {
      if (!ProcessReadResult(rv)) {
        return;
      }
      if (should_stop_reading_) {
        // If data emits to this histogram, the underlying socket is closed.
        UMA_HISTOGRAM_BOOLEAN(
            "Net.QuicChromiumPacketReader.ShouldStopReadingInLoop",
            should_stop_reading_);
        return;
      }
    }
  }
}

size_t QuicChromiumPacketReader::EstimateMemoryUsage() const {
  return read_buffer_->size();
}

bool QuicChromiumPacketReader::ProcessReadResult(int result) {
  CrashIfInvalid();

  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    if (socket_ != nullptr)
      visitor_->OnReadError(result, socket_);
    return false;
  }

  quic::QuicReceivedPacket packet(read_buffer_->data(), result, clock_->Now());
  IPEndPoint local_address;
  IPEndPoint peer_address;
  // TODO(zhongyi): once crbug.com/1014092 is root caused, consider early return
  // false if |socket_| is nulled. For debugging purpose, still report up to
  // avoid introducing behavior change.
  // If the socket has been nulled, the connection is already closed. Reporting
  // packet up to the visitor is a no-op.
  if (socket_ != nullptr) {
    socket_->GetLocalAddress(&local_address_);
    socket_->GetPeerAddress(&peer_address_);
  }
  auto self = weak_factory_.GetWeakPtr();
  // Notifies the visitor that |this| reader gets a new packet, which may delete
  // |this| if |this| is a connectivity probing reader.
  return visitor_->OnPacket(packet, ToQuicSocketAddress(local_address_),
                            ToQuicSocketAddress(peer_address_)) &&
         self;
}

void QuicChromiumPacketReader::OnReadComplete(int result) {
  CrashIfInvalid();

  if (ProcessReadResult(result)) {
    if (should_stop_reading_) {
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicChromiumPacketReader.ShouldStopReadingOnReadComplete",
          should_stop_reading_);
    } else {
      StartReading();
    }
  }
}

void QuicChromiumPacketReader::CrashIfInvalid() const {
#ifdef TEMP_INSTRUMENTATION_1014092
  Liveness liveness = liveness_;

  if (liveness == ALIVE)
    return;

  // Copy relevant variables onto the stack to guarantee they will be available
  // in minidumps, and then crash.
  base::debug::StackTrace stack_trace = stack_trace_;

  base::debug::Alias(&liveness);
  base::debug::Alias(&stack_trace);

  CHECK_EQ(ALIVE, liveness);
#endif
}

}  // namespace net
