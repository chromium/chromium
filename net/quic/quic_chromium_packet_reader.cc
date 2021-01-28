// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_packet_reader.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/quic/address_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_clock.h"

namespace net {

namespace {
// Add 1 because some of our UDP socket implementations do not read successfully
// when the packet length is equal to the read buffer size.
const size_t kReadBufferSize =
    static_cast<size_t>(quic::kMaxIncomingPacketSize + 1);
}  // namespace

QuicChromiumPacketReader::QuicChromiumPacketReader(
    DatagramClientSocket* socket,
    const quic::QuicClock* clock,
    Visitor* visitor,
    int yield_after_packets,
    quic::QuicTime::Delta yield_after_duration,
    const NetLogWithSource& net_log)
    : socket_(socket),
      visitor_(visitor),
      read_pending_(false),
      num_packets_read_(0),
      clock_(clock),
      yield_after_packets_(yield_after_packets),
      yield_after_duration_(yield_after_duration),
      yield_after_(quic::QuicTime::Infinite()),
      read_buffer_(base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize)),
      net_log_(net_log) {}

QuicChromiumPacketReader::~QuicChromiumPacketReader() {}

void QuicChromiumPacketReader::StartReading() {
  for (;;) {
    if (read_pending_)
      return;

    if (num_packets_read_ == 0)
      yield_after_ = clock_->Now() + yield_after_duration_;

    CHECK(socket_);
    read_pending_ = true;
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
    }
  }
}

size_t QuicChromiumPacketReader::EstimateMemoryUsage() const {
  return read_buffer_->size();
}

bool QuicChromiumPacketReader::ProcessReadResult(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    visitor_->OnReadError(result, socket_);
    return false;
  }

  quic::QuicReceivedPacket packet(read_buffer_->data(), result, clock_->Now());
  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  auto self = weak_factory_.GetWeakPtr();
  // Notifies the visitor that |this| reader gets a new packet, which may delete
  // |this| if |this| is a connectivity probing reader.
  return visitor_->OnPacket(packet, ToQuicSocketAddress(local_address),
                            ToQuicSocketAddress(peer_address)) &&
         self;
}

void QuicChromiumPacketReader::OnReadComplete(int result) {
  if (ProcessReadResult(result))
    StartReading();
}

}  // namespace net
