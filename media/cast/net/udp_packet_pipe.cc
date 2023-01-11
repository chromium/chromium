// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/udp_packet_pipe.h"

#include <cstring>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace media {
namespace cast {

// UdpPacketPipeReader

UdpPacketPipeReader::UdpPacketPipeReader(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : data_pipe_reader_(std::move(consumer_handle)) {
  DCHECK(data_pipe_reader_.IsPipeValid());
}

UdpPacketPipeReader::~UdpPacketPipeReader() {}

void UdpPacketPipeReader::Read(ReadCB cb) {
  DCHECK(!cb.is_null());
  data_pipe_reader_.Read(reinterpret_cast<uint8_t*>(&current_packet_size_),
                         sizeof(uint16_t),
                         base::BindOnce(&UdpPacketPipeReader::ReadPacketPayload,
                                        base::Unretained(this), std::move(cb)));
}

void UdpPacketPipeReader::ReadPacketPayload(ReadCB cb, bool success) {
  if (!success) {
    OnPacketRead(nullptr, std::move(cb), false);
    return;
  }
  auto packet = std::make_unique<Packet>(current_packet_size_);
  uint8_t* packet_data = packet->data();
  data_pipe_reader_.Read(
      packet_data, current_packet_size_,
      base::BindOnce(&UdpPacketPipeReader::OnPacketRead, base::Unretained(this),
                     std::move(packet), std::move(cb)));
}

void UdpPacketPipeReader::OnPacketRead(std::unique_ptr<Packet> packet,
                                       ReadCB cb,
                                       bool success) {
  DCHECK(!cb.is_null());
  if (!success) {
    VLOG(1) << "Failed when reading the packet.";
    // The data pipe should have been closed.
  }
  std::move(cb).Run(std::move(packet));
}

// UdpPacketPipeWriter

UdpPacketPipeWriter::UdpPacketPipeWriter(
    mojo::ScopedDataPipeProducerHandle producer_handle)
    : data_pipe_writer_(std::move(producer_handle)) {
  DCHECK(data_pipe_writer_.IsPipeValid());
}

UdpPacketPipeWriter::~UdpPacketPipeWriter() {}

void UdpPacketPipeWriter::Write(PacketRef packet, base::OnceClosure done_cb) {
  DCHECK(done_cb);
  current_packet_size_ = packet->data.size();
  data_pipe_writer_.Write(
      reinterpret_cast<uint8_t*>(&current_packet_size_), sizeof(uint16_t),
      base::BindOnce(&UdpPacketPipeWriter::WritePacketPayload,
                     base::Unretained(this), std::move(packet),
                     std::move(done_cb)));
}

void UdpPacketPipeWriter::WritePacketPayload(PacketRef packet,
                                             base::OnceClosure done_cb,
                                             bool success) {
  if (!success) {
    OnPacketWritten(PacketRef(), std::move(done_cb), false);
    return;
  }
  const uint8_t* buffer = packet->data.data();
  const int buffer_size = packet->data.size();
  data_pipe_writer_.Write(
      buffer, buffer_size,
      base::BindOnce(&UdpPacketPipeWriter::OnPacketWritten,
                     base::Unretained(this), std::move(packet),
                     std::move(done_cb)));
}

void UdpPacketPipeWriter::OnPacketWritten(PacketRef packet,
                                          base::OnceClosure done_cb,
                                          bool success) {
  DCHECK(done_cb);
  if (!success) {
    VLOG(1) << "Failed to write the packet.";
    // The data pipe should have been closed.
  }
  std::move(done_cb).Run();
}

}  // namespace cast
}  // namespace media
