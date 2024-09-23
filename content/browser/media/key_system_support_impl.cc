// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include "base/logging.h"
#include "content/browser/permissions/permission_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

namespace {

// All key systems must have either software or hardware secure capability
// supported.
bool IsValidKeySystemCapabilities(KeySystemCapabilities capabilities) {
  for (const auto& entry : capabilities) {
    auto& capability = entry.second;
    if (!capability.sw_secure_capability.has_value() &&
        !capability.hw_secure_capability.has_value()) {
      return false;
    }
  }

  return true;
}

}  // namespace

KeySystemSupportImpl::KeySystemSupportImpl(RenderFrameHost* render_frame_host)
    : DocumentUserData(render_frame_host) {}

KeySystemSupportImpl::~KeySystemSupportImpl() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_FUCHSIA)
  render_frame_host()
      .GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(permission_subscription_id_);
#endif
}

void KeySystemSupportImpl::SetGetKeySystemCapabilitiesUpdateCbForTesting(
    GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing) {
  get_support_cb_for_testing_ = std::move(get_support_cb_for_testing);
}

void KeySystemSupportImpl::Bind(
    mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver) {
  key_system_support_receivers_.Add(this, std::move(receiver));
}

void KeySystemSupportImpl::AddObserver(
    mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer) {
  DVLOG(3) << __func__;

  auto id = observer_remotes_.Add(std::move(observer));

  // If `key_system_support_` is already available, notify the new observer
  // immediately. All observers will be notified if there are updates later.
  if (key_system_capabilities_.has_value()) {
    observer_remotes_.Get(id)->OnKeySystemSupportUpdated(
        key_system_capabilities_.value());
    return;
  }

  if (!cb_subscription_) {
    ObserveKeySystemCapabilities();
  }
}

// Initializes permissions values from the
// about://settings/content/protectedContent page. Namely, the default behaviour
// of protected content IDs, and the site specific settings.
void KeySystemSupportImpl::InitializePermissions() {
  DCHECK(!are_permissions_initialized_);

  // Setup initial permission values.
  auto* web_contents = WebContentsImpl::FromRenderFrameHostImpl(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
  is_protected_content_allowed_ =
      web_contents->GetRendererPrefs().enable_encrypted_media;

// Initialize permissions for platforms that supports
// PROTECTED_MEDIA_IDENTIFIER.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_FUCHSIA)
  render_frame_host()
      .GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          &render_frame_host(),
          PermissionRequestDescription(
              blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
              render_frame_host().HasTransientUserActivation()),
          base::BindOnce(&KeySystemSupportImpl::
                             OnProtectedMediaIdentifierPermissionInitialized,
                         weak_ptr_factory_.GetWeakPtr()));
#else
  are_permissions_initialized_ = true;
  SetUpPermissionListeners();
  ObserveKeySystemCapabilities();
#endif
}

void KeySystemSupportImpl::SetUpPermissionListeners() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_FUCHSIA)
  // Setup permission listeners.
  permission_subscription_id_ =
      render_frame_host()
          .GetBrowserContext()
          ->GetPermissionController()
          ->SubscribeToPermissionStatusChange(
              blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER,
              /*render_process_host=*/nullptr, &render_frame_host(),
              PermissionUtil::GetLastCommittedOriginAsURL(&render_frame_host()),
              /*should_include_device_status=*/false,
              base::BindRepeating(
                  &KeySystemSupportImpl::
                      OnProtectedMediaIdentifierPermissionUpdated,
                  weak_ptr_factory_.GetWeakPtr()));

  if (permission_subscription_id_.is_null()) {
    LOG(ERROR) << "Could not subscribe to permissions changes for "
                  "PROTECTED_MEDIA_IDENTIFIER";
    // Since we cannot observe changes to PROTECTED_MEDIA_IDENTIFIER, revert
    // back to its default value.
    is_protected_identifier_allowed_ = false;
  }
#endif

  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      render_frame_host().GetBrowserContext(),
      preference_watcher_receiver_.BindNewPipeAndPassRemote());
}

void KeySystemSupportImpl::OnProtectedMediaIdentifierPermissionInitialized(
    blink::mojom::PermissionStatus status) {
  DCHECK(!are_permissions_initialized_);

  are_permissions_initialized_ = true;
  is_protected_identifier_allowed_ =
      status == blink::mojom::PermissionStatus::GRANTED;

  SetUpPermissionListeners();
  ObserveKeySystemCapabilities();
}

void KeySystemSupportImpl::OnProtectedMediaIdentifierPermissionUpdated(
    blink::mojom::PermissionStatus status) {
  const bool is_protected_identifier_allowed =
      status == blink::mojom::PermissionStatus::GRANTED;

  if (is_protected_identifier_allowed == is_protected_identifier_allowed_) {
    return;
  }

  is_protected_identifier_allowed_ = is_protected_identifier_allowed;
  ObserveKeySystemCapabilities();
}

void KeySystemSupportImpl::NotifyUpdate(
    const blink::RendererPreferences& new_prefs) {
  if (is_protected_content_allowed_ == new_prefs.enable_encrypted_media) {
    return;
  }

  is_protected_content_allowed_ = new_prefs.enable_encrypted_media;
  ObserveKeySystemCapabilities();
}

bool KeySystemSupportImpl::allow_hw_secure_capability_check() const {
#if BUILDFLAG(IS_WIN)
  return is_protected_content_allowed_ && is_protected_identifier_allowed_;
#else
  return true;
#endif
}

void KeySystemSupportImpl::ObserveKeySystemCapabilities() {
  if (!are_permissions_initialized_) {
    // Initialize permissions. Also, we'll continue to observe key system
    // capabilities when permissions are initialized.
    InitializePermissions();
    return;
  }

  auto result_cb =
      base::BindRepeating(&KeySystemSupportImpl::OnKeySystemCapabilitiesUpdated,
                          weak_ptr_factory_.GetWeakPtr());

  if (get_support_cb_for_testing_) {
    get_support_cb_for_testing_.Run(allow_hw_secure_capability_check(),
                                    std::move(result_cb));
    return;
  }

  cb_subscription_ =
      CdmRegistryImpl::GetInstance()->ObserveKeySystemCapabilities(
          allow_hw_secure_capability_check(), std::move(result_cb));
}

void KeySystemSupportImpl::OnKeySystemCapabilitiesUpdated(
    KeySystemCapabilities key_system_capabilities) {
  DVLOG(3) << __func__;
  DCHECK(IsValidKeySystemCapabilities(key_system_capabilities));

  if (key_system_capabilities_.has_value() &&
      key_system_capabilities_.value() == key_system_capabilities) {
    DVLOG(1) << __func__ << ": Updated with the same key system capabilities";
    return;
  }

  // TODO(b/345822323): Filter out non permitted key systems.
  key_system_capabilities_ = std::move(key_system_capabilities);

  for (auto& observer : observer_remotes_)
    observer->OnKeySystemSupportUpdated(key_system_capabilities_.value());
}

DOCUMENT_USER_DATA_KEY_IMPL(KeySystemSupportImpl);

}  // namespace content
