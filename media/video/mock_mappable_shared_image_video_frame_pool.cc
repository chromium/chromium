// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/mock_mappable_shared_image_video_frame_pool.h"

#include "base/functional/bind.h"

namespace media {

MockMappableSharedImageVideoFramePool::MockMappableSharedImageVideoFramePool(
    std::vector<base::OnceClosure>* frame_ready_cbs)
    : frame_ready_cbs_(frame_ready_cbs) {}

MockMappableSharedImageVideoFramePool::
    ~MockMappableSharedImageVideoFramePool() {}

void MockMappableSharedImageVideoFramePool::MaybeCreateHardwareFrame(
    scoped_refptr<VideoFrame> video_frame,
    FrameReadyCB frame_ready_cb) {
  frame_ready_cbs_->push_back(
      base::BindOnce(std::move(frame_ready_cb), std::move(video_frame)));
}

}  // namespace media
