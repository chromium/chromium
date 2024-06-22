// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/cdm_info.h"
#include "media/base/cdm_capability.h"
#include "media/base/key_system_capability.h"
#include "media/mojo/mojom/key_system_support.mojom.h"

namespace content {

// Map from `key_system` string to `KeySystemCapability`.
using KeySystemCapabilities =
    base::flat_map<std::string, media::KeySystemCapability>;
using KeySystemCapabilitiesUpdateCB =
    base::RepeatingCallback<void(KeySystemCapabilities)>;

class CONTENT_EXPORT CdmRegistryImpl : public CdmRegistry,
                                       public GpuDataManagerObserver {
 public:
  // Returns the CdmRegistryImpl singleton.
  static CdmRegistryImpl* GetInstance();

  CdmRegistryImpl(const CdmRegistryImpl&) = delete;
  CdmRegistryImpl& operator=(const CdmRegistryImpl&) = delete;

  // CdmRegistry implementation.
  void Init() override;
  void RegisterCdm(const CdmInfo& info) override;
  void SetHardwareSecureCdmStatus(CdmInfo::Status status) override;

  // GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() override;

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
  // thread this function was called on. If `allow_hw_secure_capability_check`
  // is true, then `this` is allowed to check capability for hardware secure key
  // systems.
  //
  // Returns a `base::CallbackListSubscription` which is owned by the caller. If
  // that is destroyed, the `cb` is cancelled.
  base::CallbackListSubscription ObserveKeySystemCapabilities(
      bool allow_hw_secure_capability_check,
      KeySystemCapabilitiesUpdateCB cb);

 private:
  // Make the test a friend class so it could create CdmRegistryImpl directly,
  // to avoid singleton issues.
  friend class CdmRegistryImplTest;

  // Make constructor/destructor private since this is a singleton.
  CdmRegistryImpl();
  ~CdmRegistryImpl() override;

  // Get the capability for `key_system` with robustness `robustness`
  // synchronously. If lazy initialization is needed, return
  // Status::kUninitialized.
  std::pair<std::optional<media::CdmCapability>, CdmInfo::Status> GetCapability(
      const std::string& key_system,
      CdmInfo::Robustness robustness);

  // Get the capability for `key_system` with robustness `robustness`
  // synchronously. All initialization should have been completed.
  std::pair<std::optional<media::CdmCapability>, CdmInfo::Status>
  GetFinalCapability(const std::string& key_system,
                     CdmInfo::Robustness robustness);

  // Finalizes KeySystemCapabilities. May lazy initialize CDM capabilities
  // asynchronously if needed.
  void FinalizeKeySystemCapabilities();

  // Attempt to finalize KeySystemCapability for `key_system` with robustness
  // `robustness`. May lazy initialize it asynchronously if needed.
  void AttemptToFinalizeKeySystemCapability(const std::string& key_system,
                                            CdmInfo::Robustness robustness);

  // Lazily initialize `key_system` with robustness `robustness`, calling
  // `cdm_capability_cb`. Callback may be called synchronously
  // or asynchronously.
  void LazyInitializeCapability(const std::string& key_system,
                                CdmInfo::Robustness robustness,
                                media::CdmCapabilityCB cdm_capability_cb);

  // Called when initialization of `key_system` with robustness `robustness`
  // is complete. `cdm_capability` will be std::nullopt if the key system
  // with specified robustness isn't supported.
  void OnCapabilityInitialized(
      const std::string& key_system,
      const CdmInfo::Robustness robustness,
      std::optional<media::CdmCapability> cdm_capability);

  // Finalizes the CdmInfo corresponding to `key_system` and `robustness` if its
  // CdmCapability is null (lazy initialization). No-op if the CdmInfo does not
  // exist, or if the CdmInfo's CdmCapability is not null. The CdmInfo will be
  // removed if `cdm_capability` is null, since the CDM does not support any
  // capability.
  void FinalizeCapability(const std::string& key_system,
                          const CdmInfo::Robustness robustness,
                          std::optional<media::CdmCapability> cdm_capability,
                          CdmInfo::Status status);

  // When capabilities for all registered key systems have been determined,
  // notify all observers with the updated values. No notification is done
  // if the capabilities have not changed.
  void UpdateAndNotifyKeySystemCapabilities();

  // Returns the set of all registered key systems.
  std::set<std::string> GetSupportedKeySystems() const;

  // Returns the capabailities for all registered key systems.
  KeySystemCapabilities GetKeySystemCapabilities();

  // Sets callbacks to query for secure capability for testing.
  using CapabilityCB =
      base::RepeatingCallback<void(const std::string&,
                                   const CdmInfo::Robustness robustness,
                                   media::CdmCapabilityCB)>;
  void SetCapabilityCBForTesting(CapabilityCB cb);

  std::vector<CdmInfo> cdms_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Observers for `key_system_capabilities_` updates.
  base::RepeatingCallbackList<KeySystemCapabilitiesUpdateCB::RunType>
      key_system_capabilities_update_callbacks_;

  // Cached current KeySystemCapabilities value.
  std::optional<KeySystemCapabilities> key_system_capabilities_;

  // Key system and robustness pairs pending CdmCapability lazy initialization.
  std::set<std::pair<std::string, CdmInfo::Robustness>>
      pending_lazy_initializations_;

  // Callback for testing to avoid device dependency.
  CapabilityCB capability_cb_for_testing_;

  // Whether HW secure capability checking is allowed.
  bool allow_hw_secure_capability_check_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CdmRegistryImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
