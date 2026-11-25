// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
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
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

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

  // Observes key system capabilities updates. `cb` is called with the current
  // capabilities and again whenever they change, always on the original thread
  // this function was called on:
  // - Capabilities that do not need lazy initialization (e.g. software secure
  //   CDMs with known capabilities) are reported immediately, even if their key
  //   system has not been requested.
  // - Capabilities that need lazy initialization are reported with
  //   CdmCapabilityQueryStatus::kPending until their key system is requested
  //   via RequestKeySystemCapabilities() and initialization finishes.
  // If `allow_hw_secure_capability_check` is true, `this` is allowed to check
  // capability for hardware secure key systems.
  //
  // Returns a `base::CallbackListSubscription` which is owned by the caller. If
  // that is destroyed, the `cb` is cancelled.
  base::CallbackListSubscription ObserveKeySystemCapabilities(
      bool allow_hw_secure_capability_check,
      KeySystemCapabilitiesUpdateCB cb);

  // Called when `key_system` is queried, e.g. by a page that calls
  // requestMediaKeySystemAccess(). Starts the lazy initialization of the
  // registered key systems that match `key_system` (which may be a sub key
  // system), for all robustness levels. Observers are notified when the results
  // are ready. If there is no observer yet, the lazy initialization starts when
  // the first one is added. Requesting the same key system again does nothing.
  // `key_system` may come from a renderer, so strings that match no registered
  // CDM are ignored.
  void RequestKeySystemCapabilities(const std::string& key_system);

 private:
  // Make the test a friend class so it could create CdmRegistryImpl directly,
  // to avoid singleton issues.
  friend class CdmRegistryImplTest;

  // Make constructor/destructor private since this is a singleton.
  CdmRegistryImpl();
  ~CdmRegistryImpl() override;

  // Get the capability for `key_system` with robustness `robustness`
  // synchronously. Returns null capability if it is not enabled or still being
  // lazily initialized (in which case Status::kUninitialized is returned).
  std::pair<std::optional<media::CdmCapability>, CdmInfo::Status> GetCapability(
      const std::string& key_system,
      CdmInfo::Robustness robustness);

  // Returns the capability for `key_system` with robustness `robustness`,
  // `kPending` if lazy initialization is allowed and either has not started yet
  // or is in progress, or `kUnknown` if no capability is supported. Also
  // reports UMA once finalized.
  media::CdmCapabilityOrStatus GetCapabilityOrStatus(
      const std::string& key_system,
      CdmInfo::Robustness robustness);

  // Returns whether capability checks are allowed for `robustness`.
  bool IsCapabilityCheckAllowed(CdmInfo::Robustness robustness) const;

  // Attempts to finalize KeySystemCapabilities. May lazy initialize CDM
  // capabilities asynchronously if needed.
  void AttemptToFinalizeKeySystemCapabilities();

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
  // is complete. `cdm_capability_or_status.has_value()` will be false if the
  // key system with specified robustness isn't supported.
  // `cdm_capability_or_status.error()` can be used to inspect the reason when
  // no capability reported.
  void OnCapabilityInitialized(
      const std::string& key_system,
      const CdmInfo::Robustness robustness,
      media::CdmCapabilityOrStatus cdm_capability_or_status);

  // Finalizes the CdmInfo corresponding to `key_system` and `robustness` if its
  // CdmCapability is null (lazy initialization). No-op if the CdmInfo does not
  // exist, or if the CdmInfo's CdmCapability is not null. The CdmInfo will be
  // removed if `cdm_capability_or_status.has_value()` is false, since the CDM
  // does not support any capability. `cdm_capability_or_status.error()` can be
  // used for user-facing information especially when no capability reported.
  void FinalizeCapability(const std::string& key_system,
                          const CdmInfo::Robustness robustness,
                          media::CdmCapabilityOrStatus cdm_capability_or_status,
                          CdmInfo::Status status);

  // Notifies all observers with the current capabilities of all registered key
  // systems, including ones still being lazily initialized. No notification is
  // done if the capabilities have not changed.
  void UpdateAndNotifyKeySystemCapabilities();

  // Returns the set of all registered key systems.
  absl::flat_hash_set<std::string> GetSupportedKeySystems() const;

  // Returns the capabailities for all registered key systems.
  KeySystemCapabilities GetKeySystemCapabilities();

  // Reports whether the software secure CDM is available. Only reported once
  // per browser session per `key_system`, unless the availability changes, e.g.
  // when the CDM is component updated.
  // TODO(crbug.com/573454556): Explore simplifying or replacing this UMA.
  void ReportSoftwareSecureCdmAvailableUMA(const std::string& key_system,
                                           bool available);

  // Reports the status and capabilities of the hardware secure CDM. Only
  // reported once per browser session per `key_system`.
  void ReportHardwareSecureCapabilityStatusUMA(
      const std::string& key_system,
      CdmInfo::Status status,
      const media::CdmCapability* hw_secure_capability);

  // Reports the status of the hardware secure capability query. Only reported
  // once per browser session per `key_system`.
  void ReportHardwareSecureCapabilityQueryStatusUMA(
      const std::string& key_system,
      media::CdmCapabilityQueryStatus capability_query_status);

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

  // Registered key systems requested via RequestKeySystemCapabilities(). Lazy
  // initialization only starts for these. Never cleared: the capabilities are
  // shared by all observers.
  absl::flat_hash_set<std::string> requested_key_systems_;

  // Tracks already-reported UMA state per key system so histograms are not
  // emitted repeatedly across capability updates.
  // TODO(crbug.com/573454556): Simplify the UMA reporting and remove these.
  absl::flat_hash_map<std::string, bool> last_reported_sw_cdm_availability_
      GUARDED_BY_CONTEXT(sequence_checker_);
  absl::flat_hash_set<std::string> reported_hw_status_key_systems_
      GUARDED_BY_CONTEXT(sequence_checker_);
  absl::flat_hash_set<std::string> reported_hw_query_status_key_systems_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback for testing to avoid device dependency.
  CapabilityCB capability_cb_for_testing_;

  // Whether HW secure capability checking is allowed.
  bool allow_hw_secure_capability_check_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CdmRegistryImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
