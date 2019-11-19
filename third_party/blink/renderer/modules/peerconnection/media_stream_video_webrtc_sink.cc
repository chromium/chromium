// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "media/base/limits.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_utils.h"
#include "third_party/blink/public/web/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_video_track_source.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/video_track_source_proxy.h"

namespace blink {

namespace {

absl::optional<bool> ToAbslOptionalBool(const base::Optional<bool>& value) {
  return value ? absl::optional<bool>(*value) : absl::nullopt;
}

}  // namespace

namespace {

// The default number of microseconds that should elapse since the last video
// frame was received, before requesting a refresh frame.
const int64_t kDefaultRefreshIntervalMicros =
    base::Time::kMicrosecondsPerSecond;

// A lower-bound for the refresh interval.
const int64_t kLowerBoundRefreshIntervalMicros =
    base::Time::kMicrosecondsPerSecond / media::limits::kMaxFramesPerSecond;

webrtc::VideoTrackInterface::ContentHint ContentHintTypeToWebRtcContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  switch (content_hint) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      return webrtc::VideoTrackInterface::ContentHint::kNone;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      NOTREACHED();
      break;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
      return webrtc::VideoTrackInterface::ContentHint::kFluid;
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
      return webrtc::VideoTrackInterface::ContentHint::kDetailed;
    case WebMediaStreamTrack::ContentHintType::kVideoText:
      return webrtc::VideoTrackInterface::ContentHint::kText;
  }
  NOTREACHED();
  return webrtc::VideoTrackInterface::ContentHint::kNone;
}

}  // namespace

// Simple help class used for receiving video frames on the IO-thread from a
// MediaStreamVideoTrack and forward the frames to a WebRtcVideoCapturerAdapter
// on libjingle's worker thread. WebRtcVideoCapturerAdapter implements a video
// capturer for libjingle.
class MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter
    : public WTF::ThreadSafeRefCounted<WebRtcVideoSourceAdapter> {
 public:
  WebRtcVideoSourceAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          libjingle_worker_thread,
      const scoped_refptr<WebRtcVideoTrackSource>& source,
      base::TimeDelta refresh_interval,
      const base::RepeatingClosure& refresh_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // MediaStreamVideoWebRtcSink can be destroyed on the main render thread or
  // libjingles worker thread since it posts video frames on that thread. But
  // |video_source_| must be released on the main render thread before the
  // PeerConnectionFactory has been destroyed. The only way to ensure that is to
  // make sure |video_source_| is released when MediaStreamVideoWebRtcSink() is
  // destroyed.
  void ReleaseSourceOnMainThread();

  void OnVideoFrameOnIO(scoped_refptr<media::VideoFrame> frame,
                        base::TimeTicks estimated_capture_time);

 private:
  friend class WTF::ThreadSafeRefCounted<WebRtcVideoSourceAdapter>;

  void OnVideoFrameOnWorkerThread(scoped_refptr<media::VideoFrame> frame);

  virtual ~WebRtcVideoSourceAdapter();

  // Called whenever a video frame was just delivered on the IO thread. This
  // restarts the delay period before the |refresh_timer_| will fire the next
  // time.
  void ResetRefreshTimerOnMainThread();

  scoped_refptr<base::SingleThreadTaskRunner> render_task_runner_;

  // |render_thread_checker_| is bound to the main render thread.
  THREAD_CHECKER(render_thread_checker_);
  // Used to DCHECK that frames are called on the IO-thread.
  THREAD_CHECKER(io_thread_checker_);

  // Used for posting frames to libjingle's worker thread. Accessed on the
  // IO-thread.
  scoped_refptr<base::SingleThreadTaskRunner> libjingle_worker_thread_;

  scoped_refptr<WebRtcVideoTrackSource> video_source_;

  // Used to protect |video_source_|. It is taken by libjingle's worker
  // thread for each video frame that is delivered but only taken on the
  // main render thread in ReleaseSourceOnMainThread() when
  // the owning MediaStreamVideoWebRtcSink is being destroyed.
  base::Lock video_source_stop_lock_;

  // Requests a refresh frame at regular intervals. The delay on this timer is
  // reset each time a frame is received so that it will not fire for at least
  // an additional period. This means refresh frames will only be requested when
  // the source has halted delivery (e.g., a screen capturer stops sending
  // frames because the screen is not being updated).
  //
  // This mechanism solves a number of problems. First, it will ensure that
  // remote clients that join a distributed session receive a first video frame
  // in a timely manner. Second, it will allow WebRTC's internal bandwidth
  // estimation logic to maintain a more optimal state, since sending a video
  // frame will "prime it." Third, it allows lossy encoders to clean up
  // artifacts in a still image.  http://crbug.com/486274
  base::RepeatingTimer refresh_timer_;
};

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::WebRtcVideoSourceAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& libjingle_worker_thread,
    const scoped_refptr<WebRtcVideoTrackSource>& source,
    base::TimeDelta refresh_interval,
    const base::RepeatingClosure& refresh_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : render_task_runner_(std::move(task_runner)),
      libjingle_worker_thread_(libjingle_worker_thread),
      video_source_(source) {
  DCHECK(render_task_runner_->RunsTasksInCurrentSequence());
  DETACH_FROM_THREAD(io_thread_checker_);
  if (!refresh_interval.is_zero()) {
    VLOG(1) << "Starting frame refresh timer with interval "
            << refresh_interval.InMillisecondsF() << " ms.";
    refresh_timer_.Start(FROM_HERE, refresh_interval, refresh_callback);
  }
}

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    ~WebRtcVideoSourceAdapter() {
  DVLOG(3) << "~WebRtcVideoSourceAdapter()";
  DCHECK(!video_source_);
  // This object can be destroyed on the main render thread or libjingles worker
  // thread since it posts video frames on that thread. But |video_source_| must
  // be released on the main render thread before the PeerConnectionFactory has
  // been destroyed. The only way to ensure that is to make sure |video_source_|
  // is released when MediaStreamVideoWebRtcSink() is destroyed.
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    ResetRefreshTimerOnMainThread() {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  if (refresh_timer_.IsRunning())
    refresh_timer_.Reset();
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    ReleaseSourceOnMainThread() {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  // Since frames are posted to the worker thread, this object might be deleted
  // on that thread. However, since |video_source_| was created on the render
  // thread, it should be released on the render thread.
  base::AutoLock auto_lock(video_source_stop_lock_);
  video_source_ = nullptr;
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::OnVideoFrameOnIO(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  render_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcVideoSourceAdapter::ResetRefreshTimerOnMainThread,
                     this));
  libjingle_worker_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread,
                     this, std::move(frame)));
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    OnVideoFrameOnWorkerThread(scoped_refptr<media::VideoFrame> frame) {
  DCHECK(libjingle_worker_thread_->BelongsToCurrentThread());
  base::AutoLock auto_lock(video_source_stop_lock_);
  if (video_source_)
    video_source_->OnFrameCaptured(std::move(frame));
}

MediaStreamVideoWebRtcSink::MediaStreamVideoWebRtcSink(
    const WebMediaStreamTrack& track,
    PeerConnectionDependencyFactory* factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  DCHECK(video_track);

  absl::optional<bool> needs_denoising =
      ToAbslOptionalBool(video_track->noise_reduction());

  bool is_screencast = video_track->is_screencast();
  base::Optional<double> min_frame_rate = video_track->min_frame_rate();
  base::Optional<double> max_frame_rate = video_track->max_frame_rate();

  // Enable automatic frame refreshes for the screen capture sources, which will
  // stop producing frames whenever screen content is not changing. Check the
  // frameRate constraint to determine the rate of refreshes. If a minimum
  // frameRate is provided, use that. Otherwise, use the maximum frameRate if it
  // happens to be less than the default.
  base::TimeDelta refresh_interval = base::TimeDelta::FromMicroseconds(0);
  if (is_screencast) {
    // Start with the default refresh interval, and refine based on constraints.
    refresh_interval =
        base::TimeDelta::FromMicroseconds(kDefaultRefreshIntervalMicros);
    if (min_frame_rate.has_value()) {
      refresh_interval =
          base::TimeDelta::FromMicroseconds(base::saturated_cast<int64_t>(
              base::Time::kMicrosecondsPerSecond / *min_frame_rate));
    }
    if (max_frame_rate.has_value()) {
      const base::TimeDelta alternate_refresh_interval =
          base::TimeDelta::FromMicroseconds(base::saturated_cast<int64_t>(
              base::Time::kMicrosecondsPerSecond / *max_frame_rate));
      refresh_interval = std::max(refresh_interval, alternate_refresh_interval);
    }
    if (refresh_interval.InMicroseconds() < kLowerBoundRefreshIntervalMicros) {
      refresh_interval =
          base::TimeDelta::FromMicroseconds(kLowerBoundRefreshIntervalMicros);
    }
  }

  // TODO(pbos): Consolidate WebRtcVideoCapturerAdapter into WebRtcVideoSource
  // by removing the need for and dependency on a cricket::VideoCapturer.
  video_source_ = scoped_refptr<WebRtcVideoTrackSource>(
      new rtc::RefCountedObject<WebRtcVideoTrackSource>(is_screencast,
                                                        needs_denoising));

  // TODO(pbos): Consolidate the local video track with the source proxy and
  // move into PeerConnectionDependencyFactory. This now separately holds on a
  // reference to the proxy object because
  // PeerConnectionFactory::CreateVideoTrack doesn't do reference counting.
  video_source_proxy_ =
      factory->CreateVideoTrackSourceProxy(video_source_.get());
  video_track_ = factory->CreateLocalVideoTrack(track.Id().Utf8(),
                                                video_source_proxy_.get());

  video_track_->set_content_hint(
      ContentHintTypeToWebRtcContentHint(track.ContentHint()));
  video_track_->set_enabled(track.IsEnabled());

  source_adapter_ = base::MakeRefCounted<WebRtcVideoSourceAdapter>(
      factory->GetWebRtcWorkerTaskRunner(), video_source_.get(),
      refresh_interval,
      base::Bind(&MediaStreamVideoWebRtcSink::RequestRefreshFrame,
                 weak_factory_.GetWeakPtr()),
      std::move(task_runner));

  MediaStreamVideoSink::ConnectToTrack(
      track,
      base::Bind(&WebRtcVideoSourceAdapter::OnVideoFrameOnIO, source_adapter_),
      false);

  DVLOG(3) << "MediaStreamVideoWebRtcSink ctor() : is_screencast "
           << is_screencast;
}

MediaStreamVideoWebRtcSink::~MediaStreamVideoWebRtcSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(3) << "MediaStreamVideoWebRtcSink dtor().";
  weak_factory_.InvalidateWeakPtrs();
  MediaStreamVideoSink::DisconnectFromTrack();
  source_adapter_->ReleaseSourceOnMainThread();
}

void MediaStreamVideoWebRtcSink::OnEnabledChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_track_->set_enabled(enabled);
}

void MediaStreamVideoWebRtcSink::OnContentHintChanged(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_track_->set_content_hint(
      ContentHintTypeToWebRtcContentHint(content_hint));
}

void MediaStreamVideoWebRtcSink::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RequestRefreshFrameFromVideoTrack(connected_track());
}

absl::optional<bool>
MediaStreamVideoWebRtcSink::SourceNeedsDenoisingForTesting() const {
  return video_source_->needs_denoising();
}

}  // namespace blink
