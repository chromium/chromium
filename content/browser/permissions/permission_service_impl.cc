// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/types/zip.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "content/browser/bad_message.h"
#include "content/browser/permissions/embedded_permission_control_checker.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

using blink::mojom::EmbeddedPermissionControlClient;
using blink::mojom::EmbeddedPermissionControlResult;
using blink::mojom::EmbeddedPermissionRequestDescriptorPtr;
using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionStatus;
using blink::mojom::PermissionStatusWithDetailsPtr;

namespace content {

namespace {

bool ValidatePermissionDescriptor(
    const blink::mojom::PermissionDescriptorPtr& descriptor) {
  if (!descriptor->extension) {
    return true;
  }
  switch (descriptor->extension->which()) {
    case blink::mojom::PermissionDescriptorExtension::Tag::kMidi:
      return descriptor->name == PermissionName::MIDI;
    case blink::mojom::PermissionDescriptorExtension::Tag::kClipboard:
      return descriptor->name == PermissionName::CLIPBOARD_READ ||
             descriptor->name == PermissionName::CLIPBOARD_WRITE;
    case blink::mojom::PermissionDescriptorExtension::Tag::kCameraDevice:
      return descriptor->name == PermissionName::VIDEO_CAPTURE;
    case blink::mojom::PermissionDescriptorExtension::Tag::
        kTopLevelStorageAccess:
      return descriptor->name == PermissionName::TOP_LEVEL_STORAGE_ACCESS;
    case blink::mojom::PermissionDescriptorExtension::Tag::kFullscreen:
      return descriptor->name == PermissionName::FULLSCREEN;
  }
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
}

// Helper wraps `RequestPageEmbeddedPermissionCallback` to
// `RequestPermissionsCallback`.
void EmbeddedPermissionRequestCallbackWrapper(
    base::OnceCallback<void(EmbeddedPermissionControlResult)> callback,
    const std::vector<PermissionResult>& results) {
  DCHECK(!results.empty());
  DCHECK(std::ranges::all_of(results, [&](auto const& result) {
    return results[0].status == result.status;
  }));
  std::move(callback).Run(
      PermissionStatusToEmbeddedPermissionControlResult(results[0].status));
}

// Helper which returns true if there are any duplicate or invalid permissions.
bool HasDuplicatesOrInvalidPermissions(
    const std::vector<PermissionDescriptorPtr>& permissions) {
  std::vector<blink::PermissionType> types(permissions.size());
  std::set<blink::PermissionType> duplicates_check;
  for (size_t i = 0; i < types.size(); ++i) {
    auto type =
        blink::MaybePermissionDescriptorToPermissionType(permissions[i]);
    if (!type) {
      return true;
    }

    types[i] = *type;

    bool inserted = duplicates_check.insert(types[i]).second;
    if (!inserted) {
      return true;
    }
  }
  return false;
}

// Helper check if permission types are all supported by Page Embedded
// Permission.
bool CheckPageEmbeddedPermissionTypes(
    const std::vector<PermissionDescriptorPtr>& permissions) {
  for (const auto& permission_type : permissions) {
    auto type = blink::PermissionDescriptorToPermissionType(permission_type);
    if (type != blink::PermissionType::GEOLOCATION &&
        type != blink::PermissionType::WEB_APP_INSTALLATION &&
        type != blink::PermissionType::AUDIO_CAPTURE &&
        type != blink::PermissionType::VIDEO_CAPTURE) {
      return false;
    }
  }
  return true;
}

}  // anonymous namespace

class PermissionServiceImpl::PendingRequest {
 public:
  PendingRequest(
      size_t request_size,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback)
      : callback_(std::move(callback)), request_size_(request_size) {}

  ~PendingRequest() {
    if (callback_.is_null())
      return;

    std::move(callback_).Run(std::vector<PermissionResult>(
        request_size_,
        PermissionResult(PermissionStatus::DENIED,
                         PermissionStatusSource::UNSPECIFIED, std::nullopt)));
  }

  void RunCallback(const std::vector<PermissionResult>& results) {
    std::move(callback_).Run(results);
  }

 private:
  InternalRequestPermissionsCallback callback_;
  size_t request_size_;
};

PermissionServiceImpl::PermissionServiceImpl(PermissionServiceContext* context,
                                             const url::Origin& origin)
    : context_(context), origin_(origin) {}

PermissionServiceImpl::~PermissionServiceImpl() {}

void PermissionServiceImpl::RegisterPageEmbeddedPermissionControl(
    std::vector<PermissionDescriptorPtr> permissions,
    blink::mojom::EmbeddedPermissionRequestDescriptorPtr descriptor,
    mojo::PendingRemote<EmbeddedPermissionControlClient> observer) {
  switch (descriptor->detail->which()) {
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kGeolocation:
      if (!base::FeatureList::IsEnabled(blink::features::kGeolocationElement)) {
        bad_message::ReceivedBadMessage(
            context_->render_frame_host()->GetProcess(),
            bad_message::PSI_REGISTER_PERMISSION_ELEMENT_WITHOUT_FEATURE);
        return;
      }
      break;
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kInstall:
      if (!base::FeatureList::IsEnabled(blink::features::kInstallElement)) {
        bad_message::ReceivedBadMessage(
            context_->render_frame_host()->GetProcess(),
            bad_message::PSI_REGISTER_PERMISSION_ELEMENT_WITHOUT_FEATURE);
        return;
      }
      break;
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kUserMedia:
      if (!base::FeatureList::IsEnabled(blink::features::kUserMediaElement)) {
        bad_message::ReceivedBadMessage(
            context_->render_frame_host()->GetProcess(),
            bad_message::PSI_REGISTER_PERMISSION_ELEMENT_WITHOUT_FEATURE);
        return;
      }
      break;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(context_->render_frame_host());
  CHECK(web_contents);
  auto* checker = EmbeddedPermissionControlChecker::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  std::set<PermissionName> permission_names;
  for (const auto& permission : permissions) {
    if (!ValidatePermissionDescriptor(permission)) {
      ReceivedBadMessage();
      return;
    }
    // Check for duplicates, and ensure we're only handling permission types
    // which can be accessed through embedded controls:
    if (PermissionUtil::IsEmbeddablePermission(permission) &&
        !permission_names.insert(permission->name).second) {
      ReceivedBadMessage();
      return;
    }
  }

  EmbeddedPermissionControlChecker::Source source =
      EmbeddedPermissionControlChecker::Source::kUserMediaElement;
  switch (descriptor->detail->which()) {
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kGeolocation:
      source = EmbeddedPermissionControlChecker::Source::kGeolocationElement;
      break;
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kInstall:
      source = EmbeddedPermissionControlChecker::Source::kInstallElement;
      break;
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kUserMedia:
      source = EmbeddedPermissionControlChecker::Source::kUserMediaElement;
      break;
  }
  checker->CheckPageEmbeddedPermission(
      source, std::move(permission_names), std::move(observer),
      base::BindOnce(
          &PermissionServiceImpl::OnPageEmbeddedPermissionControlRegistered,
          weak_factory_.GetWeakPtr(), std::move(permissions)));
}

void PermissionServiceImpl::OnPageEmbeddedPermissionControlRegistered(
    std::vector<PermissionDescriptorPtr> permissions,
    bool allow,
    const mojo::Remote<EmbeddedPermissionControlClient>& client) {
  if (!allow) {
    client->OnEmbeddedPermissionControlRegistered(
        /*allow=*/false,
        /*statuses=*/std::nullopt);
    return;
  }

  std::vector<PermissionStatus> statuses(permissions.size());
  std::ranges::transform(
      permissions, statuses.begin(), [&](const auto& permission) {
        bool should_include_device_status =
            PermissionUtil::IsDevicePermission(permission);
        return should_include_device_status
                   ? GetCombinedPermissionAndDeviceResult(permission).status
                   : GetPermissionResultForCurrentContext(permission).status;
      });
  client->OnEmbeddedPermissionControlRegistered(/*allow=*/true,
                                                std::move(statuses));
}

void PermissionServiceImpl::RequestPageEmbeddedPermission(
    std::vector<PermissionDescriptorPtr> permissions,
    EmbeddedPermissionRequestDescriptorPtr descriptor,
    RequestPageEmbeddedPermissionCallback callback) {
  if (permissions.empty()) {
    ReceivedBadMessage();
    return;
  }

  if (!std::ranges::all_of(permissions, &ValidatePermissionDescriptor)) {
    ReceivedBadMessage();
    return;
  }
  const base::Feature* required_feature = nullptr;
  switch (descriptor->detail->which()) {
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kGeolocation:
      required_feature = &blink::features::kGeolocationElement;
      break;
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kInstall:
      required_feature = &blink::features::kInstallElement;
      break;
    case blink::mojom::EmbeddedPermissionControlDescriptorExtension::Tag::
        kUserMedia:
      required_feature = &blink::features::kUserMediaElement;
      break;
  }

  if (!base::FeatureList::IsEnabled(*required_feature)) {
    bad_message::ReceivedBadMessage(
        context_->render_frame_host()->GetProcess(),
        bad_message::PSI_REQUEST_EMBEDDED_PERMISSION_WITHOUT_FEATURE);
    return;
  }

  if (auto* browser_context = context_->GetBrowserContext()) {
    if (HasDuplicatesOrInvalidPermissions(permissions) ||
        !CheckPageEmbeddedPermissionTypes(permissions)) {
      ReceivedBadMessage();
      return;
    }

    RequestPermissionsInternal(
        browser_context,
        PermissionRequestDescription(std::move(permissions),
                                     std::move(descriptor)),
        base::BindOnce(&EmbeddedPermissionRequestCallbackWrapper,
                       std::move(callback)));
  }
}

void PermissionServiceImpl::RequestPermission(
    PermissionDescriptorPtr permission,
    RequestPermissionCallback callback) {
  std::vector<PermissionDescriptorPtr> permissions;
  permissions.push_back(std::move(permission));
  RequestPermissions(
      std::move(permissions),
      base::BindOnce(
          [](std::vector<blink::mojom::PermissionStatusWithDetailsPtr>
                 results) {
            CHECK_EQ(results.size(), 1ul);
            return std::move(results[0]);
          })
          .Then(std::move(callback)));
}

void PermissionServiceImpl::RequestPermissions(
    std::vector<PermissionDescriptorPtr> permissions,
    RequestPermissionsCallback callback) {
  if (!std::ranges::all_of(permissions, &ValidatePermissionDescriptor)) {
    ReceivedBadMessage();
    return;
  }
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  if (permissions.empty()) {
    ReceivedBadMessage();
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
    std::move(callback).Run(base::ToVector(
        permissions, [this](const PermissionDescriptorPtr& permission) {
          return PermissionUtil::ToPermissionStatusWithDetails(
              permission->name, GetPermissionResult(permission));
        }));
    return;
  }

  if (HasDuplicatesOrInvalidPermissions(permissions)) {
    ReceivedBadMessage();
    return;
  }

  PermissionRequestDescription permission_request_description(
      mojo::Clone(permissions),
      context_->render_frame_host()->HasTransientUserActivation());
  RequestPermissionsInternal(
      browser_context, std::move(permission_request_description),
      base::BindOnce(
          // TODO(crbug.com/494089503): Simplify this once the migration to
          // PermissionStatusWithDetails is complete.
          [](std::vector<PermissionDescriptorPtr> permissions,
             const std::vector<PermissionResult>& results) {
            std::vector<PermissionStatusWithDetailsPtr> statuses;
            statuses.reserve(results.size());
            CHECK_EQ(permissions.size(), results.size());
            for (auto&& [permission, result] :
                 base::zip(permissions, results)) {
              statuses.push_back(PermissionUtil::ToPermissionStatusWithDetails(
                  permission->name, result));
            }
            return statuses;
          },
          std::move(permissions))
          .Then(std::move(callback)));
}

void PermissionServiceImpl::RequestPermissionsInternal(
    BrowserContext* browser_context,
    PermissionRequestDescription request_description,
    InternalRequestPermissionsCallback callback) {
  const auto& permissions = request_description.permissions;
  if (!permissions.empty() &&
      PermissionUtil::IsDomainOverride(permissions[0])) {
    if (!PermissionUtil::ValidateDomainOverride(request_description.permissions,
                                                context_->render_frame_host(),
                                                permissions[0])) {
      // To prevent crash in the top-level storage access permission request
      // used by rSAFor. See https://crbug.com/332235257 for more details.
      std::move(callback).Run(std::vector<PermissionResult>(
          permissions.size(),
          PermissionResult(PermissionStatus::DENIED,
                           PermissionStatusSource::UNSPECIFIED,
                           CONTENT_SETTING_BLOCK)));
      return;
    }
    const url::Origin& requesting_origin =
        PermissionUtil::ExtractDomainOverride(permissions[0]);
    request_description.requesting_origin = requesting_origin.GetURL();
    int pending_request_id =
        CreatePendingRequest(permissions, std::move(callback));
    PermissionControllerImpl::FromBrowserContext(browser_context)
        ->RequestPermissionsFromCurrentDocument(
            context_->render_frame_host(), std::move(request_description),
            base::BindOnce(&PermissionServiceImpl::OnRequestPermissionsResponse,
                           weak_factory_.GetWeakPtr(), pending_request_id));
  } else {
    int pending_request_id =
        CreatePendingRequest(permissions, std::move(callback));
    PermissionControllerImpl::FromBrowserContext(browser_context)
        ->RequestPermissionsFromCurrentDocument(
            context_->render_frame_host(), std::move(request_description),
            base::BindOnce(&PermissionServiceImpl::OnRequestPermissionsResponse,
                           weak_factory_.GetWeakPtr(), pending_request_id));
  }
}

int PermissionServiceImpl::CreatePendingRequest(
    const std::vector<blink::mojom::PermissionDescriptorPtr>& permissions,
    InternalRequestPermissionsCallback callback) {
  std::unique_ptr<PendingRequest> pending_request =
      std::make_unique<PendingRequest>(permissions.size(), std::move(callback));
  return pending_requests_.Add(std::move(pending_request));
}

void PermissionServiceImpl::OnRequestPermissionsResponse(
    int pending_request_id,
    const std::vector<PermissionResult>& results) {
  PendingRequest* request = pending_requests_.Lookup(pending_request_id);
  request->RunCallback(results);
  pending_requests_.Remove(pending_request_id);
}

void PermissionServiceImpl::HasPermission(PermissionDescriptorPtr permission,
                                          HasPermissionCallback callback) {
  if (!ValidatePermissionDescriptor(permission)) {
    ReceivedBadMessage();
    return;
  }
  std::move(callback).Run(PermissionUtil::ToPermissionStatusWithDetails(
      permission->name, GetPermissionResult(permission)));
}

void PermissionServiceImpl::RevokePermission(
    PermissionDescriptorPtr permission,
    RevokePermissionCallback callback) {
  if (!ValidatePermissionDescriptor(permission)) {
    ReceivedBadMessage();
    return;
  }
  auto permission_type =
      blink::MaybePermissionDescriptorToPermissionType(permission);
  if (!permission_type) {
    ReceivedBadMessage();
    return;
  }
  PermissionResult result = GetPermissionResultForCurrentContext(permission);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (result.status != PermissionStatus::GRANTED) {
    std::move(callback).Run(PermissionUtil::ToPermissionStatusWithDetails(
        permission->name, result));
    return;
  }

  ResetPermissionStatus(*permission_type);

  std::move(callback).Run(PermissionUtil::ToPermissionStatusWithDetails(
      permission->name, GetPermissionResultForCurrentContext(permission)));
}

void PermissionServiceImpl::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    blink::mojom::PermissionStatusWithDetailsPtr last_known_status,
    mojo::PendingRemote<blink::mojom::PermissionObserver> observer) {
  if (!ValidatePermissionDescriptor(permission)) {
    ReceivedBadMessage();
    return;
  }
  auto type = blink::MaybePermissionDescriptorToPermissionType(permission);
  if (!type) {
    ReceivedBadMessage();
    return;
  }

  PermissionResult current_result = GetPermissionResult(permission);
  context_->CreateSubscription(
      permission, origin_, current_result, std::move(last_known_status),
      /*should_include_device_status*/ false, std::move(observer));
}

void PermissionServiceImpl::AddPageEmbeddedPermissionObserver(
    PermissionDescriptorPtr permission,
    PermissionStatus last_known_status,
    mojo::PendingRemote<blink::mojom::PermissionObserver> observer) {
  if (!ValidatePermissionDescriptor(permission)) {
    ReceivedBadMessage();
    return;
  }
  auto type = blink::MaybePermissionDescriptorToPermissionType(permission);
  if (!type) {
    ReceivedBadMessage();
    return;
  }
  bool should_include_device_status =
      PermissionUtil::IsDevicePermission(permission);
  PermissionResult current_result =
      should_include_device_status
          ? GetCombinedPermissionAndDeviceResult(permission)
          : GetPermissionResultForCurrentContext(permission);
  context_->CreateSubscription(permission, origin_, current_result,
                               blink::mojom::PermissionStatusWithDetails::New(
                                   last_known_status, nullptr),
                               should_include_device_status,
                               std::move(observer));
}

void PermissionServiceImpl::NotifyEventListener(
    blink::mojom::PermissionDescriptorPtr permission,
    const std::string& event_type,
    bool is_added) {
  if (!ValidatePermissionDescriptor(permission)) {
    ReceivedBadMessage();
    return;
  }
  auto type = blink::MaybePermissionDescriptorToPermissionType(permission);
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

PermissionResult PermissionServiceImpl::GetPermissionResult(
    const PermissionDescriptorPtr& permission) {
  auto type = blink::MaybePermissionDescriptorToPermissionType(permission);
  if (!type) {
    ReceivedBadMessage();
    return PermissionResult(PermissionStatus::DENIED);
  }
  if (PermissionUtil::IsDomainOverride(permission) &&
      context_->render_frame_host()) {
    BrowserContext* browser_context = context_->GetBrowserContext();

    std::vector<blink::mojom::PermissionDescriptorPtr>
        permisison_descriptor_ptr_array;
    permisison_descriptor_ptr_array.emplace_back(permission->Clone());
    if (browser_context && PermissionUtil::ValidateDomainOverride(
                               permisison_descriptor_ptr_array,
                               context_->render_frame_host(), permission)) {
      return PermissionControllerImpl::FromBrowserContext(browser_context)
          ->GetPermissionResultForEmbeddedRequester(
              permission, context_->render_frame_host(),
              PermissionUtil::ExtractDomainOverride(permission));
    }
  }

  return GetPermissionResultForCurrentContext(permission);
}

PermissionResult PermissionServiceImpl::GetPermissionResultForCurrentContext(
    const PermissionDescriptorPtr& permission) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  if (context_->render_frame_host()) {
    return browser_context->GetPermissionController()
        ->GetPermissionResultForCurrentDocument(permission,
                                                context_->render_frame_host());
  }

  if (context_->render_process_host()) {
    return browser_context->GetPermissionController()
        ->GetPermissionResultForWorker(
            permission, context_->render_process_host(), origin_);
  }

  DCHECK(!context_->GetEmbeddingOrigin().has_value());
  return browser_context->GetPermissionController()
      ->GetPermissionResultForOriginWithoutContext(permission, origin_);
}

PermissionResult PermissionServiceImpl::GetCombinedPermissionAndDeviceResult(
    const PermissionDescriptorPtr& permission) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  RenderFrameHost* render_frame_host = context_->render_frame_host();
  if (!render_frame_host) {
    return PermissionResult(PermissionStatus::DENIED);
  }

  auto type = blink::MaybePermissionDescriptorToPermissionType(permission);
  if (!type) {
    ReceivedBadMessage();
    return PermissionResult(PermissionStatus::DENIED);
  }

  return PermissionResult(
      PermissionControllerImpl::FromBrowserContext(browser_context)
          ->GetCombinedPermissionAndDeviceStatus(permission,
                                                 render_frame_host));
}

void PermissionServiceImpl::ResetPermissionStatus(blink::PermissionType type) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  GURL requesting_origin(origin_.GetURL());
  // If the embedding_origin is empty we'll use |origin_| instead.
  PermissionControllerImpl::FromBrowserContext(browser_context)
      ->ResetPermission(
          type, requesting_origin,
          context_->GetEmbeddingOrigin().value_or(requesting_origin));
}

void PermissionServiceImpl::ReceivedBadMessage() {
  if (context_->render_frame_host()) {
    bad_message::ReceivedBadMessage(context_->render_frame_host()->GetProcess(),
                                    bad_message::PSI_BAD_PERMISSION_DESCRIPTOR);
  } else {
    bad_message::ReceivedBadMessage(context_->render_process_host(),
                                    bad_message::PSI_BAD_PERMISSION_DESCRIPTOR);
  }
}

}  // namespace content
