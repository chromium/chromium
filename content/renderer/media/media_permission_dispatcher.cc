// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_permission_dispatcher.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/functional/callback_forward.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

using Type = media::MediaPermission::Type;

blink::mojom::PermissionDescriptorPtr MediaPermissionTypeToPermissionDescriptor(
    Type type) {
  auto descriptor = blink::mojom::PermissionDescriptor::New();
  switch (type) {
    case Type::kProtectedMediaIdentifier:
      descriptor->name =
          blink::mojom::PermissionName::PROTECTED_MEDIA_IDENTIFIER;
      break;
    case Type::kAudioCapture:
      descriptor->name = blink::mojom::PermissionName::AUDIO_CAPTURE;
      break;
    case Type::kVideoCapture:
      descriptor->name = blink::mojom::PermissionName::VIDEO_CAPTURE;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << base::to_underlying(type);
      descriptor->name =
          blink::mojom::PermissionName::PROTECTED_MEDIA_IDENTIFIER;
  }
  return descriptor;
}

}  // namespace

namespace content {

MediaPermissionDispatcher::MediaPermissionDispatcher(
    RenderFrameImpl* render_frame)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      next_request_id_(0),
      render_frame_(render_frame) {
  DCHECK(render_frame_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

MediaPermissionDispatcher::~MediaPermissionDispatcher() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Clean up pending requests.
  OnPermissionServiceConnectionError();
}

void MediaPermissionDispatcher::OnNavigation() {
  // Behave as if there were a connection error. The browser process will be
  // closing the connection imminently.
  OnPermissionServiceConnectionError();
}

void MediaPermissionDispatcher::HasPermission(
    Type type,
    PermissionStatusCB permission_status_cb) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaPermissionDispatcher::HasPermission,
                                  weak_ptr_, type,
                                  base::BindPostTaskToCurrentDefault(
                                      std::move(permission_status_cb))));
    return;
  }

  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  int request_id = RegisterCallback(std::move(permission_status_cb));
  DVLOG(2) << __func__ << ": request ID " << request_id;

  GetPermissionService()->HasPermission(
      MediaPermissionTypeToPermissionDescriptor(type),
      base::BindOnce(&MediaPermissionDispatcher::OnPermissionStatus, weak_ptr_,
                     request_id));
}

void MediaPermissionDispatcher::RequestPermission(
    Type type,
    PermissionStatusCB permission_status_cb) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaPermissionDispatcher::RequestPermission,
                                  weak_ptr_, type,
                                  base::BindPostTaskToCurrentDefault(
                                      std::move(permission_status_cb))));
    return;
  }

  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  int request_id = RegisterCallback(std::move(permission_status_cb));
  DVLOG(2) << __func__ << ": request ID " << request_id;

  GetPermissionService()->RequestPermission(
      MediaPermissionTypeToPermissionDescriptor(type),
      render_frame_->GetWebFrame()->HasTransientUserActivation(),
      base::BindOnce(&MediaPermissionDispatcher::OnPermissionStatus, weak_ptr_,
                     request_id));
}

bool MediaPermissionDispatcher::IsEncryptedMediaEnabled() {
  return render_frame_->GetRendererPreferences().enable_encrypted_media;
}

uint32_t MediaPermissionDispatcher::RegisterCallback(
    PermissionStatusCB permission_status_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  uint32_t request_id = next_request_id_++;
  DCHECK(!requests_.count(request_id));
  requests_[request_id] = std::move(permission_status_cb);

  return request_id;
}

blink::mojom::PermissionService*
MediaPermissionDispatcher::GetPermissionService() {
  if (!permission_service_) {
    render_frame_->GetBrowserInterfaceBroker().GetInterface(
        permission_service_.BindNewPipeAndPassReceiver());
    permission_service_.set_disconnect_handler(base::BindOnce(
        &MediaPermissionDispatcher::OnPermissionServiceConnectionError,
        base::Unretained(this)));
  }

  return permission_service_.get();
}

void MediaPermissionDispatcher::OnPermissionStatus(
    uint32_t request_id,
    blink::mojom::PermissionStatus status) {
  DVLOG(2) << __func__ << ": (" << request_id << ", " << status << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto iter = requests_.find(request_id);
  CHECK(iter != requests_.end(), base::NotFatalUntil::M130);

  PermissionStatusCB permission_status_cb = std::move(iter->second);
  requests_.erase(iter);

  std::move(permission_status_cb)
      .Run(status == blink::mojom::PermissionStatus::GRANTED);
}

#if BUILDFLAG(IS_WIN)
void MediaPermissionDispatcher::IsHardwareSecureDecryptionAllowed(
    IsHardwareSecureDecryptionAllowedCB cb) {
  GetMediaFoundationPreferences()->IsHardwareSecureDecryptionAllowed(
      std::move(cb));
}

media::mojom::MediaFoundationPreferences*
MediaPermissionDispatcher::GetMediaFoundationPreferences() {
  if (!mf_preferences_) {
    render_frame_->GetBrowserInterfaceBroker().GetInterface(
        mf_preferences_.BindNewPipeAndPassReceiver());
    mf_preferences_.set_disconnect_handler(base::BindOnce(
        &MediaPermissionDispatcher::OnMediaFoundationPreferencesConnectionError,
        base::Unretained(this)));
  }

  return mf_preferences_.get();
}

void MediaPermissionDispatcher::OnMediaFoundationPreferencesConnectionError() {
  mf_preferences_.reset();
}
#endif  // BUILDFLAG(IS_WIN)

void MediaPermissionDispatcher::OnPermissionServiceConnectionError() {
  permission_service_.reset();

  // Fire all pending callbacks with |false|.
  RequestMap requests;
  requests.swap(requests_);
  for (auto& request : requests)
    std::move(request.second).Run(false);
}

}  // namespace content
