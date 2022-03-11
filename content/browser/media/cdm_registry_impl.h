// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/cdm_capability.h"
#include "media/mojo/mojom/key_system_support.mojom.h"

namespace content {

// Map from `key_system` string to `KeySystemCapability`.
using KeySystemCapabilities =
    base::flat_map<std::string, media::mojom::KeySystemCapability>;
using KeySystemCapabilitiesUpdateCB =
    base::RepeatingCallback<void(KeySystemCapabilities)>;

class CONTENT_EXPORT CdmRegistryImpl : public CdmRegistry {
 public:
  // Returns the CdmRegistryImpl singleton.
  static CdmRegistryImpl* GetInstance();

  CdmRegistryImpl(const CdmRegistryImpl&) = delete;
  CdmRegistryImpl& operator=(const CdmRegistryImpl&) = delete;

  // CdmRegistry implementation.
  void Init() override;
  void RegisterCdm(const CdmInfo& info) override;

  // Returns all registered CDMs. There might be multiple CdmInfo registered for
  // the same `key_system` and `robustness`. Notes:
  // - Only the first registered one will be used in playback.
  // - The returned CdmInfo's capability might not have been finalized.
  const std::vector<CdmInfo>& GetRegisteredCdms() const;

  // Returns CdmInfo registered for `key_system` and `robustness`. Returns null
  // if no CdmInfo is registered, or if the CdmInfo registered is invalid. There
  // might be multiple CdmInfo registered for the same `key_system` and
  // `robustness`, in which case the first registered one will be returned. The
  // returned CdmInfo's capability might not have been finalized.
  std::unique_ptr<CdmInfo> GetCdmInfo(const std::string& key_system,
                                      CdmInfo::Robustness robustness) const;

  // Observes key system capabilities updates. The updated capabilities are
  // guaranteed to be finalized. The `cb` is always called on the original
  // thread this function was called on.
  void ObserveKeySystemCapabilities(KeySystemCapabilitiesUpdateCB cb);

 private:
  // Make the test a friend class so it could create CdmRegistryImpl directly,
  // to avoid singleton issues.
  friend class CdmRegistryImplTest;

  // Make constructor/destructor private since this is a singleton.
  CdmRegistryImpl();
  ~CdmRegistryImpl() override;

  // Finalizes KeySystemCapabilities. May lazy initialize CDM capabilities
  // asynchronously if needed.
  void FinalizeKeySystemCapabilities();

  using CdmCapabilityCB =
      base::OnceCallback<void(absl::optional<media::CdmCapability>)>;
  void LazyInitializeHardwareSecureCapability(
      const std::string& key_system,
      CdmCapabilityCB cdm_capability_cb);

  void OnHardwareSecureCapabilityInitialized(
      const std::string& key_system,
      absl::optional<media::CdmCapability> cdm_capability);

  // Finalizes the CdmInfo corresponding to `key_system` and `robustness` if its
  // CdmCapability is null (lazy initialization). No-op if the CdmInfo does not
  // exist, or if the CdmInfo's CdmCapability is not null. The CdmInfo will be
  // removed if `cdm_capability` is null, since the CDM does not support any
  // capability. Returns whether the CdmInfo was successfully updated with a
  // valid CdmCapability.
  bool FinalizeHardwareSecureCapability(
      const std::string& key_system,
      absl::optional<media::CdmCapability> cdm_capability);

  void UpdateKeySystemCapabilities();

  std::set<std::string> GetSupportedKeySystems() const;

  KeySystemCapabilities GetKeySystemCapabilities();

  // Sets a callback to query for hardware secure capability for testing.
  using HardwareSecureCapabilityCB =
      base::RepeatingCallback<void(const std::string&, CdmCapabilityCB)>;
  void SetHardwareSecureCapabilityCBForTesting(HardwareSecureCapabilityCB cb);

  std::vector<CdmInfo> cdms_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Observers for `key_system_capabilities_` updates.
  base::RepeatingCallbackList<KeySystemCapabilitiesUpdateCB::RunType>
      key_system_capabilities_update_callbacks_;

  // Cached current KeySystemCapabilities value.
  absl::optional<KeySystemCapabilities> key_system_capabilities_;

  // Key systems pending CdmCapability lazy initialization.
  std::set<std::string> pending_lazy_initialize_key_systems_;

  // A callback for testing to avoid hardware dependency.
  HardwareSecureCapabilityCB hw_secure_capability_cb_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CdmRegistryImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
