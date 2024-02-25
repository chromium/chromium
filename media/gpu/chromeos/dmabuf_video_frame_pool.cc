// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "base/task/sequenced_task_runner.h"

namespace media {

DmabufVideoFramePool::DmabufVideoFramePool() = default;

DmabufVideoFramePool::~DmabufVideoFramePool() = default;

void DmabufVideoFramePool::set_parent_task_runner(
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner) {
  DCHECK(!parent_task_runner_);

  parent_task_runner_ = std::move(parent_task_runner);
}

PlatformVideoFramePool* DmabufVideoFramePool::AsPlatformVideoFramePool() {
  return nullptr;
}

bool DmabufVideoFramePool::IsFakeVideoFramePool() {
  return false;
}

}  // namespace media
