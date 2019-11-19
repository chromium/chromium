// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-shared.h"

using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionStatus;

namespace content {

namespace {

bool PermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor,
    PermissionType* permission_type) {
  switch (descriptor->name) {
    case PermissionName::GEOLOCATION:
      *permission_type = PermissionType::GEOLOCATION;
      return true;
    case PermissionName::NOTIFICATIONS:
      *permission_type = PermissionType::NOTIFICATIONS;
      return true;
    case PermissionName::MIDI: {
      if (descriptor->extension && descriptor->extension->is_midi() &&
          descriptor->extension->get_midi()->sysex) {
        *permission_type = PermissionType::MIDI_SYSEX;
        return true;
      }
      *permission_type = PermissionType::MIDI;
      return true;
    }
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
#if defined(ENABLE_PROTECTED_MEDIA_IDENTIFIER_PERMISSION)
      *permission_type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
      return true;
#else
      NOTIMPLEMENTED();
      return false;
#endif  // defined(ENABLE_PROTECTED_MEDIA_IDENTIFIER_PERMISSION)
    case PermissionName::DURABLE_STORAGE:
      *permission_type = PermissionType::DURABLE_STORAGE;
      return true;
    case PermissionName::AUDIO_CAPTURE:
      *permission_type = PermissionType::AUDIO_CAPTURE;
      return true;
    case PermissionName::VIDEO_CAPTURE:
      *permission_type = PermissionType::VIDEO_CAPTURE;
      return true;
    case PermissionName::BACKGROUND_SYNC:
      *permission_type = PermissionType::BACKGROUND_SYNC;
      return true;
    case PermissionName::SENSORS:
      *permission_type = PermissionType::SENSORS;
      return true;
    case PermissionName::ACCESSIBILITY_EVENTS:
      *permission_type = PermissionType::ACCESSIBILITY_EVENTS;
      return true;
    case PermissionName::CLIPBOARD_READ:
      *permission_type = PermissionType::CLIPBOARD_READ;
      return true;
    case PermissionName::CLIPBOARD_WRITE:
      *permission_type = PermissionType::CLIPBOARD_WRITE;
      return true;
    case PermissionName::PAYMENT_HANDLER:
      *permission_type = PermissionType::PAYMENT_HANDLER;
      return true;
    case PermissionName::BACKGROUND_FETCH:
      *permission_type = PermissionType::BACKGROUND_FETCH;
      return true;
    case PermissionName::IDLE_DETECTION:
      *permission_type = PermissionType::IDLE_DETECTION;
      return true;
    case PermissionName::PERIODIC_BACKGROUND_SYNC:
      *permission_type = PermissionType::PERIODIC_BACKGROUND_SYNC;
      return true;
    case PermissionName::WAKE_LOCK:
      if (descriptor->extension && descriptor->extension->is_wake_lock()) {
        switch (descriptor->extension->get_wake_lock()->type) {
          case blink::mojom::WakeLockType::kScreen:
            *permission_type = PermissionType::WAKE_LOCK_SCREEN;
            break;
          case blink::mojom::WakeLockType::kSystem:
            *permission_type = PermissionType::WAKE_LOCK_SYSTEM;
            break;
          default:
            NOTREACHED();
            return false;
        }
        return true;
      }
      break;
    case PermissionName::NFC:
      *permission_type = PermissionType::NFC;
      return true;
  }

  NOTREACHED();
  return false;
}

// This function allows the usage of the the multiple request map with single
// requests.
void PermissionRequestResponseCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  std::move(callback).Run(vector[0]);
}

}  // anonymous namespace

class PermissionServiceImpl::PendingRequest {
 public:
  PendingRequest(std::vector<PermissionType> types,
                 RequestPermissionsCallback callback)
      : callback_(std::move(callback)), request_size_(types.size()) {}

  ~PendingRequest() {
    if (callback_.is_null())
      return;

    std::move(callback_).Run(
        std::vector<PermissionStatus>(request_size_, PermissionStatus::DENIED));
  }

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  void RunCallback(const std::vector<PermissionStatus>& results) {
    std::move(callback_).Run(results);
  }

 private:
  // Request ID received from the PermissionController.
  int id_;
  RequestPermissionsCallback callback_;
  size_t request_size_;
};

PermissionServiceImpl::PermissionServiceImpl(PermissionServiceContext* context,
                                             const url::Origin& origin)
    : context_(context), origin_(origin) {}

PermissionServiceImpl::~PermissionServiceImpl() {}

void PermissionServiceImpl::RequestPermission(
    PermissionDescriptorPtr permission,
    bool user_gesture,
    PermissionStatusCallback callback) {
  std::vector<PermissionDescriptorPtr> permissions;
  permissions.push_back(std::move(permission));
  RequestPermissions(std::move(permissions), user_gesture,
                     base::BindOnce(&PermissionRequestResponseCallbackWrapper,
                                    std::move(callback)));
}

void PermissionServiceImpl::RequestPermissions(
    std::vector<PermissionDescriptorPtr> permissions,
    bool user_gesture,
    RequestPermissionsCallback callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned for each permission.
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return;

  if (!context_->render_frame_host()) {
    std::vector<PermissionStatus> result(permissions.size());
    for (size_t i = 0; i < permissions.size(); ++i)
      result[i] = GetPermissionStatus(permissions[i]);
    std::move(callback).Run(result);
    return;
  }

  std::vector<PermissionType> types(permissions.size());
  std::set<PermissionType> duplicates_check;
  for (size_t i = 0; i < types.size(); ++i) {
    if (!PermissionDescriptorToPermissionType(permissions[i], &types[i])) {
      ReceivedBadMessage();
      return;
    }
    // Each permission should appear at most once in the message.
    bool inserted = duplicates_check.insert(types[i]).second;
    if (!inserted) {
      ReceivedBadMessage();
      return;
    }
  }

  std::unique_ptr<PendingRequest> pending_request =
      std::make_unique<PendingRequest>(types, std::move(callback));

  int pending_request_id = pending_requests_.Add(std::move(pending_request));
  int id = PermissionControllerImpl::FromBrowserContext(browser_context)
               ->RequestPermissions(
                   types, context_->render_frame_host(), origin_.GetURL(),
                   user_gesture,
                   base::BindOnce(
                       &PermissionServiceImpl::OnRequestPermissionsResponse,
                       weak_factory_.GetWeakPtr(), pending_request_id));

  // Check if the request still exists. It may have been removed by the
  // the response callback.
  PendingRequest* in_progress_request =
      pending_requests_.Lookup(pending_request_id);
  if (!in_progress_request)
    return;
  in_progress_request->set_id(id);
}

void PermissionServiceImpl::OnRequestPermissionsResponse(
    int pending_request_id,
    const std::vector<PermissionStatus>& results) {
  PendingRequest* request = pending_requests_.Lookup(pending_request_id);
  request->RunCallback(results);
  pending_requests_.Remove(pending_request_id);
}

void PermissionServiceImpl::HasPermission(PermissionDescriptorPtr permission,
                                          PermissionStatusCallback callback) {
  std::move(callback).Run(GetPermissionStatus(permission));
}

void PermissionServiceImpl::RevokePermission(
    PermissionDescriptorPtr permission,
    PermissionStatusCallback callback) {
  PermissionType permission_type;
  if (!PermissionDescriptorToPermissionType(permission, &permission_type)) {
    ReceivedBadMessage();
    return;
  }
  PermissionStatus status = GetPermissionStatusFromType(permission_type);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (status != PermissionStatus::GRANTED) {
    std::move(callback).Run(status);
    return;
  }

  ResetPermissionStatus(permission_type);

  std::move(callback).Run(GetPermissionStatusFromType(permission_type));
}

void PermissionServiceImpl::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    PermissionStatus last_known_status,
    mojo::PendingRemote<blink::mojom::PermissionObserver> observer) {
  PermissionType type;
  if (!PermissionDescriptorToPermissionType(permission, &type)) {
    ReceivedBadMessage();
    return;
  }

  context_->CreateSubscription(type, origin_, GetPermissionStatus(permission),
                               last_known_status, std::move(observer));
}

PermissionStatus PermissionServiceImpl::GetPermissionStatus(
    const PermissionDescriptorPtr& permission) {
  PermissionType type;
  if (!PermissionDescriptorToPermissionType(permission, &type)) {
    ReceivedBadMessage();
    return PermissionStatus::DENIED;
  }
  return GetPermissionStatusFromType(type);
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromType(
    PermissionType type) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return PermissionStatus::DENIED;

  GURL requesting_origin(origin_.GetURL());
  if (context_->render_frame_host()) {
    return BrowserContext::GetPermissionController(browser_context)
        ->GetPermissionStatusForFrame(type, context_->render_frame_host(),
                                      requesting_origin);
  }

  DCHECK(context_->GetEmbeddingOrigin().is_empty());
  return BrowserContext::GetPermissionController(browser_context)
      ->GetPermissionStatus(type, requesting_origin, requesting_origin);
}

void PermissionServiceImpl::ResetPermissionStatus(PermissionType type) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return;

  GURL requesting_origin(origin_.GetURL());
  // If the embedding_origin is empty we'll use |origin_| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  PermissionControllerImpl::FromBrowserContext(browser_context)
      ->ResetPermission(
          type, requesting_origin,
          embedding_origin.is_empty() ? requesting_origin : embedding_origin);
}

void PermissionServiceImpl::ReceivedBadMessage() {
  if (context_->render_frame_host()) {
    bad_message::ReceivedBadMessage(
        context_->render_frame_host()->GetProcess(),
        bad_message::PERMISSION_SERVICE_BAD_PERMISSION_DESCRIPTOR);
  } else {
    bad_message::ReceivedBadMessage(
        context_->render_process_host(),
        bad_message::PERMISSION_SERVICE_BAD_PERMISSION_DESCRIPTOR);
  }
}

}  // namespace content
