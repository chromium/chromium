// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_exporter.h"

#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_export_manager_swift.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

NSString* const kUserEmail = @"test@example.com";

std::vector<uint8_t> GetTrustedVaultKey() {
  return {1, 2, 3, 4, 5};
}

NSData* ToNSData(const std::string& str) {
  return [NSData dataWithBytes:str.data() length:str.size()];
}

password_manager::StoredCredential CreateStoredCredential() {
  password_manager::StoredCredential cred;
  cred.username_value = u"username";
  cred.password_value = u"password";
  cred.signon_realm = "http://www.example.com/";
  cred.url = GURL("http://www.example.com/");
  cred.date_created = base::Time::FromMillisecondsSinceUnixEpoch(987654321000);
  cred.in_store = password_manager::PasswordForm::Store::kProfileStore;
  cred.notes = {password_manager::PasswordNote(u"note", base::Time::Now())};
  return cred;
}

sync_pb::WebauthnCredentialSpecifics CreatePasskeySpecifics() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_credential_id("1234567890123456");
  passkey.set_rp_id("example.com");
  passkey.set_user_id("user_id");
  passkey.set_user_name("userName");
  passkey.set_user_display_name("userDisplayName");
  passkey.set_creation_time(123456789000);
  std::vector<uint8_t> private_key = {'p', 'r', 'i', 'v', 'a', 't',
                                      'e', '_', 'k', 'e', 'y'};
  sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted;
  encrypted.set_private_key(private_key.data(), private_key.size());
  webauthn::passkey_model_utils::EncryptWebauthnCredentialSpecificsData(
      GetTrustedVaultKey(), encrypted, &passkey);
  return passkey;
}

CredentialExchangePassword* CreateCredentialExchangePassword() {
  return [[CredentialExchangePassword alloc]
       initWithURL:[NSURL URLWithString:@"http://www.example.com/"]
          username:@"username"
          password:@"password"
              note:@"note"
      creationDate:[NSDate dateWithTimeIntervalSince1970:987654321.0]];
}

CredentialExchangePasskey* CreateCredentialExchangePasskey() {
  return [[CredentialExchangePasskey alloc]
      initWithCredentialId:ToNSData("1234567890123456")
                      rpId:@"example.com"
                  userName:@"userName"
           userDisplayName:@"userDisplayName"
                    userId:ToNSData("user_id")
                privateKey:ToNSData("private_key")
              creationDate:[NSDate dateWithTimeIntervalSince1970:123456789.0]];
}

class CredentialExporterTest : public PlatformTest {
 protected:
  void SetUp() override {
    mock_delegate_ = OCMProtocolMock(@protocol(CredentialExporterDelegate));
    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    exporter_ = [[CredentialExporter alloc] initWithWindow:window_
                                                  delegate:mock_delegate_];
  }

  base::test::TaskEnvironment task_environment_;
  id mock_delegate_;
  UIWindow* window_;
  CredentialExporter* exporter_;
};

TEST_F(CredentialExporterTest, PropagatesExportError) {
  [[mock_delegate_ expect] onExportError];

  [(id<CredentialExportManagerDelegate>)exporter_ onExportError];

  [mock_delegate_ verify];
}

TEST_F(CredentialExporterTest, ExportsPasswords) {
  if (!@available(iOS 26, *)) {
    GTEST_SKIP() << "CredentialExportManager is only available on iOS 26+";
  }

  if (@available(iOS 26, *)) {
    id mockExportManager = OCMClassMock([CredentialExportManager class]);
    OCMStub([mockExportManager alloc]).andReturn(mockExportManager);
    CredentialExporter* exporter =
        [[CredentialExporter alloc] initWithWindow:window_
                                          delegate:mock_delegate_];

    [[mockExportManager expect]
        startExportWithPasswords:@[ CreateCredentialExchangePassword() ]
                        passkeys:@[]
                          window:window_
                       userEmail:kUserEmail
                    exporterName:[OCMArg any]];

    password_manager::CredentialUIEntry credential(
        password_manager::ToPasswordForm(CreateStoredCredential()));
    [exporter startExportWithPasswords:{credential}
        passkeys:{}
        trustedVaultKeys:{}
        userEmail:kUserEmail];

    [mockExportManager verify];
  }
}

TEST_F(CredentialExporterTest, ExportsPasskeys) {
  if (!@available(iOS 26, *)) {
    GTEST_SKIP() << "CredentialExportManager is only available on iOS 26+";
  }

  if (@available(iOS 26, *)) {
    id mockExportManager = OCMClassMock([CredentialExportManager class]);
    OCMStub([mockExportManager alloc]).andReturn(mockExportManager);
    CredentialExporter* exporter =
        [[CredentialExporter alloc] initWithWindow:window_
                                          delegate:mock_delegate_];

    [[mockExportManager expect]
        startExportWithPasswords:@[]
                        passkeys:@[ CreateCredentialExchangePasskey() ]
                          window:window_
                       userEmail:kUserEmail
                    exporterName:[OCMArg any]];

    [exporter startExportWithPasswords:{}
                              passkeys:{CreatePasskeySpecifics()}
                      trustedVaultKeys:{GetTrustedVaultKey()}
                             userEmail:kUserEmail];

    [mockExportManager verify];
  }
}

}  // namespace
