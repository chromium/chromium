// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_nalu_util.h"

#include <array>

#include "base/numerics/byte_conversions.h"
#include "media/base/media_log.h"

namespace media {

VideoToolboxParameterSetTracker::VideoToolboxParameterSetTracker(
    std::string_view name,
    MediaLog* media_log)
    : name_(name), media_log_(media_log) {}

VideoToolboxParameterSetTracker::~VideoToolboxParameterSetTracker() = default;

void VideoToolboxParameterSetTracker::Process(
    int id,
    base::span<const uint8_t> nalu_data) {
  seen_data_[id] = std::vector<uint8_t>(nalu_data.begin(), nalu_data.end());
}

void VideoToolboxParameterSetTracker::ReferenceInFrame(int id) {
  frame_ids_.insert(id);
}

bool VideoToolboxParameterSetTracker::ExtractForFormat(
    std::vector<const uint8_t*>& parameter_set_data_out,
    std::vector<size_t>& parameter_set_size_out) {
  active_data_.clear();
  for (int id : frame_ids_) {
    // Check that the ID is valid.
    const auto it = seen_data_.find(id);
    if (it == seen_data_.end()) {
      MEDIA_LOG(ERROR, media_log_) << "Missing " << name_ << " " << id;
      return false;
    }
    // Update active parameter set data.
    active_data_[id] = it->second;
    // Extract the parameter set data.
    parameter_set_data_out.push_back(it->second.data());
    parameter_set_size_out.push_back(it->second.size());
  }
  return true;
}

bool VideoToolboxParameterSetTracker::ExtractForInbandUpdate(
    std::vector<base::span<const uint8_t>>& parameter_set_data_out) {
  for (int id : frame_ids_) {
    // Check that the ID is valid.
    const auto seen_it = seen_data_.find(id);
    if (seen_it == seen_data_.end()) {
      MEDIA_LOG(ERROR, media_log_) << "Missing " << name_ << " " << id;
      return false;
    }
    // Check if the data has changed.
    const auto active_it = active_data_.find(id);
    if (active_it == active_data_.end() ||
        active_it->second != seen_it->second) {
      // Update active parameter set data.
      active_data_[id] = seen_it->second;
      // Extract the parameter set data.
      parameter_set_data_out.push_back(base::span(seen_it->second));
    }
  }
  return true;
}

void VideoToolboxParameterSetTracker::ResetFrame() {
  frame_ids_.clear();
}

void VideoToolboxParameterSetTracker::ResetActive() {
  active_data_.clear();
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef>
VideoToolboxCreateSampleBufferFromNALUs(
    base::span<const base::span<const uint8_t>> nalu_spans,
    CMFormatDescriptionRef format_description,
    MediaLog* media_log) {
  // Determine the final size of the converted bitstream.
  size_t data_size = 0;
  for (const auto& nalu_data : nalu_spans) {
    data_size += kNALUHeaderLength + nalu_data.size();
  }

  // Allocate a buffer.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      /*structureAllocator=*/kCFAllocatorDefault,
      /*memoryBlock=*/nullptr,
      /*blockLength=*/data_size,
      /*blockAllocator=*/kCFAllocatorDefault,
      /*customBlockSource=*/nullptr,
      /*offsetToData=*/0,
      /*dataLength=*/data_size,
      /*flags=*/0, data.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log)
        << "CMBlockBufferCreateWithMemoryBlock()";
    return base::apple::ScopedCFTypeRef<CMSampleBufferRef>();
  }

  status = CMBlockBufferAssureBlockMemory(data.get());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log)
        << "CMBlockBufferAssureBlockMemory()";
    return base::apple::ScopedCFTypeRef<CMSampleBufferRef>();
  }

  // Copy each NALU into the buffer, prefixed with a length header.
  size_t offset = 0u;
  for (const auto& nalu_data : nalu_spans) {
    // Write length header.
    std::array<uint8_t, kNALUHeaderLength> header =
        base::U32ToBigEndian(static_cast<uint32_t>(nalu_data.size()));
    status = CMBlockBufferReplaceDataBytes(header.data(), data.get(), offset,
                                           header.size());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log)
          << "CMBlockBufferReplaceDataBytes()";
      return base::apple::ScopedCFTypeRef<CMSampleBufferRef>();
    }
    offset += header.size();

    // Write NALU data.
    status = CMBlockBufferReplaceDataBytes(nalu_data.data(), data.get(), offset,
                                           nalu_data.size());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log)
          << "CMBlockBufferReplaceDataBytes()";
      return base::apple::ScopedCFTypeRef<CMSampleBufferRef>();
    }
    offset += nalu_data.size();
  }

  // Wrap in a sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(
      /*allocator=*/kCFAllocatorDefault,
      /*dataBuffer=*/data.get(),
      /*dataReady=*/true,
      /*makeDataReadyCallback=*/nullptr,
      /*makeDataReadyRefcon=*/nullptr,
      /*formatDescription=*/format_description,
      /*numSamples=*/1,
      /*numSampleTimingEntries=*/0,
      /*sampleTimingArray=*/nullptr,
      /*numSampleSizeEntries=*/1,
      /*sampleSizeArray=*/&data_size, sample.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log) << "CMSampleBufferCreate()";
    return base::apple::ScopedCFTypeRef<CMSampleBufferRef>();
  }

  return sample;
}

}  // namespace media
