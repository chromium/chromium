// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/mappable_video_frame_converter.h"

#include <memory>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/macros.h"

namespace media {

// static.
std::unique_ptr<FrameResourceConverter>
MappableVideoFrameConverter::CreateForTesting() {
  return base::WrapUnique<FrameResourceConverter>(
      new MappableVideoFrameConverter());
}

MappableVideoFrameConverter::MappableVideoFrameConverter()
    : test_sii_(base::MakeRefCounted<gpu::TestSharedImageInterface>()) {}

MappableVideoFrameConverter::~MappableVideoFrameConverter() = default;

void MappableVideoFrameConverter::ConvertFrameImpl(
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);

  if (!frame) {
    return OnError(FROM_HERE, "Invalid frame.");
  }
  LOG_ASSERT(frame->AsNativePixmapFrameResource())
      << "|frame| is expected to be a NativePixmapFrameResource";
  scoped_refptr<VideoFrame> video_frame =
      frame->AsNativePixmapFrameResource()->CreateMappableVideoFrame(
          test_sii_.get());
  if (!video_frame) {
    return OnError(FROM_HERE, "Failed to convert FrameResource to VideoFrame.");
  }
  Output(std::move(video_frame));
}

}  // namespace media
