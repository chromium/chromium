// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_packet_reader.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/quic/address_utils.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"

namespace net {

namespace {
// Add 1 because some of our UDP socket implementations do not read successfully
// when the packet length is equal to the read buffer size.
const size_t kReadBufferSize =
    static_cast<size_t>(quic::kMaxIncomingPacketSize + 1);

constexpr size_t kNumPacketsToRead = 32;
}  // namespace

QuicChromiumPacketReader::QuicChromiumPacketReader(
    std::unique_ptr<DatagramClientSocket> socket,
    const quic::QuicClock* clock,
    Visitor* visitor,
    int yield_after_packets,
    quic::QuicTime::Delta yield_after_duration,
    const NetLogWithSource& net_log)
    : socket_(std::move(socket)),
      visitor_(visitor),
      use_read_multiple_(
          base::FeatureList::IsEnabled(features::kQuicUseReadMultiple)),
      clock_(clock),
      yield_after_packets_(yield_after_packets),
      yield_after_duration_(yield_after_duration),
      yield_after_(quic::QuicTime::Infinite()),
      read_buffer_(base::MakeRefCounted<IOBufferWithSize>(
          use_read_multiple_ ? kNumPacketsToRead * kReadBufferSize
                             : kReadBufferSize)),
      net_log_(net_log) {}

QuicChromiumPacketReader::~QuicChromiumPacketReader() = default;

// Do not start a new read if the reader is already busy (either waiting
// for a socket read to complete, or processing buffered packets from a
// previous batch). During these states, `read_pending_` remains true to
// protect `read_buffer_` from being overwritten and corrupting
// unprocessed packets in `pending_datagrams_`.
// We can safely return because when the pending read or processing
// completes, the loop will automatically resume and call StartReading()
// again.
void QuicChromiumPacketReader::StartReading() {
  if (read_pending_) {
    return;
  }

  for (;;) {

    if (num_packets_read_ == 0)
      yield_after_ = clock_->Now() + yield_after_duration_;

    CHECK(socket_);
    read_pending_ = true;

    if (use_read_multiple_) {
      auto result = socket_->ReadMultiple(
          read_buffer_.get(), read_buffer_->size(), kReadBufferSize,
          base::BindOnce(&QuicChromiumPacketReader::OnReadMultipleComplete,
                         weak_factory_.GetWeakPtr()));
      if (!result.has_value() && result.error() == ERR_IO_PENDING) {
        // If the read is pending, reset the packet read count since we are
        // yielding the thread. The read loop will resume when the asynchronous
        // callback (OnReadMultipleComplete) is triggered.
        num_packets_read_ = 0;
        return;
      }
      // The read completed synchronously (either successfully or with a
      // immediate error). Process the result.
      if (ProcessReadMultipleResult(std::move(result))) {
        // All packets from the synchronous read were processed successfully
        // without yielding. Loop again to perform another read.
        continue;
      }
      return;
    } else {
      int rv = socket_->Read(
          read_buffer_.get(), read_buffer_->size(),
          base::BindOnce(&QuicChromiumPacketReader::OnReadComplete,
                         weak_factory_.GetWeakPtr()));
      UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.AsyncRead", rv == ERR_IO_PENDING);
      if (rv == ERR_IO_PENDING) {
        num_packets_read_ = 0;
        return;
      }

      if (ShouldYield()) {
        // Data was read, process it.
        // Schedule the work through the message loop to 1) prevent infinite
        // recursion and 2) avoid blocking the thread for too long.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&QuicChromiumPacketReader::OnReadComplete,
                                      weak_factory_.GetWeakPtr(), rv));
        return;
      } else {
        if (!ProcessReadResult(rv)) {
          return;
        }
      }
    }
  }
}

void QuicChromiumPacketReader::CloseSocket() {
  socket_->Close();
}

static_assert(static_cast<EcnCodePoint>(quic::ECN_NOT_ECT) == ECN_NOT_ECT &&
                  static_cast<EcnCodePoint>(quic::ECN_ECT1) == ECN_ECT1 &&
                  static_cast<EcnCodePoint>(quic::ECN_ECT0) == ECN_ECT0 &&
                  static_cast<EcnCodePoint>(quic::ECN_CE) == ECN_CE,
              "Mismatch ECN codepoint values");
bool QuicChromiumPacketReader::ProcessReadResult(int result) {
  read_pending_ = false;
  if (result <= 0 && net_log_.IsCapturing()) {
    net_log_.AddEventWithIntParams(NetLogEventType::QUIC_READ_ERROR,
                                   "net_error", result);
  }
  if (result == 0) {
    // 0-length UDP packets are legal but useless, ignore them.
    return true;
  }
  if (result == ERR_MSG_TOO_BIG) {
    // This indicates that we received a UDP packet larger than our receive
    // buffer, ignore it.
    return true;
  }
  if (result < 0) {
    // Report all other errors to the visitor.
    return visitor_->OnReadError(result, socket_.get());
  }

  DscpAndEcn tos = socket_->GetLastTos();
  quic::QuicEcnCodepoint ecn = static_cast<quic::QuicEcnCodepoint>(tos.ecn);
  quic::QuicReceivedPacket packet(read_buffer_->data(), result, clock_->Now(),
                                  /*owns_buffer=*/false, /*ttl=*/0,
                                  /*ttl_valid=*/true,
                                  /*packet_headers=*/nullptr,
                                  /*headers_length=*/0,
                                  /*owns_header_buffer=*/false, ecn);
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

bool QuicChromiumPacketReader::ShouldYield() {
  if (++num_packets_read_ > yield_after_packets_ ||
      clock_->Now() > yield_after_) {
    num_packets_read_ = 0;
    return true;
  }
  return false;
}

void QuicChromiumPacketReader::OnReadComplete(int result) {
  if (ProcessReadResult(result)) {
    StartReading();
  }
}

void QuicChromiumPacketReader::OnReadMultipleComplete(
    base::expected<DatagramsMetadata, Error> result) {
  if (ProcessReadMultipleResult(std::move(result))) {
    StartReading();
  }
}

bool QuicChromiumPacketReader::ProcessReadMultipleResult(
    base::expected<DatagramsMetadata, Error> result) {
  if (!result.has_value()) {
    read_pending_ = false;
    int error = result.error();
    if (net_log_.IsCapturing()) {
      net_log_.AddEventWithIntParams(NetLogEventType::QUIC_READ_ERROR,
                                     "net_error", error);
    }
    // We must never continue reading if an error occurs. This is a deliberate
    // departure from the single-read path (where ERR_MSG_TOO_BIG is ignored).
    // In ReadMultiple, some packets might have been successfully read into the
    // buffer before the error was encountered (for example, if recvmmsg reads 3
    // packets successfully, but the 4th one is truncated, the socket returns a
    // single ERR_MSG_TOO_BIG error for the entire batch). If we were to ignore
    // this error and continue reading, the successfully read packets (1 to 3)
    // would be silently dropped.
    visitor_->OnReadError(error, socket_.get());
    return false;
  }

  auto& datagrams = result.value();
  CHECK(!datagrams.empty());

  if (base::ShouldRecordSubsampledMetric(0.1)) {
    UMA_HISTOGRAM_EXACT_LINEAR("Net.QuicSession.ReadMultipleNumPackets",
                               datagrams.size(), kNumPacketsToRead + 1);
  }

  for (auto& datagram : datagrams) {
    pending_datagrams_.push_back(std::move(datagram));
  }

  return ProcessPendingPackets();
}

bool QuicChromiumPacketReader::ProcessPendingPackets() {
  base::WeakPtr<QuicChromiumPacketReader> weak_this =
      weak_factory_.GetWeakPtr();
  while (!pending_datagrams_.empty()) {
    if (num_packets_read_ == 0) {
      yield_after_ = clock_->Now() + yield_after_duration_;
    }

    DatagramMetadata datagram = pending_datagrams_.front();
    pending_datagrams_.pop_front();
    if (!ProcessSingleDatagram(datagram)) {
      // If `this` was destroyed during packet processing (which can happen
      // if `visitor_->OnPacket()` triggers session teardown or connection
      // closure), we must return immediately to avoid a use-after-free
      // when accessing `pending_datagrams_` or other member variables.
      if (!weak_this) {
        return false;
      }
      pending_datagrams_.clear();
      return false;
    }

    if (ShouldYield()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &QuicChromiumPacketReader::ResumeProcessingPendingPackets,
              weak_factory_.GetWeakPtr()));
      return false;
    }
  }

  // Reached when all packets in the current batch have been processed.
  read_pending_ = false;
  return true;
}

void QuicChromiumPacketReader::ResumeProcessingPendingPackets() {
  if (ProcessPendingPackets()) {
    StartReading();
  }
}

bool QuicChromiumPacketReader::ProcessSingleDatagram(
    const DatagramMetadata& datagram) {
  // Skip empty datagrams but continue processing the rest of the batch.
  if (datagram.length == 0) {
    return true;
  }

  DscpAndEcn tos = TosToDscpAndEcn(datagram.tos);
  quic::QuicEcnCodepoint ecn = static_cast<quic::QuicEcnCodepoint>(tos.ecn);
  auto packet_span =
      read_buffer_->span().subspan(datagram.offset, datagram.length);
  quic::QuicReceivedPacket packet(
      reinterpret_cast<const char*>(packet_span.data()), datagram.length,
      clock_->Now(),
      /*owns_buffer=*/false, /*ttl=*/0, /*ttl_valid=*/true,
      /*packet_headers=*/nullptr, /*headers_length=*/0,
      /*owns_header_buffer=*/false, ecn);

  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  auto self = weak_factory_.GetWeakPtr();
  return visitor_->OnPacket(packet, ToQuicSocketAddress(local_address),
                            ToQuicSocketAddress(peer_address)) &&
         self;
}

}  // namespace net
