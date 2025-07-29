// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/cluster_builder.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "media/base/data_buffer.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

static const uint8_t kClusterHeader[] = {
    0x1F, 0x43, 0xB6, 0x75,                          // CLUSTER ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // cluster(size = 0)
    0xE7,                                            // Timecode ID
    0x88,                                            // timecode(size=8)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // timecode value
};

static const uint8_t kSimpleBlockHeader[] = {
    0xA3,                                            // SimpleBlock ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SimpleBlock(size = 0)
};

static const uint8_t kBlockGroupHeader[] = {
    0xA0,                                            // BlockGroup ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BlockGroup(size = 0)
    0x9B,                                            // BlockDuration ID
    0x88,                                            // BlockDuration(size = 8)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // duration
    0xA1,                                            // Block ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Block(size = 0)
};

static const uint8_t kBlockGroupHeaderWithoutBlockDuration[] = {
    0xA0,                                            // BlockGroup ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BlockGroup(size = 0)
    0xA1,                                            // Block ID
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Block(size = 0)
};

static const uint8_t kBlockGroupReferenceBlock[] = {
    0xFB,        // ReferenceBlock ID
    0x81, 0x00,  // ReferenceBlock (size=1, value=0)
};

enum {
  kClusterSizeOffset = 4,
  kClusterTimecodeOffset = 14,

  kSimpleBlockSizeOffset = 1,

  kBlockGroupSizeOffset = 1,
  kBlockGroupWithoutBlockDurationBlockSizeOffset = 10,
  kBlockGroupDurationOffset = 11,
  kBlockGroupBlockSizeOffset = 20,

  kInitialBufferSize = 32768,
};

Cluster::Cluster(base::HeapArray<uint8_t> data, int bytes_used)
    : data_(std::move(data)), bytes_used_(bytes_used) {}
Cluster::~Cluster() = default;

ClusterBuilder::ClusterBuilder() { Reset(); }
ClusterBuilder::~ClusterBuilder() = default;

void ClusterBuilder::SetClusterTimecode(int64_t cluster_timecode) {
  DCHECK_EQ(cluster_timecode_, -1);

  cluster_timecode_ = cluster_timecode;

  // Write the timecode into the header.
  uint8_t* buf = buffer_.data() + kClusterTimecodeOffset;
  for (int i = 7; i >= 0; --i) {
    buf[i] = cluster_timecode & 0xff;
    cluster_timecode >>= 8;
  }
}

void ClusterBuilder::AddSimpleBlock(int track_num,
                                    int64_t timecode,
                                    int flags,
                                    const uint8_t* data,
                                    int size) {
  int block_size = size + 4;
  size_t bytes_needed = sizeof(kSimpleBlockHeader) + block_size;
  if (bytes_needed > (buffer_.size() - bytes_used_)) {
    ExtendBuffer(bytes_needed);
  }

  uint8_t* buf = buffer_.data() + bytes_used_;
  int block_offset = bytes_used_;
  memcpy(buf, kSimpleBlockHeader, sizeof(kSimpleBlockHeader));
  UpdateUInt64(block_offset + kSimpleBlockSizeOffset, block_size);
  buf += sizeof(kSimpleBlockHeader);

  WriteBlock(buf, track_num, timecode, flags, data, size);

  bytes_used_ += bytes_needed;
}

void ClusterBuilder::AddBlockGroup(int track_num,
                                   int64_t timecode,
                                   int duration,
                                   int flags,
                                   bool is_key_frame,
                                   const uint8_t* data,
                                   int size) {
  AddBlockGroupInternal(track_num, timecode, true, duration, flags,
                        is_key_frame, data, size);
}

void ClusterBuilder::AddBlockGroupWithoutBlockDuration(int track_num,
                                                       int64_t timecode,
                                                       int flags,
                                                       bool is_key_frame,
                                                       const uint8_t* data,
                                                       int size) {
  AddBlockGroupInternal(track_num, timecode, false, 0, flags, is_key_frame,
                        data, size);
}

void ClusterBuilder::AddBlockGroupInternal(int track_num,
                                           int64_t timecode,
                                           bool include_block_duration,
                                           int duration,
                                           int flags,
                                           bool is_key_frame,
                                           const uint8_t* data,
                                           int size) {
  int block_size = size + 4;
  size_t bytes_needed = block_size;
  if (include_block_duration) {
    bytes_needed += sizeof(kBlockGroupHeader);
  } else {
    bytes_needed += sizeof(kBlockGroupHeaderWithoutBlockDuration);
  }
  if (!is_key_frame) {
    bytes_needed += sizeof(kBlockGroupReferenceBlock);
  }

  int block_group_size = bytes_needed - 9;

  if (bytes_needed > (buffer_.size() - bytes_used_)) {
    ExtendBuffer(bytes_needed);
  }

  uint8_t* buf = buffer_.data() + bytes_used_;
  int block_group_offset = bytes_used_;
  if (include_block_duration) {
    memcpy(buf, kBlockGroupHeader, sizeof(kBlockGroupHeader));
    UpdateUInt64(block_group_offset + kBlockGroupDurationOffset, duration);
    UpdateUInt64(block_group_offset + kBlockGroupBlockSizeOffset, block_size);
    buf += sizeof(kBlockGroupHeader);
  } else {
    memcpy(buf, kBlockGroupHeaderWithoutBlockDuration,
           sizeof(kBlockGroupHeaderWithoutBlockDuration));
    UpdateUInt64(
        block_group_offset + kBlockGroupWithoutBlockDurationBlockSizeOffset,
        block_size);
    buf += sizeof(kBlockGroupHeaderWithoutBlockDuration);
  }

  UpdateUInt64(block_group_offset + kBlockGroupSizeOffset, block_group_size);

  // Make sure the 4 most-significant bits are 0.
  // http://www.matroska.org/technical/specs/index.html#block_structure
  flags &= 0x0f;

  WriteBlock(buf, track_num, timecode, flags, data, size);
  buf += size + 4;

  if (!is_key_frame) {
    memcpy(buf, kBlockGroupReferenceBlock, sizeof(kBlockGroupReferenceBlock));
  }

  bytes_used_ += bytes_needed;
}

void ClusterBuilder::WriteBlock(uint8_t* buf,
                                int track_num,
                                int64_t timecode,
                                int flags,
                                const uint8_t* data,
                                int size) {
  DCHECK_GE(track_num, 0);
  DCHECK_LE(track_num, 126);
  DCHECK_GE(flags, 0);
  DCHECK_LE(flags, 0xff);
  DCHECK(data);
  DCHECK_GE(size, 0);  // For testing, allow 0-byte coded frames.
  DCHECK_NE(cluster_timecode_, -1);

  int64_t timecode_delta = timecode - cluster_timecode_;
  DCHECK_GE(timecode_delta, -32768);
  DCHECK_LE(timecode_delta, 32767);

  buf[0] = 0x80 | (track_num & 0x7F);
  buf[1] = (timecode_delta >> 8) & 0xff;
  buf[2] = timecode_delta & 0xff;
  buf[3] = flags & 0xff;
  memcpy(buf + 4, data, size);
}

std::unique_ptr<Cluster> ClusterBuilder::Finish() {
  DCHECK_NE(cluster_timecode_, -1);

  UpdateUInt64(kClusterSizeOffset, bytes_used_ - (kClusterSizeOffset + 8));

  std::unique_ptr<Cluster> ret(new Cluster(std::move(buffer_), bytes_used_));
  Reset();
  return ret;
}

std::unique_ptr<Cluster> ClusterBuilder::FinishWithUnknownSize() {
  DCHECK_NE(cluster_timecode_, -1);

  UpdateUInt64(kClusterSizeOffset, kWebMUnknownSize);

  std::unique_ptr<Cluster> ret(new Cluster(std::move(buffer_), bytes_used_));
  Reset();
  return ret;
}

void ClusterBuilder::Reset() {
  buffer_ = base::HeapArray<uint8_t>::Uninit(kInitialBufferSize);
  memcpy(buffer_.data(), kClusterHeader, sizeof(kClusterHeader));
  bytes_used_ = sizeof(kClusterHeader);
  cluster_timecode_ = -1;
}

void ClusterBuilder::ExtendBuffer(size_t bytes_needed) {
  size_t new_buffer_size = 2 * buffer_.size();

  while ((new_buffer_size - bytes_used_) < bytes_needed)
    new_buffer_size *= 2;

  auto new_buffer = base::HeapArray<uint8_t>::Uninit(new_buffer_size);

  memcpy(new_buffer.data(), buffer_.data(), bytes_used_);
  buffer_ = std::move(new_buffer);
}

void ClusterBuilder::UpdateUInt64(int offset, int64_t value) {
  DCHECK_LE(offset + 7u, buffer_.size());
  uint8_t* buf = buffer_.data() + offset;

  // Fill the last 7 bytes of size field in big-endian order.
  for (int i = 7; i > 0; i--) {
    buf[i] = value & 0xff;
    value >>= 8;
  }
}

}  // namespace media
