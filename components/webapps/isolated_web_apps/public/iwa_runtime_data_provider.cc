// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/check_is_test.h"
#include "base/containers/to_value_list.h"

namespace web_app {

namespace {

IwaRuntimeDataProvider* g_instance = nullptr;

}  // namespace

IwaRuntimeDataProvider::KeyRotationInfo::KeyRotationInfo(
    PublicKeyData public_key,
    std::optional<PublicKeyData> previous_key)
    : public_key(std::move(public_key)),
      previous_key(std::move(previous_key)) {}

IwaRuntimeDataProvider::KeyRotationInfo::~KeyRotationInfo() = default;

IwaRuntimeDataProvider::KeyRotationInfo::KeyRotationInfo(
    const KeyRotationInfo&) = default;

base::Value IwaRuntimeDataProvider::KeyRotationInfo::AsDebugValue() const {
  auto dict =
      base::DictValue().Set("public_key", base::Base64Encode(public_key));
  if (previous_key) {
    dict.Set("previous_key", base::Base64Encode(*previous_key));
  }
  return base::Value(std::move(dict));
}

base::Value IwaRuntimeDataProvider::SpecialAppPermissionsInfo::AsDebugValue()
    const {
  return base::Value(base::DictValue()
                         .Set("skip_capture_started_notification",
                              skip_capture_started_notification)
                         .Set("allow_set_shape", allow_set_shape));
}

IwaRuntimeDataProvider::UserInstallAllowlistItemData::
    UserInstallAllowlistItemData(const std::string& enterprise_name,
                                 std::vector<IwaEntitlementsSet> entitlements)
    : enterprise_name(enterprise_name), entitlements(std::move(entitlements)) {}

IwaRuntimeDataProvider::UserInstallAllowlistItemData::
    ~UserInstallAllowlistItemData() = default;

IwaRuntimeDataProvider::UserInstallAllowlistItemData::
    UserInstallAllowlistItemData(const UserInstallAllowlistItemData&) = default;

base::Value IwaRuntimeDataProvider::UserInstallAllowlistItemData::AsDebugValue()
    const {
  return base::Value(
      base::DictValue()
          .Set("enterprise_name", enterprise_name)
          .Set("entitlements",
               base::ToValueList(entitlements,
                                 &IwaEntitlementsSet::AsDebugValue)));
}

// static
IwaRuntimeDataProvider& IwaRuntimeDataProvider::GetInstance() {
  CHECK(g_instance)
      << "IwaRuntimeDataProvider must be initialized by the time "
         "of the call to GetInstance(). This normally happens in at the "
         "startup of the embedder, like BrowserProcess (either in "
         "BrowserProcessImpl or "
         "TestingBrowserProcess).";
  return *g_instance;
}

// static
void IwaRuntimeDataProvider::SetInstance(
    base::PassKey<BrowserProcessImpl, TestingBrowserProcess>,
    IwaRuntimeDataProvider* instance) {
  g_instance = instance;
}

// static
base::AutoReset<IwaRuntimeDataProvider*>
IwaRuntimeDataProvider::SetInstanceForTesting(
    IwaRuntimeDataProvider* instance) {
  CHECK_IS_TEST();
  return base::AutoReset<IwaRuntimeDataProvider*>(&g_instance, instance);
}

}  // namespace web_app
