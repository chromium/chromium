// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"

#include <algorithm>
#include <memory>

#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_video_track_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/video_track_source_proxy.h"

namespace blink {

namespace {

absl::optional<bool> ToAbslOptionalBool(const base::Optional<bool>& value) {
  return value ? absl::optional<bool>(*value) : absl::nullopt;
}

}  // namespace

namespace {

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
};

MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::WebRtcVideoSourceAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& libjingle_worker_thread,
    const scoped_refptr<WebRtcVideoTrackSource>& source,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : render_task_runner_(std::move(task_runner)),
      libjingle_worker_thread_(libjingle_worker_thread),
      video_source_(source) {
  DCHECK(render_task_runner_->RunsTasksInCurrentSequence());
  DETACH_FROM_THREAD(io_thread_checker_);
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
  PostCrossThreadTask(
      *libjingle_worker_thread_.get(), FROM_HERE,
      CrossThreadBindOnce(&WebRtcVideoSourceAdapter::OnVideoFrameOnWorkerThread,
                          WrapRefCounted(this), std::move(frame)));
}

void MediaStreamVideoWebRtcSink::WebRtcVideoSourceAdapter::
    OnVideoFrameOnWorkerThread(scoped_refptr<media::VideoFrame> frame) {
  DCHECK(libjingle_worker_thread_->BelongsToCurrentThread());
  base::AutoLock auto_lock(video_source_stop_lock_);
  if (video_source_)
    video_source_->OnFrameCaptured(std::move(frame));
}

MediaStreamVideoWebRtcSink::MediaStreamVideoWebRtcSink(
    MediaStreamComponent* component,
    PeerConnectionDependencyFactory* factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(component);
  DCHECK(video_track);

  absl::optional<bool> needs_denoising =
      ToAbslOptionalBool(video_track->noise_reduction());

  bool is_screencast = video_track->is_screencast();

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

absl::optional<bool>
MediaStreamVideoWebRtcSink::SourceNeedsDenoisingForTesting() const {
  return video_source_->needs_denoising();
}

}  // namespace blink
