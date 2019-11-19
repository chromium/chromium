// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// This alias mimics the definition of VideoCaptureDeliverFrameCB.
using VideoCaptureDeliverFrameInternalCallback =
    WTF::CrossThreadFunction<void(scoped_refptr<media::VideoFrame> video_frame,
                                  base::TimeTicks estimated_capture_time)>;

void ResetCallback(
    std::unique_ptr<VideoCaptureDeliverFrameInternalCallback> callback) {
  // |callback| will be deleted when this exits.
}

}  // namespace

// MediaStreamVideoTrack::FrameDeliverer is a helper class used for registering
// VideoCaptureDeliverFrameCB on the main render thread to receive video frames
// on the IO-thread.
// Frames are only delivered to the sinks if the track is enabled. If the track
// is disabled, a black frame is instead forwarded to the sinks at the same
// frame rate.
class MediaStreamVideoTrack::FrameDeliverer
    : public WTF::ThreadSafeRefCounted<FrameDeliverer> {
 public:
  using VideoSinkId = WebMediaStreamSink*;

  FrameDeliverer(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                 base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track,
                 bool enabled);

  void SetEnabled(bool enabled);

  // Add |callback| to receive video frames on the IO-thread.
  // Must be called on the main render thread.
  void AddCallback(VideoSinkId id, VideoCaptureDeliverFrameCB callback);

  // Removes |callback| associated with |id| from receiving video frames if |id|
  // has been added. It is ok to call RemoveCallback even if the |id| has not
  // been added. Note that the added callback will be reset on the main thread.
  // Must be called on the main render thread.
  void RemoveCallback(VideoSinkId id);

  // Triggers all registered callbacks with |frame|, |format| and
  // |estimated_capture_time| as parameters. Must be called on the IO-thread.
  void DeliverFrameOnIO(scoped_refptr<media::VideoFrame> frame,
                        base::TimeTicks estimated_capture_time);

 private:
  friend class WTF::ThreadSafeRefCounted<FrameDeliverer>;
  virtual ~FrameDeliverer();
  void AddCallbackOnIO(VideoSinkId id,
                       VideoCaptureDeliverFrameInternalCallback callback);
  void RemoveCallbackOnIO(
      VideoSinkId id,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void SetEnabledOnIO(bool enabled);

  // Returns a black frame where the size and time stamp is set to the same as
  // as in |reference_frame|.
  scoped_refptr<media::VideoFrame> GetBlackFrame(
      const media::VideoFrame& reference_frame);

  // Used to DCHECK that AddCallback and RemoveCallback are called on the main
  // Render Thread.
  THREAD_CHECKER(main_render_thread_checker_);
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  // Can be null in testing.
  scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner_;

  base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track_;

  bool enabled_;
  scoped_refptr<media::VideoFrame> black_frame_;
  bool emit_frame_drop_events_;

  using VideoIdCallbackPair =
      std::pair<VideoSinkId, VideoCaptureDeliverFrameInternalCallback>;
  std::vector<VideoIdCallbackPair> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

MediaStreamVideoTrack::FrameDeliverer::FrameDeliverer(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<MediaStreamVideoTrack> media_stream_video_track,
    bool enabled)
    : io_task_runner_(std::move(io_task_runner)),
      media_stream_video_track_(media_stream_video_track),
      enabled_(enabled),
      emit_frame_drop_events_(true) {
  DCHECK(io_task_runner_.get());

  WebLocalFrame* web_frame = WebLocalFrame::FrameForCurrentContext();
  if (web_frame) {
    main_render_task_runner_ =
        web_frame->GetTaskRunner(TaskType::kInternalMedia);
  }
}

MediaStreamVideoTrack::FrameDeliverer::~FrameDeliverer() {
  DCHECK(callbacks_.empty());
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallback(
    VideoSinkId id,
    VideoCaptureDeliverFrameCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &FrameDeliverer::AddCallbackOnIO, WrapRefCounted(this),
          WTF::CrossThreadUnretained(id),
          WTF::Passed(CrossThreadBindRepeating(std::move(callback)))));
}

void MediaStreamVideoTrack::FrameDeliverer::AddCallbackOnIO(
    VideoSinkId id,
    VideoCaptureDeliverFrameInternalCallback callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  callbacks_.push_back(std::make_pair(id, std::move(callback)));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallback(VideoSinkId id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&FrameDeliverer::RemoveCallbackOnIO,
                          WrapRefCounted(this), WTF::CrossThreadUnretained(id),
                          Thread::Current()->GetTaskRunner()));
}

void MediaStreamVideoTrack::FrameDeliverer::RemoveCallbackOnIO(
    VideoSinkId id,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = callbacks_.begin();
  for (; it != callbacks_.end(); ++it) {
    if (it->first == id) {
      // Callback is copied to heap and then deleted on the target thread.
      std::unique_ptr<VideoCaptureDeliverFrameInternalCallback> callback;
      callback.reset(
          new VideoCaptureDeliverFrameInternalCallback(std::move(it->second)));
      callbacks_.erase(it);
      PostCrossThreadTask(
          *task_runner, FROM_HERE,
          CrossThreadBindOnce(&ResetCallback, std::move(callback)));
      return;
    }
  }
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  PostCrossThreadTask(*io_task_runner_, FROM_HERE,
                      CrossThreadBindOnce(&FrameDeliverer::SetEnabledOnIO,
                                          WrapRefCounted(this), enabled));
}

void MediaStreamVideoTrack::FrameDeliverer::SetEnabledOnIO(bool enabled) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (enabled != enabled_) {
    enabled_ = enabled;
    emit_frame_drop_events_ = true;
  }
  if (enabled_)
    black_frame_ = nullptr;
}

void MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!enabled_ && main_render_task_runner_ && emit_frame_drop_events_) {
    emit_frame_drop_events_ = false;

    // TODO(crbug.com/964947): A weak ptr instance of MediaStreamVideoTrack is
    // passed to FrameDeliverer in order to avoid the re-binding the instance of
    // a WTF::CrossThreadFunction.
    PostCrossThreadTask(
        *main_render_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &MediaStreamVideoTrack::OnFrameDropped, media_stream_video_track_,
            media::VideoCaptureFrameDropReason::
                kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame));
  }
  auto video_frame = enabled_ ? std::move(frame) : GetBlackFrame(*frame);
  for (const auto& entry : callbacks_)
    entry.second.Run(video_frame, estimated_capture_time);
}

scoped_refptr<media::VideoFrame>
MediaStreamVideoTrack::FrameDeliverer::GetBlackFrame(
    const media::VideoFrame& reference_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!black_frame_.get() ||
      black_frame_->natural_size() != reference_frame.natural_size()) {
    black_frame_ =
        media::VideoFrame::CreateBlackFrame(reference_frame.natural_size());
  }

  // Wrap |black_frame_| so we get a fresh timestamp we can modify. Frames
  // returned from this function may still be in use.
  scoped_refptr<media::VideoFrame> wrapped_black_frame =
      media::VideoFrame::WrapVideoFrame(black_frame_, black_frame_->format(),
                                        black_frame_->visible_rect(),
                                        black_frame_->natural_size());
  if (!wrapped_black_frame)
    return nullptr;

  wrapped_black_frame->set_timestamp(reference_frame.timestamp());
  base::TimeTicks reference_time;
  if (reference_frame.metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &reference_time)) {
    wrapped_black_frame->metadata()->SetTimeTicks(
        media::VideoFrameMetadata::REFERENCE_TIME, reference_time);
  }

  return wrapped_black_frame;
}

// static
WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled) {
  WebMediaStreamTrack track;
  track.Initialize(source->Owner());
  track.SetPlatformTrack(
      std::make_unique<MediaStreamVideoTrack>(source, callback, enabled));
  return track;
}

// static
WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    const WebString& id,
    MediaStreamVideoSource* source,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled) {
  WebMediaStreamTrack track;
  track.Initialize(id, source->Owner());
  track.SetPlatformTrack(
      std::make_unique<MediaStreamVideoTrack>(source, callback, enabled));
  return track;
}

// static
WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const VideoTrackAdapterSettings& adapter_settings,
    const base::Optional<bool>& noise_reduction,
    bool is_screencast,
    const base::Optional<double>& min_frame_rate,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled) {
  WebMediaStreamTrack track;
  track.Initialize(source->Owner());
  track.SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      source, adapter_settings, noise_reduction, is_screencast, min_frame_rate,
      callback, enabled));
  return track;
}

// static
MediaStreamVideoTrack* MediaStreamVideoTrack::GetVideoTrack(
    const WebMediaStreamTrack& track) {
  if (track.IsNull() ||
      track.Source().GetType() != WebMediaStreamSource::kTypeVideo) {
    return nullptr;
  }
  return static_cast<MediaStreamVideoTrack*>(track.GetPlatformTrack());
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled)
    : WebPlatformMediaStreamTrack(true),
      adapter_settings_(std::make_unique<VideoTrackAdapterSettings>(
          VideoTrackAdapterSettings())),
      is_screencast_(false),
      source_(source->GetWeakPtr()) {
  frame_deliverer_ =
      base::MakeRefCounted<MediaStreamVideoTrack::FrameDeliverer>(
          source->io_task_runner(), weak_factory_.GetWeakPtr(), enabled);
  source->AddTrack(this, VideoTrackAdapterSettings(),
                   ConvertToBaseCallback(CrossThreadBindRepeating(
                       &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO,
                       frame_deliverer_)),
                   media::BindToCurrentLoop(WTF::BindRepeating(
                       &MediaStreamVideoTrack::SetSizeAndComputedFrameRate,
                       weak_factory_.GetWeakPtr())),
                   media::BindToCurrentLoop(WTF::BindRepeating(
                       &MediaStreamVideoTrack::set_computed_source_format,
                       weak_factory_.GetWeakPtr())),
                   callback);
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const VideoTrackAdapterSettings& adapter_settings,
    const base::Optional<bool>& noise_reduction,
    bool is_screen_cast,
    const base::Optional<double>& min_frame_rate,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled)
    : WebPlatformMediaStreamTrack(true),
      adapter_settings_(
          std::make_unique<VideoTrackAdapterSettings>(adapter_settings)),
      noise_reduction_(noise_reduction),
      is_screencast_(is_screen_cast),
      min_frame_rate_(min_frame_rate),
      source_(source->GetWeakPtr()) {
  frame_deliverer_ =
      base::MakeRefCounted<MediaStreamVideoTrack::FrameDeliverer>(
          source->io_task_runner(), weak_factory_.GetWeakPtr(), enabled);
  source->AddTrack(this, adapter_settings,
                   ConvertToBaseCallback(CrossThreadBindRepeating(
                       &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO,
                       frame_deliverer_)),
                   media::BindToCurrentLoop(WTF::BindRepeating(
                       &MediaStreamVideoTrack::SetSizeAndComputedFrameRate,
                       weak_factory_.GetWeakPtr())),
                   media::BindToCurrentLoop(WTF::BindRepeating(
                       &MediaStreamVideoTrack::set_computed_source_format,
                       weak_factory_.GetWeakPtr())),
                   callback);
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(sinks_.empty());
  Stop();
  DVLOG(3) << "~MediaStreamVideoTrack()";
}

void MediaStreamVideoTrack::AddSink(WebMediaStreamSink* sink,
                                    const VideoCaptureDeliverFrameCB& callback,
                                    bool is_sink_secure) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(!base::Contains(sinks_, sink));
  sinks_.push_back(sink);
  frame_deliverer_->AddCallback(sink, callback);
  secure_tracker_.Add(sink, is_sink_secure);
  // Request source to deliver a frame because a new sink is added.
  if (!source_)
    return;
  source_->UpdateHasConsumers(this, true);
  source_->RequestRefreshFrame();
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());
}

void MediaStreamVideoTrack::RemoveSink(WebMediaStreamSink* sink) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  auto it = std::find(sinks_.begin(), sinks_.end(), sink);
  DCHECK(it != sinks_.end());
  sinks_.erase(it);
  frame_deliverer_->RemoveCallback(sink);
  secure_tracker_.Remove(sink);
  if (!source_)
    return;
  if (sinks_.empty())
    source_->UpdateHasConsumers(this, false);
  source_->UpdateCapturingLinkSecure(this,
                                     secure_tracker_.is_capturing_secure());
}

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  frame_deliverer_->SetEnabled(enabled);
  for (auto* sink : sinks_)
    sink->OnEnabledChanged(enabled);
}

void MediaStreamVideoTrack::SetContentHint(
    WebMediaStreamTrack::ContentHintType content_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  for (auto* sink : sinks_)
    sink->OnContentHintChanged(content_hint);
}

void MediaStreamVideoTrack::StopAndNotify(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (source_) {
    source_->RemoveTrack(this, std::move(callback));
    source_ = nullptr;
  } else if (callback) {
    std::move(callback).Run();
  }
  OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
}

void MediaStreamVideoTrack::GetSettings(
    WebMediaStreamTrack::Settings& settings) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_)
    return;

  if (width_ && height_) {
    settings.width = width_;
    settings.height = height_;
    settings.aspect_ratio = static_cast<double>(width_) / height_;
  }

  // 0.0 means the track is using the source's frame rate.
  if (frame_rate_ != 0.0) {
    settings.frame_rate = frame_rate_;
  }

  base::Optional<media::VideoCaptureFormat> format =
      source_->GetCurrentFormat();
  if (format) {
    if (frame_rate_ == 0.0)
      settings.frame_rate = format->frame_rate;
    settings.video_kind = GetVideoKindForFormat(*format);
  } else {
    // Format is only set for local tracks. For other tracks, use the frame rate
    // reported through settings callback SetSizeAndComputedFrameRate().
    if (computed_frame_rate_)
      settings.frame_rate = *computed_frame_rate_;
  }

  settings.facing_mode = ToWebFacingMode(source_->device().video_facing);
  settings.resize_mode = WebString::FromASCII(std::string(
      adapter_settings().target_size() ? WebMediaStreamTrack::kResizeModeRescale
                                       : WebMediaStreamTrack::kResizeModeNone));
  if (source_->device().display_media_info.has_value()) {
    const auto& info = source_->device().display_media_info.value();
    settings.display_surface = info->display_surface;
    settings.logical_surface = info->logical_surface;
    settings.cursor = info->cursor;
  }
}

void MediaStreamVideoTrack::OnReadyStateChanged(
    WebMediaStreamSource::ReadyState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  for (auto* sink : sinks_)
    sink->OnReadyStateChanged(state);
}

void MediaStreamVideoTrack::SetTrackAdapterSettings(
    const VideoTrackAdapterSettings& settings) {
  adapter_settings_ = std::make_unique<VideoTrackAdapterSettings>(settings);
}

media::VideoCaptureFormat MediaStreamVideoTrack::GetComputedSourceFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return computed_source_format_;
}

void MediaStreamVideoTrack::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (!source_)
    return;
  source_->OnFrameDropped(reason);
}

}  // namespace blink
