// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"

namespace media {

// static
DmabufVideoFramePool::DmabufId DmabufVideoFramePool::GetDmabufId(
    const VideoFrame& frame) {
  return &(frame.DmabufFds());
}

DmabufVideoFramePool::DmabufVideoFramePool() = default;

DmabufVideoFramePool::~DmabufVideoFramePool() = default;

void DmabufVideoFramePool::set_parent_task_runner(
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner) {
  DCHECK(!parent_task_runner_);

  parent_task_runner_ = std::move(parent_task_runner);
}

}  // namespace media
