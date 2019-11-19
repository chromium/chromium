// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/macros.h"
#include "media/video/h264_parser.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#endif

namespace media {
namespace test {

EncodedDataHelper::EncodedDataHelper(const std::vector<uint8_t>& stream,
                                     VideoCodecProfile profile)
    : data_(std::string(reinterpret_cast<const char*>(stream.data()),
                        stream.size())),
      profile_(profile) {}

EncodedDataHelper::~EncodedDataHelper() {
  base::STLClearObject(&data_);
}

bool EncodedDataHelper::IsNALHeader(const std::string& data, size_t pos) {
  return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 0 &&
         data[pos + 3] == 1;
}

std::string EncodedDataHelper::GetBytesForNextData() {
  switch (VideoCodecProfileToVideoCodec(profile_)) {
    case kCodecH264:
      return GetBytesForNextFragment();
    case kCodecVP8:
    case kCodecVP9:
      return GetBytesForNextFrame();
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string EncodedDataHelper::GetBytesForNextFragment() {
  if (next_pos_to_decode_ == 0) {
    size_t skipped_fragments_count = 0;
    if (!LookForSPS(&skipped_fragments_count)) {
      next_pos_to_decode_ = 0;
      return std::string();
    }
    num_skipped_fragments_ += skipped_fragments_count;
  }

  size_t start_pos = next_pos_to_decode_;
  size_t next_nalu_pos = GetBytesForNextNALU(start_pos);

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = next_nalu_pos;
  return data_.substr(start_pos, next_nalu_pos - start_pos);
}

size_t EncodedDataHelper::GetBytesForNextNALU(size_t start_pos) {
  size_t pos = start_pos;
  if (pos + 4 > data_.size())
    return pos;
  if (!IsNALHeader(data_, pos)) {
    ADD_FAILURE();
    return std::numeric_limits<std::size_t>::max();
  }
  pos += 4;
  while (pos + 4 <= data_.size() && !IsNALHeader(data_, pos)) {
    ++pos;
  }
  if (pos + 3 >= data_.size())
    pos = data_.size();
  return pos;
}

bool EncodedDataHelper::LookForSPS(size_t* skipped_fragments_count) {
  *skipped_fragments_count = 0;
  while (next_pos_to_decode_ + 4 < data_.size()) {
    if ((data_[next_pos_to_decode_ + 4] & 0x1f) == 0x7) {
      return true;
    }
    *skipped_fragments_count += 1;
    next_pos_to_decode_ = GetBytesForNextNALU(next_pos_to_decode_);
  }
  return false;
}

std::string EncodedDataHelper::GetBytesForNextFrame() {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  constexpr size_t kIVFHeaderSize = 32;
  constexpr size_t kIVFFrameHeaderSize = 12;

  size_t pos = next_pos_to_decode_;
  std::string bytes;

  // Only IVF video files are supported. The first 4bytes of an IVF video file's
  // header should be "DKIF".
  if (pos == 0) {
    if ((data_.size() < kIVFHeaderSize) || strncmp(&data_[0], "DKIF", 4) != 0) {
      LOG(ERROR) << "Unexpected data encountered while parsing IVF header";
      return bytes;
    }
    pos = kIVFHeaderSize;  // Skip IVF header.
  }

  // Read VP8/9 frame size from IVF header.
  if (pos + kIVFFrameHeaderSize > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    return bytes;
  }
  uint32_t frame_size = *reinterpret_cast<uint32_t*>(&data_[pos]);
  pos += kIVFFrameHeaderSize;  // Skip IVF frame header.

  // Make sure we are not reading out of bounds.
  if (pos + frame_size > data_.size()) {
    LOG(ERROR) << "Unexpected data encountered while parsing IVF frame header";
    next_pos_to_decode_ = data_.size();
    return bytes;
  }
  bytes.append(data_.substr(pos, frame_size));

  // Update next_pos_to_decode_.
  next_pos_to_decode_ = pos + frame_size;
  return bytes;
}

// static
bool EncodedDataHelper::HasConfigInfo(const uint8_t* data,
                                      size_t size,
                                      VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    H264Parser parser;
    parser.SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result != H264Parser::kOk) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }

    return nalu.nal_unit_type == H264NALU::kSPS;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return (size > 0 && !(data[0] & 0x01));
  }
  // Shouldn't happen at this point.
  LOG(FATAL) << "Invalid profile: " << GetProfileName(profile);
  return false;
}

}  // namespace test
}  // namespace media
