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

base::expected<IwaKeyDistributionInfoProvider::KeyDistributionData,
               IwaComponentUpdateError>
LoadKeyDistributionDataImpl(const base::FilePath& file_path) {
  std::string key_distribution_data;
  if (!base::ReadFileToString(file_path, &key_distribution_data)) {
    return base::unexpected(IwaComponentUpdateError::kFileNotFound);
  }

  IwaKeyDistribution key_distribution;
  if (!key_distribution.ParseFromString(key_distribution_data)) {
    return base::unexpected(IwaComponentUpdateError::kProtoParsingFailure);
  }

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

  return std::make_tuple(std::move(key_rotations),
                         std::move(special_app_permissions),
                         std::move(managed_allowlist));
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

IwaKeyDistributionInfoProvider::KeyRotationInfo::KeyRotationInfo(
    std::optional<PublicKeyData> public_key)
    : public_key(std::move(public_key)) {}

IwaKeyDistributionInfoProvider::KeyRotationInfo::~KeyRotationInfo() = default;

IwaKeyDistributionInfoProvider::KeyRotationInfo::KeyRotationInfo(
    const KeyRotationInfo&) = default;

base::Value IwaKeyDistributionInfoProvider::KeyRotationInfo::AsDebugValue()
    const {
  return base::Value(base::Value::Dict().Set(
      "public_key", public_key ? base::Base64Encode(*public_key) : "null"));
}

base::Value
IwaKeyDistributionInfoProvider::SpecialAppPermissionsInfo::AsDebugValue()
    const {
  return base::Value(base::Value::Dict().Set(
      "skip_capture_started_notification", skip_capture_started_notification));
}

IwaKeyDistributionInfoProvider* IwaKeyDistributionInfoProvider::GetInstance() {
  auto& instance = GetGlobalIwaKeyDistributionInfoProviderInstance();
  if (!instance) {
    instance.reset(new IwaKeyDistributionInfoProvider());
  }
  return instance.get();
}

void IwaKeyDistributionInfoProvider::DestroyInstanceForTesting() {
  GetGlobalIwaKeyDistributionInfoProviderInstance().reset();
}

const IwaKeyDistributionInfoProvider::KeyRotationInfo*
IwaKeyDistributionInfoProvider::GetKeyRotationInfo(
    const std::string& web_bundle_id) const {
  if (const auto* kr_info =
          base::FindOrNull(GetDevModeKeyRotationData(), web_bundle_id)) {
    return kr_info;
  }

  if (data_) {
    base::UmaHistogramEnumeration(kIwaKeyRotationInfoSource,
                                  data_->is_preloaded
                                      ? KeyRotationInfoSource::kPreloaded
                                      : KeyRotationInfoSource::kDownloaded);
    return base::FindOrNull(data_->key_rotations, web_bundle_id);
  }

  base::UmaHistogramEnumeration(kIwaKeyRotationInfoSource,
                                KeyRotationInfoSource::kNone);
  return nullptr;
}
bool IwaKeyDistributionInfoProvider::IsManagedInstallPermitted(
    std::string_view web_bundle_id) const {
  if (!IsIsolatedWebAppManagedAllowlistEnabled()) {
    return true;
  }
  return data_ && data_->managed_allowlist.contains(web_bundle_id);
}

void IwaKeyDistributionInfoProvider::SetUp(
    QueueOnDemandUpdateCallback callback) {
  queue_on_demand_update_ = callback;
}

void IwaKeyDistributionInfoProvider::LoadKeyDistributionData(
    const base::Version& component_version,
    const base::FilePath& file_path,
    bool is_preloaded) {
  if (data_ && data_->version > component_version) {
    DispatchComponentUpdateError(component_version,
                                 IwaComponentUpdateError::kStaleVersion);
    return;
  }

  // `base::Unretained(this)` is fine as this is a singleton that never goes
  // away.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), GetLoadTaskPriority()},
      base::BindOnce(&LoadKeyDistributionDataImpl, file_path),
      base::BindOnce(
          &IwaKeyDistributionInfoProvider::OnKeyDistributionDataLoaded,
          base::Unretained(this), component_version, is_preloaded));
}

const IwaKeyDistributionInfoProvider::SpecialAppPermissionsInfo*
IwaKeyDistributionInfoProvider::GetSpecialAppPermissionsInfo(
    const std::string& web_bundle_id) const {
  if (data_) {
    return base::FindOrNull(data_->special_app_permissions, web_bundle_id);
  }
  return nullptr;
}

std::vector<std::string>
IwaKeyDistributionInfoProvider::GetSkipMultiCaptureNotificationBundleIds()
    const {
  if (!data_) {
    return {};
  }

  std::vector<std::string> skip_multi_capture_notification_bundle_ids;
  for (const auto& [bundle_id, special_app_permissions] :
       data_->special_app_permissions) {
    if (special_app_permissions.skip_capture_started_notification) {
      skip_multi_capture_notification_bundle_ids.push_back(bundle_id);
    }
  }
  return skip_multi_capture_notification_bundle_ids;
}

IwaKeyDistributionInfoProvider::IwaKeyDistributionInfoProvider() = default;
IwaKeyDistributionInfoProvider::~IwaKeyDistributionInfoProvider() = default;

void IwaKeyDistributionInfoProvider::OnKeyDistributionDataLoaded(
    const base::Version& component_version,
    bool is_preloaded,
    base::expected<KeyDistributionData, IwaComponentUpdateError> result) {
  if (data_ && data_->version > component_version) {
    // This might happen if two tasks with different versions have been posted
    // to the task runner in `LoadKeyDistributionData()`.
    DispatchComponentUpdateError(component_version,
                                 IwaComponentUpdateError::kStaleVersion);
    return;
  }

  ASSIGN_OR_RETURN(
      (auto [key_rotations, special_app_permissions, managed_allowlist]),
      std::move(result), [&](IwaComponentUpdateError error) {
        DispatchComponentUpdateError(component_version, error);
      });

  // TODO(crbug.com/410532804): Add allowlist to the proto file.
  data_ = ComponentData(component_version, std::move(key_rotations),
                        std::move(special_app_permissions),
                        std::move(managed_allowlist), is_preloaded);
  SignalOnDataReady(is_preloaded);
  DispatchComponentUpdateSuccess(component_version, is_preloaded);
}

void IwaKeyDistributionInfoProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IwaKeyDistributionInfoProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void IwaKeyDistributionInfoProvider::RotateKeyForDevMode(
    base::PassKey<IwaInternalsHandler>,
    const std::string& web_bundle_id,
    const std::optional<std::vector<uint8_t>>& rotated_key) {
  GetDevModeKeyRotationData().insert_or_assign(web_bundle_id,
                                               KeyRotationInfo(rotated_key));
  DispatchComponentUpdateSuccess(base::Version(), /*is_preloaded=*/false);
}

base::OneShotEvent&
IwaKeyDistributionInfoProvider::OnMaybeDownloadedComponentDataReady() {
  if (!queue_on_demand_update_) {
    return AlreadySignalled();
  }

  PostMaybeQueueComponentUpdateOnceOnDataReady();
  return maybe_downloaded_data_ready_;
}

std::optional<bool> IwaKeyDistributionInfoProvider::IsPreloadedForTesting()
    const {
  CHECK_IS_TEST();
  return data_ ? std::make_optional(data_->is_preloaded) : std::nullopt;
}

void IwaKeyDistributionInfoProvider::SetComponentDataForTesting(
    ComponentData component_data) {
  CHECK_IS_TEST();
  data_ = std::move(component_data);
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
  if (data_) {
    debug_data.Set("component_version", data_->version.GetString());
    debug_data.Set("managed_allowlist",
                   base::ToValueList(data_->managed_allowlist));
    auto* key_rotations = debug_data.EnsureDict("key_rotations");
    for (const auto& [web_bundle_id, kr_info] : data_->key_rotations) {
      key_rotations->Set(web_bundle_id, kr_info.AsDebugValue());
    }

    auto* app_permissions = debug_data.EnsureDict("special_app_permissions");
    for (const auto& [web_bundle_id, app_permissions_info] :
         data_->special_app_permissions) {
      app_permissions->Set(web_bundle_id, app_permissions_info.AsDebugValue());
    }

    if (data_->is_preloaded) {
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
  if (!data_) {
    // Will be displayed as <null>.
    log.Set("component", base::Value());
    return;
  }

  auto* component = log.EnsureDict("component");
  component->Set("version", data_->version.GetString());
  if (data_->is_preloaded) {
    component->Set("is_preloaded", true);
  }
}

void IwaKeyDistributionInfoProvider::DispatchComponentUpdateSuccess(
    const base::Version& version,
    bool is_preloaded) {
  if (data_ && version.IsValid()) {
    // Custom key rotations via chrome://web-app-internals (indicated by an
    // invalid version) should not be logged.
    base::UmaHistogramEnumeration(kIwaKeyDistributionComponentUpdateSource,
                                  data_->is_preloaded
                                      ? IwaComponentUpdateSource::kPreloaded
                                      : IwaComponentUpdateSource::kDownloaded);
  }

  observers_.Notify(&Observer::OnComponentUpdateSuccess, version, is_preloaded);
}

void IwaKeyDistributionInfoProvider::DispatchComponentUpdateError(
    const base::Version& version,
    IwaComponentUpdateError error) {
  base::UmaHistogramEnumeration(kIwaKeyDistributionComponentUpdateError, error);

  observers_.Notify(&Observer::OnComponentUpdateError, version, error);
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
  CHECK(queue_on_demand_update_);

  if (!data_ || data_->is_preloaded) {
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

IwaKeyDistributionInfoProvider::ComponentData::ComponentData(
    base::Version version,
    KeyRotations key_rotations,
    SpecialAppPermissions special_app_permissions,
    ManagedAllowlist managed_allowlist,
    bool is_preloaded)
    : version(std::move(version)),
      key_rotations(std::move(key_rotations)),
      special_app_permissions(std::move(special_app_permissions)),
      managed_allowlist(std::move(managed_allowlist)),
      is_preloaded(is_preloaded) {}
IwaKeyDistributionInfoProvider::ComponentData::~ComponentData() = default;
IwaKeyDistributionInfoProvider::ComponentData::ComponentData(
    const ComponentData&) = default;

}  // namespace web_app
