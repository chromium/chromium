// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/test_media_stream_video_renderer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

TestMediaStreamVideoRenderer::TestMediaStreamVideoRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const gfx::Size& size,
    const base::TimeDelta& frame_duration,
    const blink::WebMediaStreamVideoRenderer::RepaintCB& repaint_cb)
    : task_runner_(blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      io_task_runner_(io_task_runner),
      size_(size),
      state_(kStopped),
      frame_duration_(frame_duration),
      repaint_cb_(repaint_cb) {}

TestMediaStreamVideoRenderer::~TestMediaStreamVideoRenderer() {}

void TestMediaStreamVideoRenderer::Start() {
  DVLOG(1) << "TestMediaStreamVideoRenderer::Start";
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kStarted;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestMediaStreamVideoRenderer::GenerateFrame, this));
}

void TestMediaStreamVideoRenderer::Stop() {
  DVLOG(1) << "TestMediaStreamVideoRenderer::Stop";
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kStopped;
}

void TestMediaStreamVideoRenderer::Resume() {
  DVLOG(1) << "TestMediaStreamVideoRenderer::Resume";
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kPaused)
    state_ = kStarted;
}

void TestMediaStreamVideoRenderer::Pause() {
  DVLOG(1) << "TestMediaStreamVideoRenderer::Pause";
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kStarted)
    state_ = kPaused;
}

void TestMediaStreamVideoRenderer::GenerateFrame() {
  DVLOG(1) << "TestMediaStreamVideoRenderer::GenerateFrame";
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kStopped)
    return;

  if (state_ == kStarted) {
    // Always allocate a new frame filled with white color.
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateColorFrame(size_, 255, 128, 128,
                                            current_time_);

    // TODO(wjia): set pixel data to pre-defined patterns if it's desired to
    // verify frame content.
    io_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(repaint_cb_, video_frame));
  }

  current_time_ += frame_duration_;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestMediaStreamVideoRenderer::GenerateFrame, this),
      frame_duration_);
}

}  // namespace content
