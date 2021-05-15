// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
#define CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/cdm_capability.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT KeySystemSupportImpl final
    : public media::mojom::KeySystemSupport {
 public:
  KeySystemSupportImpl();
  KeySystemSupportImpl(const KeySystemSupportImpl&) = delete;
  KeySystemSupportImpl& operator=(const KeySystemSupportImpl&) = delete;
  ~KeySystemSupportImpl() final;

  // Create a KeySystemSupportImpl object and bind it to `receiver`.
  static void Create(
      mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver);

  // media::mojom::KeySystemSupport implementation.
  void IsKeySystemSupported(const std::string& key_system,
                            IsKeySystemSupportedCallback callback) final;

 private:
  friend class KeySystemSupportImplTest;

  using CdmCapabilityCB =
      base::OnceCallback<void(absl::optional<media::CdmCapability>)>;
  using HardwareSecureCapabilityCB =
      base::RepeatingCallback<void(const std::string&, CdmCapabilityCB)>;

  // Sets a callback to query for hardware secure capability for testing.
  void SetHardwareSecureCapabilityCBForTesting(HardwareSecureCapabilityCB cb);

  void LazyInitializeHardwareSecureCapability(
      const std::string& key_system,
      CdmCapabilityCB cdm_capability_cb);

  void OnHardwareSecureCapability(
      const std::string& key_system,
      IsKeySystemSupportedCallback callback,
      bool lazy_initialize,
      absl::optional<media::CdmCapability> hw_secure_capability);

  HardwareSecureCapabilityCB hw_secure_capability_cb_for_testing_;

  base::WeakPtrFactory<KeySystemSupportImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
