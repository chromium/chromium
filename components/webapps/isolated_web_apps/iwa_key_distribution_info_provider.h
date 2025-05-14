// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_KEY_DISTRIBUTION_INFO_PROVIDER_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_KEY_DISTRIBUTION_INFO_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/one_shot_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "base/version.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_histograms.h"
#include "components/webapps/isolated_web_apps/proto/key_distribution.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace web_app {

class IwaInternalsHandler;

// This class is a singleton responsible for processing the IWA Key Distribution
// Component data.
class IwaKeyDistributionInfoProvider {
 public:
  struct KeyRotationInfo {
    using PublicKeyData = std::vector<uint8_t>;

    explicit KeyRotationInfo(std::optional<PublicKeyData> public_key);
    ~KeyRotationInfo();
    KeyRotationInfo(const KeyRotationInfo&);

    base::Value AsDebugValue() const;

    std::optional<PublicKeyData> public_key;
  };

  struct SpecialAppPermissionsInfo {
    base::Value AsDebugValue() const;

    bool skip_capture_started_notification;
  };

  using KeyRotations = base::flat_map<std::string, KeyRotationInfo>;
  using ManagedAllowlist = base::flat_set<std::string>;
  using SpecialAppPermissions =
      base::flat_map<std::string, SpecialAppPermissionsInfo>;
  using KeyDistributionData =
      std::tuple<KeyRotations, SpecialAppPermissions, ManagedAllowlist>;

  using QueueOnDemandUpdateCallback = base::RepeatingCallback<bool(
      base::PassKey<IwaKeyDistributionInfoProvider>)>;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnComponentUpdateSuccess(const base::Version& version,
                                          bool is_preloaded) {}
    virtual void OnComponentUpdateError(const base::Version& version,
                                        IwaComponentUpdateError error) {}
  };

  struct ComponentData {
    ComponentData(base::Version version,
                  KeyRotations key_rotations,
                  SpecialAppPermissions special_app_permissions,
                  ManagedAllowlist managed_allowlist,
                  bool is_preloaded);
    ~ComponentData();
    ComponentData(const ComponentData&);

    base::Version version;
    KeyRotations key_rotations;
    SpecialAppPermissions special_app_permissions;
    ManagedAllowlist managed_allowlist;

    bool is_preloaded = false;
  };

  static IwaKeyDistributionInfoProvider* GetInstance();
  static void DestroyInstanceForTesting();

  ~IwaKeyDistributionInfoProvider();

  IwaKeyDistributionInfoProvider(const IwaKeyDistributionInfoProvider&) =
      delete;
  IwaKeyDistributionInfoProvider& operator=(
      const IwaKeyDistributionInfoProvider&) = delete;

  const KeyRotationInfo* GetKeyRotationInfo(
      const std::string& web_bundle_id) const;
  const SpecialAppPermissionsInfo* GetSpecialAppPermissionsInfo(
      const std::string& web_bundle_id) const;
  std::vector<std::string> GetSkipMultiCaptureNotificationBundleIds() const;

  // Only bundles present in the managed allowlist can be installed and updated.
  bool IsManagedInstallPermitted(std::string_view web_bundle_id) const;

  // Sets up the `IwaKeyDistributionInfoProvider`, i.e. adds the capability to
  // schedule on demand callbacks.
  void SetUp(QueueOnDemandUpdateCallback callback);

  // Asynchronously loads new component data and replaces the current `data_`
  // upon success and if `component_version` is greater than the stored one, and
  // informs observers about the operation result.
  void LoadKeyDistributionData(const base::Version& component_version,
                               const base::FilePath& file_path,
                               bool is_preloaded);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets a custom key rotation outside of the component updater flow and
  // triggers an `OnComponentUpdateSuccess()` event. The usage of this function
  // is intentionally limited to chrome://web-app-internals.
  void RotateKeyForDevMode(
      base::PassKey<IwaInternalsHandler>,
      const std::string& web_bundle_id,
      const std::optional<std::vector<uint8_t>>& rotated_key);

  // Dumps the entire component data to web-app-internals.
  base::Value AsDebugValue() const;

  // Writes component metadata (version and whether it's preloaded) to `log`.
  void WriteComponentMetadata(base::Value::Dict& log) const;

  // Use this to post IWA-related tasks if they rely on the key distribution
  // component. The event will be signalled
  //  * Immediately if the kIwaKeyDistributionComponent flag is disabled
  //  * Upon component loading if the available component is downloaded and not
  //    preloaded
  //  * In 15 seconds after the first call to this function if the available
  //  component is preloaded and the component updater is unable to fetch a
  //  newer version.
  base::OneShotEvent& OnMaybeDownloadedComponentDataReady();

  std::optional<bool> IsPreloadedForTesting() const;
  void SetComponentDataForTesting(ComponentData component_data);

 private:
  IwaKeyDistributionInfoProvider();

  // Posts `MaybeQueueComponentUpdate()` onto `any_data_ready_` once.
  void PostMaybeQueueComponentUpdateOnceOnDataReady();

  // Queues a component update request with a fallback.
  // By the time of this call, `any_data_ready_` is already signalled.
  //  * If the request succeeds, will signal `maybe_downloaded_data_ready_` via
  //    OnKeyDistributionDataLoaded();
  //  * If not, will signal `maybe_downloaded_data_ready_` in 15 seconds after
  //    the call. Note that the fallback preloaded version is guaranteed to be
  //    loaded in this case.
  void MaybeQueueComponentUpdate();

  void OnKeyDistributionDataLoaded(
      const base::Version& version,
      bool is_preloaded,
      base::expected<KeyDistributionData, IwaComponentUpdateError>);

  void DispatchComponentUpdateSuccess(const base::Version& version,
                                      bool is_preloaded);

  void DispatchComponentUpdateError(const base::Version& version,
                                    IwaComponentUpdateError error);

  void SignalOnDataReady(bool is_preloaded);

  // Will be signalled once any component version (regardless of whether
  // preloaded or downloaded) is loaded.
  base::OneShotEvent any_data_ready_;

  // Will be signalled either if a downloaded component version is loaded or in
  // 15 seconds after the preloaded version has been loaded. See
  // OnMaybeDownloadedComponentDataReady() for details.
  base::OneShotEvent maybe_downloaded_data_ready_;

  bool maybe_queue_component_update_posted_ = false;

  QueueOnDemandUpdateCallback queue_on_demand_update_;

  std::optional<ComponentData> data_;
  base::ObserverList<Observer> observers_;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_KEY_DISTRIBUTION_INFO_PROVIDER_H_
