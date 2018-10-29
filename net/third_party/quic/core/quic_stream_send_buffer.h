// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_SEND_BUFFER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_SEND_BUFFER_H_

#include "net/third_party/quic/core/frames/quic_stream_frame.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_iovec.h"
#include "net/third_party/quic/platform/api/quic_mem_slice.h"

namespace quic {

namespace test {
class QuicStreamSendBufferPeer;
class QuicStreamPeer;
}  // namespace test

class QuicDataWriter;

// BufferedSlice comprises information of a piece of stream data stored in
// contiguous memory space. Please note, BufferedSlice is constructed when
// stream data is saved in send buffer and is removed when stream data is fully
// acked. It is move-only.
struct BufferedSlice {
  BufferedSlice(QuicMemSlice mem_slice, QuicStreamOffset offset);
  BufferedSlice(BufferedSlice&& other);
  BufferedSlice& operator=(BufferedSlice&& other);

  BufferedSlice(const BufferedSlice& other) = delete;
  BufferedSlice& operator=(const BufferedSlice& other) = delete;
  ~BufferedSlice();

  // Stream data of this data slice.
  QuicMemSlice slice;
  // Location of this data slice in the stream.
  QuicStreamOffset offset;
};

struct StreamPendingRetransmission {
  StreamPendingRetransmission(QuicStreamOffset offset, QuicByteCount length)
      : offset(offset), length(length) {}

  // Starting offset of this pending retransmission.
  QuicStreamOffset offset;
  // Length of this pending retransmission.
  QuicByteCount length;

  QUIC_EXPORT_PRIVATE bool operator==(
      const StreamPendingRetransmission& other) const;
};

// QuicStreamSendBuffer contains a list of QuicStreamDataSlices. New data slices
// are added to the tail of the list. Data slices are removed from the head of
// the list when they get fully acked. Stream data can be retrieved and acked
// across slice boundaries.
class QUIC_EXPORT_PRIVATE QuicStreamSendBuffer {
 public:
  explicit QuicStreamSendBuffer(QuicBufferAllocator* allocator);
  QuicStreamSendBuffer(const QuicStreamSendBuffer& other) = delete;
  QuicStreamSendBuffer(QuicStreamSendBuffer&& other) = delete;
  ~QuicStreamSendBuffer();

  // Save |data_length| of data starts at |iov_offset| in |iov| to send buffer.
  void SaveStreamData(const struct iovec* iov,
                      int iov_count,
                      size_t iov_offset,
                      QuicByteCount data_length);

  // Save |slice| to send buffer.
  void SaveMemSlice(QuicMemSlice slice);

  // Called when |bytes_consumed| bytes has been consumed by the stream.
  void OnStreamDataConsumed(size_t bytes_consumed);

  // Write |data_length| of data starts at |offset|.
  bool WriteStreamData(QuicStreamOffset offset,
                       QuicByteCount data_length,
                       QuicDataWriter* writer);

  // Called when data [offset, offset + data_length) is acked or removed as
  // stream is canceled. Removes fully acked data slice from send buffer. Set
  // |newly_acked_length|. Returns false if trying to ack unsent data.
  bool OnStreamDataAcked(QuicStreamOffset offset,
                         QuicByteCount data_length,
                         QuicByteCount* newly_acked_length);

  // Called when data [offset, offset + data_length) is considered as lost.
  void OnStreamDataLost(QuicStreamOffset offset, QuicByteCount data_length);

  // Called when data [offset, offset + length) was retransmitted.
  void OnStreamDataRetransmitted(QuicStreamOffset offset,
                                 QuicByteCount data_length);

  // Returns true if there is pending retransmissions.
  bool HasPendingRetransmission() const;

  // Returns next pending retransmissions.
  StreamPendingRetransmission NextPendingRetransmission() const;

  // Returns true if data [offset, offset + data_length) is outstanding and
  // waiting to be acked. Returns false otherwise.
  bool IsStreamDataOutstanding(QuicStreamOffset offset,
                               QuicByteCount data_length) const;

  // Number of data slices in send buffer.
  size_t size() const;

  QuicStreamOffset stream_offset() const { return stream_offset_; }

  uint64_t stream_bytes_written() const { return stream_bytes_written_; }

  uint64_t stream_bytes_outstanding() const {
    return stream_bytes_outstanding_;
  }

  const QuicIntervalSet<QuicStreamOffset>& bytes_acked() const {
    return bytes_acked_;
  }

  const QuicIntervalSet<QuicStreamOffset>& pending_retransmissions() const {
    return pending_retransmissions_;
  }

 private:
  friend class test::QuicStreamSendBufferPeer;
  friend class test::QuicStreamPeer;

  // Called when data within offset [start, end) gets acked. Frees fully
  // acked buffered slices if any. Returns false if the corresponding data does
  // not exist or has been acked.
  bool FreeMemSlices(QuicStreamOffset start, QuicStreamOffset end);

  // Cleanup empty slices in order from buffered_slices_.
  void CleanUpBufferedSlices();

  QuicDeque<BufferedSlice> buffered_slices_;

  // Offset of next inserted byte.
  QuicStreamOffset stream_offset_;

  QuicBufferAllocator* allocator_;

  // Bytes that have been consumed by the stream.
  uint64_t stream_bytes_written_;

  // Bytes that have been consumed and are waiting to be acked.
  uint64_t stream_bytes_outstanding_;

  // Offsets of data that has been acked.
  QuicIntervalSet<QuicStreamOffset> bytes_acked_;

  // Data considered as lost and needs to be retransmitted.
  QuicIntervalSet<QuicStreamOffset> pending_retransmissions_;

  // Index of slice which contains data waiting to be written for the first
  // time. -1 if send buffer is empty or all data has been written.
  int32_t write_index_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_SEND_BUFFER_H_
