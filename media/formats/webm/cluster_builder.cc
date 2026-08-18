// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/cluster_builder.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
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
  buffer_.subspan(kClusterTimecodeOffset, sizeof(cluster_timecode))
      .copy_from(base::I64ToBigEndian(cluster_timecode));
}

void ClusterBuilder::AddSimpleBlock(int track_num,
                                    int64_t timecode,
                                    int flags,
                                    base::span<const uint8_t> data) {
  const size_t block_size = data.size() + 4u;
  const size_t bytes_needed = sizeof(kSimpleBlockHeader) + block_size;
  if (bytes_needed > (buffer_.size() - bytes_used_)) {
    ExtendBuffer(bytes_needed);
  }

  const size_t block_offset = bytes_used_;
  base::SpanWriter writer(buffer_.subspan(block_offset, bytes_needed));
  CHECK(writer.Write(kSimpleBlockHeader));
  UpdateUInt64(block_offset + kSimpleBlockSizeOffset,
               base::checked_cast<int64_t>(block_size));
  WriteBlock(writer, track_num, timecode, flags, data);
  CHECK_EQ(writer.remaining(), 0u);

  bytes_used_ += bytes_needed;
}

void ClusterBuilder::AddBlockGroup(int track_num,
                                   int64_t timecode,
                                   int duration,
                                   int flags,
                                   bool is_key_frame,
                                   base::span<const uint8_t> data) {
  AddBlockGroupInternal(track_num, timecode, true, duration, flags,
                        is_key_frame, data);
}

void ClusterBuilder::AddBlockGroupWithoutBlockDuration(
    int track_num,
    int64_t timecode,
    int flags,
    bool is_key_frame,
    base::span<const uint8_t> data) {
  AddBlockGroupInternal(track_num, timecode, false, 0, flags, is_key_frame,
                        data);
}

void ClusterBuilder::AddBlockGroupInternal(int track_num,
                                           int64_t timecode,
                                           bool include_block_duration,
                                           int duration,
                                           int flags,
                                           bool is_key_frame,
                                           base::span<const uint8_t> data) {
  const size_t block_size = data.size() + 4u;
  size_t bytes_needed = block_size;
  if (include_block_duration) {
    bytes_needed += sizeof(kBlockGroupHeader);
  } else {
    bytes_needed += sizeof(kBlockGroupHeaderWithoutBlockDuration);
  }
  if (!is_key_frame) {
    bytes_needed += sizeof(kBlockGroupReferenceBlock);
  }

  const size_t block_group_size = bytes_needed - 9u;

  if (bytes_needed > (buffer_.size() - bytes_used_)) {
    ExtendBuffer(bytes_needed);
  }

  const size_t block_group_offset = bytes_used_;
  base::SpanWriter writer(buffer_.subspan(block_group_offset, bytes_needed));
  if (include_block_duration) {
    CHECK(writer.Write(kBlockGroupHeader));
    UpdateUInt64(block_group_offset + kBlockGroupDurationOffset, duration);
    UpdateUInt64(block_group_offset + kBlockGroupBlockSizeOffset,
                 base::checked_cast<int64_t>(block_size));
  } else {
    CHECK(writer.Write(kBlockGroupHeaderWithoutBlockDuration));
    UpdateUInt64(
        block_group_offset + kBlockGroupWithoutBlockDurationBlockSizeOffset,
        base::checked_cast<int64_t>(block_size));
  }

  UpdateUInt64(block_group_offset + kBlockGroupSizeOffset,
               base::checked_cast<int64_t>(block_group_size));

  // Make sure the 4 most-significant bits are 0.
  // http://www.matroska.org/technical/specs/index.html#block_structure
  flags &= 0x0f;

  WriteBlock(writer, track_num, timecode, flags, data);

  if (!is_key_frame) {
    CHECK(writer.Write(kBlockGroupReferenceBlock));
  }
  CHECK_EQ(writer.remaining(), 0u);

  bytes_used_ += bytes_needed;
}

void ClusterBuilder::WriteBlock(base::SpanWriter<uint8_t>& writer,
                                int track_num,
                                int64_t timecode,
                                int flags,
                                base::span<const uint8_t> data) {
  DCHECK_GE(track_num, 0);
  DCHECK_LE(track_num, 126);
  DCHECK_GE(flags, 0);
  DCHECK_LE(flags, 0xff);
  DCHECK_NE(cluster_timecode_, -1);

  const int64_t timecode_delta = timecode - cluster_timecode_;
  DCHECK_GE(timecode_delta, -32768);
  DCHECK_LE(timecode_delta, 32767);

  CHECK(writer.Write(static_cast<uint8_t>(0x80 | (track_num & 0x7F))));
  CHECK(writer.WriteI16BigEndian(base::checked_cast<int16_t>(timecode_delta)));
  CHECK(writer.Write(base::checked_cast<uint8_t>(flags)));
  CHECK(writer.Write(data));
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
  buffer_.copy_prefix_from(kClusterHeader);
  bytes_used_ = sizeof(kClusterHeader);
  cluster_timecode_ = -1;
}

void ClusterBuilder::ExtendBuffer(size_t bytes_needed) {
  size_t new_buffer_size = 2 * buffer_.size();

  while ((new_buffer_size - bytes_used_) < bytes_needed) {
    new_buffer_size *= 2;
  }

  auto new_buffer = base::HeapArray<uint8_t>::Uninit(new_buffer_size);
  new_buffer.copy_prefix_from(buffer_.first(bytes_used_));
  buffer_ = std::move(new_buffer);
}

void ClusterBuilder::UpdateUInt64(size_t offset, int64_t value) {
  DCHECK_LE(offset + 8u, buffer_.size());

  // Fill the last 7 bytes of size field in big-endian order.
  const auto bytes = base::I64ToBigEndian(value);
  buffer_.subspan(offset + 1u, 7u).copy_from(base::span(bytes).last<7u>());
}

}  // namespace media
