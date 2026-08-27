// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/test_support/key_distribution/test_utils.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/webapps/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"

namespace web_app::test {

namespace {

using ComponentMetadataOrError =
    base::expected<IwaComponentMetadata, IwaComponentUpdateError>;

using ComponentUpdateFuture = base::test::TestFuture<ComponentMetadataOrError>;

}  // namespace

base::expected<void, IwaComponentUpdateError>
KeyDistributionComponent::UploadFromComponentFolder() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return UpdateKeyDistributionInfo(metadata.version, component_data);
}

void KeyDistributionComponent::InjectComponentDataDirectly() {
  IwaKeyDistributionInfoProvider::GetInstanceForTesting()
      .SetComponentDataForTesting(metadata.version, metadata.is_preloaded,
                                  component_data);
}

KeyDistributionComponentBuilder::KeyDistributionComponentBuilder(
    const base::Version& component_version,
    bool is_preloaded)
    : component_(
          /*metadata=*/IwaComponentMetadata{component_version, is_preloaded},
          /*component_data=*/IwaKeyDistribution{}) {}

KeyDistributionComponentBuilder::~KeyDistributionComponentBuilder() = default;

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToKeyRotations(
    const web_package::SignedWebBundleId& web_bundle_id,
    base::span<const uint8_t> expected_key) & {
  IwaKeyRotations::KeyRotationInfo kr_info_proto;
  kr_info_proto.set_expected_key(base::Base64Encode(expected_key));
  (*component_.component_data.mutable_key_rotation_data()
        ->mutable_key_rotations())[web_bundle_id.id()] =
      std::move(kr_info_proto);
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToKeyRotations(
    const web_package::SignedWebBundleId& web_bundle_id,
    base::span<const uint8_t> expected_key) && {
  return std::move(AddToKeyRotations(web_bundle_id, expected_key));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::WithKeyRotations(
    const std::vector<std::pair<web_package::SignedWebBundleId,
                                std::vector<uint8_t>>>& key_rotations) & {
  for (const auto& [id, key] : key_rotations) {
    AddToKeyRotations(id, key);
  }
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::WithKeyRotations(
    const std::vector<std::pair<web_package::SignedWebBundleId,
                                std::vector<uint8_t>>>& key_rotations) && {
  return std::move(WithKeyRotations(key_rotations));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToManagedAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id) & {
  component_.component_data.mutable_iwa_access_control()
      ->mutable_managed_allowlist()
      ->emplace(web_bundle_id.id(),
                IwaAccessControl_ManagedAllowlistItemData());
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToManagedAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id) && {
  return std::move(AddToManagedAllowlist(web_bundle_id));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToBlocklist(
    const web_package::SignedWebBundleId& web_bundle_id) & {
  component_.component_data.mutable_iwa_access_control()
      ->mutable_blocklist()
      ->emplace(web_bundle_id.id(), IwaAccessControl_BlocklistItemData());
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToBlocklist(
    const web_package::SignedWebBundleId& web_bundle_id) && {
  return std::move(AddToBlocklist(web_bundle_id));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::WithManagedAllowlist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) & {
  for (const auto& bundle_id : bundle_ids) {
    AddToManagedAllowlist(bundle_id);
  }
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::WithManagedAllowlist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) && {
  return std::move(WithManagedAllowlist(bundle_ids));
}

KeyDistributionComponentBuilder& KeyDistributionComponentBuilder::WithBlocklist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) & {
  for (const auto& bundle_id : bundle_ids) {
    AddToBlocklist(bundle_id);
  }
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::WithBlocklist(
    const std::vector<web_package::SignedWebBundleId>& bundle_ids) && {
  return std::move(WithBlocklist(bundle_ids));
}

KeyDistributionComponentBuilder&
KeyDistributionComponentBuilder::AddToSpecialAppPermissions(
    const web_package::SignedWebBundleId& web_bundle_id,
    const SpecialAppPermissions& permissions) & {
  IwaSpecialAppPermissions::SpecialAppPermissions info;
  info.mutable_multi_screen_capture()->set_skip_capture_started_notification(
      permissions.skip_capture_started_notification);
  info.mutable_chrome_os_permissions()->set_allow_set_shape(
      permissions.allow_set_shape);
  (*component_.component_data.mutable_special_app_permissions_data()
        ->mutable_special_app_permissions())[web_bundle_id.id()] =
      std::move(info);
  return *this;
}

KeyDistributionComponentBuilder&&
KeyDistributionComponentBuilder::AddToSpecialAppPermissions(
    const web_package::SignedWebBundleId& web_bundle_id,
    const SpecialAppPermissions& permissions) && {
  return std::move(AddToSpecialAppPermissions(web_bundle_id, permissions));
}

KeyDistributionComponent KeyDistributionComponentBuilder::Build() && {
  return std::move(component_);
}

base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const base::FilePath& path) {
  ComponentUpdateFuture future;
  auto waiter = SetOnComponentUpdatedForTesting(future.GetRepeatingCallback());
  IwaKeyDistributionInfoProvider::GetInstanceForTesting()
      .LoadKeyDistributionData(version, path, /*is_preloaded=*/false);
  ASSIGN_OR_RETURN((auto [loaded_version, is_preloaded]), future.Take());
  CHECK(version == loaded_version && !is_preloaded);
  return base::ok();
}

base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const IwaKeyDistribution& kd_proto) {
  base::ScopedTempDir component_install_dir;
  CHECK(component_install_dir.CreateUniqueTempDir());
  auto path = component_install_dir.GetPath().AppendASCII("krc");
  CHECK(base::WriteFile(path, kd_proto.SerializeAsString()));
  return UpdateKeyDistributionInfo(version, path);
}

base::CallbackListSubscription SetOnComponentUpdatedForTesting(
    base::RepeatingCallback<void(ComponentMetadataOrError)> callback) {
  return IwaKeyDistributionInfoProvider::GetInstanceForTesting()
      .OnComponentUpdatedForTesting(
          base::BindRepeating([](base::expected<void, IwaComponentUpdateError>
                                     result) {
            return result.transform([]() -> IwaComponentMetadata {
              auto& instance =
                  IwaKeyDistributionInfoProvider::GetInstanceForTesting();
              return {.version = *instance.GetVersion(),
                      .is_preloaded = *instance.IsPreloadedForTesting()};
            });
          }).Then(callback));
}

base::expected<void, IwaComponentUpdateError> ConfigureSetShapeAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version) {
  return KeyDistributionComponentBuilder(version)
      .AddToSpecialAppPermissions(
          web_bundle_id,
          KeyDistributionComponentBuilder::SpecialAppPermissions{
              .allow_set_shape = true})
      .Build()
      .UploadFromComponentFolder();
}

}  // namespace web_app::test
