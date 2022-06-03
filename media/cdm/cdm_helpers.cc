// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_helpers.h"

#include "base/check.h"
#include "ui/gfx/color_space.h"

namespace media {

namespace {

// See ISO 23001-8:2016, section 7. Value 2 means "Unspecified".
constexpr cdm::ColorSpace kUnspecifiedColorSpace = {2, 2, 2,
                                                    cdm::ColorRange::kInvalid};

gfx::ColorSpace::RangeID ToGfxColorSpaceRange(cdm::ColorRange range) {
  switch (range) {
    case cdm::ColorRange::kInvalid:
      return gfx::ColorSpace::RangeID::INVALID;
    case cdm::ColorRange::kLimited:
      return gfx::ColorSpace::RangeID::LIMITED;
    case cdm::ColorRange::kFull:
      return gfx::ColorSpace::RangeID::FULL;
    case cdm::ColorRange::kDerived:
      return gfx::ColorSpace::RangeID::DERIVED;
  }
}

}  // namespace

DecryptedBlockImpl::DecryptedBlockImpl() : buffer_(nullptr), timestamp_(0) {}

DecryptedBlockImpl::~DecryptedBlockImpl() {
  if (buffer_)
    buffer_->Destroy();
}

void DecryptedBlockImpl::SetDecryptedBuffer(cdm::Buffer* buffer) {
  buffer_ = buffer;
}

cdm::Buffer* DecryptedBlockImpl::DecryptedBuffer() {
  return buffer_;
}

void DecryptedBlockImpl::SetTimestamp(int64_t timestamp) {
  timestamp_ = timestamp;
}

int64_t DecryptedBlockImpl::Timestamp() const {
  return timestamp_;
}

VideoFrameImpl::VideoFrameImpl()
    : format_(cdm::kUnknownVideoFormat),
      color_space_(kUnspecifiedColorSpace),
      frame_buffer_(nullptr),
      timestamp_(0) {
  for (uint32_t i = 0; i < cdm::kMaxPlanes; ++i) {
    plane_offsets_[i] = 0;
    strides_[i] = 0;
  }
}

VideoFrameImpl::~VideoFrameImpl() {
  if (frame_buffer_)
    frame_buffer_->Destroy();
}

void VideoFrameImpl::SetFormat(cdm::VideoFormat format) {
  format_ = format;
}

cdm::VideoFormat VideoFrameImpl::Format() const {
  return format_;
}

void VideoFrameImpl::SetSize(cdm::Size size) {
  size_ = size;
}

cdm::Size VideoFrameImpl::Size() const {
  return size_;
}

void VideoFrameImpl::SetFrameBuffer(cdm::Buffer* frame_buffer) {
  frame_buffer_ = frame_buffer;
}

cdm::Buffer* VideoFrameImpl::FrameBuffer() {
  return frame_buffer_;
}

void VideoFrameImpl::SetPlaneOffset(cdm::VideoPlane plane, uint32_t offset) {
  DCHECK(plane < cdm::kMaxPlanes);
  plane_offsets_[plane] = offset;
}

uint32_t VideoFrameImpl::PlaneOffset(cdm::VideoPlane plane) {
  DCHECK(plane < cdm::kMaxPlanes);
  return plane_offsets_[plane];
}

void VideoFrameImpl::SetStride(cdm::VideoPlane plane, uint32_t stride) {
  DCHECK(plane < cdm::kMaxPlanes);
  strides_[plane] = stride;
}

uint32_t VideoFrameImpl::Stride(cdm::VideoPlane plane) {
  DCHECK(plane < cdm::kMaxPlanes);
  return strides_[plane];
}

void VideoFrameImpl::SetTimestamp(int64_t timestamp) {
  timestamp_ = timestamp;
}

int64_t VideoFrameImpl::Timestamp() const {
  return timestamp_;
}

void VideoFrameImpl::SetColorSpace(cdm::ColorSpace color_space) {
  color_space_ = color_space;
}

media::VideoColorSpace VideoFrameImpl::MediaColorSpace() const {
  return media::VideoColorSpace(
      color_space_.primary_id, color_space_.transfer_id, color_space_.matrix_id,
      ToGfxColorSpaceRange(color_space_.range));
}

AudioFramesImpl::AudioFramesImpl()
    : buffer_(nullptr), format_(cdm::kUnknownAudioFormat) {}

AudioFramesImpl::~AudioFramesImpl() {
  if (buffer_)
    buffer_->Destroy();
}

void AudioFramesImpl::SetFrameBuffer(cdm::Buffer* buffer) {
  buffer_ = buffer;
}

cdm::Buffer* AudioFramesImpl::FrameBuffer() {
  return buffer_;
}

void AudioFramesImpl::SetFormat(cdm::AudioFormat format) {
  format_ = format;
}

cdm::AudioFormat AudioFramesImpl::Format() const {
  return format_;
}

cdm::Buffer* AudioFramesImpl::PassFrameBuffer() {
  cdm::Buffer* temp_buffer = buffer_;
  buffer_ = nullptr;
  return temp_buffer;
}

}  // namespace media
