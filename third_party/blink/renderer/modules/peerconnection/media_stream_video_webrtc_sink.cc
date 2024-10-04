// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"

#include <algorithm>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_video_track_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

namespace {

std::optional<bool> ToAbslOptionalBool(const std::optional<bool>& value) {
  return value ? std::optional<bool>(*value) : std::nullopt;
}

webrtc::VideoTrackInterface::ContentHint ContentHintTypeToWebRtcContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  switch (content_hint) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      return webrtc::VideoTrackInterface::ContentHint::kNone;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      NOTREACHED_IN_MIGRATION();
      break;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
      return webrtc::VideoTrackInterface::ContentHint::kFluid;
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
      return webrtc::VideoTrackInterface::ContentHint::kDetailed;
    case WebMediaStreamTrack::ContentHintType::kVideoText:
      return webrtc::VideoTrackInterface::ContentHint::kText;
  }
  NOTREACHED_IN_MIGRATION();
  return webrtc::VideoTrackInterface::ContentHint::kNone;
}

void RequestRefreshFrameOnRenderTaskRunner(MediaStreamComponent* component) {
  if (!component)
    return;
  if (MediaStreamVideoTrack* video_track =
          MediaStreamVideoTrack::From(component)) {
    if (MediaStreamVideoSource* source = video_track->source()) {
      source->RequestRefreshFrame();
    }
  }
}

void RequestRefreshFrame(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CrossThreadWeakPersistent<MediaStreamComponent> component) {
  PostCrossThreadTask(*task_runner, FROM_HERE,
                      CrossThreadBindOnce(RequestRefreshFrameOnRenderTaskRunner,
                                          std::move(component)));
}

}  // namespace

// Simple help class used for receiving video frames on the IO-thread from a
// MediaStreamVideoTrack and forward the frames to a WebRtcVideoCapturerAdapter
// on libjingle's network thread. WebRtcVideoCapturerAdapter implements a video
// capturer for libjingle.
class MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter
    : public WTF::ThreadSafeRefCounted<WebRtcVideoSourceAdapter> {
 public:
  WebRtcVideoSourceAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          libjingle_network_task_runner,
      const scoped_refptr<WebRtcVideoTrackSource>& source,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // MediaStreamVideoWebRtcSink can be destroyed on the main render thread or
  // libjingles network thread since it posts video frames on that thread. But
  // |video_source_| must be released on the main render thread before the
  // PeerConnectionFactory has been destroyed. The only way to ensure that is to
  // make sure |video_source_| is released when MediaStreamVideoWebRtcSink() is
  // destroyed.
  void ReleaseSourceOnMainThread();

  void OnVideoFrameOnIO(
      scoped_refptr<media::VideoFrame> frame,
      base::TimeTicks estimated_capture_time);

  void OnNotifyVideoFrameDroppedOnIO(media::VideoCaptureFrameDropReason);

 private:
  friend class WTF::ThreadSafeRefCounted<WebRtcVideoSourceAdapter>;

  void OnVideoFrameOnNetworkThread(scoped_refptr<media::VideoFrame> frame);

  void OnNotifyVideoFrameDroppedOnNetworkThread();

  virtual ~WebRtcVideoSourceAdapter();

  scoped_refptr<base::SingleThreadTaskRunner> render_task_runner_;

  // |render_thread_checker_| is bound to the main render thread.
  THREAD_CHECKER(render_thread_checker_);
  // Used to DCHECK that frames are called on the IO-thread.
  SEQUENCE_CHECKER(io_sequence_checker_);

  // Used for posting frames to libjingle's network thread. Accessed on the
  // IO-thread.
  scoped_refptr<base::SingleThreadTaskRunner> libjingle_network_task_runner_;

  scoped_refptr<WebRtcVideoTrackSource> video_source_;

  // Used to protect |video_source_|. It is taken by libjingle's network
  // thread for each video frame that is delivered but only taken on the
  // main render thread in ReleaseSourceOnMainThread() when
  // the owning MediaStreamVideoWebRtcSink is being destroyed.
  base::Lock video_source_stop_lock_;
};

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::WebRtcVideoSourceAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>&
        libjingle_network_task_runner,
    const scoped_refptr<WebRtcVideoTrackSource>& source,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : render_task_runner_(std::move(task_runner)),
      libjingle_network_task_runner_(libjingle_network_task_runner),
      video_source_(source) {
  DCHECK(render_task_runner_->RunsTasksInCurrentSequence());
  DETACH_FROM_SEQUENCE(io_sequence_checker_);
}

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    ~WebRtcVideoSourceAdapter() {
  DVLOG(3) << "~WebRtcVideoSourceAdapter()";
  DCHECK(!video_source_);
  // This object can be destroyed on the main render thread or libjingles
  // network thread since it posts video frames on that thread. But
  // |video_source_| must be released on the main render thread before the
  // PeerConnectionFactory has been destroyed. The only way to ensure that is to
  // make sure |video_source_| is released when MediaStreamVideoWebRtcSink() is
  // destroyed.
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    ReleaseSourceOnMainThread() {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  // Since frames are posted to the network thread, this object might be deleted
  // on that thread. However, since |video_source_| was created on the render
  // thread, it should be released on the render thread.
  base::AutoLock auto_lock(video_source_stop_lock_);
  video_source_->Dispose();
  video_source_ = nullptr;
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::OnVideoFrameOnIO(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  PostCrossThreadTask(
      *libjingle_network_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &WebRtcVideoSourceAdapter::OnVideoFrameOnNetworkThread,
          WrapRefCounted(this), std::move(frame)));
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    OnNotifyVideoFrameDroppedOnIO(media::VideoCaptureFrameDropReason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  DVLOG(1) << __func__;
  PostCrossThreadTask(
      *libjingle_network_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &WebRtcVideoSourceAdapter::OnNotifyVideoFrameDroppedOnNetworkThread,
          WrapRefCounted(this)));
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    OnVideoFrameOnNetworkThread(scoped_refptr<media::VideoFrame> frame) {
  DCHECK(libjingle_network_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(video_source_stop_lock_);
  if (video_source_)
    video_source_->OnFrameCaptured(std::move(frame));
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    OnNotifyVideoFrameDroppedOnNetworkThread() {
  DCHECK(libjingle_network_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(video_source_stop_lock_);
  if (video_source_)
    video_source_->OnNotifyFrameDropped();
}

MediaStreamVideoWebRtcSink::MediaStreamVideoWebRtcSink(
    MediaStreamComponent* component,
    PeerConnectionDependencyFactory* factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(component);
  DCHECK(video_track);

  std::optional<bool> needs_denoising =
      ToAbslOptionalBool(video_track->noise_reduction());

  bool is_screencast = video_track->is_screencast();

  MediaStreamVideoSource* source = video_track->source();
  VideoCaptureFeedbackCB feedback_cb =
      source ? source->GetFeedbackCallback() : base::DoNothing();
  base::RepeatingClosure request_refresh_frame_closure =
      source ? ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                   RequestRefreshFrame, task_runner,
                   WrapCrossThreadWeakPersistent(component)))
             : base::DoNothing();

  // TODO(pbos): Consolidate WebRtcVideoCapturerAdapter into WebRtcVideoSource
  // by removing the need for and dependency on a cricket::VideoCapturer.
  video_source_ = scoped_refptr<WebRtcVideoTrackSource>(
      new rtc::RefCountedObject<WebRtcVideoTrackSource>(
          is_screencast, needs_denoising, feedback_cb,
          request_refresh_frame_closure, factory->GetGpuFactories()));

  // TODO(pbos): Consolidate the local video track with the source proxy and
  // move into PeerConnectionDependencyFactory. This now separately holds on a
  // reference to the proxy object because
  // PeerConnectionFactory::CreateVideoTrack doesn't do reference counting.
  video_source_proxy_ =
      factory->CreateVideoTrackSourceProxy(video_source_.get());
  video_track_ = factory->CreateLocalVideoTrack(component->Id(),
                                                video_source_proxy_.get());

  video_track_->set_content_hint(
      ContentHintTypeToWebRtcContentHint(component->ContentHint()));
  video_track_->set_enabled(component->Enabled());

  source_adapter_ = base::MakeRefCounted<WebRtcVideoSourceAdapter>(
      factory->GetWebRtcNetworkTaskRunner(), video_source_.get(),
      std::move(task_runner));

  MediaStreamVideoSink::ConnectToTrack(
      WebMediaStreamTrack(component),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &WebRtcVideoSourceAdapter::OnVideoFrameOnIO, source_adapter_)),
      MediaStreamVideoSink::IsSecure::kNo,
      MediaStreamVideoSink::UsesAlpha::kNo);
  video_track->SetSinkNotifyFrameDroppedCallback(
      this, ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                &WebRtcVideoSourceAdapter::OnNotifyVideoFrameDroppedOnIO,
                source_adapter_)));

  DVLOG(3) << "MediaStreamVideoWebRtcSink ctor() : is_screencast "
           << is_screencast;
}

MediaStreamVideoWebRtcSink::~MediaStreamVideoWebRtcSink() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << "MediaStreamVideoWebRtcSink dtor().";
  weak_factory_.InvalidateWeakPtrs();
  MediaStreamVideoSink::DisconnectFromTrack();
  source_adapter_->ReleaseSourceOnMainThread();
}

void MediaStreamVideoWebRtcSink::OnEnabledChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_track_->set_enabled(enabled);
}

void MediaStreamVideoWebRtcSink::OnContentHintChanged(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_track_->set_content_hint(
      ContentHintTypeToWebRtcContentHint(content_hint));
}

void MediaStreamVideoWebRtcSink::OnVideoConstraintsChanged(
    std::optional<double> min_fps,
    std::optional<double> max_fps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << " min " << min_fps.value_or(-1) << " max "
           << max_fps.value_or(-1);
  video_source_proxy_->ProcessConstraints(
      webrtc::VideoTrackSourceConstraints{min_fps, max_fps});
}

std::optional<bool> MediaStreamVideoWebRtcSink::SourceNeedsDenoisingForTesting()
    const {
  return video_source_->needs_denoising();
}

}  // namespace blink
