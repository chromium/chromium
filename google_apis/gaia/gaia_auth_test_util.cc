// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_test_util.h"

#include <memory>

#include "base/base64.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/list_accounts_response.pb.h"
#include "google_apis/gaia/oauth2_mint_token_consent_result.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "google_apis/gaia/android/jni_headers/DeviceManagementErrorDetails_jni.h"
#include "google_apis/gaia/android_device_management_error_details.h"
#else
namespace {

class FakeDeviceManagementErrorDetails
    : public gaia::DeviceManagementErrorDetails {
 public:
  FakeDeviceManagementErrorDetails() = default;
  ~FakeDeviceManagementErrorDetails() override = default;

  std::unique_ptr<gaia::DeviceManagementErrorDetails> Clone() const override {
    return std::make_unique<FakeDeviceManagementErrorDetails>();
  }

  bool Equals(const gaia::DeviceManagementErrorDetails& other) const override {
    return true;
  }

  bool IsUserActionable() const override { return false; }
};

}  // namespace
#endif  // BUILDFLAG(IS_ANDROID)

namespace gaia {

std::string GenerateOAuth2MintTokenConsentResult(
    std::optional<bool> approved,
    const std::optional<std::string>& encrypted_approval_data,
    const std::optional<GaiaId>& obfuscated_id,
    base::Base64UrlEncodePolicy encode_policy) {
  OAuth2MintTokenConsentResult consent_result;
  if (approved.has_value()) {
    consent_result.set_approved(approved.value());
  }
  if (encrypted_approval_data.has_value()) {
    consent_result.set_encrypted_approval_data(encrypted_approval_data.value());
  }
  if (obfuscated_id.has_value()) {
    consent_result.set_obfuscated_id(obfuscated_id->ToString());
  }
  std::string serialized_consent = consent_result.SerializeAsString();
  std::string encoded_consent;
  base::Base64UrlEncode(serialized_consent, encode_policy, &encoded_consent);
  return encoded_consent;
}

std::string CreateListAccountsResponseInBinaryFormat(
    const std::vector<gaia::CookieParams>& params) {
  gaia::ListAccountsResponse response;

  for (const auto& param : params) {
    gaia::Account* account = response.add_account();

    account->set_display_email(param.email);
    account->set_valid_session(param.valid);
    account->set_obfuscated_id(param.gaia_id.ToString());
    account->set_signed_out(param.signed_out);
    account->set_is_verified(param.verified);
  }

  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  return base::Base64Encode(serialized_response);
}

std::unique_ptr<DeviceManagementErrorDetails>
CreateFakeDeviceManagementErrorDetails() {
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  return std::make_unique<AndroidDeviceManagementErrorDetails>(
      base::android::ScopedJavaGlobalRef<jobject>(
          Java_DeviceManagementErrorDetails_Constructor(env, nullptr)));
#else
  return std::make_unique<FakeDeviceManagementErrorDetails>();
#endif
}

}  // namespace gaia
