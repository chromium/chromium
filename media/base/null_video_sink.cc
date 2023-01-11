// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/null_video_sink.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace media {

NullVideoSink::NullVideoSink(
    bool clockless,
    base::TimeDelta interval,
    const NewFrameCB& new_frame_cb,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : clockless_(clockless),
      interval_(interval),
      new_frame_cb_(new_frame_cb),
      task_runner_(task_runner),
      started_(false),
      callback_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      background_render_(false) {}

NullVideoSink::~NullVideoSink() {
  DCHECK(!started_);
}

void NullVideoSink::Start(RenderCallback* callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!started_);
  callback_ = callback;
  started_ = true;
  last_now_ = current_render_time_ = tick_clock_->NowTicks();
  cancelable_worker_.Reset(
      base::BindRepeating(&NullVideoSink::CallRender, base::Unretained(this)));
  task_runner_->PostTask(FROM_HERE, cancelable_worker_.callback());
}

void NullVideoSink::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  cancelable_worker_.Cancel();
  started_ = false;
  if (stop_cb_)
    std::move(stop_cb_).Run();
}

void NullVideoSink::CallRender() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(started_);

  const base::TimeTicks end_of_interval = current_render_time_ + interval_;
  scoped_refptr<VideoFrame> new_frame = callback_->Render(
      current_render_time_, end_of_interval,
      background_render_
          ? VideoRendererSink::RenderCallback::RenderingMode::kBackground
          : VideoRendererSink::RenderCallback::RenderingMode::kNormal);
  DCHECK(new_frame);
  const bool is_new_frame = new_frame != last_frame_;
  last_frame_ = new_frame;
  if (is_new_frame && new_frame_cb_)
    new_frame_cb_.Run(new_frame);

  current_render_time_ += interval_;

  if (clockless_) {
    task_runner_->PostTask(FROM_HERE, cancelable_worker_.callback());
    return;
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta delay;
  if (last_now_ == now) {
    // The tick clock is frozen in this case, so don't advance deadline.
    delay = interval_;
    current_render_time_ = now;
  } else {
    // If we're behind, find the next nearest on time interval.
    delay = current_render_time_ - now;
    if (delay.is_negative())
      delay = interval_ + (delay % interval_);
    current_render_time_ = now + delay;
    last_now_ = now;
  }

  task_runner_->PostDelayedTask(FROM_HERE, cancelable_worker_.callback(),
                                delay);
}

void NullVideoSink::PaintSingleFrame(scoped_refptr<VideoFrame> frame,
                                     bool repaint_duplicate_frame) {
  if (!repaint_duplicate_frame && frame == last_frame_)
    return;

  last_frame_ = frame;
  if (new_frame_cb_)
    new_frame_cb_.Run(std::move(frame));
}

}  // namespace media
