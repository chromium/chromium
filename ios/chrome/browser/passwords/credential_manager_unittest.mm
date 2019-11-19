// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/credential_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/ios/credential_manager_util.h"
#import "ios/chrome/browser/passwords/test/test_password_manager_client.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/web_js_test.h"
#include "ios/web/public/test/web_test_with_web_state.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using url::Origin;

namespace {

// Test hostname for cert verification.
constexpr char kHostName[] = "www.example.com";
// HTTPS origin corresponding to kHostName.
constexpr char kHttpsWebOrigin[] = "https://www.example.com/";
// HTTP origin corresponding to kHostName.
constexpr char kHttpWebOrigin[] = "http://www.example.com/";
// HTTP origin representing localhost. It should be considered secure.
constexpr char kLocalhostOrigin[] = "http://localhost";
// Origin with data scheme. It should be considered insecure.
constexpr char kDataUriSchemeOrigin[] = "data://www.example.com";
// File origin.
constexpr char kFileOrigin[] = "file://example_file";

// SSL certificate to load for testing.
constexpr char kCertFileName[] = "ok_cert.pem";

class MockLeakDetectionCheck : public password_manager::LeakDetectionCheck {
 public:
  MOCK_METHOD3(Start, void(const GURL&, base::string16, base::string16));
};

class MockLeakDetectionCheckFactory
    : public password_manager::LeakDetectionCheckFactory {
 public:
  MOCK_CONST_METHOD3(TryCreateLeakCheck,
                     std::unique_ptr<password_manager::LeakDetectionCheck>(
                         password_manager::LeakDetectionDelegateInterface*,
                         signin::IdentityManager*,
                         scoped_refptr<network::SharedURLLoaderFactory>));
};

}  // namespace

class CredentialManagerBaseTest
    : public web::WebJsTest<web::WebTestWithWebState> {
 public:
  CredentialManagerBaseTest()
      : web::WebJsTest<web::WebTestWithWebState>(@[ @"credential_manager" ]) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
  }

  // Updates SSLStatus on web_state()->GetNavigationManager()->GetVisibleItem()
  // with given |cert_status|, |security_style| and |content_status|.
  // SSLStatus fields |certificate|, |connection_status| and |cert_status_host|
  // are the same for all tests.
  void UpdateSslStatus(net::CertStatus cert_status,
                       web::SecurityStyle security_style,
                       web::SSLStatus::ContentStatusFlags content_status) {
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kCertFileName);
    web::SSLStatus& ssl =
        web_state()->GetNavigationManager()->GetVisibleItem()->GetSSL();
    ssl.security_style = security_style;
    ssl.certificate = cert;
    ssl.cert_status = cert_status;
    ssl.content_status = content_status;
    ssl.cert_status_host = kHostName;
  }
};

// Tests CredentialManager class. Tests are performed as follows:
// 1. CredentialManager is instantiated. In its constructor it registers a
//     script command callback for 'credentials' prefix.
// 2. credential_manager.js is injected to the web page.
// 3. JavaScript code invoking one of exposed API methods is executed.
// 4. This results in CredentialManager::HandleScriptCommand being called.
// 5. Wait for background tasks to finish, optionally for returned Promise to be
//    resolved or rejected.
// 6. Check values in JavaScript, stored in variables under 'test_*' prefix by
//    resolver/rejecter functions.
// 7. Optionally expect PasswordManagerClient methods to be (not) called.
class CredentialManagerTest : public CredentialManagerBaseTest {
 public:
  void SetUp() override {
    CredentialManagerBaseTest::SetUp();

    client_ = std::make_unique<TestPasswordManagerClient>();
    manager_ = std::make_unique<CredentialManager>(client_.get(), web_state());

    // Inject JavaScript and set up secure context.
    LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
    LoadHtmlAndInject(@"<html></html>");
    UpdateSslStatus(net::CERT_STATUS_IS_EV, web::SECURITY_STYLE_AUTHENTICATED,
                    web::SSLStatus::NORMAL_CONTENT);

    ON_CALL(*client_, OnCredentialManagerUsed())
        .WillByDefault(testing::Return(true));

    password_credential_form_1_.username_value = base::ASCIIToUTF16("id1");
    password_credential_form_1_.display_name = base::ASCIIToUTF16("Name One");
    password_credential_form_1_.icon_url = GURL("https://example.com/icon.png");
    password_credential_form_1_.password_value = base::ASCIIToUTF16("secret1");
    password_credential_form_1_.origin = GURL(kHttpsWebOrigin);
    password_credential_form_1_.signon_realm = kHttpsWebOrigin;
    password_credential_form_1_.scheme = autofill::PasswordForm::Scheme::kHtml;

    password_credential_form_2_.username_value = base::ASCIIToUTF16("id2");
    password_credential_form_2_.display_name = base::ASCIIToUTF16("Name Two");
    password_credential_form_2_.icon_url = GURL("https://example.com/icon.png");
    password_credential_form_2_.password_value = base::ASCIIToUTF16("secret2");
    password_credential_form_2_.origin = GURL(kHttpsWebOrigin);
    password_credential_form_2_.signon_realm = kHttpsWebOrigin;
    password_credential_form_2_.scheme = autofill::PasswordForm::Scheme::kHtml;

    federated_credential_form_.username_value = base::ASCIIToUTF16("id");
    federated_credential_form_.display_name = base::ASCIIToUTF16("name");
    federated_credential_form_.icon_url =
        GURL("https://federation.com/icon.png");
    federated_credential_form_.federation_origin =
        Origin::Create(GURL("https://federation.com"));
    federated_credential_form_.origin = GURL(kHttpsWebOrigin);
    federated_credential_form_.signon_realm =
        "federation://www.example.com/www.federation.com";
    federated_credential_form_.scheme = autofill::PasswordForm::Scheme::kHtml;
  }

  void TearDown() override {
    manager_.reset();

    // Shutdown PasswordStore.
    if (client_->password_store()) {
      client_->password_store()->ShutdownOnUIThread();
    }

    CredentialManagerBaseTest::TearDown();
  }

 protected:
  std::unique_ptr<TestPasswordManagerClient> client_;
  std::unique_ptr<CredentialManager> manager_;

  autofill::PasswordForm password_credential_form_1_;
  autofill::PasswordForm password_credential_form_2_;
  autofill::PasswordForm federated_credential_form_;
};

// Tests storing a PasswordCredential.
TEST_F(CredentialManagerTest, StorePasswordCredential) {
  auto mock_factory =
      std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
  auto* weak_factory = mock_factory.get();
  manager_->set_leak_factory(std::move(mock_factory));

  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(GURL(kHttpsWebOrigin), base::ASCIIToUTF16("id"),
                    base::ASCIIToUTF16("pencil")));
  EXPECT_CALL(*weak_factory, TryCreateLeakCheck)
      .WillOnce(testing::Return(testing::ByMove(std::move(check_instance))));

  // Call API method |store|.
  ExecuteJavaScript(
      @"var credential = new PasswordCredential({"
       "  id: 'id',"
       "  password: 'pencil',"
       "  name: 'name',"
       "  iconURL: 'https://example.com/icon.png'"
       "});"
       "navigator.credentials.store(credential).then(function(result) {"
       "  test_result_ = (result == undefined);"
       "  test_promise_resolved_ = true;"
       "});");

  // Wait for the Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Check that Promise was resolved with undefined.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_"));

  // Wait for credential to be stored.
  WaitForBackgroundTasks();
  client_->pending_manager()->Save();
  WaitForBackgroundTasks();
  EXPECT_FALSE(client_->password_store()->IsEmpty());

  // Get the stored credential and check its fields.
  TestPasswordStore::PasswordMap passwords =
      client_->password_store()->stored_passwords();
  EXPECT_EQ(1u, passwords.size());
  EXPECT_EQ(1u, passwords[kHttpsWebOrigin].size());
  autofill::PasswordForm form = passwords[kHttpsWebOrigin][0];
  EXPECT_EQ(base::ASCIIToUTF16("id"), form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("name"), form.display_name);
  EXPECT_EQ(base::ASCIIToUTF16("pencil"), form.password_value);
  EXPECT_EQ(GURL("https://example.com/icon.png"), form.icon_url);
  EXPECT_EQ(GURL(kHttpsWebOrigin), form.origin);
  EXPECT_EQ(GURL(kHttpsWebOrigin), form.signon_realm);
}

// Tests storing a FederatedCredential.
TEST_F(CredentialManagerTest, StoreFederatedCredential) {
  // Call API method |store|.
  ExecuteJavaScript(
      @"var credential = new FederatedCredential({"
       "  id: 'id',"
       "  provider: 'https://www.federation.com/',"
       "  name: 'name',"
       "  iconURL: 'https://federation.com/icon.png'"
       "});"
       "navigator.credentials.store(credential).then(function(result) {"
       "  test_result_ = (result == undefined);"
       "  test_promise_resolved_ = true;"
       "});");

  // Wait for the Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Check that Promise was resolved with undefined.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_"));

  // Wait for credential to be stored.
  WaitForBackgroundTasks();
  client_->pending_manager()->Save();
  WaitForBackgroundTasks();
  EXPECT_FALSE(client_->password_store()->IsEmpty());

  // Get the stored credential and check its fields.
  TestPasswordStore::PasswordMap passwords =
      client_->password_store()->stored_passwords();
  EXPECT_EQ(1u, passwords.size());
  std::string federated_origin =
      "federation://www.example.com/www.federation.com";
  EXPECT_EQ(1u, passwords[federated_origin].size());
  autofill::PasswordForm form = passwords[federated_origin][0];
  EXPECT_EQ(base::ASCIIToUTF16("id"), form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("name"), form.display_name);
  EXPECT_EQ(Origin::Create(GURL("https://www.federation.com")),
            form.federation_origin);
  EXPECT_EQ(GURL("https://federation.com/icon.png"), form.icon_url);
  EXPECT_EQ(GURL("https://www.example.com"), form.origin);
  EXPECT_EQ(federated_origin, form.signon_realm);
}

// Tests that storing a credential from insecure context will not happen.
TEST_F(CredentialManagerTest, TryToStoreCredentialFromInsecureContext) {
  // Inject JavaScript, set up WebState to have mixed content.
  LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
  LoadHtmlAndInject(@"<html></html>");
  UpdateSslStatus(net::CERT_STATUS_IS_EV, web::SECURITY_STYLE_AUTHENTICATED,
                  web::SSLStatus::DISPLAYED_INSECURE_CONTENT);

  // Expect that user will NOT be prompted to save or update password.
  EXPECT_CALL(*client_, PromptUserToSavePasswordPtr(_)).Times(0);

  // Expect that PasswordManagerClient method used by
  // CredentialManagerImpl::Store will not be called.
  EXPECT_CALL(*client_, OnCredentialManagerUsed()).Times(0);

  // Call API method |store|.
  ExecuteJavaScript(
      @"var credential = new PasswordCredential({"
       "  id: 'id',"
       "  password: 'pencil',"
       "  name: 'name',"
       "  iconURL: 'https://example.com/icon.png'"
       "});"
       "navigator.credentials.store(credential).catch(function(reason) {"
       "  test_result_valid_type_ = (reason instanceof DOMException && "
       "      reason.name == DOMException.INVALID_STATE_ERR);"
       "  test_promise_rejected_ = true;"
       "});");
  WaitForBackgroundTasks();

  // Wait for Promise to be rejected.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_rejected_") isEqual:@YES]);
  });
  // Check that Promise was rejected with InvalidStateError.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_valid_type_"));
}

// Tests that Promise will be rejected with TypeError for invalid Credential.
TEST_F(CredentialManagerTest, RejectOnInvalidCredential) {
  // Call |store| with invalid Credential: |iconURL| is not a valid URL.
  ExecuteJavaScript(
      @"var credential = new PasswordCredential({"
       "  id: 'id',"
       "  password: 'pencil',"
       "  name: 'name',"
       "  iconURL: 'https://'"
       "});"
       "navigator.credentials.store(credential).catch(function(reason) {"
       "  test_result_valid_type_ = (reason instanceof TypeError);"
       "  test_promise_rejected_ = true;"
       "});");

  // Wait for Promise to be rejected.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_rejected_") isEqual:@YES]);
  });

  // Check that Promise was rejected with TypeError.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_valid_type_"));
}

// Tests retrieving a PasswordCredential.
TEST_F(CredentialManagerTest, GetPasswordCredential) {
  // Manually store a PasswordForm in password store.
  client_->password_store()->AddLogin(password_credential_form_1_);

  // Call API method |get|.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  password: true,"
       "  mediation: 'silent'"
       "}).then(function(credential) {"
       "  test_credential_ = credential; "
       "  test_promise_resolved_ = true;"
       "})");

  // Wait for PasswordCredential to be obtained and for Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Check PasswordCredential fields.
  ASSERT_NSEQ(@"password", ExecuteJavaScript(@"test_credential_.type"));
  ASSERT_NSEQ(@"id1", ExecuteJavaScript(@"test_credential_.id"));
  ASSERT_NSEQ(@"Name One", ExecuteJavaScript(@"test_credential_.name"));
  ASSERT_NSEQ(@"secret1", ExecuteJavaScript(@"test_credential_.password"));
  ASSERT_NSEQ(@"https://example.com/icon.png",
              ExecuteJavaScript(@"test_credential_.iconURL"));
}

// Tests retrieving a FederatedCredential.
TEST_F(CredentialManagerTest, GetFederatedCredential) {
  // Manually store a PasswordForm in password store.
  client_->password_store()->AddLogin(federated_credential_form_);

  // Call API method |get|.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  providers: ['https://federation.com'], "
       "  mediation: 'silent'"
       "}).then(function(credential) {"
       "  test_credential_ = credential;"
       "  test_promise_resolved_ = true;"
       "})");

  // Wait for FederatedCredential to be obtained and for Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Check FederatedCredential fields.
  ASSERT_NSEQ(@"federated", ExecuteJavaScript(@"test_credential_.type"));
  ASSERT_NSEQ(@"id", ExecuteJavaScript(@"test_credential_.id"));
  ASSERT_NSEQ(@"name", ExecuteJavaScript(@"test_credential_.name"));
  ASSERT_NSEQ(@"https://federation.com",
              ExecuteJavaScript(@"test_credential_.provider"));
  ASSERT_NSEQ(@"https://federation.com/icon.png",
              ExecuteJavaScript(@"test_credential_.iconURL"));
}

// Tests that requesting a credential from insecure context will not happen.
TEST_F(CredentialManagerTest, TryToGetCredentialFromInsecureContext) {
  // Set up WebState to have non-cryptographic scheme.
  LoadHtml(@"<html></html>", GURL(kHttpWebOrigin));
  LoadHtmlAndInject(@"<html></html>");
  client_->set_current_url(GURL(kHttpWebOrigin));

  // Expect that PasswordManagerClient method used by
  // CredentialManagerImpl::Get will not be called.
  EXPECT_CALL(*client_, OnCredentialManagerUsed()).Times(0);

  // Call API method |get|.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  password: true,"
       "  mediation: 'required'"
       "}).catch(function(reason) {"
       "  test_result_valid_type_ = (reason instanceof DOMException && "
       "      reason.name == DOMException.INVALID_STATE_ERR);"
       "  test_promise_rejected_ = true;"
       "});");

  // Wait for Promise to be rejected.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_rejected_") isEqual:@YES]);
  });
  // Check that Promise was rejected with InvalidStateError.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_valid_type_"));
}

// Tests that when Credential is requested with required mediation, a prompt to
// choose credential will be shown to the user.
TEST_F(CredentialManagerTest, GetCredentialWithRequiredMediation) {
  // Manually store a PasswordForm in password store.
  client_->password_store()->AddLogin(password_credential_form_1_);

  // Expect that user will be prompted to choose credentials.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _));

  // Call API method |get|.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  password: true,"
       "  mediation: 'required'"
       "}).then(function(credential) {"
       "  test_credential_ = credential; "
       "  test_promise_resolved_ = true;"
       "})");
  // Wait for Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Expect that returned Credential is not null.
  EXPECT_NSEQ(@"object", ExecuteJavaScript(@"typeof test_credential_"));
}

// Tests that Promise returned by |navigator.credentials.get| will resolve with
// |null| if PasswordStore is empty.
TEST_F(CredentialManagerTest, NullCredentialFromEmptyPasswordStore) {
  // Call API method |get|.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "    password: true,"
       "    mediation: 'silent'"
       "}).then(function(credential) {"
       "    test_credential_ = credential; "
       "    test_promise_resolved_ = true;"
       "})");
  // Wait for Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Expect that returned Credential is null.
  EXPECT_NSEQ(NULL, ExecuteJavaScript(@"test_credential_"));
}

// Tests that if multiple credentials are stored for a website and mediation
// requirement is set to 'optional', user will be prompted to choose
// credentials.
TEST_F(CredentialManagerTest, PromptUserOnMultipleCredentials) {
  // Manually store two PasswordForms in password store.
  client_->password_store()->AddLogin(password_credential_form_1_);
  client_->password_store()->AddLogin(password_credential_form_2_);

  // Expect that user will be prompted to choose credentials.
  EXPECT_CALL(*client_, PromptUserToChooseCredentialsPtr(_, _, _));

  // Call API method |get|.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  password: true, "
       "  mediation: 'optional'"
       "}).then(function(credential) {"
       "  test_credential_ = credential;"
       "  test_promise_resolved_ = true;"
       "})");
  // Wait for Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Expect that returned Credential is not null.
  EXPECT_NSEQ(@"object", ExecuteJavaScript(@"typeof test_credential_"));
}

// Tests that Promise returned by |navigator.credentials.get| is rejected with
// TypeError if |mediation| value is invalid.
TEST_F(CredentialManagerTest, RejectOnInvalidMediationValue) {
  // Call API method |get| with invalid |mediation| field.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  password: true,"
       "  mediation: 'maybe'"
       "}).catch(function(reason) {"
       "  test_result_valid_type_ = (reason instanceof TypeError);"
       "  test_promise_rejected_ = true;"
       "})");

  // Wait for Promise to be rejected.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_rejected_") isEqual:@YES]);
  });
  // Check that Promise was rejected with TypeError.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_valid_type_"));
}

// Tests that Promise returned by |navigator.credentials.get| is rejected with
// TypeError if |providers| value is invalid.
TEST_F(CredentialManagerTest, RejectOnInvalidProvidersValue) {
  // Call API method |get| with invalid |providers| field.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  providers: 'https://exampleprovider.com' /* not a list */"
       "}).catch(function(reason) {"
       "  test_result_valid_type_ = (reason instanceof TypeError);"
       "  test_promise_rejected_ = true;"
       "})");

  // Wait for Promise to be rejected.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_rejected_") isEqual:@YES]);
  });
  // Check that Promise was rejected with TypeError.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_valid_type_"));
}

// Tests that Promise returned by |navigator.credentials.get| is rejected with
// NotSupportedError if password store is not available.
TEST_F(CredentialManagerTest, RejectOnPasswordStoreUnavailable) {
  // Make password store unavailable.
  client_->password_store()->ShutdownOnUIThread();
  client_->set_password_store(nullptr);

  // Call API method |get| with correct arguments, set up rejecter to store
  // reason for failure in |test_*| variables.
  ExecuteJavaScript(
      @"navigator.credentials.get({"
       "  password: true,"
       "}).catch(function(reason) {"
       "  test_result_valid_type_ = (reason instanceof DOMException) && "
       "      (reason.name == DOMException.NOT_SUPPORTED_ERR);"
       "  test_result_message_ = reason.message;"
       "  test_promise_rejected_ = true;"
       "})");

  // Wait for Promise to be rejected.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_rejected_") isEqual:@YES]);
  });

  // Check that Promise was rejected with NotSupportedError.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_valid_type_"));

  // Check that Promise message says "Password store is unavailable."
  EXPECT_NSEQ(@"Password store is unavailable.",
              ExecuteJavaScript(@"test_result_message_"));
}

// Test that calling |preventSilentAccess| from insecure context will not reach
// CredentialManagerImpl::PreventSilentAccess.
TEST_F(CredentialManagerTest, TryToPreventSilentAccessFromInsecureContext) {
  // Inject JavaScript, set up WebState to have non-cryptographic scheme.
  LoadHtml(@"<html></html>", GURL(kHttpWebOrigin));
  LoadHtmlAndInject(@"<html></html>");
  client_->set_current_url(GURL(kHttpWebOrigin));

  // Manually store a PasswordForm in password store.
  client_->password_store()->AddLogin(password_credential_form_1_);

  // Call API method |preventSilentAccess|.
  ExecuteJavaScript(
      @"navigator.credentials.preventSilentAccess().catch(function(reason) {"
       "  test_result_valid_type_ = (reason instanceof DOMException && "
       "      reason.name == DOMException.INVALID_STATE_ERR);"
       "  test_promise_rejected_ = true;"
       "});");

  // Wait for Promise to be rejected.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_rejected_") isEqual:@YES]);
  });
  // Check that Promise was rejected with InvalidStateError.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_valid_type_"));

  // Check that credential in password store was not affected by the call.
  WaitForBackgroundTasks();
  TestPasswordStore::PasswordMap passwords =
      client_->password_store()->stored_passwords();
  EXPECT_EQ(1u, passwords.size());
  ASSERT_EQ(1u, passwords[kHttpsWebOrigin].size());
  autofill::PasswordForm form = passwords[kHttpsWebOrigin][0];
  EXPECT_EQ(false, form.skip_zero_click);
}

// Tests that after |navigator.credentials.preventSilentAccess| is called, user
// will be prompted to choose credentials.
TEST_F(CredentialManagerTest, PreventSilentAccess) {
  // Manually store two PasswordForms in password store.
  password_credential_form_1_.skip_zero_click = false;
  password_credential_form_2_.skip_zero_click = false;
  client_->password_store()->AddLogin(password_credential_form_1_);
  client_->password_store()->AddLogin(password_credential_form_2_);

  // Call API method |preventSilentAccess|.
  ExecuteJavaScript(
      @"navigator.credentials.preventSilentAccess()"
       ".then(function(result) {"
       "  test_result_ = (result == undefined);"
       "  test_promise_resolved_ = true;"
       "});");

  // Wait for Promise to be resolved.
  WaitForCondition(^{
    return static_cast<bool>(
        [ExecuteJavaScript(@"test_promise_resolved_") isEqual:@YES]);
  });

  // Check that Promise was resolved with |undefined|.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"test_result_"));

  // Check that |preventSilentAccess| set a |skip_zero_click| flag on stored
  // credential.
  WaitForBackgroundTasks();
  TestPasswordStore::PasswordMap passwords =
      client_->password_store()->stored_passwords();
  std::vector<autofill::PasswordForm> forms = passwords[kHttpsWebOrigin];
  ASSERT_EQ(2u, forms.size());
  EXPECT_TRUE(forms[0].skip_zero_click);
  EXPECT_TRUE(forms[1].skip_zero_click);
}

class WebStateContentIsSecureHtmlTest : public CredentialManagerBaseTest {};

// Tests that HTTPS websites with valid SSL certificate are recognized as
// secure.
TEST_F(WebStateContentIsSecureHtmlTest, AcceptHttpsUrls) {
  LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
  UpdateSslStatus(net::CERT_STATUS_IS_EV, web::SECURITY_STYLE_AUTHENTICATED,
                  web::SSLStatus::NORMAL_CONTENT);
  EXPECT_TRUE(password_manager::WebStateContentIsSecureHtml(web_state()));
}

// Tests that WebStateContentIsSecureHtml returns false for HTTP origin.
TEST_F(WebStateContentIsSecureHtmlTest, HttpIsNotSecureContext) {
  LoadHtml(@"<html></html>", GURL(kHttpWebOrigin));
  EXPECT_FALSE(password_manager::WebStateContentIsSecureHtml(web_state()));
}

// Tests that WebStateContentIsSecureHtml returns false for HTTPS origin with
// valid SSL certificate but mixed contents.
TEST_F(WebStateContentIsSecureHtmlTest, InsecureContent) {
  LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
  UpdateSslStatus(net::CERT_STATUS_IS_EV, web::SECURITY_STYLE_AUTHENTICATED,
                  web::SSLStatus::DISPLAYED_INSECURE_CONTENT);
  EXPECT_FALSE(password_manager::WebStateContentIsSecureHtml(web_state()));
}

// Tests that WebStateContentIsSecureHtml returns false for HTTPS origin with
// invalid SSL certificate.
TEST_F(WebStateContentIsSecureHtmlTest, InvalidSslCertificate) {
  LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
  UpdateSslStatus(net::CERT_STATUS_INVALID, web::SECURITY_STYLE_UNAUTHENTICATED,
                  web::SSLStatus::NORMAL_CONTENT);
  EXPECT_FALSE(password_manager::WebStateContentIsSecureHtml(web_state()));
}

// Tests that data:// URI scheme is not accepted as secure context.
TEST_F(WebStateContentIsSecureHtmlTest, DataUriSchemeIsNotSecureContext) {
  LoadHtml(@"<html></html>", GURL(kDataUriSchemeOrigin));
  EXPECT_FALSE(password_manager::WebStateContentIsSecureHtml(web_state()));
}

// Tests that localhost is accepted as secure context.
TEST_F(WebStateContentIsSecureHtmlTest, LocalhostIsSecureContext) {
  LoadHtml(@"<html></html>", GURL(kLocalhostOrigin));
  EXPECT_TRUE(password_manager::WebStateContentIsSecureHtml(web_state()));
}

// Tests that file origin is accepted as secure context.
TEST_F(WebStateContentIsSecureHtmlTest, FileIsSecureContext) {
  LoadHtml(@"<html></html>", GURL(kFileOrigin));
  EXPECT_TRUE(password_manager::WebStateContentIsSecureHtml(web_state()));
}

// Tests that content must be HTML.
TEST_F(WebStateContentIsSecureHtmlTest, ContentMustBeHtml) {
  // No HTML is loaded on purpose, so that web_state()->ContentIsHTML() will
  // return false.
  EXPECT_FALSE(password_manager::WebStateContentIsSecureHtml(web_state()));
}
