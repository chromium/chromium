// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/default_video_frame_converter.h"

#include <memory>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/gpu/macros.h"

namespace media {

// static.
std::unique_ptr<FrameResourceConverter> DefaultFrameConverter::Create() {
  return base::WrapUnique<FrameResourceConverter>(new DefaultFrameConverter());
}

void DefaultFrameConverter::ConvertFrameImpl(
    scoped_refptr<FrameResource> frame) {
  DVLOGF(4);

  if (!frame) {
    return OnError(FROM_HERE, "Invalid frame.");
  }
  LOG_ASSERT(frame->AsVideoFrameResource() ||
             frame->AsNativePixmapFrameResource())
      << "|frame| is expected to be a VideoFrameResource or "
         "NativePixmapFrameResource";
  scoped_refptr<VideoFrame> video_frame =
      frame->AsVideoFrameResource()
          ? frame->AsVideoFrameResource()->GetMutableVideoFrame()
          : frame->AsNativePixmapFrameResource()->CreateVideoFrame();
  if (!video_frame) {
    return OnError(FROM_HERE, "Failed to convert FrameResource to VideoFrame.");
  }
  Output(std::move(video_frame));
}

}  // namespace media
