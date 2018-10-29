// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_SEQUENCER_BUFFER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_SEQUENCER_BUFFER_H_

// QuicStreamSequencerBuffer is a circular stream buffer with random write and
// in-sequence read. It consists of a vector of pointers pointing
// to memory blocks created as needed and an interval set recording received
// data.
// - Data are written in with offset indicating where it should be in the
// stream, and the buffer grown as needed (up to the maximum buffer capacity),
// without expensive copying (extra blocks are allocated).
// - Data can be read from the buffer if there is no gap before it,
// and the buffer shrinks as the data are consumed.
// - An upper limit on the number of blocks in the buffer provides an upper
//   bound on memory use.
//
// This class is thread-unsafe.
//
// QuicStreamSequencerBuffer maintains a concept of the readable region, which
// contains all written data that has not been read.
// It promises stability of the underlying memory addresses in the readable
// region, so pointers into it can be maintained, and the offset of a pointer
// from the start of the read region can be calculated.
//
// Expected Use:
//  QuicStreamSequencerBuffer buffer(2.5 * 8 * 1024);
//  QuicString source(1024, 'a');
//  QuicStringPiece string_piece(source.data(), source.size());
//  size_t written = 0;
//  buffer.OnStreamData(800, string_piece, GetEpollClockNow(), &written);
//  source = QuicString{800, 'b'};
//  QuicStringPiece string_piece1(source.data(), 800);
//  // Try to write to [1, 801), but should fail due to overlapping,
//  // res should be QUIC_INVALID_STREAM_DATA
//  auto res = buffer.OnStreamData(1, string_piece1, &written));
//  // write to [0, 800), res should be QUIC_NO_ERROR
//  auto res = buffer.OnStreamData(0, string_piece1, GetEpollClockNow(),
//                                  &written);
//
//  // Read into a iovec array with total capacity of 120 bytes.
//  char dest[120];
//  iovec iovecs[3]{iovec{dest, 40}, iovec{dest + 40, 40},
//                  iovec{dest + 80, 40}};
//  size_t read = buffer.Readv(iovecs, 3);
//
//  // Get single readable region.
//  iovec iov;
//  buffer.GetReadableRegion(iov);
//
//  // Get readable regions from [256, 1024) and consume some of it.
//  iovec iovs[2];
//  int iov_count = buffer.GetReadableRegions(iovs, 2);
//  // Consume some bytes in iovs, returning number of bytes having been
//  consumed.
//  size_t consumed = consume_iovs(iovs, iov_count);
//  buffer.MarkConsumed(consumed);

#include <cstddef>
#include <functional>
#include <list>
#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_iovec.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

namespace test {
class QuicStreamSequencerBufferPeer;
}  // namespace test

class QUIC_EXPORT_PRIVATE QuicStreamSequencerBuffer {
 public:
  // Size of blocks used by this buffer.
  // Choose 8K to make block large enough to hold multiple frames, each of
  // which could be up to 1.5 KB.
  static const size_t kBlockSizeBytes = 8 * 1024;  // 8KB

  // The basic storage block used by this buffer.
  struct BufferBlock {
    char buffer[kBlockSizeBytes];
  };

  explicit QuicStreamSequencerBuffer(size_t max_capacity_bytes);
  QuicStreamSequencerBuffer(const QuicStreamSequencerBuffer&) = delete;
  QuicStreamSequencerBuffer& operator=(const QuicStreamSequencerBuffer&) =
      delete;
  ~QuicStreamSequencerBuffer();

  // Free the space used to buffer data.
  void Clear();

  // Returns true if there is nothing to read in this buffer.
  bool Empty() const;

  // Called to buffer new data received for this stream.  If the data was
  // successfully buffered, returns QUIC_NO_ERROR and stores the number of
  // bytes buffered in |bytes_buffered|. Returns an error otherwise.
  QuicErrorCode OnStreamData(QuicStreamOffset offset,
                             QuicStringPiece data,
                             size_t* bytes_buffered,
                             QuicString* error_details);

  // Reads from this buffer into given iovec array, up to number of iov_len
  // iovec objects and returns the number of bytes read.
  QuicErrorCode Readv(const struct iovec* dest_iov,
                      size_t dest_count,
                      size_t* bytes_read,
                      QuicString* error_details);

  // Returns the readable region of valid data in iovec format. The readable
  // region is the buffer region where there is valid data not yet read by
  // client.
  // Returns the number of iovec entries in |iov| which were populated.
  // If the region is empty, one iovec entry with 0 length
  // is returned, and the function returns 0. If there are more readable
  // regions than |iov_size|, the function only processes the first
  // |iov_size| of them.
  int GetReadableRegions(struct iovec* iov, int iov_len) const;

  // Fills in one iovec with data from the next readable region.
  // Returns false if there is no readable region available.
  bool GetReadableRegion(iovec* iov) const;

  // Called after GetReadableRegions() to free up |bytes_used| space if these
  // bytes are processed.
  // Pre-requisite: bytes_used <= available bytes to read.
  bool MarkConsumed(size_t bytes_buffered);

  // Deletes and records as consumed any buffered data and clear the buffer.
  // (To be called only after sequencer's StopReading has been called.)
  size_t FlushBufferedFrames();

  // Free the memory of buffered data.
  void ReleaseWholeBuffer();

  // Whether there are bytes can be read out.
  bool HasBytesToRead() const;

  // Count how many bytes have been consumed (read out of buffer).
  QuicStreamOffset BytesConsumed() const;

  // Count how many bytes are in buffer at this moment.
  size_t BytesBuffered() const;

  // Returns number of bytes available to be read out.
  size_t ReadableBytes() const;

 private:
  friend class test::QuicStreamSequencerBufferPeer;

  // Copies |data| to blocks_, sets |bytes_copy|. Returns true if the copy is
  // successful. Otherwise, sets |error_details| and returns false.
  bool CopyStreamData(QuicStreamOffset offset,
                      QuicStringPiece data,
                      size_t* bytes_copy,
                      QuicString* error_details);

  // Dispose the given buffer block.
  // After calling this method, blocks_[index] is set to nullptr
  // in order to indicate that no memory set is allocated for that block.
  // Returns true on success, false otherwise.
  bool RetireBlock(size_t index);

  // Should only be called after the indexed block is read till the end of the
  // block or missing data has been reached.
  // If the block at |block_index| contains no buffered data, the block
  // should be retired.
  // Return false on success, or false otherwise.
  bool RetireBlockIfEmpty(size_t block_index);

  // Calculate the capacity of block at specified index.
  // Return value should be either kBlockSizeBytes for non-trailing blocks and
  // max_buffer_capacity % kBlockSizeBytes for trailing block.
  size_t GetBlockCapacity(size_t index) const;

  // Does not check if offset is within reasonable range.
  size_t GetBlockIndex(QuicStreamOffset offset) const;

  // Given an offset in the stream, return the offset from the beginning of the
  // block which contains this data.
  size_t GetInBlockOffset(QuicStreamOffset offset) const;

  // Get offset relative to index 0 in logical 1st block to start next read.
  size_t ReadOffset() const;

  // Get the index of the logical 1st block to start next read.
  size_t NextBlockToRead() const;

  // Returns offset of first missing byte.
  QuicStreamOffset FirstMissingByte() const;

  // Returns offset of highest received byte + 1.
  QuicStreamOffset NextExpectedByte() const;

  // Return |gaps_| as a string: [1024, 1500) [1800, 2048)... for debugging.
  QuicString GapsDebugString();

  // Return all received frames as a string in same format as GapsDebugString();
  QuicString ReceivedFramesDebugString();

  // The maximum total capacity of this buffer in byte, as constructed.
  const size_t max_buffer_capacity_bytes_;

  // How many blocks this buffer would need when it reaches full capacity.
  const size_t blocks_count_;

  // Number of bytes read out of buffer.
  QuicStreamOffset total_bytes_read_;

  // An ordered, variable-length list of blocks, with the length limited
  // such that the number of blocks never exceeds blocks_count_.
  // Each list entry can hold up to kBlockSizeBytes bytes.
  std::unique_ptr<BufferBlock* []> blocks_;

  // Number of bytes in buffer.
  size_t num_bytes_buffered_;

  // Currently received data.
  QuicIntervalSet<QuicStreamOffset> bytes_received_;
};
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_STREAM_SEQUENCER_BUFFER_H_
