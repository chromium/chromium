// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_RUNTIME_DATA_PROVIDER_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_RUNTIME_DATA_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/one_shot_event.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/webapps/isolated_web_apps/public/iwa_entitlements.h"

class BrowserProcessImpl;
class TestingBrowserProcess;

namespace web_app {

// This class is an abstract interface for providers of IWA runtime data.
// The `components/` layer uses this interface to get data without needing
// to know where that data comes from (e.g., Component Updater). The concrete
// implementation is provided by the embedder (e.g., Chrome).
class IwaRuntimeDataProvider {
 public:
  // The KeyRotationInfo struct provides information about expected public keys
  // for Isolated Web Apps, which is fundamental to IWA security.
  struct KeyRotationInfo {
    using PublicKeyData = std::vector<uint8_t>;

    explicit KeyRotationInfo(
        PublicKeyData public_key,
        std::optional<PublicKeyData> previous_key = std::nullopt);
    ~KeyRotationInfo();
    KeyRotationInfo(const KeyRotationInfo&);

    base::Value AsDebugValue() const;

    PublicKeyData public_key;
    std::optional<PublicKeyData> previous_key;
  };

  struct SpecialAppPermissionsInfo {
    base::Value AsDebugValue() const;
    bool skip_capture_started_notification = false;
    bool allow_set_shape = false;
  };

  struct UserInstallAllowlistItemData {
    explicit UserInstallAllowlistItemData(
        const std::string& enterprise_name,
        std::vector<IwaEntitlementsSet> entitlements = {});
    ~UserInstallAllowlistItemData();
    UserInstallAllowlistItemData(const UserInstallAllowlistItemData&);

    base::Value AsDebugValue() const;

    std::string enterprise_name;
    std::vector<IwaEntitlementsSet> entitlements;
  };

  static IwaRuntimeDataProvider& GetInstance();

  // Note that these methods do not take ownership of `instance`; the lifetime
  // management remains the caller's responsibility.
  static void SetInstance(
      base::PassKey<BrowserProcessImpl, TestingBrowserProcess>,
      IwaRuntimeDataProvider* instance);
  static base::AutoReset<IwaRuntimeDataProvider*> SetInstanceForTesting(
      IwaRuntimeDataProvider* instance);

  virtual ~IwaRuntimeDataProvider() = default;

  virtual const KeyRotationInfo* GetKeyRotationInfo(
      const std::string& web_bundle_id) const = 0;

  // Called when the underlying runtime data (including key data) may have
  // changed. Consumers should re-validate any data that depends on this
  // information.
  virtual base::CallbackListSubscription OnRuntimeDataChanged(
      base::RepeatingClosure callback) = 0;

  // Allows a consumer to wait until the provider has the most up-to-date
  // data that it can have within a reasonable time budget. The concrete
  // implementation is left to the embedder.
  virtual base::OneShotEvent& OnBestEffortRuntimeDataReady() = 0;

  // Only bundles present in the managed allowlist can be installed and updated.
  virtual bool IsManagedInstallPermitted(
      std::string_view web_bundle_id) const = 0;
  virtual bool IsManagedUpdatePermitted(
      std::string_view web_bundle_id) const = 0;
  virtual bool IsBundleBlocklisted(std::string_view web_bundle_id) const = 0;

  virtual const SpecialAppPermissionsInfo* GetSpecialAppPermissionsInfo(
      const std::string& web_bundle_id) const = 0;
  virtual std::vector<std::string> GetSkipMultiCaptureNotificationBundleIds()
      const = 0;

  virtual const UserInstallAllowlistItemData* GetUserInstallAllowlistData(
      const std::string& web_bundle_id) const = 0;

  virtual void WriteDebugMetadata(base::DictValue& log) const = 0;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_RUNTIME_DATA_PROVIDER_H_
