// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_tab_helper.h"

#import "base/base64url.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

constexpr char kCredentialId[] = "credential_id";
constexpr char kRpId[] = "example.com";

constexpr char kWebAuthenticationIOSContentAreaEventHistogram[] =
    "WebAuthentication.IOS.ContentAreaEvent";

}  // namespace

class PasskeyTabHelperTest : public PlatformTest {
 public:
  PasskeyTabHelperTest() {
    PasskeyTabHelper::CreateForWebState(&fake_web_state_, passkey_model_.get());
  }

 protected:
  PasskeyTabHelper* passkey_tab_helper() {
    return PasskeyTabHelper::FromWebState(&fake_web_state_);
  }

  web::WebTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<webauthn::PasskeyModel> passkey_model_ =
      std::make_unique<webauthn::TestPasskeyModel>();
  web::FakeWebState fake_web_state_;
};

TEST_F(PasskeyTabHelperTest, LogsEventFromGetRequestedString) {
  passkey_tab_helper()->LogEventFromString("getRequested");

  constexpr int kGetRequestedBucket = 0;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateRequestedString) {
  passkey_tab_helper()->LogEventFromString("createRequested");

  constexpr int kCreateRequestedBucket = 1;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsGetResolvedEventGpmPasskey) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_credential_id(kCredentialId);
  passkey.set_rp_id(kRpId);
  passkey_model_->AddNewPasskeyForTesting(std::move(passkey));

  std::string credential_id_base64url_encoded;
  base::Base64UrlEncode(kCredentialId,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &credential_id_base64url_encoded);
  passkey_tab_helper()->HandleGetResolvedEvent(credential_id_base64url_encoded,
                                               kRpId);

  constexpr int kGetResolvedGpmBucket = 2;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetResolvedGpmBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsGetResolvedEventNonGpmPasskey) {
  std::string credential_id_base64url_encoded;
  base::Base64UrlEncode(kCredentialId,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &credential_id_base64url_encoded);
  passkey_tab_helper()->HandleGetResolvedEvent(credential_id_base64url_encoded,
                                               kRpId);

  constexpr int kGetResolvedNonGpmBucket = 3;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetResolvedNonGpmBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateResolvedGpmString) {
  passkey_tab_helper()->LogEventFromString("createResolvedGpm");

  constexpr int kCreateRequestedBucket = 4;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateResolvedNonGpmString) {
  passkey_tab_helper()->LogEventFromString("createResolvedNonGpm");

  constexpr int kCreateRequestedBucket = 5;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}
