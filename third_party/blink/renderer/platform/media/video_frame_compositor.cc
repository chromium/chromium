// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/media/video_frame_compositor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"

namespace blink {

using RenderingMode = ::media::VideoRendererSink::RenderCallback::RenderingMode;

// Amount of time to wait between UpdateCurrentFrame() callbacks before starting
// background rendering to keep the Render() callbacks moving.
const int kBackgroundRenderingTimeoutMs = 250;
const int kForceBeginFramesTimeoutMs = 1000;

// static
constexpr const char VideoFrameCompositor::kTracingCategory[];

VideoFrameCompositor::VideoFrameCompositor(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<WebVideoFrameSubmitter> submitter)
    : task_runner_(task_runner),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      background_rendering_timer_(
          FROM_HERE,
          base::Milliseconds(kBackgroundRenderingTimeoutMs),
          base::BindRepeating(&VideoFrameCompositor::BackgroundRender,
                              base::Unretained(this),
                              RenderingMode::kBackground)),
      force_begin_frames_timer_(
          FROM_HERE,
          base::Milliseconds(kForceBeginFramesTimeoutMs),
          base::BindRepeating(&VideoFrameCompositor::StopForceBeginFrames,
                              base::Unretained(this))),
      submitter_(std::move(submitter)) {
  if (submitter_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoFrameCompositor::InitializeSubmitter,
                                  weak_ptr_factory_.GetWeakPtr()));
    update_submission_state_callback_ = base::BindPostTask(
        task_runner_,
        base::BindRepeating(&VideoFrameCompositor::SetIsSurfaceVisible,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

cc::UpdateSubmissionStateCB
VideoFrameCompositor::GetUpdateSubmissionStateCallback() {
  return update_submission_state_callback_;
}

void VideoFrameCompositor::SetIsSurfaceVisible(
    bool is_visible,
    base::WaitableEvent* done_event) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  submitter_->SetIsSurfaceVisible(is_visible);
  if (done_event)
    done_event->Signal();
}

void VideoFrameCompositor::InitializeSubmitter() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  submitter_->Initialize(this, /* is_media_stream = */ false);
}

VideoFrameCompositor::~VideoFrameCompositor() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!callback_);
  DCHECK(!rendering_);
  if (client_)
    client_->StopUsingProvider();
}

void VideoFrameCompositor::EnableSubmission(
    const viz::SurfaceId& id,
    media::VideoTransformation transform,
    bool force_submit) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If we're switching to |submitter_| from some other client, then tell it.
  if (client_ && client_ != submitter_.get())
    client_->StopUsingProvider();

  submitter_->SetTransform(transform);
  submitter_->SetForceSubmit(force_submit);
  submitter_->EnableSubmission(id);
  client_ = submitter_.get();
  if (rendering_)
    client_->StartRendering();
}

bool VideoFrameCompositor::IsClientSinkAvailable() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return client_;
}

void VideoFrameCompositor::OnRendererStateUpdate(bool new_state) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(rendering_, new_state);
  rendering_ = new_state;

  if (!auto_open_close_) {
    auto_open_close_ = std::make_unique<
        base::trace_event::AutoOpenCloseEvent<kTracingCategory>>(
        base::trace_event::AutoOpenCloseEvent<kTracingCategory>::Type::ASYNC,
        "VideoPlayback");
  }

  if (rendering_) {
    auto_open_close_->Begin();
  } else {
    new_processed_frame_cb_.Reset();
    auto_open_close_->End();
  }

  if (rendering_) {
    // Always start playback in background rendering mode, if |client_| kicks
    // in right away it's okay.
    BackgroundRender(RenderingMode::kStartup);
  } else if (background_rendering_enabled_) {
    background_rendering_timer_.Stop();
  } else {
    DCHECK(!background_rendering_timer_.IsRunning());
  }

  if (!IsClientSinkAvailable())
    return;

  if (rendering_)
    client_->StartRendering();
  else
    client_->StopRendering();
}

void VideoFrameCompositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->StopUsingProvider();
  client_ = client;

  // |client_| may now be null, so verify before calling it.
  if (rendering_ && client_)
    client_->StartRendering();
}

scoped_refptr<media::VideoFrame> VideoFrameCompositor::GetCurrentFrame() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return current_frame_;
}

scoped_refptr<media::VideoFrame>
VideoFrameCompositor::GetCurrentFrameOnAnyThread() {
  base::AutoLock lock(current_frame_lock_);

  // Treat frames vended to external consumers as being rendered. This ensures
  // that hidden elements that are being driven by WebGL/WebGPU/Canvas rendering
  // don't mark all frames as dropped.
  rendered_last_frame_ = true;

  return current_frame_;
}

void VideoFrameCompositor::SetCurrentFrame_Locked(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks expected_display_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media", "VideoFrameCompositor::SetCurrentFrame", "frame",
               frame->AsHumanReadableString());
  current_frame_lock_.AssertAcquired();
  current_frame_ = std::move(frame);
  last_presentation_time_ = tick_clock_->NowTicks();
  last_expected_display_time_ = expected_display_time;
  ++presentation_counter_;
}

void VideoFrameCompositor::PutCurrentFrame() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(current_frame_lock_);
  rendered_last_frame_ = true;
}

bool VideoFrameCompositor::UpdateCurrentFrame(base::TimeTicks deadline_min,
                                              base::TimeTicks deadline_max) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT2("media", "VideoFrameCompositor::UpdateCurrentFrame",
               "deadline_min", deadline_min, "deadline_max", deadline_max);
  return CallRender(deadline_min, deadline_max, RenderingMode::kNormal);
}

bool VideoFrameCompositor::HasCurrentFrame() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return static_cast<bool>(GetCurrentFrame());
}

base::TimeDelta VideoFrameCompositor::GetPreferredRenderInterval() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(callback_lock_);

  if (!callback_)
    return viz::BeginFrameArgs::MinInterval();
  return callback_->GetPreferredRenderInterval();
}

void VideoFrameCompositor::Start(RenderCallback* callback) {
  // Called from the media thread, so acquire the callback under lock before
  // returning in case a Stop() call comes in before the PostTask is processed.
  base::AutoLock lock(callback_lock_);
  DCHECK(!callback_);
  callback_ = callback;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameCompositor::OnRendererStateUpdate,
                                weak_ptr_factory_.GetWeakPtr(), true));
}

void VideoFrameCompositor::Stop() {
  // Called from the media thread, so release the callback under lock before
  // returning to avoid a pending UpdateCurrentFrame() call occurring before
  // the PostTask is processed.
  base::AutoLock lock(callback_lock_);
  DCHECK(callback_);
  callback_ = nullptr;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameCompositor::OnRendererStateUpdate,
                                weak_ptr_factory_.GetWeakPtr(), false));
}

void VideoFrameCompositor::PaintSingleFrame(
    scoped_refptr<media::VideoFrame> frame,
    bool repaint_duplicate_frame) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoFrameCompositor::PaintSingleFrame,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(frame), repaint_duplicate_frame));
    return;
  }
  if (ProcessNewFrame(std::move(frame), tick_clock_->NowTicks(),
                      repaint_duplicate_frame) &&
      IsClientSinkAvailable()) {
    client_->DidReceiveFrame();
  }
}

void VideoFrameCompositor::UpdateCurrentFrameIfStale(UpdateType type) {
  TRACE_EVENT0("media", "VideoFrameCompositor::UpdateCurrentFrameIfStale");
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If we're not rendering, then the frame can't be stale.
  if (!rendering_ || !is_background_rendering_)
    return;

  // If we have a client, and it is currently rendering, then it's not stale
  // since the client is driving the frame updates at the proper rate.
  if (type != UpdateType::kBypassClient && IsClientSinkAvailable() &&
      client_->IsDrivingFrameUpdates()) {
    return;
  }

  // We're rendering, but the client isn't driving the updates.  See if the
  // frame is stale, and update it.

  DCHECK(!last_background_render_.is_null());

  const base::TimeTicks now = tick_clock_->NowTicks();
  const base::TimeDelta interval = now - last_background_render_;

  // Cap updates to 250Hz which should be more than enough for everyone.
  if (interval < base::Milliseconds(4))
    return;

  {
    base::AutoLock lock(callback_lock_);
    // Update the interval based on the time between calls and call background
    // render which will give this information to the client.
    last_interval_ = interval;
  }
  BackgroundRender();
}

void VideoFrameCompositor::SetOnNewProcessedFrameCallback(
    OnNewProcessedFrameCB cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  new_processed_frame_cb_ = std::move(cb);
}

void VideoFrameCompositor::SetOnFramePresentedCallback(
    OnNewFramePresentedCB present_cb) {
  base::AutoLock lock(current_frame_lock_);
  new_presented_frame_cb_ = std::move(present_cb);

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoFrameCompositor::StartForceBeginFrames,
                                weak_ptr_factory_.GetWeakPtr()));
}

void VideoFrameCompositor::StartForceBeginFrames() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!submitter_)
    return;

  submitter_->SetForceBeginFrames(true);
  force_begin_frames_timer_.Reset();
}

void VideoFrameCompositor::StopForceBeginFrames() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  submitter_->SetForceBeginFrames(false);
}

std::unique_ptr<WebMediaPlayer::VideoFramePresentationMetadata>
VideoFrameCompositor::GetLastPresentedFrameMetadata() {
  auto frame_metadata =
      std::make_unique<WebMediaPlayer::VideoFramePresentationMetadata>();

  scoped_refptr<media::VideoFrame> last_frame;
  {
    // Manually acquire the lock instead of calling GetCurrentFrameOnAnyThread()
    // to also fetch the other frame dependent properties.
    base::AutoLock lock(current_frame_lock_);
    last_frame = current_frame_;
    frame_metadata->presentation_time = last_presentation_time_;
    frame_metadata->expected_display_time = last_expected_display_time_;
    frame_metadata->presented_frames = presentation_counter_;
  }

  frame_metadata->width = last_frame->visible_rect().width();
  frame_metadata->height = last_frame->visible_rect().height();

  frame_metadata->media_time = last_frame->timestamp();

  frame_metadata->metadata.MergeMetadataFrom(last_frame->metadata());

  {
    base::AutoLock lock(callback_lock_);
    if (callback_) {
      frame_metadata->average_frame_duration =
          callback_->GetPreferredRenderInterval();
    }
    frame_metadata->rendering_interval = last_interval_;
  }

  return frame_metadata;
}

bool VideoFrameCompositor::ProcessNewFrame(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks presentation_time,
    bool repaint_duplicate_frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!frame || (GetCurrentFrame() && !repaint_duplicate_frame &&
                 frame->unique_id() == GetCurrentFrame()->unique_id())) {
    return false;
  }

  // TODO(crbug.com/1447318): Add other cases where the frame is not readable.
  bool is_frame_readable = !frame->metadata().dcomp_surface;

  // Copy to a local variable to avoid potential deadlock when executing the
  // callback.
  OnNewFramePresentedCB frame_presented_cb;
  {
    base::AutoLock lock(current_frame_lock_);

    // Set the flag indicating that the current frame is unrendered, if we get a
    // subsequent PutCurrentFrame() call it will mark it as rendered.
    rendered_last_frame_ = false;

    SetCurrentFrame_Locked(std::move(frame), presentation_time);
    frame_presented_cb = std::move(new_presented_frame_cb_);
  }

  if (new_processed_frame_cb_) {
    std::move(new_processed_frame_cb_)
        .Run(tick_clock_->NowTicks(), is_frame_readable);
  }

  if (frame_presented_cb) {
    std::move(frame_presented_cb).Run();
  }

  return true;
}

void VideoFrameCompositor::SetIsPageVisible(bool is_visible) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (submitter_)
    submitter_->SetIsPageVisible(is_visible);
}

void VideoFrameCompositor::SetForceSubmit(bool force_submit) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // The `submitter_` can be null in tests.
  if (submitter_)
    submitter_->SetForceSubmit(force_submit);
}

base::TimeDelta VideoFrameCompositor::GetLastIntervalWithoutLock()
    NO_THREAD_SAFETY_ANALYSIS {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // |last_interval_| is only updated on the compositor thread, so it's safe to
  // return it without acquiring |callback_lock_|
  return last_interval_;
}

void VideoFrameCompositor::BackgroundRender(RenderingMode mode) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  const base::TimeTicks now = tick_clock_->NowTicks();
  last_background_render_ = now;
  bool new_frame = CallRender(now, now + GetLastIntervalWithoutLock(), mode);
  if (new_frame && IsClientSinkAvailable())
    client_->DidReceiveFrame();
}

bool VideoFrameCompositor::CallRender(base::TimeTicks deadline_min,
                                      base::TimeTicks deadline_max,
                                      RenderingMode mode) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  bool have_unseen_frame;
  {
    base::AutoLock lock(current_frame_lock_);
    have_unseen_frame = !rendered_last_frame_ && HasCurrentFrame();
  }

  base::AutoLock lock(callback_lock_);

  if (!callback_) {
    // Even if we no longer have a callback, return true if we have a frame
    // which |client_| hasn't seen before.
    return have_unseen_frame;
  }

  DCHECK(rendering_);

  // If the previous frame was never rendered and we're in the normal rendering
  // mode and haven't just exited background rendering, let the client know.
  //
  // We don't signal for mode == kBackground since we expect to drop frames. We
  // also don't signal for mode == kStartup since UpdateCurrentFrame() may occur
  // before the PutCurrentFrame() for the kStartup induced CallRender().
  const bool was_background_rendering = is_background_rendering_;
  if (have_unseen_frame && mode == RenderingMode::kNormal &&
      !was_background_rendering) {
    callback_->OnFrameDropped();
  }

  const bool new_frame = ProcessNewFrame(
      callback_->Render(deadline_min, deadline_max, mode), deadline_min, false);

  // In cases where mode == kStartup we still want to treat it like background
  // rendering mode since CallRender() wasn't generated by UpdateCurrentFrame().
  is_background_rendering_ = mode != RenderingMode::kNormal;
  last_interval_ = deadline_max - deadline_min;

  // We may create a new frame here with background rendering, but the provider
  // has no way of knowing that a new frame had been processed, so keep track of
  // the new frame, and return true on the next call to |CallRender|.
  const bool had_new_background_frame = new_background_frame_;
  new_background_frame_ = is_background_rendering_ && new_frame;

  // Restart the background rendering timer whether we're background rendering
  // or not; in either case we should wait for |kBackgroundRenderingTimeoutMs|.
  if (background_rendering_enabled_)
    background_rendering_timer_.Reset();
  return new_frame || had_new_background_frame;
}

void VideoFrameCompositor::OnContextLost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // current_frame_'s resource in the context has been lost, so current_frame_
  // is not valid any more. current_frame_ should be reset. Now the compositor
  // has no concept of resetting current_frame_, so a black frame is set.
  base::AutoLock lock(current_frame_lock_);
  if (!current_frame_ || (!current_frame_->HasSharedImage() &&
                          !current_frame_->HasMappableGpuBuffer())) {
    return;
  }
  scoped_refptr<media::VideoFrame> black_frame =
      media::VideoFrame::CreateBlackFrame(current_frame_->natural_size());
  SetCurrentFrame_Locked(std::move(black_frame), tick_clock_->NowTicks());
}

}  // namespace blink
