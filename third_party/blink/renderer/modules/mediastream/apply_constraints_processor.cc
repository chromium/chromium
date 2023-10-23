// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/apply_constraints_processor.h"

#include <utility>

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_audio.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

void RequestFailed(blink::ApplyConstraintsRequest* request,
                   const String& constraint,
                   const String& message) {
  DCHECK(request);
  request->RequestFailed(constraint, message);
}

void RequestSucceeded(blink::ApplyConstraintsRequest* request) {
  DCHECK(request);
  request->RequestSucceeded();
}

}  // namespace

BASE_FEATURE(kApplyConstraintsRestartsVideoContentSources,
             "ApplyConstraintsRestartsVideoContentSources",
             base::FEATURE_ENABLED_BY_DEFAULT);

ApplyConstraintsProcessor::ApplyConstraintsProcessor(
    LocalFrame* frame,
    MediaDevicesDispatcherCallback media_devices_dispatcher_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : frame_(frame),
      media_devices_dispatcher_cb_(std::move(media_devices_dispatcher_cb)),
      task_runner_(std::move(task_runner)) {
  DCHECK(frame_);
}

ApplyConstraintsProcessor::~ApplyConstraintsProcessor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ApplyConstraintsProcessor::ProcessRequest(
    blink::ApplyConstraintsRequest* request,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!request_completed_cb_);
  DCHECK(!current_request_);
  DCHECK(request->Track());
  if (!request->Track()->Source()) {
    CannotApplyConstraints(
        "Track has no source. ApplyConstraints not possible.");
    return;
  }
  request_completed_cb_ = std::move(callback);
  current_request_ = request;
  if (current_request_->Track()->GetSourceType() ==
      MediaStreamSource::kTypeVideo) {
    ProcessVideoRequest();
  } else {
    DCHECK_EQ(current_request_->Track()->GetSourceType(),
              MediaStreamSource::kTypeAudio);
    ProcessAudioRequest();
  }
}

void ApplyConstraintsProcessor::ProcessAudioRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_);
  DCHECK_EQ(current_request_->Track()->GetSourceType(),
            MediaStreamSource::kTypeAudio);
  DCHECK(request_completed_cb_);
  blink::MediaStreamAudioSource* audio_source = GetCurrentAudioSource();
  if (!audio_source) {
    CannotApplyConstraints("The track is not connected to any source");
    return;
  }

  blink::AudioCaptureSettings settings =
      SelectSettingsAudioCapture(audio_source, current_request_->Constraints());
  if (settings.HasValue()) {
    ApplyConstraintsSucceeded();
  } else {
    ApplyConstraintsFailed(settings.failed_constraint_name());
  }
}

void ApplyConstraintsProcessor::ProcessVideoRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_);
  DCHECK_EQ(current_request_->Track()->GetSourceType(),
            MediaStreamSource::kTypeVideo);
  DCHECK(request_completed_cb_);
  video_source_ = GetCurrentVideoSource();
  if (!video_source_) {
    CannotApplyConstraints("The track is not connected to any source");
    return;
  }

  // The sub-capture-target version is lost if the capture is restarted, because
  // of this we don't try to restart the source if cropTo() has ever been
  // called.
  const blink::MediaStreamDevice& device_info = video_source_->device();
  if (device_info.type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    ProcessVideoDeviceRequest();
  } else if (base::FeatureList::IsEnabled(
                 kApplyConstraintsRestartsVideoContentSources) &&
             video_source_->GetSubCaptureTargetVersion() == 0 &&
             (device_info.type ==
                  mojom::blink::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
              device_info.type ==
                  mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
              device_info.type == mojom::blink::MediaStreamType::
                                      DISPLAY_VIDEO_CAPTURE_THIS_TAB)) {
    ProcessVideoContentRequest();
  } else {
    FinalizeVideoRequest();
  }
}

void ApplyConstraintsProcessor::ProcessVideoDeviceRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_device_request_trace_ =
      ScopedMediaStreamTrace::CreateIfEnabled("VideoDeviceRequest");

  if (AbortIfVideoRequestStateInvalid())
    return;

  // TODO(crbug.com/768205): Support restarting the source even if there is more
  // than one track in the source.
  if (video_source_->NumTracks() > 1U) {
    FinalizeVideoRequest();
    return;
  }

  if (video_device_request_trace_)
    video_device_request_trace_->AddStep("GetAllVideoInputDeviceFormats");

  // It might be necessary to restart the video source. Before doing that,
  // check if the current format is the best format to satisfy the new
  // constraints. If this is the case, then the source does not need to be
  // restarted. To determine if the current format is the best, it is necessary
  // to know all the formats potentially supported by the source.
  GetMediaDevicesDispatcher()->GetAllVideoInputDeviceFormats(
      String(video_source_->device().id.data()),
      WTF::BindOnce(
          &ApplyConstraintsProcessor::MaybeStopVideoDeviceSourceForRestart,
          WrapWeakPersistent(this)));
}

void ApplyConstraintsProcessor::ProcessVideoContentRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (AbortIfVideoRequestStateInvalid()) {
    return;
  }

  // TODO(crbug.com/768205): Support restarting the source even if there is more
  // than one track in the source.
  if (video_source_->NumTracks() > 1U) {
    FinalizeVideoRequest();
    return;
  }

  MaybeStopVideoContentSourceForRestart();
}

void ApplyConstraintsProcessor::MaybeStopVideoDeviceSourceForRestart(
    const Vector<media::VideoCaptureFormat>& formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (AbortIfVideoRequestStateInvalid())
    return;

  blink::VideoCaptureSettings settings = SelectVideoDeviceSettings(formats);
  if (!settings.HasValue()) {
    ApplyConstraintsFailed(settings.failed_constraint_name());
    return;
  }

  if (video_source_->GetCurrentFormat() == settings.Format()) {
    video_source_->ReconfigureTrack(GetCurrentVideoTrack(),
                                    settings.track_adapter_settings());
    ApplyConstraintsSucceeded();
    GetCurrentVideoTrack()->NotifyConstraintsConfigurationComplete();
  } else {
    if (video_device_request_trace_)
      video_device_request_trace_->AddStep("StopForRestart");

    video_source_->StopForRestart(WTF::BindOnce(
        &ApplyConstraintsProcessor::MaybeDeviceSourceStoppedForRestart,
        WrapWeakPersistent(this)));
  }
}

void ApplyConstraintsProcessor::MaybeStopVideoContentSourceForRestart() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (AbortIfVideoRequestStateInvalid()) {
    return;
  }

  blink::VideoCaptureSettings settings = SelectVideoContentSettings();

  if (!settings.HasValue()) {
    ApplyConstraintsFailed(settings.failed_constraint_name());
    return;
  }

  if (video_source_->GetCurrentFormat() == settings.Format()) {
    if (settings.min_frame_rate().has_value()) {
      GetCurrentVideoTrack()->SetMinimumFrameRate(
          settings.min_frame_rate().value());
    }
    video_source_->ReconfigureTrack(GetCurrentVideoTrack(),
                                    settings.track_adapter_settings());
    ApplyConstraintsSucceeded();
    GetCurrentVideoTrack()->NotifyConstraintsConfigurationComplete();
  } else {
    video_source_->StopForRestart(WTF::BindOnce(
        &ApplyConstraintsProcessor::MaybeRestartStoppedVideoContentSource,
        WrapWeakPersistent(this)));
  }
}

void ApplyConstraintsProcessor::MaybeDeviceSourceStoppedForRestart(
    blink::MediaStreamVideoSource::RestartResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (AbortIfVideoRequestStateInvalid())
    return;

  if (result == blink::MediaStreamVideoSource::RestartResult::IS_RUNNING) {
    FinalizeVideoRequest();
    return;
  }

  if (video_device_request_trace_)
    video_device_request_trace_->AddStep("GetAvailableVideoInputDeviceFormats");

  DCHECK_EQ(result, blink::MediaStreamVideoSource::RestartResult::IS_STOPPED);
  GetMediaDevicesDispatcher()->GetAvailableVideoInputDeviceFormats(
      String(video_source_->device().id.data()),
      WTF::BindOnce(
          &ApplyConstraintsProcessor::FindNewFormatAndRestartDeviceSource,
          WrapWeakPersistent(this)));
}

void ApplyConstraintsProcessor::MaybeRestartStoppedVideoContentSource(
    blink::MediaStreamVideoSource::RestartResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (AbortIfVideoRequestStateInvalid()) {
    return;
  }

  if (result == blink::MediaStreamVideoSource::RestartResult::IS_RUNNING) {
    FinalizeVideoRequest();
    return;
  }

  DCHECK_EQ(result, blink::MediaStreamVideoSource::RestartResult::IS_STOPPED);

  blink::VideoCaptureSettings settings = SelectVideoContentSettings();
  // |settings| should have a value. If it does not due to some unexpected
  // reason (perhaps a race with another renderer process), restart the source
  // with the old format.
  DCHECK(video_source_->GetCurrentFormat());
  video_source_->Restart(
      settings.HasValue() ? settings.Format()
                          : *video_source_->GetCurrentFormat(),
      WTF::BindOnce(&ApplyConstraintsProcessor::MaybeSourceRestarted,
                    WrapWeakPersistent(this)));
}

void ApplyConstraintsProcessor::FindNewFormatAndRestartDeviceSource(
    const Vector<media::VideoCaptureFormat>& formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (AbortIfVideoRequestStateInvalid())
    return;

  if (video_device_request_trace_)
    video_device_request_trace_->AddStep("Restart");

  blink::VideoCaptureSettings settings = SelectVideoDeviceSettings(formats);
  DCHECK(video_source_->GetCurrentFormat());
  // |settings| should have a value. If it does not due to some unexpected
  // reason (perhaps a race with another renderer process), restart the source
  // with the old format.
  video_source_->Restart(
      settings.HasValue() ? settings.Format()
                          : *video_source_->GetCurrentFormat(),
      WTF::BindOnce(&ApplyConstraintsProcessor::MaybeSourceRestarted,
                    WrapWeakPersistent(this)));
}

void ApplyConstraintsProcessor::MaybeSourceRestarted(
    blink::MediaStreamVideoSource::RestartResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (AbortIfVideoRequestStateInvalid())
    return;

  if (result == blink::MediaStreamVideoSource::RestartResult::IS_RUNNING) {
    FinalizeVideoRequest();
  } else {
    if (video_device_request_trace_)
      video_device_request_trace_->AddStep("StopSource");

    DCHECK_EQ(result, blink::MediaStreamVideoSource::RestartResult::IS_STOPPED);
    CannotApplyConstraints("Source failed to restart");
    video_source_->StopSource();
  }
}

void ApplyConstraintsProcessor::FinalizeVideoRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (video_device_request_trace_)
    video_device_request_trace_->AddStep(__func__);

  if (AbortIfVideoRequestStateInvalid())
    return;

  media::VideoCaptureFormat format;
  if (video_source_->GetCurrentFormat()) {
    format = *video_source_->GetCurrentFormat();
  } else {
    format = GetCurrentVideoTrack()->GetComputedSourceFormat();
  }
  blink::VideoCaptureSettings settings = SelectVideoDeviceSettings({format});

  if (settings.HasValue()) {
    if (settings.min_frame_rate().has_value()) {
      GetCurrentVideoTrack()->SetMinimumFrameRate(
          settings.min_frame_rate().value());
    }
    video_source_->ReconfigureTrack(GetCurrentVideoTrack(),
                                    settings.track_adapter_settings());
    ApplyConstraintsSucceeded();
    GetCurrentVideoTrack()->NotifyConstraintsConfigurationComplete();
  } else {
    ApplyConstraintsFailed(settings.failed_constraint_name());
  }
}

blink::VideoCaptureSettings
ApplyConstraintsProcessor::SelectVideoDeviceSettings(
    Vector<media::VideoCaptureFormat> formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_);
  DCHECK_EQ(current_request_->Track()->GetSourceType(),
            MediaStreamSource::kTypeVideo);
  DCHECK(request_completed_cb_);
  DCHECK_GT(formats.size(), 0U);

  blink::VideoInputDeviceCapabilities device_capabilities;
  device_capabilities.device_id = current_request_->Track()->Source()->Id();
  device_capabilities.group_id = current_request_->Track()->Source()->GroupId();
  device_capabilities.facing_mode =
      GetCurrentVideoSource()
          ? static_cast<mojom::blink::FacingMode>(
                GetCurrentVideoSource()->device().video_facing)
          : mojom::blink::FacingMode::kNone;
  device_capabilities.formats = std::move(formats);

  blink::VideoDeviceCaptureCapabilities video_capabilities;
  video_capabilities.noise_reduction_capabilities.push_back(
      GetCurrentVideoTrack()->noise_reduction());
  video_capabilities.device_capabilities.push_back(
      std::move(device_capabilities));

  // Run SelectSettings using the track's current settings as the default
  // values. However, initialize |settings| with the default values as a
  // fallback in case GetSettings returns nothing and leaves |settings|
  // unmodified.
  MediaStreamTrackPlatform::Settings settings;
  settings.width = blink::MediaStreamVideoSource::kDefaultWidth;
  settings.height = blink::MediaStreamVideoSource::kDefaultHeight;
  settings.frame_rate = blink::MediaStreamVideoSource::kDefaultFrameRate;
  GetCurrentVideoTrack()->GetSettings(settings);

  return SelectSettingsVideoDeviceCapture(
      video_capabilities, current_request_->Constraints(), settings.width,
      settings.height, settings.frame_rate);
}

blink::VideoCaptureSettings
ApplyConstraintsProcessor::SelectVideoContentSettings() {
  DCHECK(video_source_);
  gfx::Size screen_size = MediaStreamUtils::GetScreenSize(frame_);
  return blink::SelectSettingsVideoContentCapture(
      current_request_->Constraints(), video_source_->device().type,
      screen_size.width(), screen_size.height());
}

blink::MediaStreamAudioSource*
ApplyConstraintsProcessor::GetCurrentAudioSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_);
  DCHECK(current_request_->Track());
  return blink::MediaStreamAudioSource::From(
      current_request_->Track()->Source());
}

blink::MediaStreamVideoTrack*
ApplyConstraintsProcessor::GetCurrentVideoTrack() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MediaStreamVideoTrack* track =
      MediaStreamVideoTrack::From(current_request_->Track());
  DCHECK(track);
  return track;
}

blink::MediaStreamVideoSource*
ApplyConstraintsProcessor::GetCurrentVideoSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCurrentVideoTrack()->source();
}

bool ApplyConstraintsProcessor::AbortIfVideoRequestStateInvalid() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_);
  DCHECK_EQ(current_request_->Track()->GetSourceType(),
            MediaStreamSource::kTypeVideo);
  DCHECK(request_completed_cb_);

  if (GetCurrentVideoSource() != video_source_) {
    if (video_device_request_trace_)
      video_device_request_trace_->AddStep("Aborted");
    CannotApplyConstraints(
        "Track stopped or source changed. ApplyConstraints not possible.");
    return true;
  }
  return false;
}

void ApplyConstraintsProcessor::ApplyConstraintsSucceeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&ApplyConstraintsProcessor::CleanupRequest,
                    WrapWeakPersistent(this),
                    WTF::BindOnce(&RequestSucceeded,
                                  WrapPersistent(current_request_.Get()))));
}

void ApplyConstraintsProcessor::ApplyConstraintsFailed(
    const char* failed_constraint_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(
          &ApplyConstraintsProcessor::CleanupRequest, WrapWeakPersistent(this),
          WTF::BindOnce(&RequestFailed, WrapPersistent(current_request_.Get()),
                        String(failed_constraint_name),
                        String("Cannot satisfy constraints"))));
}

void ApplyConstraintsProcessor::CannotApplyConstraints(const String& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(
          &ApplyConstraintsProcessor::CleanupRequest, WrapWeakPersistent(this),
          WTF::BindOnce(&RequestFailed, WrapPersistent(current_request_.Get()),
                        String(), message)));
}

void ApplyConstraintsProcessor::CleanupRequest(
    base::OnceClosure user_media_request_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_);
  DCHECK(request_completed_cb_);
  std::move(request_completed_cb_).Run();
  std::move(user_media_request_callback).Run();
  current_request_ = nullptr;
  video_source_ = nullptr;
  video_device_request_trace_.reset();
}

blink::mojom::blink::MediaDevicesDispatcherHost*
ApplyConstraintsProcessor::GetMediaDevicesDispatcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return media_devices_dispatcher_cb_.Run();
}

}  // namespace blink
