// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
#define CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"

namespace content {

// A class living in the browser process per render frame handling all
// KeySystemSupport requests for that frame.
class CONTENT_EXPORT KeySystemSupportImpl final
    : public DocumentUserData<KeySystemSupportImpl>,
      public blink::mojom::RendererPreferenceWatcher,
      public media::mojom::KeySystemSupport {
 public:
  ~KeySystemSupportImpl() final;

  KeySystemSupportImpl(const KeySystemSupportImpl&) = delete;
  KeySystemSupportImpl& operator=(const KeySystemSupportImpl&) = delete;

  // `get_support_cb_for_testing` is used to get support update for testing.
  // If null, we'll use `CdmRegistryImpl` to get the update.
  using GetKeySystemCapabilitiesUpdateCB =
      base::RepeatingCallback<void(bool allow_hw_secure_capability_check,
                                   KeySystemCapabilitiesUpdateCB)>;
  void SetGetKeySystemCapabilitiesUpdateCbForTesting(
      GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing);

  // Binds the `receiver` to `this`.
  void Bind(mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver);

  // media::mojom::KeySystemSupport implementation.
  void AddObserver(mojo::PendingRemote<media::mojom::KeySystemSupportObserver>
                       observer) final;

 private:
  friend class KeySystemSupportImplTest;
  friend class DocumentUserData<KeySystemSupportImpl>;

  explicit KeySystemSupportImpl(RenderFrameHost* rfh);
  DOCUMENT_USER_DATA_KEY_DECL();

  // Initializes permissions values `is_protected_content_allowed_` and
  // `is_protected_identifier_allowed_`.
  void InitializePermissions();

  // Sets up permission listeners for updates.
  void SetUpPermissionListeners();

  // Initializes `is_protected_identifier_allowed_` with `status`.
  void OnProtectedMediaIdentifierPermissionInitialized(
      blink::mojom::PermissionStatus status);

  // Updates `is_protected_identifier_allowed_` with `status`.
  void OnProtectedMediaIdentifierPermissionUpdated(
      blink::mojom::PermissionStatus status);

  // blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(const blink::RendererPreferences& new_prefs) override;

  // Returns whether HW secure capability check is allow based on site settings.
  bool allow_hw_secure_capability_check() const;

  void ObserveKeySystemCapabilities();

  void OnKeySystemCapabilitiesUpdated(
      KeySystemCapabilities key_system_capabilities);

  GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing_;
  mojo::ReceiverSet<KeySystemSupport> key_system_support_receivers_;
  // TODO(crbug.com/348128751): Drop the RemoteSet.
  mojo::RemoteSet<media::mojom::KeySystemSupportObserver> observer_remotes_;
  std::optional<KeySystemCapabilities> key_system_capabilities_;
  // Callback subscription to keep the callback alive in the CdmRegistry.
  base::CallbackListSubscription cb_subscription_;

  bool is_protected_content_allowed_ = false;
  bool is_protected_identifier_allowed_ = false;
  bool are_permissions_initialized_ = false;
  mojo::Receiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver_{this};
  PermissionController::SubscriptionId permission_subscription_id_;

  base::WeakPtrFactory<KeySystemSupportImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
