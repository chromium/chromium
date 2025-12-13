// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "build/build_config.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
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

  VideoCaptureCallbacks video_capture_callbacks;
  video_capture_callbacks.deliver_frame_cb = frame_callback_;
  video_capture_callbacks.capture_version_cb = capture_version_callback_;
  video_capture_callbacks.frame_dropped_cb = frame_dropped_callback_;
  source_->StartCapture(
      capture_params_, std::move(video_capture_callbacks),
      blink::BindRepeating(&MediaStreamVideoCapturerSource::OnRunStateChanged,
                           weak_factory_.GetWeakPtr(), capture_params_));
}

void MediaStreamVideoCapturerSource::ApplySubCaptureTarget(
    media::mojom::blink::SubCaptureTargetType type,
    const base::Token& sub_capture_target,
    uint32_t sub_capture_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::optional<base::UnguessableToken>& session_id =
      device().serializable_session_id();
  if (!session_id.has_value()) {
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);
    return;
  }
  GetMediaStreamDispatcherHost()->ApplySubCaptureTarget(
      session_id.value(), type, sub_capture_target, sub_capture_version,
      std::move(callback));
}

media::CaptureVersion MediaStreamVideoCapturerSource::GetCaptureVersion()
    const {
  return media::CaptureVersion(capture_params_.capture_version_source,
                               sub_capture_version_);
}

std::optional<media::CaptureVersion>
MediaStreamVideoCapturerSource::GetNextCaptureVersion() {
  if (NumTracks() != 1) {
    return std::nullopt;
  }

  return media::CaptureVersion(capture_params_.capture_version_source,
                               ++sub_capture_version_);
}

base::WeakPtr<MediaStreamVideoSource>
MediaStreamVideoCapturerSource::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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
  DCHECK(frame_);
  if (!host_) {
    frame_->GetBrowserInterfaceBroker().GetInterface(
        host_.BindNewPipeAndPassReceiver());
  }
  return host_.get();
}

void MediaStreamVideoCapturerSource::SetMediaStreamDispatcherHostForTesting(
    mojo::PendingRemote<mojom::blink::MediaStreamDispatcherHost> host) {
  host_.Bind(std::move(host));
}

VideoCapturerSource* MediaStreamVideoCapturerSource::GetSourceForTesting() {
  return source_.get();
}

}  // namespace blink
