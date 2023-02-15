// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/image_capture/image_capture_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

namespace {

void GetPhotoStateOnIOThread(const std::string& source_id,
                             MediaStreamManager* media_stream_manager,
                             ImageCaptureImpl::GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const base::UnguessableToken session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);
  if (session_id.is_empty())
    return;

  media_stream_manager->video_capture_manager()->GetPhotoState(
      session_id, std::move(callback));
}

void SetPhotoOptionsOnIOThread(
    const std::string& source_id,
    MediaStreamManager* media_stream_manager,
    media::mojom::PhotoSettingsPtr settings,
    ImageCaptureImpl::SetPhotoOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const base::UnguessableToken session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);
  if (session_id.is_empty())
    return;
  media_stream_manager->video_capture_manager()->SetPhotoOptions(
      session_id, std::move(settings), std::move(callback));
}

void TakePhotoOnIOThread(const std::string& source_id,
                         MediaStreamManager* media_stream_manager,
                         ImageCaptureImpl::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "image_capture_impl.cc::TakePhotoOnIOThread",
                       TRACE_EVENT_SCOPE_PROCESS);

  const base::UnguessableToken session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);
  if (session_id.is_empty())
    return;

  media_stream_manager->video_capture_manager()->TakePhoto(session_id,
                                                           std::move(callback));
}

}  // anonymous namespace

// static
void ImageCaptureImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::ImageCapture> receiver) {
  CHECK(render_frame_host);
  // ImageCaptureImpl owns itself. It will self-destruct when a Mojo interface
  // error occurs, the RenderFrameHost is deleted, or the RenderFrameHost
  // navigates to a new document.
  new ImageCaptureImpl(*render_frame_host, std::move(receiver));
}

void ImageCaptureImpl::GetPhotoState(const std::string& source_id,
                                     GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::GetPhotoState",
                       TRACE_EVENT_SCOPE_PROCESS);

  GetPhotoStateCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(
              base::BindOnce(&ImageCaptureImpl::OnGetPhotoState,
                             weak_factory_.GetWeakPtr(), std::move(callback))),
          mojo::CreateEmptyPhotoState());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetPhotoStateOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

void ImageCaptureImpl::SetPhotoOptions(const std::string& source_id,
                                       media::mojom::PhotoSettingsPtr settings,
                                       SetPhotoOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::SetPhotoOptions",
                       TRACE_EVENT_SCOPE_PROCESS);

  if ((settings->has_pan || settings->has_tilt || settings->has_zoom) &&
      !HasPanTiltZoomPermissionGranted()) {
    std::move(callback).Run(false);
    return;
  }

  SetPhotoOptionsCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)), false);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SetPhotoOptionsOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(settings), std::move(scoped_callback)));
}

void ImageCaptureImpl::TakePhoto(const std::string& source_id,
                                 TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::TakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);

  TakePhotoCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)),
          media::mojom::Blob::New());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TakePhotoOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

ImageCaptureImpl::ImageCaptureImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<media::mojom::ImageCapture> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

ImageCaptureImpl::~ImageCaptureImpl() = default;

void ImageCaptureImpl::OnGetPhotoState(GetPhotoStateCallback callback,
                                       media::mojom::PhotoStatePtr state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!HasPanTiltZoomPermissionGranted()) {
    state->pan = media::mojom::Range::New();
    state->tilt = media::mojom::Range::New();
    state->zoom = media::mojom::Range::New();
  }
  std::move(callback).Run(std::move(state));
}

bool ImageCaptureImpl::HasPanTiltZoomPermissionGranted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return MediaDevicesPermissionChecker::
      HasPanTiltZoomPermissionGrantedOnUIThread(
          render_frame_host().GetProcess()->GetID(),
          render_frame_host().GetRoutingID());
}
}  // namespace content
