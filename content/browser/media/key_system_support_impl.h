// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
#define CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// TODO(xhwang): Use StructTraits to convert to KeySystemCapabilities to avoid
// this type.
using KeySystemCapabilityPtrMap =
    base::flat_map<std::string, media::mojom::KeySystemCapabilityPtr>;

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

  // `get_support_cb_for_testing` is used to get support update for testing.
  // If null, we'll use `CdmRegistryImpl` to get the update.
  using GetKeySystemCapabilitiesUpdateCB =
      base::RepeatingCallback<void(KeySystemCapabilitiesUpdateCB)>;
  void SetGetKeySystemCapabilitiesUpdateCbForTesting(
      GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing);

  // Binds the `receiver` to `this`.
  void Bind(mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver);

  // media::mojom::KeySystemSupport implementation.
  void AddObserver(mojo::PendingRemote<media::mojom::KeySystemSupportObserver>
                       observer) final;

 private:
  friend class base::NoDestructor<KeySystemSupportImpl>;
  friend class KeySystemSupportImplTest;

  KeySystemSupportImpl();
  ~KeySystemSupportImpl() final;

  void ObserveKeySystemCapabilities();

  void OnKeySystemCapabilitiesUpdated(
      KeySystemCapabilities key_system_capabilities);

  // `KeySystemSupport` uses `media::mojom::KeySystemCapability` while the mojom
  // interface uses `media::mojom::KeySystemCapabilityPtr`. This function does
  // the conversion.
  KeySystemCapabilityPtrMap CloneKeySystemCapabilities();

  GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing_;
  bool is_observing_ = false;
  mojo::ReceiverSet<KeySystemSupport> key_system_support_receivers_;
  mojo::RemoteSet<media::mojom::KeySystemSupportObserver> observer_remotes_;
  absl::optional<KeySystemCapabilities> key_system_capabilities_;

  base::WeakPtrFactory<KeySystemSupportImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
