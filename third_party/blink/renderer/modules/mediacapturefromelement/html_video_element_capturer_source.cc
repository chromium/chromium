// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_video_element_capturer_source.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace {
constexpr float kMinFramesPerSecond = 1.0;
}  // anonymous namespace

namespace blink {

// static
std::unique_ptr<HtmlVideoElementCapturerSource>
HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
    blink::WebMediaPlayer* player,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // Save histogram data so we can see how much HTML Video capture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(RTCAPIName::kVideoCaptureStream);

  // TODO(crbug.com/963651): Remove the need for AsWeakPtr altogether.
  return base::WrapUnique(new HtmlVideoElementCapturerSource(
      player->AsWeakPtr(), io_task_runner, task_runner));
}

HtmlVideoElementCapturerSource::HtmlVideoElementCapturerSource(
    const base::WeakPtr<blink::WebMediaPlayer>& player,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : web_media_player_(player),
      io_task_runner_(io_task_runner),
      task_runner_(task_runner),
      capture_frame_rate_(0.0) {
  DCHECK(web_media_player_);
}

HtmlVideoElementCapturerSource::~HtmlVideoElementCapturerSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

media::VideoCaptureFormats
HtmlVideoElementCapturerSource::GetPreferredFormats() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // WebMediaPlayer has a setRate() but can't be read back.
  // TODO(mcasas): Add getRate() to WMPlayer and/or fix the spec to allow users
  // to specify it.
  const media::VideoCaptureFormat format(
      gfx::Size(web_media_player_->NaturalSize()),
      blink::MediaStreamVideoSource::kDefaultFrameRate,
      media::PIXEL_FORMAT_I420);
  media::VideoCaptureFormats formats;
  formats.push_back(format);
  return formats;
}

void HtmlVideoElementCapturerSource::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const VideoCaptureSubCaptureTargetVersionCB&
        sub_capture_target_version_callback,
    // The HTML element does not report frame drops.
    const VideoCaptureNotifyFrameDroppedCB&,
    const RunningCallback& running_callback) {
  DVLOG(2) << __func__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK(params.requested_format.IsValid());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  running_callback_ = running_callback;
  if (!web_media_player_ || !web_media_player_->HasVideo()) {
    running_callback_.Run(RunState::kStopped);
    return;
  }

  new_frame_callback_ = new_frame_callback;
  // Force |capture_frame_rate_| to be in between k{Min,Max}FramesPerSecond.
  capture_frame_rate_ =
      std::max(kMinFramesPerSecond,
               std::min(static_cast<float>(media::limits::kMaxFramesPerSecond),
                        params.requested_format.frame_rate));

  running_callback_.Run(RunState::kRunning);
  task_runner_->PostTask(
      FROM_HERE, WTF::BindOnce(&HtmlVideoElementCapturerSource::sendNewFrame,
                               weak_factory_.GetWeakPtr()));
}

void HtmlVideoElementCapturerSource::StopCapture() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  running_callback_.Reset();
  new_frame_callback_.Reset();
  next_capture_time_ = base::TimeTicks();
}

void HtmlVideoElementCapturerSource::sendNewFrame() {
  DVLOG(3) << __func__;
  TRACE_EVENT0("media", "HtmlVideoElementCapturerSource::sendNewFrame");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!web_media_player_ || new_frame_callback_.is_null() ||
      web_media_player_->WouldTaintOrigin()) {
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  if (start_capture_time_.is_null())
    start_capture_time_ = current_time;

  if (auto frame = web_media_player_->GetCurrentFrameThenUpdate()) {
    auto new_frame = media::VideoFrame::WrapVideoFrame(
        frame, frame->format(), frame->visible_rect(), frame->natural_size());
    new_frame->set_timestamp(current_time - start_capture_time_);

    // Post with CrossThreadBind here, instead of CrossThreadBindOnce,
    // otherwise the |new_frame_callback_| ivar can be nulled out
    // unintentionally.
    PostCrossThreadTask(
        *io_task_runner_, FROM_HERE,
        CrossThreadBindOnce(new_frame_callback_, std::move(new_frame),
                            current_time));
  }

  // Calculate the time in the future where the next frame should be created.
  const base::TimeDelta frame_interval =
      base::Microseconds(1E6 / capture_frame_rate_);
  if (next_capture_time_.is_null()) {
    next_capture_time_ = current_time + frame_interval;
  } else {
    next_capture_time_ += frame_interval;
    // Don't accumulate any debt if we are lagging behind - just post next frame
    // immediately and continue as normal.
    if (next_capture_time_ < current_time)
      next_capture_time_ = current_time;
  }
  // Schedule next capture.
  PostDelayedCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&HtmlVideoElementCapturerSource::sendNewFrame,
                          weak_factory_.GetWeakPtr()),
      next_capture_time_ - current_time);
}

}  // namespace blink
