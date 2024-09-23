// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_frame_resource.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"

namespace media {

scoped_refptr<VideoFrameResource> VideoFrameResource::Create(
    scoped_refptr<VideoFrame> frame) {
  if (!frame) {
    return nullptr;
  }
  // Uses WrapRefCounted since MakeRefCounted cannot access a private
  // constructor.
  return base::WrapRefCounted<VideoFrameResource>(
      new VideoFrameResource(std::move(frame)));
}

scoped_refptr<const VideoFrameResource> VideoFrameResource::CreateConst(
    scoped_refptr<const VideoFrame> frame) {
  if (!frame) {
    return nullptr;
  }
  // Uses WrapRefCounted since MakeRefCounted cannot access a private
  // constructor.
  return base::WrapRefCounted<const VideoFrameResource>(
      new VideoFrameResource(std::move(frame)));
}

VideoFrameResource::VideoFrameResource(scoped_refptr<const VideoFrame> frame)
    : FrameResource(), frame_(std::move(frame)) {
  CHECK(frame_);
}

VideoFrameResource::~VideoFrameResource() = default;

VideoFrameResource* VideoFrameResource::AsVideoFrameResource() {
  return this;
}

bool VideoFrameResource::IsMappable() const {
  return VideoFrame::IsStorageTypeMappable(storage_type());
}

const uint8_t* VideoFrameResource::data(size_t plane) const {
  return frame_->data(plane);
}

uint8_t* VideoFrameResource::writable_data(size_t plane) {
  return GetMutableVideoFrame()->writable_data(plane);
}

const uint8_t* VideoFrameResource::visible_data(size_t plane) const {
  return frame_->visible_data(plane);
}

uint8_t* VideoFrameResource::GetWritableVisibleData(size_t plane) {
  return GetMutableVideoFrame()->GetWritableVisibleData(plane);
}

size_t VideoFrameResource::NumDmabufFds() const {
  return frame_->NumDmabufFds();
}

int VideoFrameResource::GetDmabufFd(size_t i) const {
  return frame_->GetDmabufFd(i);
}

scoped_refptr<const gfx::NativePixmapDmaBuf>
VideoFrameResource::GetNativePixmapDmaBuf() const {
  return media::CreateNativePixmapDmaBuf(frame_.get());
}

gfx::GpuMemoryBufferHandle VideoFrameResource::CreateGpuMemoryBufferHandle()
    const {
  return media::CreateGpuMemoryBufferHandle(frame_.get());
}

std::unique_ptr<VideoFrame::ScopedMapping>
VideoFrameResource::MapGMBOrSharedImage() const {
  return frame_->MapGMBOrSharedImage();
}

gfx::GenericSharedMemoryId VideoFrameResource::GetSharedMemoryId() const {
  return media::GetSharedMemoryId(*frame_);
}

const VideoFrameLayout& VideoFrameResource::layout() const {
  return frame_->layout();
}

VideoPixelFormat VideoFrameResource::format() const {
  return frame_->format();
}

int VideoFrameResource::stride(size_t plane) const {
  return frame_->stride(plane);
}

VideoFrame::StorageType VideoFrameResource::storage_type() const {
  return frame_->storage_type();
}

int VideoFrameResource::row_bytes(size_t plane) const {
  return frame_->row_bytes(plane);
}

const gfx::Size& VideoFrameResource::coded_size() const {
  return frame_->coded_size();
}

const gfx::Rect& VideoFrameResource::visible_rect() const {
  return frame_->visible_rect();
}

const gfx::Size& VideoFrameResource::natural_size() const {
  return frame_->natural_size();
}

gfx::ColorSpace VideoFrameResource::ColorSpace() const {
  return frame_->ColorSpace();
}

void VideoFrameResource::set_color_space(const gfx::ColorSpace& color_space) {
  GetMutableVideoFrame()->set_color_space(color_space);
}

const std::optional<gfx::HDRMetadata>& VideoFrameResource::hdr_metadata()
    const {
  return frame_->hdr_metadata();
}

void VideoFrameResource::set_hdr_metadata(
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  GetMutableVideoFrame()->set_hdr_metadata(hdr_metadata);
}

const VideoFrameMetadata& VideoFrameResource::metadata() const {
  return frame_->metadata();
}

VideoFrameMetadata& VideoFrameResource::metadata() {
  return GetMutableVideoFrame()->metadata();
}

void VideoFrameResource::set_metadata(const VideoFrameMetadata& metadata) {
  GetMutableVideoFrame()->set_metadata(metadata);
}

base::TimeDelta VideoFrameResource::timestamp() const {
  return frame_->timestamp();
}

void VideoFrameResource::set_timestamp(base::TimeDelta timestamp) {
  GetMutableVideoFrame()->set_timestamp(timestamp);
}

void VideoFrameResource::AddDestructionObserver(base::OnceClosure callback) {
  GetMutableVideoFrame()->AddDestructionObserver(std::move(callback));
}

scoped_refptr<FrameResource> VideoFrameResource::CreateWrappingFrame(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size) {
  auto wrapping_frame = Create(VideoFrame::WrapVideoFrame(
      GetMutableVideoFrame(), format(), visible_rect, natural_size));
  if (!wrapping_frame) {
    return nullptr;
  }

  // Adds a reference to |this| from the wrapping frame via a destruction
  // observer. This avoids destroying the original frame before the wrapping
  // frame has been destroyed.
  wrapping_frame->AddDestructionObserver(base::DoNothingWithBoundArgs(
      base::WrapRefCounted<VideoFrameResource>(this)));

  return wrapping_frame;
}

std::string VideoFrameResource::AsHumanReadableString() const {
  return frame_->AsHumanReadableString();
}

gfx::GpuMemoryBufferHandle
VideoFrameResource::GetGpuMemoryBufferHandleForTesting() const {
  return frame_->GetGpuMemoryBufferHandle();
}

scoped_refptr<VideoFrame> VideoFrameResource::GetMutableVideoFrame() {
  return const_cast<VideoFrame*>(frame_.get());
}

scoped_refptr<const VideoFrame> VideoFrameResource::GetVideoFrame() const {
  return frame_;
}

}  // namespace media
