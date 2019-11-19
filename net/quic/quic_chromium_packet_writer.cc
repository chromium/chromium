// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_packet_writer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

namespace {

enum NotReusableReason {
  NOT_REUSABLE_NULLPTR = 0,
  NOT_REUSABLE_TOO_SMALL = 1,
  NOT_REUSABLE_REF_COUNT = 2,
  NUM_NOT_REUSABLE_REASONS = 3,
};

const int kMaxRetries = 12;  // 2^12 = 4 seconds, which should be a LOT.

void RecordNotReusableReason(NotReusableReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.WritePacketNotReusable", reason,
                            NUM_NOT_REUSABLE_REASONS);
}

void RecordRetryCount(int count) {
  UMA_HISTOGRAM_EXACT_LINEAR("Net.QuicSession.RetryAfterWriteErrorCount2",
                             count, kMaxRetries + 1);
}

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("quic_chromium_packet_writer", R"(
        semantics {
          sender: "QUIC Packet Writer"
          description:
            "A QUIC packet is written to the wire based on a request from "
            "a QUIC stream."
          trigger:
            "A request from QUIC stream."
          data: "Any data sent by the stream."
          destination: OTHER
          destination_other: "Any destination choosen by the stream."
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "Essential for network access."
        }
        comments:
          "All requests that are received by QUIC streams have network traffic "
          "annotation, but the annotation is not passed to the writer function "
          "due to technial overheads. Please see QuicChromiumClientSession and "
          "QuicChromiumClientStream classes for references."
    )");

}  // namespace

QuicChromiumPacketWriter::ReusableIOBuffer::ReusableIOBuffer(size_t capacity)
    : IOBuffer(capacity), capacity_(capacity), size_(0) {}

QuicChromiumPacketWriter::ReusableIOBuffer::~ReusableIOBuffer() {}

void QuicChromiumPacketWriter::ReusableIOBuffer::Set(const char* buffer,
                                                     size_t buf_len) {
  CHECK_LE(buf_len, capacity_);
  CHECK(HasOneRef());
  size_ = buf_len;
  std::memcpy(data(), buffer, buf_len);
}

QuicChromiumPacketWriter::QuicChromiumPacketWriter() {}

QuicChromiumPacketWriter::QuicChromiumPacketWriter(
    DatagramClientSocket* socket,
    base::SequencedTaskRunner* task_runner)
    : socket_(socket),
      delegate_(nullptr),
      packet_(
          base::MakeRefCounted<ReusableIOBuffer>(quic::kMaxOutgoingPacketSize)),
      write_in_progress_(false),
      force_write_blocked_(false),
      retry_count_(0) {
  retry_timer_.SetTaskRunner(task_runner);
  write_callback_ = base::BindRepeating(
      &QuicChromiumPacketWriter::OnWriteComplete, weak_factory_.GetWeakPtr());
}

QuicChromiumPacketWriter::~QuicChromiumPacketWriter() {}

void QuicChromiumPacketWriter::set_force_write_blocked(
    bool force_write_blocked) {
  force_write_blocked_ = force_write_blocked;
  if (!IsWriteBlocked() && delegate_ != nullptr)
    delegate_->OnWriteUnblocked();
}

void QuicChromiumPacketWriter::SetPacket(const char* buffer, size_t buf_len) {
  if (UNLIKELY(!packet_)) {
    packet_ = base::MakeRefCounted<ReusableIOBuffer>(
        std::max(buf_len, static_cast<size_t>(quic::kMaxOutgoingPacketSize)));
    RecordNotReusableReason(NOT_REUSABLE_NULLPTR);
  }
  if (UNLIKELY(packet_->capacity() < buf_len)) {
    packet_ = base::MakeRefCounted<ReusableIOBuffer>(buf_len);
    RecordNotReusableReason(NOT_REUSABLE_TOO_SMALL);
  }
  if (UNLIKELY(!packet_->HasOneRef())) {
    packet_ = base::MakeRefCounted<ReusableIOBuffer>(
        std::max(buf_len, static_cast<size_t>(quic::kMaxOutgoingPacketSize)));
    RecordNotReusableReason(NOT_REUSABLE_REF_COUNT);
  }
  packet_->Set(buffer, buf_len);
}

quic::WriteResult QuicChromiumPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const quic::QuicIpAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    quic::PerPacketOptions* /*options*/) {
  DCHECK(!IsWriteBlocked());
  SetPacket(buffer, buf_len);
  return WritePacketToSocketImpl();
}

void QuicChromiumPacketWriter::WritePacketToSocket(
    scoped_refptr<ReusableIOBuffer> packet) {
  DCHECK(!force_write_blocked_);
  packet_ = std::move(packet);
  quic::WriteResult result = WritePacketToSocketImpl();
  if (result.error_code != ERR_IO_PENDING)
    OnWriteComplete(result.error_code);
}

quic::WriteResult QuicChromiumPacketWriter::WritePacketToSocketImpl() {
  base::TimeTicks now = base::TimeTicks::Now();

  int rv = socket_->Write(packet_.get(), packet_->size(), write_callback_,
                          kTrafficAnnotation);

  if (MaybeRetryAfterWriteError(rv))
    return quic::WriteResult(quic::WRITE_STATUS_BLOCKED_DATA_BUFFERED,
                             ERR_IO_PENDING);

  if (rv < 0 && rv != ERR_IO_PENDING && delegate_ != nullptr) {
    // If write error, then call delegate's HandleWriteError, which
    // may be able to migrate and rewrite packet on a new socket.
    // HandleWriteError returns the outcome of that rewrite attempt.
    rv = delegate_->HandleWriteError(rv, std::move(packet_));
    DCHECK(packet_ == nullptr);
  }

  quic::WriteStatus status = quic::WRITE_STATUS_OK;
  if (rv < 0) {
    if (rv != ERR_IO_PENDING) {
      status = quic::WRITE_STATUS_ERROR;
    } else {
      status = quic::WRITE_STATUS_BLOCKED_DATA_BUFFERED;
      write_in_progress_ = true;
    }
  }

  base::TimeDelta delta = base::TimeTicks::Now() - now;
  if (status == quic::WRITE_STATUS_OK) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.PacketWriteTime.Synchronous", delta);
  } else if (quic::IsWriteBlockedStatus(status)) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.PacketWriteTime.Asynchronous", delta);
  }

  return quic::WriteResult(status, rv);
}

void QuicChromiumPacketWriter::RetryPacketAfterNoBuffers() {
  DCHECK_GT(retry_count_, 0);
  quic::WriteResult result = WritePacketToSocketImpl();
  if (result.error_code != ERR_IO_PENDING)
    OnWriteComplete(result.error_code);
}

bool QuicChromiumPacketWriter::IsWriteBlocked() const {
  return (force_write_blocked_ || write_in_progress_);
}

void QuicChromiumPacketWriter::SetWritable() {
  write_in_progress_ = false;
}

void QuicChromiumPacketWriter::OnWriteComplete(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  write_in_progress_ = false;
  if (delegate_ == nullptr)
    return;

  if (rv < 0) {
    if (MaybeRetryAfterWriteError(rv))
      return;

    // If write error, then call delegate's HandleWriteError, which
    // may be able to migrate and rewrite packet on a new socket.
    // HandleWriteError returns the outcome of that rewrite attempt.
    rv = delegate_->HandleWriteError(rv, std::move(packet_));
    DCHECK(packet_ == nullptr);
    if (rv == ERR_IO_PENDING) {
      // Set write blocked back as write error is encountered in this writer,
      // delegate may be able to handle write error but this writer will never
      // be used to write any new data.
      write_in_progress_ = true;
      return;
    }
  }
  if (retry_count_ != 0) {
    RecordRetryCount(retry_count_);
    retry_count_ = 0;
  }

  if (rv < 0)
    delegate_->OnWriteError(rv);
  else if (!force_write_blocked_)
    delegate_->OnWriteUnblocked();
}

bool QuicChromiumPacketWriter::MaybeRetryAfterWriteError(int rv) {
  if (rv != ERR_NO_BUFFER_SPACE)
    return false;

  if (retry_count_ >= kMaxRetries) {
    RecordRetryCount(retry_count_);
    return false;
  }

  retry_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(UINT64_C(1) << retry_count_),
      base::BindOnce(&QuicChromiumPacketWriter::RetryPacketAfterNoBuffers,
                     weak_factory_.GetWeakPtr()));
  retry_count_++;
  write_in_progress_ = true;
  return true;
}

quic::QuicByteCount QuicChromiumPacketWriter::GetMaxPacketSize(
    const quic::QuicSocketAddress& peer_address) const {
  return quic::kMaxOutgoingPacketSize;
}

bool QuicChromiumPacketWriter::SupportsReleaseTime() const {
  return false;
}

bool QuicChromiumPacketWriter::IsBatchMode() const {
  return false;
}

char* QuicChromiumPacketWriter::GetNextWriteLocation(
    const quic::QuicIpAddress& self_address,
    const quic::QuicSocketAddress& peer_address) {
  return nullptr;
}

quic::WriteResult QuicChromiumPacketWriter::Flush() {
  return quic::WriteResult(quic::WRITE_STATUS_OK, 0);
}

}  // namespace net
