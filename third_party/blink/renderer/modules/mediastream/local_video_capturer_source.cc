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
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const VideoCaptureSubCaptureTargetVersionCB&
        sub_capture_target_version_callback,
    const VideoCaptureNotifyFrameDroppedCB& frame_dropped_callback,
    const RunningCallback& running_callback) {
  DCHECK(params.requested_format.IsValid());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  running_callback_ = running_callback;

  stop_capture_cb_ = manager_->StartCapture(
      session_id_, params,
      base::BindPostTask(
          task_runner_, ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                            &LocalVideoCapturerSource::OnStateUpdate,
                            weak_factory_.GetWeakPtr()))),
      new_frame_callback, sub_capture_target_version_callback,
      frame_dropped_callback);
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
  RunState run_state;
  switch (state) {
    case VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED:
      run_state = RunState::kSystemPermissionsError;
      break;
    case VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY:
      run_state = RunState::kCameraBusyError;
      break;
    default:
      run_state = RunState::kStopped;
  }

  auto* frame = LocalFrame::FromFrameToken(frame_token_);
  switch (state) {
    case VIDEO_CAPTURE_STATE_STARTED:
      OnLog(
          "LocalVideoCapturerSource::OnStateUpdate signaling to "
          "consumer that source is now running.");
      running_callback_.Run(RunState::kRunning);
      break;

    case VIDEO_CAPTURE_STATE_STOPPING:
    case VIDEO_CAPTURE_STATE_STOPPED:
    case VIDEO_CAPTURE_STATE_ERROR:
    case VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED:
    case VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY:
    case VIDEO_CAPTURE_STATE_ENDED:
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
