// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
#define CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// A singleton class living in the browser process handling all KeySystemSupport
// requests.
class CONTENT_EXPORT KeySystemSupportImpl final
    : public media::mojom::KeySystemSupport {
 public:
  static KeySystemSupportImpl* GetInstance();

  static void BindReceiver(
      mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver);

  KeySystemSupportImpl(const KeySystemSupportImpl&) = delete;
  KeySystemSupportImpl& operator=(const KeySystemSupportImpl&) = delete;

  // Binds the `receiver` to `this`.
  void Bind(mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver);

  // media::mojom::KeySystemSupport implementation.
  void IsKeySystemSupported(const std::string& key_system,
                            IsKeySystemSupportedCallback callback) final;

 private:
  friend class base::NoDestructor<KeySystemSupportImpl>;
  friend class KeySystemSupportImplTest;

  using GetKeySystemCapabilitiesUpdateCB =
      base::RepeatingCallback<void(KeySystemCapabilitiesUpdateCB)>;

  // `get_support_cb_for_testing` is used to get support update for testing.
  // If null, we'll use `CdmRegistryImpl` to get the update.
  explicit KeySystemSupportImpl(
      GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing =
          base::NullCallback());
  ~KeySystemSupportImpl() final;

  void OnKeySystemCapabilitiesUpdated(
      KeySystemCapabilities key_system_capabilities);
  void NotifyIsKeySystemSupportedCallback(
      const std::string& key_system,
      IsKeySystemSupportedCallback callback);

  absl::optional<KeySystemCapabilities> key_system_capabilities_;

  mojo::ReceiverSet<media::mojom::KeySystemSupport>
      key_system_support_receivers_;

  // Key system to IsKeySystemSupportedCallback map.
  using PendingCallbacks =
      std::vector<std::pair<std::string, IsKeySystemSupportedCallback>>;
  PendingCallbacks pending_callbacks_;

  base::WeakPtrFactory<KeySystemSupportImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
