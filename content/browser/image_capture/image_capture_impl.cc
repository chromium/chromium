// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/image_capture/image_capture_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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

void SetOptionsOnIOThread(const std::string& source_id,
                          MediaStreamManager* media_stream_manager,
                          media::mojom::PhotoSettingsPtr settings,
                          ImageCaptureImpl::SetOptionsCallback callback) {
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

ImageCaptureImpl::ImageCaptureImpl() {}

ImageCaptureImpl::~ImageCaptureImpl() {}

// static
void ImageCaptureImpl::Create(
    mojo::PendingReceiver<media::mojom::ImageCapture> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ImageCaptureImpl>(),
                              std::move(receiver));
}

void ImageCaptureImpl::GetPhotoState(const std::string& source_id,
                                     GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::GetPhotoState",
                       TRACE_EVENT_SCOPE_PROCESS);

  GetPhotoStateCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          media::BindToCurrentLoop(std::move(callback)),
          mojo::CreateEmptyPhotoState());
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetPhotoStateOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

void ImageCaptureImpl::SetOptions(const std::string& source_id,
                                  media::mojom::PhotoSettingsPtr settings,
                                  SetOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::SetOptions",
                       TRACE_EVENT_SCOPE_PROCESS);

  SetOptionsCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          media::BindToCurrentLoop(std::move(callback)), false);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SetOptionsOnIOThread, source_id,
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
          media::BindToCurrentLoop(std::move(callback)),
          media::mojom::Blob::New());
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&TakePhotoOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

}  // namespace content
