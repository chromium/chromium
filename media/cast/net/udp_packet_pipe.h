// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_UDP_PACKET_PIPE_H_
#define MEDIA_CAST_NET_UDP_PACKET_PIPE_H_

#include "media/cast/net/cast_transport_defines.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"

namespace media {
namespace cast {

// Reads UDP packets from the mojo data pipe. Assuming the size of each UDP
// packet was written before the packet itself was written into the pipe.
class UdpPacketPipeReader {
 public:
  explicit UdpPacketPipeReader(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  UdpPacketPipeReader(const UdpPacketPipeReader&) = delete;
  UdpPacketPipeReader& operator=(const UdpPacketPipeReader&) = delete;

  ~UdpPacketPipeReader();

  using ReadCB = base::OnceCallback<void(std::unique_ptr<Packet>)>;
  // Reads one UDP packet from the data pipe. This should only be called when
  // there is no other reading in process.
  void Read(ReadCB cb);

 private:
  // Reads packet payload from the data pipe. |success| indicates whether the
  // size of the packet was successfully read from the data pipe. |cb| will be
  // called after reading is done.
  void ReadPacketPayload(ReadCB cb, bool success);

  // Called by |data_pipe_reader_| when the reading completes.
  void OnPacketRead(std::unique_ptr<Packet> packet, ReadCB cb, bool success);

  MojoDataPipeReader data_pipe_reader_;

  uint16_t current_packet_size_;
};

// Writes UDP packets into the data mojo pipe. The size of each packet is
// written before the packet itself is written into the data pipe.
class UdpPacketPipeWriter {
 public:
  explicit UdpPacketPipeWriter(
      mojo::ScopedDataPipeProducerHandle producer_handle);

  UdpPacketPipeWriter(const UdpPacketPipeWriter&) = delete;
  UdpPacketPipeWriter& operator=(const UdpPacketPipeWriter&) = delete;

  ~UdpPacketPipeWriter();

  // Writes the |packet| into the mojo data pipe. |done_cb| will be
  // called when the writing completes. This should only be called when there is
  // no other writing in process.
  void Write(PacketRef packet, base::OnceClosure done_cb);

 private:
  // Writes the |packet| payload in the data pipe. |success| indicates whether
  // the size of the packet was successfully written in the data pipe. |done_cb|
  // will be called after writing is done.
  void WritePacketPayload(PacketRef packet,
                          base::OnceClosure done_cb,
                          bool success);

  // Called by |data_pipe_writer_| when the writing completes.
  void OnPacketWritten(PacketRef packet,
                       base::OnceClosure done_cb,
                       bool success);

  MojoDataPipeWriter data_pipe_writer_;

  uint16_t current_packet_size_ = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_UDP_PACKET_PIPE_H_
