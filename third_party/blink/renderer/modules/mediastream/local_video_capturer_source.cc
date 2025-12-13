// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/local_video_capturer_source.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

LocalVideoCapturerSource::LocalVideoCapturerSource(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    LocalFrame* frame,
    const base::UnguessableToken& session_id)
    : session_id_(session_id),
      manager_(Platform::Current()->GetVideoCaptureImplManager()),
      frame_token_(frame->GetLocalFrameToken()),
      release_device_cb_(
          manager_->UseDevice(session_id_, frame->GetBrowserInterfaceBroker())),
      task_runner_(std::move(task_runner)) {}

LocalVideoCapturerSource::~LocalVideoCapturerSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(release_device_cb_).Run();
}

media::VideoCaptureFormats LocalVideoCapturerSource::GetPreferredFormats() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return media::VideoCaptureFormats();
}

void LocalVideoCapturerSource::StartCapture(
    const media::VideoCaptureParams& params,
    VideoCaptureCallbacks video_capture_callbacks,
    VideoCaptureRunningCallbackCB running_callback) {
  DCHECK(params.requested_format.IsValid());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  running_callback_ = std::move(running_callback);

  // Combine all callbacks into MediaStreamVideoSourceCallbacks structure
  VideoCaptureCallbacks new_video_capture_callbacks;
  new_video_capture_callbacks.state_update_cb = base::BindPostTask(
      task_runner_, ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                        &LocalVideoCapturerSource::OnStateUpdate,
                        weak_factory_.GetWeakPtr())));
  new_video_capture_callbacks.deliver_frame_cb =
      std::move(video_capture_callbacks.deliver_frame_cb);
  new_video_capture_callbacks.capture_version_cb =
      std::move(video_capture_callbacks.capture_version_cb);
  new_video_capture_callbacks.frame_dropped_cb =
      std::move(video_capture_callbacks.frame_dropped_cb);
  stop_capture_cb_ = manager_->StartCapture(
      session_id_, params, std::move(new_video_capture_callbacks));
}

media::VideoCaptureFeedbackCB LocalVideoCapturerSource::GetFeedbackCallback()
    const {
  return manager_->GetFeedbackCallback(session_id_);
}

void LocalVideoCapturerSource::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!stop_capture_cb_)
    return;  // Do not request frames if the source is stopped.
  manager_->RequestRefreshFrame(session_id_);
}

void LocalVideoCapturerSource::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  manager_->Suspend(session_id_);
}

void LocalVideoCapturerSource::Resume() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  manager_->Resume(session_id_);
}

void LocalVideoCapturerSource::StopCapture() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Immediately make sure we don't provide more frames.
  if (stop_capture_cb_)
    std::move(stop_capture_cb_).Run();
}

void LocalVideoCapturerSource::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  manager_->OnLog(session_id_, WebString::FromUTF8(message));
}

void LocalVideoCapturerSource::OnStateUpdate(blink::VideoCaptureState state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (running_callback_.is_null()) {
    OnLog("LocalVideoCapturerSource::OnStateUpdate discarding state update.");
    return;
  }
  VideoCaptureRunState run_state;
  switch (state) {
    case VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED:
      run_state = VideoCaptureRunState::kSystemPermissionsError;
      break;
    case VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY:
      run_state = VideoCaptureRunState::kCameraBusyError;
      break;
    case VIDEO_CAPTURE_STATE_ERROR_START_TIMEOUT:
      run_state = VideoCaptureRunState::kStartTimeoutError;
      break;
    default:
      run_state = VideoCaptureRunState::kStopped;
  }

  auto* frame = LocalFrame::FromFrameToken(frame_token_);
  switch (state) {
    case VIDEO_CAPTURE_STATE_STARTED:
      OnLog(
          "LocalVideoCapturerSource::OnStateUpdate signaling to "
          "consumer that source is now running.");
      running_callback_.Run(VideoCaptureRunState::kRunning);
      break;

    case VIDEO_CAPTURE_STATE_STOPPING:
    case VIDEO_CAPTURE_STATE_STOPPED:
    case VIDEO_CAPTURE_STATE_ERROR:
    case VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED:
    case VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY:
    case VIDEO_CAPTURE_STATE_ENDED:
    case VIDEO_CAPTURE_STATE_ERROR_START_TIMEOUT:
      std::move(release_device_cb_).Run();
      release_device_cb_ =
          frame && frame->Client()
              ? manager_->UseDevice(session_id_,
                                    frame->GetBrowserInterfaceBroker())
              : base::DoNothing();
      OnLog(
          "LocalVideoCapturerSource::OnStateUpdate signaling to "
          "consumer that source is no longer running.");
      running_callback_.Run(run_state);
      break;

    case VIDEO_CAPTURE_STATE_STARTING:
    case VIDEO_CAPTURE_STATE_PAUSED:
    case VIDEO_CAPTURE_STATE_RESUMED:
      // Not applicable to reporting on device starts or errors.
      break;
  }
}

// static
std::unique_ptr<VideoCapturerSource> LocalVideoCapturerSource::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    LocalFrame* frame,
    const base::UnguessableToken& session_id) {
  return std::make_unique<LocalVideoCapturerSource>(std::move(task_runner),
                                                    frame, session_id);
}

}  // namespace blink
