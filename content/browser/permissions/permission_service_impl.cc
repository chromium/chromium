// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/permissions/features.h"
#include "content/browser/bad_message.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-shared.h"
#include "url/origin.h"

using blink::mojom::EmbeddedPermissionControlResult;
using blink::mojom::EmbeddedPermissionRequestDescriptorPtr;
using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionStatus;

namespace content {

namespace {

// This function allows the usage of the the multiple request map with single
// requests.
void PermissionRequestResponseCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  std::move(callback).Run(vector[0]);
}

// Helper converts given `PermissionStatus` to `EmbeddedPermissionControlResult`
EmbeddedPermissionControlResult
PermissionStatusToEmbeddedPermissionControlResult(PermissionStatus status) {
  switch (status) {
    case PermissionStatus::GRANTED:
      return EmbeddedPermissionControlResult::kGranted;
    case PermissionStatus::DENIED:
      return EmbeddedPermissionControlResult::kDenied;
    case PermissionStatus::ASK:
      return EmbeddedPermissionControlResult::kDismissed;
  }

  NOTREACHED();
  return EmbeddedPermissionControlResult::kNotSupported;
}

// Helper wraps `RequestPageEmbeddedPermissionCallback` to
// `RequestPermissionsCallback`.
void EmbeddedPermissionRequestCallbackWrapper(
    base::OnceCallback<void(EmbeddedPermissionControlResult)> callback,
    const std::vector<PermissionStatus>& statuses) {
  DCHECK(base::ranges::all_of(
      statuses, [&](auto const& status) { return statuses[0] == status; }));
  std::move(callback).Run(
      PermissionStatusToEmbeddedPermissionControlResult(statuses[0]));
}

// Helper converts the given vector `PermissionDescriptorPtr` to vector
// `PermissionType`. Checks and returns empty vector if there's duplicate
// permission in the input vector.
std::vector<blink::PermissionType> GetPermissionTypesAndCheckDuplicates(
    const std::vector<PermissionDescriptorPtr>& permissions) {
  std::vector<blink::PermissionType> types(permissions.size());
  std::set<blink::PermissionType> duplicates_check;
  for (size_t i = 0; i < types.size(); ++i) {
    auto type = blink::PermissionDescriptorToPermissionType(permissions[i]);
    if (!type) {
      return std::vector<blink::PermissionType>();
    }

    types[i] = *type;

    bool inserted = duplicates_check.insert(types[i]).second;
    if (!inserted) {
      return std::vector<blink::PermissionType>();
    }
  }

  return types;
}

}  // anonymous namespace

class PermissionServiceImpl::PendingRequest {
 public:
  PendingRequest(std::vector<blink::PermissionType> types,
                 RequestPermissionsCallback callback)
      : callback_(std::move(callback)), request_size_(types.size()) {}

  ~PendingRequest() {
    if (callback_.is_null())
      return;

    std::move(callback_).Run(
        std::vector<PermissionStatus>(request_size_, PermissionStatus::DENIED));
  }

  void RunCallback(const std::vector<PermissionStatus>& results) {
    std::move(callback_).Run(results);
  }

 private:
  RequestPermissionsCallback callback_;
  size_t request_size_;
};

PermissionServiceImpl::PermissionServiceImpl(PermissionServiceContext* context,
                                             const url::Origin& origin)
    : context_(context), origin_(origin) {}

PermissionServiceImpl::~PermissionServiceImpl() {}

void PermissionServiceImpl::RequestPageEmbeddedPermission(
    EmbeddedPermissionRequestDescriptorPtr descriptor,
    RequestPageEmbeddedPermissionCallback callback) {
  if (!base::FeatureList::IsEnabled(
          permissions::features::kPermissionElement)) {
    bad_message::ReceivedBadMessage(
        context_->render_frame_host()->GetProcess(),
        bad_message::
            PERMISSION_SERVICE_REQUEST_EMBEDDED_PERMISSION_WITHOUT_FEATURE);
    std::move(callback).Run(EmbeddedPermissionControlResult::kNotSupported);
    return;
  }

  if (auto* browser_context = context_->GetBrowserContext()) {
    std::vector<blink::PermissionType> permission_types =
        GetPermissionTypesAndCheckDuplicates(descriptor->permissions);
    if (permission_types.empty()) {
      ReceivedBadMessage();
      return;
    }

    RequestPermissionsInternal(
        browser_context, descriptor->permissions,
        PermissionRequestDescription(
            permission_types, /*user_gesture=*/true,
            /*requesting_origin=*/GURL(),
            /*embedded_permission_element_initiated=*/true),
        base::BindOnce(&EmbeddedPermissionRequestCallbackWrapper,
                       std::move(callback)));
  }
}

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
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  // This condition is valid if the call is coming from a ChildThread instead
  // of a RenderFrame. Some consumers of the service run in Workers and some
  // in Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to
  // show any UI, we want to still return something relevant so the current
  // permission status is returned for each permission.
  if (!context_->render_frame_host()) {
    std::vector<PermissionStatus> result(permissions.size());
    for (size_t i = 0; i < permissions.size(); ++i) {
      result[i] = GetPermissionStatus(permissions[i]);
    }
    std::move(callback).Run(result);
    return;
  }

  std::vector<blink::PermissionType> permission_types =
      GetPermissionTypesAndCheckDuplicates(permissions);
  if (permission_types.empty()) {
    ReceivedBadMessage();
    return;
  }

  RequestPermissionsInternal(
      browser_context, permissions,
      PermissionRequestDescription(permission_types, user_gesture),
      std::move(callback));
}

void PermissionServiceImpl::RequestPermissionsInternal(
    BrowserContext* browser_context,
    const std::vector<PermissionDescriptorPtr>& permissions,
    PermissionRequestDescription request_description,
    RequestPermissionsCallback callback) {
  std::unique_ptr<PendingRequest> pending_request =
      std::make_unique<PendingRequest>(request_description.permissions,
                                       std::move(callback));

  int pending_request_id = pending_requests_.Add(std::move(pending_request));

  if (!permissions.empty() &&
      PermissionUtil::IsDomainOverride(permissions[0])) {
    if (!PermissionUtil::ValidateDomainOverride(request_description.permissions,
                                                context_->render_frame_host(),
                                                permissions[0])) {
      ReceivedBadMessage();
      return;
    }
    const url::Origin& requesting_origin =
        PermissionUtil::ExtractDomainOverride(permissions[0]);
    request_description.requesting_origin = requesting_origin.GetURL();
    PermissionControllerImpl::FromBrowserContext(browser_context)
        ->RequestPermissions(
            context_->render_frame_host(), request_description,
            base::BindOnce(&PermissionServiceImpl::OnRequestPermissionsResponse,
                           weak_factory_.GetWeakPtr(), pending_request_id));
  } else {
    PermissionControllerImpl::FromBrowserContext(browser_context)
        ->RequestPermissionsFromCurrentDocument(
            context_->render_frame_host(), std::move(request_description),
            base::BindOnce(&PermissionServiceImpl::OnRequestPermissionsResponse,
                           weak_factory_.GetWeakPtr(), pending_request_id));
  }
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
  auto permission_type =
      blink::PermissionDescriptorToPermissionType(permission);
  if (!permission_type) {
    ReceivedBadMessage();
    return;
  }
  PermissionStatus status = GetPermissionStatusFromType(*permission_type);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (status != PermissionStatus::GRANTED) {
    std::move(callback).Run(status);
    return;
  }

  ResetPermissionStatus(*permission_type);

  std::move(callback).Run(GetPermissionStatusFromType(*permission_type));
}

void PermissionServiceImpl::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    PermissionStatus last_known_status,
    mojo::PendingRemote<blink::mojom::PermissionObserver> observer) {
  auto type = blink::PermissionDescriptorToPermissionType(permission);
  if (!type) {
    ReceivedBadMessage();
    return;
  }

  context_->CreateSubscription(*type, origin_, GetPermissionStatus(permission),
                               last_known_status, std::move(observer));
}

void PermissionServiceImpl::NotifyEventListener(
    blink::mojom::PermissionDescriptorPtr permission,
    const std::string& event_type,
    bool is_added) {
  auto type = blink::PermissionDescriptorToPermissionType(permission);
  if (!type) {
    ReceivedBadMessage();
    return;
  }

  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  if (!context_->render_frame_host()) {
    return;
  }

  if (event_type == "change") {
    if (is_added) {
      context_->GetOnchangeEventListeners().insert(*type);
    } else {
      context_->GetOnchangeEventListeners().erase(*type);
    }
  }

  PermissionControllerImpl::FromBrowserContext(browser_context)
      ->NotifyEventListener();
}

PermissionStatus PermissionServiceImpl::GetPermissionStatus(
    const PermissionDescriptorPtr& permission) {
  auto type = blink::PermissionDescriptorToPermissionType(permission);
  if (!type) {
    ReceivedBadMessage();
    return PermissionStatus::DENIED;
  }
  if (PermissionUtil::IsDomainOverride(permission) &&
      context_->render_frame_host()) {
    BrowserContext* browser_context = context_->GetBrowserContext();
    if (browser_context &&
        PermissionUtil::ValidateDomainOverride(
            {type.value()}, context_->render_frame_host(), permission)) {
      return PermissionControllerImpl::FromBrowserContext(browser_context)
          ->GetPermissionStatusForEmbeddedRequester(
              *type, context_->render_frame_host(),
              PermissionUtil::ExtractDomainOverride(permission));
    }
  }
  return GetPermissionStatusFromType(*type);
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromType(
    blink::PermissionType type) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return PermissionStatus::DENIED;
  }

  if (context_->render_frame_host()) {
    return browser_context->GetPermissionController()
        ->GetPermissionStatusForCurrentDocument(type,
                                                context_->render_frame_host());
  }

  if (context_->render_process_host()) {
    return browser_context->GetPermissionController()
        ->GetPermissionStatusForWorker(type, context_->render_process_host(),
                                       origin_);
  }

  DCHECK(context_->GetEmbeddingOrigin().is_empty());
  return browser_context->GetPermissionController()
      ->GetPermissionResultForOriginWithoutContext(type, origin_)
      .status;
}

void PermissionServiceImpl::ResetPermissionStatus(blink::PermissionType type) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return;
  }

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
