// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <numeric>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/token.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

// static
MediaStreamVideoSource* MediaStreamVideoSource::GetVideoSource(
    MediaStreamSource* source) {
  if (!source || source->GetType() != MediaStreamSource::kTypeVideo) {
    return nullptr;
  }
  return static_cast<MediaStreamVideoSource*>(source->GetPlatformSource());
}

MediaStreamVideoSource::MediaStreamVideoSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : WebPlatformMediaStreamSource(std::move(task_runner)), state_(NEW) {}

MediaStreamVideoSource::~MediaStreamVideoSource() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (remove_last_track_callback_) {
    std::move(remove_last_track_callback_).Run();
  }
}

void MediaStreamVideoSource::AddTrack(
    MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& track_adapter_settings,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const VideoCaptureNotifyFrameDroppedCB& notify_frame_dropped_callback,
    const EncodedVideoFrameCB& encoded_frame_callback,
    const VideoCaptureSubCaptureTargetVersionCB&
        sub_capture_target_version_callback,
    const VideoTrackSettingsCallback& settings_callback,
    const VideoTrackFormatCallback& format_callback,
    ConstraintsOnceCallback callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(!base::Contains(tracks_, track));
  tracks_.push_back(track);
  secure_tracker_.Add(track, true);

  pending_tracks_.push_back(PendingTrackInfo{
      track, frame_callback, notify_frame_dropped_callback,
      encoded_frame_callback, sub_capture_target_version_callback,
      settings_callback, format_callback,
      std::make_unique<VideoTrackAdapterSettings>(track_adapter_settings),
      std::move(callback)});

  switch (state_) {
    case NEW: {
      state_ = STARTING;
      auto deliver_frame_on_video_callback =
          ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
              &VideoTrackAdapter::DeliverFrameOnVideoTaskRunner,
              GetTrackAdapter()));
      auto deliver_encoded_frame_on_video_callback =
          ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
              &VideoTrackAdapter::DeliverEncodedVideoFrameOnVideoTaskRunner,
              GetTrackAdapter()));
      auto new_sub_capture_target_version_on_video_callback =
          ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
              &VideoTrackAdapter::NewSubCaptureTargetVersionOnVideoTaskRunner,
              GetTrackAdapter()));
      VideoCaptureNotifyFrameDroppedCB frame_dropped_callback =
          ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
              &VideoTrackAdapter::OnFrameDroppedOnVideoTaskRunner,
              GetTrackAdapter()));
      // Callbacks are invoked from the IO thread. With
      // UseThreadPoolForMediaStreamVideoTaskRunner disabled, the video task
      // runner is the same as the IO thread and there is no need to post frames
      // to the video task runner.
      if (base::FeatureList::IsEnabled(
              features::kUseThreadPoolForMediaStreamVideoTaskRunner)) {
        StartSourceImpl(
            base::BindPostTask(video_task_runner(),
                               std::move(deliver_frame_on_video_callback)),
            base::BindPostTask(
                video_task_runner(),
                std::move(deliver_encoded_frame_on_video_callback)),
            base::BindPostTask(
                video_task_runner(),
                std::move(new_sub_capture_target_version_on_video_callback)),
            base::BindPostTask(video_task_runner(),
                               std::move(frame_dropped_callback)));
      } else {
        StartSourceImpl(
            std::move(deliver_frame_on_video_callback),
            std::move(deliver_encoded_frame_on_video_callback),
            std::move(new_sub_capture_target_version_on_video_callback),
            std::move(frame_dropped_callback));
      }
      break;
    }
    case STARTING:
    case STOPPING_FOR_RESTART:
    case STOPPED_FOR_RESTART:
    case RESTARTING: {
      // These cases are handled by OnStartDone(), OnStoppedForRestartDone()
      // and OnRestartDone().
      break;
    }
    case ENDED: {
      FinalizeAddPendingTracks(
          mojom::blink::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO);
      break;
    }
    case STARTED: {
      FinalizeAddPendingTracks(mojom::blink::MediaStreamRequestResult::OK);
      break;
    }
  }

  UpdateCanDiscardAlpha();
}

void MediaStreamVideoSource::RemoveTrack(MediaStreamVideoTrack* video_track,
                                         base::OnceClosure callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  {
    auto it = tracks_.Find(video_track);
    DCHECK_NE(it, kNotFound);
    tracks_.EraseAt(it);
  }
  secure_tracker_.Remove(video_track);

  {
    auto it = suspended_tracks_.Find(video_track);
    if (it != kNotFound)
      suspended_tracks_.EraseAt(it);
  }

  for (auto it = pending_tracks_.begin(); it != pending_tracks_.end(); ++it) {
    if (it->track == video_track) {
      pending_tracks_.erase(it);
      break;
    }
  }

  // Call |frame_adapter_->RemoveTrack| here even if adding the track has
  // failed and |frame_adapter_->AddCallback| has not been called.
  GetTrackAdapter()->RemoveTrack(video_track);

  if (video_track->CountEncodedSinks()) {
    // Notifies the source that encoded sinks have been removed.
    UpdateNumEncodedSinks();
  }

  if (tracks_.empty()) {
    if (callback) {
      // Use StopForRestart() in order to get a notification of when the
      // source is actually stopped (if supported). The source will not be
      // restarted.
      // The intent is to have the same effect as StopSource() (i.e., having
      // the readyState updated and invoking the source's stop callback on this
      // task), but getting a notification of when the source has actually
      // stopped so that clients have a mechanism to serialize the creation and
      // destruction of video sources. Without such serialization it is possible
      // that concurrent creation and destruction of sources that share the same
      // underlying implementation results in failed source creation since
      // stopping a source with StopSource() can have side effects that affect
      // sources created after that StopSource() call, but before the actual
      // stop takes place. See https://crbug.com/778039.
      remove_last_track_callback_ = std::move(callback);
      StopForRestart(
          WTF::BindOnce(&MediaStreamVideoSource::DidStopSource, GetWeakPtr()));
      if (state_ == STOPPING_FOR_RESTART || state_ == STOPPED_FOR_RESTART) {
        // If the source supports restarting, it is necessary to call
        // FinalizeStopSource() to ensure the same behavior as StopSource(),
        // even if the underlying implementation takes longer to actually stop.
        // In particular, Tab capture and capture from element require the
        // source's stop callback to be invoked on this task in order to ensure
        // correct behavior.
        FinalizeStopSource();
      } else {
        // If the source does not support restarting, call StopSource()
        // to ensure stop on this task. DidStopSource() will be called on
        // another task even if the source does not support restarting, as
        // StopForRestart() always posts a task to run its callback.
        StopSource();
      }
    } else {
      StopSource();
    }
  } else if (callback) {
    std::move(callback).Run();
  }

  UpdateCanDiscardAlpha();
}

void MediaStreamVideoSource::DidStopSource(RestartResult result) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(remove_last_track_callback_);
  if (result == RestartResult::IS_STOPPED) {
    state_ = ENDED;
  }

  if (state_ != ENDED) {
    // This can happen if a source that supports StopForRestart() fails to
    // actually stop the source after trying to stop it. The contract for
    // StopForRestart() allows this, but it should not happen in practice.
    LOG(WARNING) << "Source unexpectedly failed to stop. Force stopping and "
                    "sending notification anyway";
    StopSource();
  }
  std::move(remove_last_track_callback_).Run();
}

void MediaStreamVideoSource::ReconfigureTrack(
    MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& adapter_settings) {
  GetTrackAdapter()->ReconfigureTrack(track, adapter_settings);
  // It's OK to reconfigure settings even if ReconfigureTrack fails, provided
  // |track| is not connected to a different source, which is a precondition
  // for calling this method.
  UpdateTrackSettings(track, adapter_settings);
}

void MediaStreamVideoSource::StopForRestart(RestartCallback callback,
                                            bool send_black_frame) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (state_ != STARTED) {
    GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::BindOnce(std::move(callback), RestartResult::INVALID_STATE));
    return;
  }

  DCHECK(!restart_callback_);
  GetTrackAdapter()->StopFrameMonitoring();
  state_ = STOPPING_FOR_RESTART;
  restart_callback_ = std::move(callback);

  if (send_black_frame) {
    const std::optional<gfx::Size> source_size =
        GetTrackAdapter()->source_frame_size();
    scoped_refptr<media::VideoFrame> black_frame =
        media::VideoFrame::CreateBlackFrame(
            source_size.has_value() ? *source_size
                                    : gfx::Size(kDefaultWidth, kDefaultHeight));
    PostCrossThreadTask(
        *video_task_runner(), FROM_HERE,
        CrossThreadBindOnce(&VideoTrackAdapter::DeliverFrameOnVideoTaskRunner,
                            GetTrackAdapter(), black_frame,
                            base::TimeTicks::Now()));
  }

  StopSourceForRestartImpl();
}

void MediaStreamVideoSource::StopSourceForRestartImpl() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(state_, STOPPING_FOR_RESTART);
  OnStopForRestartDone(false);
}

void MediaStreamVideoSource::OnStopForRestartDone(bool did_stop_for_restart) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (state_ == ENDED) {
    return;
  }

  DCHECK_EQ(state_, STOPPING_FOR_RESTART);
  if (did_stop_for_restart) {
    state_ = STOPPED_FOR_RESTART;
  } else {
    state_ = STARTED;
    StartFrameMonitoring();
    FinalizeAddPendingTracks(mojom::blink::MediaStreamRequestResult::OK);
  }
  DCHECK(restart_callback_);

  RestartResult result = did_stop_for_restart ? RestartResult::IS_STOPPED
                                              : RestartResult::IS_RUNNING;
  GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(std::move(restart_callback_), result));
}

void MediaStreamVideoSource::Restart(
    const media::VideoCaptureFormat& new_format,
    RestartCallback callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (state_ != STOPPED_FOR_RESTART) {
    GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::BindOnce(std::move(callback), RestartResult::INVALID_STATE));
    return;
  }
  DCHECK(!restart_callback_);
  state_ = RESTARTING;
  restart_callback_ = std::move(callback);
  RestartSourceImpl(new_format);
}

void MediaStreamVideoSource::RestartSourceImpl(
    const media::VideoCaptureFormat& new_format) {
  NOTREACHED_IN_MIGRATION();
}

void MediaStreamVideoSource::OnRestartDone(bool did_restart) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (state_ == ENDED)
    return;

  DCHECK_EQ(state_, RESTARTING);
  if (did_restart) {
    state_ = STARTED;
    StartFrameMonitoring();
    FinalizeAddPendingTracks(mojom::blink::MediaStreamRequestResult::OK);
  } else {
    state_ = STOPPED_FOR_RESTART;
  }

  DCHECK(restart_callback_);
  RestartResult result =
      did_restart ? RestartResult::IS_RUNNING : RestartResult::IS_STOPPED;
  GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(std::move(restart_callback_), result));
}

void MediaStreamVideoSource::OnRestartBySourceSwitchDone(bool did_restart) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(base::FeatureList::IsEnabled(
      features::kAllowSourceSwitchOnPausedVideoMediaStream));
  if (state_ == ENDED)
    return;
  DCHECK_EQ(state_, STOPPED_FOR_RESTART);
  if (did_restart) {
    state_ = STARTED;
    StartFrameMonitoring();
    FinalizeAddPendingTracks(mojom::blink::MediaStreamRequestResult::OK);
  }
}

void MediaStreamVideoSource::UpdateHasConsumers(MediaStreamVideoTrack* track,
                                                bool has_consumers) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const auto it = suspended_tracks_.Find(track);
  if (has_consumers) {
    if (it != kNotFound)
      suspended_tracks_.EraseAt(it);
  } else {
    if (it == kNotFound)
      suspended_tracks_.push_back(track);
  }
  OnHasConsumers(suspended_tracks_.size() < tracks_.size());
}

void MediaStreamVideoSource::UpdateCapturingLinkSecure(
    MediaStreamVideoTrack* track,
    bool is_secure) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  secure_tracker_.Update(track, is_secure);
  NotifyCapturingLinkSecured(CountEncodedSinks());
}

void MediaStreamVideoSource::NotifyCapturingLinkSecured(
    size_t num_encoded_sinks) {
  // Encoded sinks imply insecure sinks.
  OnCapturingLinkSecured(secure_tracker_.is_capturing_secure() &&
                         num_encoded_sinks == 0);
}

void MediaStreamVideoSource::SetDeviceRotationDetection(bool enabled) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  enable_device_rotation_detection_ = enabled;
}

base::SequencedTaskRunner* MediaStreamVideoSource::video_task_runner() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return Platform::Current()->GetMediaStreamVideoSourceVideoTaskRunner().get();
}

std::optional<media::VideoCaptureFormat>
MediaStreamVideoSource::GetCurrentFormat() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return std::optional<media::VideoCaptureFormat>();
}

size_t MediaStreamVideoSource::CountEncodedSinks() const {
  return std::accumulate(tracks_.begin(), tracks_.end(), size_t(0),
                         [](size_t accum, MediaStreamVideoTrack* track) {
                           return accum + track->CountEncodedSinks();
                         });
}

void MediaStreamVideoSource::UpdateNumEncodedSinks() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  size_t count = CountEncodedSinks();
  if (count == 1) {
    OnEncodedSinkEnabled();
  } else if (count == 0) {
    OnEncodedSinkDisabled();
  }
  // Encoded sinks are insecure.
  NotifyCapturingLinkSecured(count);
}

void MediaStreamVideoSource::DoChangeSource(
    const MediaStreamDevice& new_device) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "MediaStreamVideoSource::DoChangeSource: "
           << ", new device id = " << new_device.id
           << ", session id = " << new_device.session_id();
  if (!base::FeatureList::IsEnabled(
          features::kAllowSourceSwitchOnPausedVideoMediaStream) &&
      state_ != STARTED) {
    return;
  }
  if (state_ != STARTED && state_ != STOPPED_FOR_RESTART) {
    return;
  }

  ChangeSourceImpl(new_device);
}

void MediaStreamVideoSource::DoStopSource() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(3) << "DoStopSource()";
  if (state_ == ENDED)
    return;
  GetTrackAdapter()->StopFrameMonitoring();
  StopSourceImpl();
  state_ = ENDED;
  SetReadyState(WebMediaStreamSource::kReadyStateEnded);
}

void MediaStreamVideoSource::OnStartDone(
    mojom::blink::MediaStreamRequestResult result) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(3) << "OnStartDone({result =" << result << "})";
  if (state_ == ENDED) {
    OnLog(
        "MediaStreamVideoSource::OnStartDone dropping event because state_ == "
        "ENDED.");
    return;
  }

  if (result == mojom::blink::MediaStreamRequestResult::OK) {
    DCHECK_EQ(STARTING, state_);
    OnLog("MediaStreamVideoSource changing state to STARTED");
    state_ = STARTED;
    SetReadyState(WebMediaStreamSource::kReadyStateLive);
    StartFrameMonitoring();
  } else {
    StopSource();
  }

  if (start_callback_) {
    std::move(start_callback_).Run(this, result);
  }

  // This object can be deleted after calling FinalizeAddPendingTracks. See
  // comment in the header file.
  FinalizeAddPendingTracks(result);
}

void MediaStreamVideoSource::FinalizeAddPendingTracks(
    mojom::blink::MediaStreamRequestResult result) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  Vector<PendingTrackInfo> pending_track_descriptors;
  pending_track_descriptors.swap(pending_tracks_);
  for (auto& track_info : pending_track_descriptors) {
    if (result == mojom::blink::MediaStreamRequestResult::OK) {
      GetTrackAdapter()->AddTrack(
          track_info.track, track_info.frame_callback,
          track_info.notify_frame_dropped_callback,
          track_info.encoded_frame_callback,
          track_info.sub_capture_target_version_callback,
          track_info.settings_callback, track_info.format_callback,
          *track_info.adapter_settings);
      UpdateTrackSettings(track_info.track, *track_info.adapter_settings);
    }

    if (!track_info.callback.is_null()) {
      OnLog(
          "MediaStreamVideoSource invoking callback indicating result of "
          "starting track.");
      std::move(track_info.callback).Run(this, result, WebString());
    } else {
      OnLog(
          "MediaStreamVideoSource dropping event indicating result of starting "
          "track.");
    }
  }
}

void MediaStreamVideoSource::StartFrameMonitoring() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  std::optional<media::VideoCaptureFormat> current_format = GetCurrentFormat();
  double frame_rate = current_format ? current_format->frame_rate : 0.0;
  if (current_format && enable_device_rotation_detection_) {
    GetTrackAdapter()->SetSourceFrameSize(current_format->frame_size);
  }
  GetTrackAdapter()->StartFrameMonitoring(
      frame_rate,
      WTF::BindRepeating(&MediaStreamVideoSource::SetMutedState, GetWeakPtr()));
}

void MediaStreamVideoSource::SetReadyState(
    WebMediaStreamSource::ReadyState state) {
  DVLOG(3) << "MediaStreamVideoSource::SetReadyState state " << state;
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!Owner().IsNull())
    Owner().SetReadyState(state);
  for (auto* track : tracks_)
    track->OnReadyStateChanged(state);
}

void MediaStreamVideoSource::SetMutedState(bool muted_state) {
  DVLOG(3) << "MediaStreamVideoSource::SetMutedState state=" << muted_state;
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!Owner().IsNull()) {
    Owner().SetReadyState(muted_state ? WebMediaStreamSource::kReadyStateMuted
                                      : WebMediaStreamSource::kReadyStateLive);
  }
}

void MediaStreamVideoSource::UpdateTrackSettings(
    MediaStreamVideoTrack* track,
    const VideoTrackAdapterSettings& adapter_settings) {
  // If the source does not provide a format, do not set any target dimensions
  // or frame rate.
  if (!GetCurrentFormat())
    return;

  // Calculate resulting frame size if the source delivers frames
  // according to the current format. Note: Format may change later.
  gfx::Size desired_size;
  if (VideoTrackAdapter::CalculateDesiredSize(
          false /* is_rotated */, GetCurrentFormat()->frame_size,
          adapter_settings, &desired_size)) {
    track->SetTargetSize(desired_size.width(), desired_size.height());
  }
  track->SetTrackAdapterSettings(adapter_settings);
}

bool MediaStreamVideoSource::SupportsEncodedOutput() const {
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
void MediaStreamVideoSource::ApplySubCaptureTarget(
    media::mojom::blink::SubCaptureTargetType type,
    const base::Token& sub_capture_target,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  std::move(callback).Run(
      media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);
}

std::optional<uint32_t>
MediaStreamVideoSource::GetNextSubCaptureTargetVersion() {
  return std::nullopt;
}
#endif

uint32_t MediaStreamVideoSource::GetSubCaptureTargetVersion() const {
  return 0;
}

VideoCaptureFeedbackCB MediaStreamVideoSource::GetFeedbackCallback() const {
  // Each source implementation has to implement its own feedback callbacks.
  return base::DoNothing();
}

void MediaStreamVideoSource::SetStartCallback(SourceStartCallback callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  start_callback_ = std::move(callback);
}

scoped_refptr<VideoTrackAdapter> MediaStreamVideoSource::GetTrackAdapter() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!track_adapter_) {
    track_adapter_ = base::MakeRefCounted<VideoTrackAdapter>(
        video_task_runner(), GetWeakPtr());
  }
  return track_adapter_;
}

void MediaStreamVideoSource::UpdateCanDiscardAlpha() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  bool using_alpha = false;
  for (auto* track : tracks_) {
    if (track->UsingAlpha()) {
      using_alpha = true;
      break;
    }
  }
  OnSourceCanDiscardAlpha(!using_alpha);
}

}  // namespace blink
