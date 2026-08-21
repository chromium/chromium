// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_KEY_DISTRIBUTION_TEST_UTILS_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_KEY_DISTRIBUTION_TEST_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/key_distribution/iwa_key_distribution_histograms.h"
#include "components/webapps/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"

namespace web_app::test {

struct IwaComponentMetadata {
  base::Version version;
  bool is_preloaded;
};

// Represents the on-disk state of the key distribution component.
struct KeyDistributionComponent {
  IwaComponentMetadata metadata;
  // In-memory data parsed from the protobuf file inside the component
  // directory.
  IwaKeyDistribution component_data;

  // Uploads the component just by replacing the saved internal data in
  // IwaKeyDistributionInfoProvider.
  void InjectComponentDataDirectly();
  // Uploads the component using more e2e approach. The proto file with the
  // component data is saved and loaded by IwaKeyDistributionInfoProvider.
  base::expected<void, IwaComponentUpdateError> UploadFromComponentFolder();
};

// A builder-style class to help create and update the key distribution
// component in tests. After configuring the desired state using the
// `AddTo...`/`With..` methods, call `Build()` and update the component with one
// of KeyDistributionComponent methods. Unset fields will be set to the default
// value, i.e. the empty set/list.
class KeyDistributionComponentBuilder {
 public:
  struct SpecialAppPermissions {
    bool skip_capture_started_notification = false;
    bool allow_set_shape = false;
  };
  // Component update requires the higher component version than the current
  // one.
  explicit KeyDistributionComponentBuilder(
      const base::Version& component_version,
      bool is_preloaded = false);
  ~KeyDistributionComponentBuilder();

  KeyDistributionComponentBuilder(const KeyDistributionComponentBuilder&) =
      delete;
  KeyDistributionComponentBuilder& operator=(
      const KeyDistributionComponentBuilder&) = delete;

  KeyDistributionComponentBuilder& AddToKeyRotations(
      const web_package::SignedWebBundleId& web_bundle_id,
      base::span<const uint8_t> expected_key) &;
  KeyDistributionComponentBuilder&& AddToKeyRotations(
      const web_package::SignedWebBundleId& web_bundle_id,
      base::span<const uint8_t> expected_key) &&;

  KeyDistributionComponentBuilder& WithKeyRotations(
      const std::vector<std::pair<web_package::SignedWebBundleId,
                                  std::vector<uint8_t>>>& key_rotations) &;
  KeyDistributionComponentBuilder&& WithKeyRotations(
      const std::vector<std::pair<web_package::SignedWebBundleId,
                                  std::vector<uint8_t>>>& key_rotations) &&;

  KeyDistributionComponentBuilder& WithManagedAllowlist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &;
  KeyDistributionComponentBuilder&& WithManagedAllowlist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &&;
  KeyDistributionComponentBuilder& AddToManagedAllowlist(
      const web_package::SignedWebBundleId& web_bundle_id) &;
  KeyDistributionComponentBuilder&& AddToManagedAllowlist(
      const web_package::SignedWebBundleId& web_bundle_id) &&;

  KeyDistributionComponentBuilder& WithBlocklist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &;
  KeyDistributionComponentBuilder&& WithBlocklist(
      const std::vector<web_package::SignedWebBundleId>& bundle_ids) &&;
  KeyDistributionComponentBuilder& AddToBlocklist(
      const web_package::SignedWebBundleId& web_bundle_id) &;
  KeyDistributionComponentBuilder&& AddToBlocklist(
      const web_package::SignedWebBundleId& web_bundle_id) &&;

  KeyDistributionComponentBuilder& AddToSpecialAppPermissions(
      const web_package::SignedWebBundleId& web_bundle_id,
      const SpecialAppPermissions& permissions) &;
  KeyDistributionComponentBuilder&& AddToSpecialAppPermissions(
      const web_package::SignedWebBundleId& web_bundle_id,
      const SpecialAppPermissions& permissions) &&;

  KeyDistributionComponent Build() &&;

 private:
  KeyDistributionComponent component_;
};

// Generates the component data on disk using the provided protobuf and triggers
// loading it into IwaKeyDistributionInfoProvider.
base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const IwaKeyDistribution& kd_proto);

// Uses the file on disk as component data and triggers loading it into
// IwaKeyDistributionInfoProvider.
base::expected<void, IwaComponentUpdateError> UpdateKeyDistributionInfo(
    const base::Version& version,
    const base::FilePath& path);

// Subscribes to IwaKeyDistributionInfoProvider updates, passing the version and
// preloaded status to `callback`.
base::CallbackListSubscription SetOnComponentUpdatedForTesting(
    base::RepeatingCallback<
        void(base::expected<IwaComponentMetadata, IwaComponentUpdateError>)>
        callback);

// Configures the key distribution component to allow the given IWA to use the
// setShape API.
base::expected<void, IwaComponentUpdateError> ConfigureSetShapeAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version = base::Version("1.0.0"));

}  // namespace web_app::test

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_KEY_DISTRIBUTION_TEST_UTILS_H_
