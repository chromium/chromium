// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_ref.h"
#include "components/webapps/isolated_web_apps/features.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_histograms.h"
#include "components/webapps/isolated_web_apps/proto/key_distribution.pb.h"

namespace web_app {

namespace {

// The maximum time to wait for downloaded component data after preloaded data
// has loaded. After this duration, readiness is signaled via
// OnMaybeDownloadedComponentDataReady().
constexpr base::TimeDelta kDownloadedComponentDataWaitTime = base::Seconds(15);

bool IsIsolatedWebAppManagedAllowlistEnabled() {
  return base::FeatureList::IsEnabled(
      features::kIsolatedWebAppManagedAllowlist);
}

IwaKeyDistributionInfoProvider::KeyRotations& GetDevModeKeyRotationData() {
  static base::NoDestructor<IwaKeyDistributionInfoProvider::KeyRotations>
      dev_mode_kr_data;
  return *dev_mode_kr_data;
}

bool GetSkipCaptureStartedNotification(
    const IwaSpecialAppPermissions::SpecialAppPermissions& special_permission) {
  if (!special_permission.has_multi_screen_capture()) {
    return false;
  }
  const auto& multi_screen_capture = special_permission.multi_screen_capture();
  if (!multi_screen_capture.has_skip_capture_started_notification()) {
    return false;
  }
  return multi_screen_capture.skip_capture_started_notification();
}
base::expected<IwaKeyDistribution, IwaComponentUpdateError>
LoadKeyDistributionDataFile(const base::FilePath& file_path) {
  std::string key_distribution_data;
  if (!base::ReadFileToString(file_path, &key_distribution_data)) {
    return base::unexpected(IwaComponentUpdateError::kFileNotFound);
  }

  IwaKeyDistribution key_distribution;
  if (!key_distribution.ParseFromString(key_distribution_data)) {
    return base::unexpected(IwaComponentUpdateError::kProtoParsingFailure);
  }
  return std::move(key_distribution);
}

std::unique_ptr<IwaKeyDistributionInfoProvider>&
GetGlobalIwaKeyDistributionInfoProviderInstance() {
  static base::NoDestructor<std::unique_ptr<IwaKeyDistributionInfoProvider>>
      instance;
  return *instance;
}

base::OneShotEvent& AlreadySignalled() {
  static base::NoDestructor<base::OneShotEvent> kEvent;
  if (!kEvent->is_signaled()) {
    kEvent->Signal();
  }
  return *kEvent;
}

base::TaskPriority GetLoadTaskPriority() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::TaskPriority::USER_VISIBLE;
#else
  return base::TaskPriority::BEST_EFFORT;
#endif
}
}  // namespace

base::Value
IwaKeyDistributionInfoProvider::SpecialAppPermissionsInfo::AsDebugValue()
    const {
  return base::Value(base::Value::Dict().Set(
      "skip_capture_started_notification", skip_capture_started_notification));
}

IwaKeyDistributionInfoProvider& IwaKeyDistributionInfoProvider::GetInstance() {
  auto& instance = GetGlobalIwaKeyDistributionInfoProviderInstance();
  if (!instance) {
    instance.reset(new IwaKeyDistributionInfoProvider());
  }
  return *instance.get();
}

void IwaKeyDistributionInfoProvider::DestroyInstanceForTesting() {
  GetGlobalIwaKeyDistributionInfoProviderInstance().reset();
}

const IwaRuntimeDataProvider::KeyRotationInfo*
IwaKeyDistributionInfoProvider::GetKeyRotationInfo(
    const std::string& web_bundle_id) const {
  if (const auto* kr_info =
          base::FindOrNull(GetDevModeKeyRotationData(), web_bundle_id)) {
    return kr_info;
  }

  base::UmaHistogramEnumeration(kIwaKeyRotationInfoSource,
                                GetComponentDataSource());

  return component_
             ? base::FindOrNull(component_->data.key_rotations, web_bundle_id)
             : nullptr;
}

bool IwaKeyDistributionInfoProvider::IsManagedInstallPermitted(
    std::string_view web_bundle_id) const {
  if (skip_managed_checks_for_testing_) {
    CHECK_IS_TEST();
    return true;
  }

  bool is_permitted =
      component_ &&
      base::Contains(component_->data.managed_allowlist, web_bundle_id);

  base::UmaHistogramEnumeration(
      kIwaKeyDistributionManagedInstallCheckInfoSourceHistogramName,
      GetComponentDataSource());
  base::UmaHistogramBoolean(
      kIwaKeyDistributionManagedInstallAllowedHistogramName, is_permitted);

  return IsIsolatedWebAppManagedAllowlistEnabled() ? is_permitted : true;
}

bool IwaKeyDistributionInfoProvider::IsManagedUpdatePermitted(
    std::string_view web_bundle_id) const {
  if (skip_managed_checks_for_testing_) {
    CHECK_IS_TEST();
    return true;
  }

  bool is_permitted =
      component_ &&
      base::Contains(component_->data.managed_allowlist, web_bundle_id);

  base::UmaHistogramEnumeration(
      kIwaKeyDistributionManagedUpdateCheckInfoSourceHistogramName,
      GetComponentDataSource());
  base::UmaHistogramBoolean(
      kIwaKeyDistributionManagedUpdateAllowedHistogramName, is_permitted);

  return IsIsolatedWebAppManagedAllowlistEnabled() ? is_permitted : true;
}

void IwaKeyDistributionInfoProvider::SkipManagedAllowlistChecksForTesting(
    bool skip_managed_checks) {
  CHECK_IS_TEST();
  skip_managed_checks_for_testing_ = skip_managed_checks;
}

void IwaKeyDistributionInfoProvider::SetUp(
    bool is_on_demand_supported,
    QueueOnDemandUpdateCallback callback) {
  is_on_demand_supported_ = is_on_demand_supported;
  queue_on_demand_update_ = callback;
}

void IwaKeyDistributionInfoProvider::LoadKeyDistributionData(
    const base::Version& component_version,
    const base::FilePath& file_path,
    bool is_preloaded) {
  if (component_ && component_->version > component_version) {
    DispatchComponentUpdateError(IwaComponentUpdateError::kStaleVersion);
    return;
  }

  // `base::Unretained(this)` is fine as this is a singleton that never goes
  // away.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), GetLoadTaskPriority()},
      base::BindOnce(&LoadKeyDistributionDataFile, file_path),
      base::BindOnce(
          &IwaKeyDistributionInfoProvider::OnKeyDistributionDataFileLoaded,
          base::Unretained(this), component_version, is_preloaded));
}

const IwaKeyDistributionInfoProvider::SpecialAppPermissionsInfo*
IwaKeyDistributionInfoProvider::GetSpecialAppPermissionsInfo(
    const std::string& web_bundle_id) const {
  if (component_) {
    return base::FindOrNull(component_->data.special_app_permissions,
                            web_bundle_id);
  }
  return nullptr;
}

std::vector<std::string>
IwaKeyDistributionInfoProvider::GetSkipMultiCaptureNotificationBundleIds()
    const {
  if (!component_) {
    return {};
  }

  std::vector<std::string> skip_multi_capture_notification_bundle_ids;
  for (const auto& [bundle_id, special_app_permissions] :
       component_->data.special_app_permissions) {
    if (special_app_permissions.skip_capture_started_notification) {
      skip_multi_capture_notification_bundle_ids.push_back(bundle_id);
    }
  }
  return skip_multi_capture_notification_bundle_ids;
}

std::optional<base::Version> IwaKeyDistributionInfoProvider::GetVersion()
    const {
  if (!component_) {
    return std::nullopt;
  }
  return component_->version;
}

IwaKeyDistributionInfoProvider::IwaKeyDistributionInfoProvider() = default;
IwaKeyDistributionInfoProvider::~IwaKeyDistributionInfoProvider() = default;

void IwaKeyDistributionInfoProvider::OnKeyDistributionDataFileLoaded(
    const base::Version& component_version,
    bool is_preloaded,
    base::expected<IwaKeyDistribution, IwaComponentUpdateError> result) {
  if (component_ && component_->version > component_version) {
    // This might happen if two tasks with different versions have been posted
    // to the task runner in `LoadKeyDistributionData()`.
    DispatchComponentUpdateError(IwaComponentUpdateError::kStaleVersion);
    return;
  }

  ASSIGN_OR_RETURN(auto component_raw_data, std::move(result),
                   [&](IwaComponentUpdateError error) {
                     DispatchComponentUpdateError(error);
                   });
  ASSIGN_OR_RETURN(auto component_data,
                   ParseKeyDistributionData(std::move(component_raw_data)),
                   [&](IwaComponentUpdateError error) {
                     DispatchComponentUpdateError(error);
                   });

  component_ = Component(component_version, is_preloaded, component_data);

  base::UmaHistogramEnumeration(kIwaKeyDistributionComponentUpdateSource,
                                component_->is_preloaded
                                    ? IwaComponentUpdateSource::kPreloaded
                                    : IwaComponentUpdateSource::kDownloaded);
  SignalOnDataReady(is_preloaded);
  DispatchComponentUpdateSuccess(is_preloaded);
}

base::expected<IwaKeyDistributionInfoProvider::Data, IwaComponentUpdateError>
IwaKeyDistributionInfoProvider::ParseKeyDistributionData(
    const IwaKeyDistribution& key_distribution) {
  IwaKeyDistributionInfoProvider::KeyRotations key_rotations;
  if (key_distribution.has_key_rotation_data()) {
    for (const auto& [web_bundle_id, kr_info] :
         key_distribution.key_rotation_data().key_rotations()) {
      if (!kr_info.has_expected_key()) {
        key_rotations.emplace(web_bundle_id,
                              IwaKeyDistributionInfoProvider::KeyRotationInfo(
                                  /*public_key=*/std::nullopt));
        continue;
      }
      std::optional<std::vector<uint8_t>> decoded_public_key =
          base::Base64Decode(kr_info.expected_key());
      if (!decoded_public_key) {
        return base::unexpected(IwaComponentUpdateError::kMalformedBase64Key);
      }
      key_rotations.emplace(web_bundle_id,
                            IwaKeyDistributionInfoProvider::KeyRotationInfo(
                                std::move(decoded_public_key)));
    }
  }

  IwaKeyDistributionInfoProvider::SpecialAppPermissions special_app_permissions;
  if (key_distribution.has_special_app_permissions_data()) {
    for (const auto& [web_bundle_id, special_app_permission_data] :
         key_distribution.special_app_permissions_data()
             .special_app_permissions()) {
      special_app_permissions.emplace(
          web_bundle_id,
          IwaKeyDistributionInfoProvider::SpecialAppPermissionsInfo{
              GetSkipCaptureStartedNotification(special_app_permission_data)});
    }
  }

  IwaKeyDistributionInfoProvider::ManagedAllowlist managed_allowlist;
  if (key_distribution.has_iwa_access_control()) {
    managed_allowlist = base::MakeFlatSet<std::string>(
        key_distribution.iwa_access_control().managed_allowlist(), /*comp=*/{},
        /*proj=*/[](const auto& pair) { return pair.first; });
  }

  return Data(std::move(key_rotations), std::move(special_app_permissions),
              std::move(managed_allowlist));
}

void IwaKeyDistributionInfoProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IwaKeyDistributionInfoProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void IwaKeyDistributionInfoProvider::AddObserver(
    IwaRuntimeDataProvider::Observer* observer) {
  key_provider_observers_.AddObserver(observer);
}

void IwaKeyDistributionInfoProvider::RemoveObserver(
    IwaRuntimeDataProvider::Observer* observer) {
  key_provider_observers_.RemoveObserver(observer);
}

void IwaKeyDistributionInfoProvider::RotateKeyForDevMode(
    base::PassKey<IwaInternalsHandler>,
    const std::string& web_bundle_id,
    const std::optional<std::vector<uint8_t>>& rotated_key) {
  GetDevModeKeyRotationData().insert_or_assign(
      web_bundle_id, IwaRuntimeDataProvider::KeyRotationInfo(rotated_key));
  DispatchComponentUpdateSuccess(/*is_preloaded=*/false);
}

base::OneShotEvent&
IwaKeyDistributionInfoProvider::OnBestEffortRuntimeDataReady() {
  if (!is_on_demand_supported_) {
    return AlreadySignalled();
  }

  PostMaybeQueueComponentUpdateOnceOnDataReady();
  return maybe_downloaded_data_ready_;
}

std::optional<bool> IwaKeyDistributionInfoProvider::IsPreloadedForTesting()
    const {
  CHECK_IS_TEST();
  return component_ ? std::make_optional(component_->is_preloaded)
                    : std::nullopt;
}

void IwaKeyDistributionInfoProvider::SetComponentDataForTesting(
    base::Version component_version,
    bool is_preloaded,
    IwaKeyDistribution component_data) {
  CHECK_IS_TEST();

  auto component_internal_data =
      ParseKeyDistributionData(std::move(component_data));
  CHECK(component_internal_data.has_value());

  component_ = Component(component_version, is_preloaded,
                         component_internal_data.value());
}

base::Value IwaKeyDistributionInfoProvider::AsDebugValue() const {
  base::Value::Dict debug_data;

  if (!GetDevModeKeyRotationData().empty()) {
    auto* dev_mode_key_rotations =
        debug_data.EnsureDict("dev_mode_key_rotations");
    for (const auto& [web_bundle_id, kr_info] : GetDevModeKeyRotationData()) {
      dev_mode_key_rotations->Set(web_bundle_id, kr_info.AsDebugValue());
    }
  }
  if (component_) {
    debug_data.Set("component_version", component_->version.GetString());
    debug_data.Set("managed_allowlist",
                   base::ToValueList(component_->data.managed_allowlist));
    auto* key_rotations = debug_data.EnsureDict("key_rotations");
    for (const auto& [web_bundle_id, kr_info] :
         component_->data.key_rotations) {
      key_rotations->Set(web_bundle_id, kr_info.AsDebugValue());
    }

    auto* app_permissions = debug_data.EnsureDict("special_app_permissions");
    for (const auto& [web_bundle_id, app_permissions_info] :
         component_->data.special_app_permissions) {
      app_permissions->Set(web_bundle_id, app_permissions_info.AsDebugValue());
    }

    if (component_->is_preloaded) {
      debug_data.Set("is_preloaded", true);
    }
  } else {
    debug_data.Set("component_version", "null");
  }

  return base::Value(std::move(debug_data));
}

// Writes component metadata (version and whether it's preloaded) to `log`.
void IwaKeyDistributionInfoProvider::WriteComponentMetadata(
    base::Value::Dict& log) const {
  if (!component_) {
    // Will be displayed as <null>.
    log.Set("component", base::Value());
    return;
  }

  auto* component = log.EnsureDict("component");
  component->Set("version", component_->version.GetString());
  if (component_->is_preloaded) {
    component->Set("is_preloaded", true);
  }
}

void IwaKeyDistributionInfoProvider::DispatchComponentUpdateSuccess(
    bool is_preloaded) {
  observers_.Notify(&Observer::OnComponentUpdateSuccess, is_preloaded);
  key_provider_observers_.Notify(
      &IwaRuntimeDataProvider::Observer::OnRuntimeDataChanged);
}

void IwaKeyDistributionInfoProvider::DispatchComponentUpdateError(
    IwaComponentUpdateError error) {
  base::UmaHistogramEnumeration(kIwaKeyDistributionComponentUpdateError, error);
  observers_.Notify(&Observer::OnComponentUpdateError, error);
}

void IwaKeyDistributionInfoProvider::
    PostMaybeQueueComponentUpdateOnceOnDataReady() {
  if (maybe_queue_component_update_posted_) {
    return;
  }
  maybe_queue_component_update_posted_ = true;
  any_data_ready_.Post(
      FROM_HERE,
      base::BindOnce(&IwaKeyDistributionInfoProvider::MaybeQueueComponentUpdate,
                     base::Unretained(this)));
}

void IwaKeyDistributionInfoProvider::MaybeQueueComponentUpdate() {
  CHECK(maybe_queue_component_update_posted_);
  CHECK(any_data_ready_.is_signaled());
  CHECK(is_on_demand_supported_);
  CHECK(queue_on_demand_update_);

  if (!component_ || component_->is_preloaded) {
    queue_on_demand_update_.Run(
        base::PassKey<IwaKeyDistributionInfoProvider>());
    //  Schedule a fallback signaller.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IwaKeyDistributionInfoProvider::SignalOnDataReady,
                       base::Unretained(this),
                       /*is_preloaded=*/false),
        kDownloadedComponentDataWaitTime);
  }
}

void IwaKeyDistributionInfoProvider::SignalOnDataReady(bool is_preloaded) {
  if (!any_data_ready_.is_signaled()) {
    any_data_ready_.Signal();
  }
  if (!is_preloaded && !maybe_downloaded_data_ready_.is_signaled()) {
    maybe_downloaded_data_ready_.Signal();
  }
}

KeyDistributionComponentSource
IwaKeyDistributionInfoProvider::GetComponentDataSource() const {
  if (component_) {
    return component_->is_preloaded
               ? KeyDistributionComponentSource::kPreloaded
               : KeyDistributionComponentSource::kDownloaded;
  }
  return KeyDistributionComponentSource::kNone;
}

IwaKeyDistributionInfoProvider::Data::Data(
    KeyRotations key_rotations,
    SpecialAppPermissions special_app_permissions,
    ManagedAllowlist managed_allowlist)
    : key_rotations(std::move(key_rotations)),
      special_app_permissions(std::move(special_app_permissions)),
      managed_allowlist(std::move(managed_allowlist)) {}
IwaKeyDistributionInfoProvider::Data::~Data() = default;
IwaKeyDistributionInfoProvider::Data::Data(const Data&) = default;

IwaKeyDistributionInfoProvider::Component::Component(base::Version version,
                                                     bool is_preloaded,
                                                     Data data)
    : version(std::move(version)),
      is_preloaded(is_preloaded),
      data(std::move(data)) {}
IwaKeyDistributionInfoProvider::Component::~Component() = default;
IwaKeyDistributionInfoProvider::Component::Component(const Component&) =
    default;

}  // namespace web_app
