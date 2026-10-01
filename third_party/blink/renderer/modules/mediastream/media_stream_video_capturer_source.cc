// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "build/build_config.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/video_capture/video_capturer_source.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::MediaStreamRequestResult;

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    LocalFrame* frame,
    SourceStoppedCallback stop_callback,
    std::unique_ptr<VideoCapturerSource> source)
    : MediaStreamVideoSource(std::move(main_task_runner)),
      frame_(frame),
      source_(std::move(source)) {
  media::VideoCaptureFormats preferred_formats = source_->GetPreferredFormats();
  if (!preferred_formats.empty())
    capture_params_.requested_format = preferred_formats.front();
  SetStopCallback(std::move(stop_callback));
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    LocalFrame* frame,
    SourceStoppedCallback stop_callback,
    const MediaStreamDevice& device,
    const media::VideoCaptureParams& capture_params,
    DeviceCapturerFactoryCallback device_capturer_factory_callback)
    : MediaStreamVideoSource(std::move(main_task_runner)),
      frame_(frame),
      source_(device_capturer_factory_callback.Run(device.session_id())),
      capture_params_(capture_params),
      device_capturer_factory_callback_(
          std::move(device_capturer_factory_callback)) {
  DCHECK(!device.session_id().is_empty());
  SetStopCallback(std::move(stop_callback));
  SetDevice(device);
  SetDeviceRotationDetection(true /* enabled */);
  switch (device.type) {
    case mojom::blink::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      capture_params_.request_type =
          media::CaptureSourceRequestType::kGetUserMedia;
      break;
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
      capture_params_.request_type =
          media::CaptureSourceRequestType::kGetDisplayMedia;
      break;
    default:
      capture_params_.request_type = media::CaptureSourceRequestType::kUnknown;
      break;
  }
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void MediaStreamVideoCapturerSource::SetDeviceCapturerFactoryCallbackForTesting(
    DeviceCapturerFactoryCallback testing_factory_callback) {
  device_capturer_factory_callback_ = std::move(testing_factory_callback);
}

void MediaStreamVideoCapturerSource::OnSourceCanDiscardAlpha(
    bool can_discard_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->SetCanDiscardAlpha(can_discard_alpha);
}

void MediaStreamVideoCapturerSource::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->RequestRefreshFrame();
}

void MediaStreamVideoCapturerSource::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->OnLog(message);
}

void MediaStreamVideoCapturerSource::OnHasConsumers(bool has_consumers) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (has_consumers)
    source_->Resume();
  else
    source_->MaybeSuspend();
}

void MediaStreamVideoCapturerSource::OnCapturingLinkSecured(bool is_secure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!frame_ || !frame_->Client())
    return;
  GetMediaStreamDispatcherHost()->SetCapturingLinkSecured(
      device().serializable_session_id(),
      static_cast<mojom::blink::MediaStreamType>(device().type), is_secure);
}

void MediaStreamVideoCapturerSource::StartSourceImpl(
    MediaStreamVideoSourceCallbacks media_stream_callbacks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  state_ = kStarting;

  frame_callback_ = media_stream_callbacks.deliver_frame_cb;
  capture_version_callback_ = media_stream_callbacks.capture_version_cb;
  frame_dropped_callback_ = media_stream_callbacks.frame_dropped_cb;

  VideoCaptureCallbacks video_capture_callbacks;
  video_capture_callbacks.deliver_frame_cb =
      std::move(media_stream_callbacks.deliver_frame_cb);
  video_capture_callbacks.capture_version_cb =
      std::move(media_stream_callbacks.capture_version_cb);
  video_capture_callbacks.frame_dropped_cb =
      std::move(media_stream_callbacks.frame_dropped_cb);
  source_->StartCapture(
      capture_params_, std::move(video_capture_callbacks),
      blink::BindRepeating(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                           weak_factory_.GetWeakPtr(), capture_params_));
}

media::VideoCaptureFeedbackCB
MediaStreamVideoCapturerSource::GetFeedbackCallback() const {
  return source_->GetFeedbackCallback();
}

void MediaStreamVideoCapturerSource::StopSourceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  source_->StopCapture();
}

void MediaStreamVideoCapturerSource::StopSourceForRestartImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ != kStarted) {
    OnStopForRestartDone(false);
    return;
  }
  state_ = kStoppingForRestart;
  source_->StopCapture();

  // Force state update for nondevice sources, since they do not
  // automatically update state after StopCapture().
  if (device().type == mojom::blink::MediaStreamType::NO_SERVICE)
    OnRunStateChanged(capture_params_, VideoCaptureRunState::kStopped);
}

void MediaStreamVideoCapturerSource::RestartSourceImpl(
    const media::VideoCaptureFormat& new_format) {
  DCHECK(new_format.IsValid());
  media::VideoCaptureParams new_capture_params = capture_params_;
  new_capture_params.requested_format = new_format;
  state_ = kRestarting;

  VideoCaptureCallbacks video_capture_callbacks;
  video_capture_callbacks.deliver_frame_cb = frame_callback_;
  video_capture_callbacks.capture_version_cb = capture_version_callback_;
  video_capture_callbacks.frame_dropped_cb = frame_dropped_callback_;

  source_->StartCapture(
      new_capture_params, std::move(video_capture_callbacks),
      blink::BindRepeating(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                           weak_factory_.GetWeakPtr(), new_capture_params));
}

std::optional<media::VideoCaptureFormat>
MediaStreamVideoCapturerSource::GetCurrentFormat() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return capture_params_.requested_format;
}

void MediaStreamVideoCapturerSource::ChangeSourceImpl(
    const MediaStreamDevice& new_device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(device_capturer_factory_callback_);

  if (state_ != kStarted && state_ != kStoppedForRestart) {
    return;
  }

  if (state_ == kStarted) {
    state_ = kStoppingForChangeSource;
    source_->StopCapture();
  } else {
    DCHECK_EQ(state_, kStoppedForRestart);
    state_ = kRestartingAfterSourceChange;
  }
  SetDevice(new_device);
  source_ = device_capturer_factory_callback_.Run(new_device.session_id());

  capture_params_.capture_version_source += 1;
  sub_capture_version_ = 0;
  track_targets_.clear();
  gpu_target_ = base::Token();
  active_gpu_target_type_ = media::mojom::SubCaptureTargetType::kCropTarget;
  for (auto* track : Tracks()) {
    SetTrackSubCaptureTarget(track, base::Token());
  }

  VideoCaptureCallbacks video_capture_callbacks;
  video_capture_callbacks.deliver_frame_cb = frame_callback_;
  video_capture_callbacks.capture_version_cb = capture_version_callback_;
  video_capture_callbacks.frame_dropped_cb = frame_dropped_callback_;
  source_->StartCapture(
      capture_params_, std::move(video_capture_callbacks),
      blink::BindRepeating(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                           weak_factory_.GetWeakPtr(), capture_params_));
}

bool MediaStreamVideoCapturerSource::HasActiveRestrictionTarget() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& entry : track_targets_) {
    if (entry.value.type ==
            media::mojom::SubCaptureTargetType::kRestrictionTarget &&
        !entry.value.target.is_zero()) {
      return true;
    }
  }
  return false;
}

bool MediaStreamVideoCapturerSource::CanApplySubCaptureTarget(
    media::mojom::SubCaptureTargetType type) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Element Capture (restrictTo) requires single-track physical restriction
  // on GPU and cannot support clones or multi-track mode. Region Capture
  // (cropTo) is limited the same way when kRegionCaptureOfClonedTracks is off.
  if (type == media::mojom::SubCaptureTargetType::kRestrictionTarget ||
      !base::FeatureList::IsEnabled(features::kRegionCaptureOfClonedTracks)) {
    return NumTracks() == 1;
  }
  return NumTracks() <= 1 || !HasActiveRestrictionTarget();
}

base::Token MediaStreamVideoCapturerSource::ComputeSharedGpuCropTarget() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::Token shared_target;
  for (auto* t : Tracks()) {
    auto it = track_targets_.find(t);
    if (it == track_targets_.end() ||
        it->value.type != media::mojom::SubCaptureTargetType::kCropTarget ||
        it->value.target.is_zero()) {
      return base::Token();
    }
    const base::Token& target = it->value.target;
    if (shared_target.is_zero()) {
      shared_target = target;
    } else if (shared_target != target) {
      return base::Token();
    }
  }
  return shared_target;
}

void MediaStreamVideoCapturerSource::ApplySubCaptureTarget(
    MediaStreamVideoTrack* track,
    media::mojom::SubCaptureTargetType type,
    const base::Token& sub_capture_target,
    uint32_t sub_capture_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Per-track bookkeeping below is what ComputeSharedGpuCropTarget() reads to
  // decide between single-target GPU cropping and full-frame capture, so a
  // target cannot be applied without knowing which track it belongs to.
  CHECK(track);
  const std::optional<base::UnguessableToken>& session_id =
      device().serializable_session_id();
  if (!session_id.has_value()) {
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);
    return;
  }

  auto* host = GetMediaStreamDispatcherHost();
  if (!host) {
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);
    return;
  }

  if (!CanApplySubCaptureTarget(type)) {
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kInvalidTarget);
    return;
  }

  track_targets_.Set(track, SubCaptureTargetInfo{type, sub_capture_target});
  const base::Token track_adapter_target =
      (type == media::mojom::SubCaptureTargetType::kCropTarget)
          ? sub_capture_target
          : base::Token();
  SetTrackSubCaptureTarget(track, track_adapter_target);

  const base::Token desired_gpu_target =
      (type == media::mojom::SubCaptureTargetType::kCropTarget)
          ? ComputeSharedGpuCropTarget()
          : sub_capture_target;
  gpu_target_ = desired_gpu_target;
  active_gpu_target_type_ = type;

  // Always send the IPC even if |desired_gpu_target == gpu_target_| so that Viz
  // increments its sub-capture version counter and stamps subsequent frames
  // with |sub_capture_version|, which resolves the JS cropTo()/restrictTo()
  // Promise.
  host->ApplySubCaptureTarget(session_id.value(), type, desired_gpu_target,
                              sub_capture_version, std::move(callback));
}

void MediaStreamVideoCapturerSource::OnTrackCloned(
    const MediaStreamVideoTrack* original_track,
    MediaStreamVideoTrack* cloned_track) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = track_targets_.find(original_track);
  if (it != track_targets_.end()) {
    const SubCaptureTargetInfo inherited_target = it->value;
    track_targets_.Set(cloned_track, inherited_target);

    if (inherited_target.type ==
        media::mojom::SubCaptureTargetType::kCropTarget) {
      SetTrackSubCaptureTarget(cloned_track, inherited_target.target);
    }
  }

  ReevaluateCaptureMode();
}

void MediaStreamVideoCapturerSource::OnTrackRemoved(
    MediaStreamVideoTrack* track) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  track_targets_.erase(track);
  ReevaluateCaptureMode();
}

void MediaStreamVideoCapturerSource::ReevaluateCaptureMode() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::optional<base::UnguessableToken>& session_id =
      device().serializable_session_id();
  if (!session_id.has_value() || HasActiveRestrictionTarget()) {
    return;
  }

  base::Token desired_gpu_target = ComputeSharedGpuCropTarget();
  if (desired_gpu_target != gpu_target_) {
    active_gpu_target_type_ = media::mojom::SubCaptureTargetType::kCropTarget;
    gpu_target_ = desired_gpu_target;
    ++sub_capture_version_;
    if (auto* host = GetMediaStreamDispatcherHost()) {
      host->ApplySubCaptureTarget(session_id.value(), active_gpu_target_type_,
                                  desired_gpu_target, sub_capture_version_,
                                  base::DoNothing());
    }
  }
}

media::CaptureVersion MediaStreamVideoCapturerSource::GetCaptureVersion()
    const {
  return media::CaptureVersion(capture_params_.capture_version_source,
                               sub_capture_version_);
}

std::optional<media::CaptureVersion>
MediaStreamVideoCapturerSource::GetNextCaptureVersion(
    media::mojom::SubCaptureTargetType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!CanApplySubCaptureTarget(type)) {
    return std::nullopt;
  }

  return media::CaptureVersion(capture_params_.capture_version_source,
                               ++sub_capture_version_);
}

base::WeakPtr<MediaStreamVideoSource>
MediaStreamVideoCapturerSource::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool MediaStreamVideoCapturerSource::AllowsVideoThreadTypeOverride() const {
  switch (device().type) {
    case mojom::blink::MediaStreamType::NO_SERVICE:
    case mojom::blink::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::DEVICE_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
    case mojom::blink::MediaStreamType::NUM_MEDIA_TYPES:
      return false;
    case mojom::blink::MediaStreamType::DEVICE_VIDEO_CAPTURE:
    case mojom::blink::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
    case mojom::blink::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
    case mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      return true;
  }
}

void MediaStreamVideoCapturerSource::OnRunStateChanged(
    const media::VideoCaptureParams& new_capture_params,
    VideoCaptureRunState run_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool is_running = (run_state == VideoCaptureRunState::kRunning);
  switch (state_) {
    case kStarting:
      source_->OnLog("MediaStreamVideoCapturerSource sending OnStartDone");
      if (is_running) {
        state_ = kStarted;
        DCHECK(capture_params_ == new_capture_params);
        OnStartDone(MediaStreamRequestResult::OK);
      } else {
        state_ = kStopped;
        MediaStreamRequestResult result;
        switch (run_state) {
          case VideoCaptureRunState::kSystemPermissionsError:
            result = MediaStreamRequestResult::PERMISSION_DENIED_BY_SYSTEM;
            break;
          case VideoCaptureRunState::kCameraBusyError:
            result = MediaStreamRequestResult::DEVICE_IN_USE;
            break;
          case VideoCaptureRunState::kStartTimeoutError:
            result = MediaStreamRequestResult::START_TIMEOUT;
            break;
          case VideoCaptureRunState::kStopped:
            result = MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO;
            break;
          case VideoCaptureRunState::kRunning:
            NOTREACHED();
        }
        OnStartDone(result);
      }
      break;
    case kStarted:
      if (!is_running) {
        state_ = kStopped;
        StopSource();
      }
      break;
    case kStoppingForRestart:
      source_->OnLog(
          "MediaStreamVideoCapturerSource sending OnStopForRestartDone");
      state_ = is_running ? kStarted : kStoppedForRestart;
      OnStopForRestartDone(!is_running);
      break;
    case kStoppingForChangeSource:
      state_ = is_running ? kStarted : kStopped;
      break;
    case kRestarting:
      if (is_running) {
        state_ = kStarted;
        capture_params_ = new_capture_params;
      } else {
        state_ = kStoppedForRestart;
      }
      source_->OnLog("MediaStreamVideoCapturerSource sending OnRestartDone");
      OnRestartDone(is_running);
      break;
    case kRestartingAfterSourceChange:
      if (is_running) {
        state_ = kStarted;
        capture_params_ = new_capture_params;
      } else {
        state_ = kStoppedForRestart;
      }
      source_->OnLog("MediaStreamVideoCapturerSource sending OnRestartDone");
      OnRestartBySourceSwitchDone(is_running);
      break;
    case kStopped:
    case kStoppedForRestart:
      break;
  }
}

mojom::blink::MediaStreamDispatcherHost*
MediaStreamVideoCapturerSource::GetMediaStreamDispatcherHost() {
  if (host_) {
    return host_.get();
  }
  if (!frame_) {
    return nullptr;
  }
  frame_->GetBrowserInterfaceBroker().GetInterface(
      host_.BindNewPipeAndPassReceiver());
  return host_.get();
}

void MediaStreamVideoCapturerSource::SetMediaStreamDispatcherHostForTesting(
    mojo::PendingRemote<mojom::blink::MediaStreamDispatcherHost> host) {
  host_.reset();
  if (host.is_valid()) {
    host_.Bind(std::move(host));
  }
}

VideoCapturerSource* MediaStreamVideoCapturerSource::GetSourceForTesting() {
  return source_.get();
}

}  // namespace blink
