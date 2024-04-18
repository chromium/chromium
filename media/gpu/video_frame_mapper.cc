// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_frame_mapper.h"

#include "media/gpu/chromeos/video_frame_resource.h"

namespace media {
scoped_refptr<VideoFrame> VideoFrameMapper::Map(
    scoped_refptr<const VideoFrame> video_frame,
    int permissions) {
  return MapFrame(VideoFrameResource::CreateConst(std::move(video_frame)),
                  permissions);
}

}  // namespace media
