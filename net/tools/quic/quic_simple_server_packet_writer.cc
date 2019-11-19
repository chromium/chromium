// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server_packet_writer.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/address_utils.h"
#include "net/socket/udp_server_socket.h"
#include "net/third_party/quiche/src/quic/core/quic_dispatcher.h"

namespace net {

QuicSimpleServerPacketWriter::QuicSimpleServerPacketWriter(
    UDPServerSocket* socket,
    quic::QuicDispatcher* dispatcher)
    : socket_(socket), dispatcher_(dispatcher), write_blocked_(false) {}

QuicSimpleServerPacketWriter::~QuicSimpleServerPacketWriter() = default;

void QuicSimpleServerPacketWriter::OnWriteComplete(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  write_blocked_ = false;
  quic::WriteResult result(
      rv < 0 ? quic::WRITE_STATUS_ERROR : quic::WRITE_STATUS_OK, rv);
  if (!callback_.is_null()) {
    std::move(callback_).Run(result);
  }
  dispatcher_->OnCanWrite();
}

bool QuicSimpleServerPacketWriter::IsWriteBlocked() const {
  return write_blocked_;
}

void QuicSimpleServerPacketWriter::SetWritable() {
  write_blocked_ = false;
}

quic::WriteResult QuicSimpleServerPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const quic::QuicIpAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    quic::PerPacketOptions* options) {
  scoped_refptr<StringIOBuffer> buf =
      base::MakeRefCounted<StringIOBuffer>(std::string(buffer, buf_len));
  DCHECK(!IsWriteBlocked());
  int rv;
  if (buf_len <= static_cast<size_t>(std::numeric_limits<int>::max())) {
    rv = socket_->SendTo(
        buf.get(), static_cast<int>(buf_len), ToIPEndPoint(peer_address),
        base::BindOnce(&QuicSimpleServerPacketWriter::OnWriteComplete,
                       weak_factory_.GetWeakPtr()));
  } else {
    rv = ERR_MSG_TOO_BIG;
  }
  quic::WriteStatus status = quic::WRITE_STATUS_OK;
  if (rv < 0) {
    if (rv != ERR_IO_PENDING) {
      base::UmaHistogramSparse("Net.quic::QuicSession.WriteError", -rv);
      status = quic::WRITE_STATUS_ERROR;
    } else {
      status = quic::WRITE_STATUS_BLOCKED_DATA_BUFFERED;
      write_blocked_ = true;
    }
  }
  return quic::WriteResult(status, rv);
}

quic::QuicByteCount QuicSimpleServerPacketWriter::GetMaxPacketSize(
    const quic::QuicSocketAddress& peer_address) const {
  return quic::kMaxOutgoingPacketSize;
}

bool QuicSimpleServerPacketWriter::SupportsReleaseTime() const {
  return false;
}

bool QuicSimpleServerPacketWriter::IsBatchMode() const {
  return false;
}

char* QuicSimpleServerPacketWriter::GetNextWriteLocation(
    const quic::QuicIpAddress& self_address,
    const quic::QuicSocketAddress& peer_address) {
  return nullptr;
}

quic::WriteResult QuicSimpleServerPacketWriter::Flush() {
  return quic::WriteResult(quic::WRITE_STATUS_OK, 0);
}

}  // namespace net
