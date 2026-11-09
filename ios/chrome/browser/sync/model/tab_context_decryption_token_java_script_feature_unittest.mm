// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/tab_context_decryption_token_java_script_feature.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/uuid.h"
#import "base/values.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync/base/features.h"
#import "components/sync_tab_context/container_id.h"
#import "components/sync_tab_context/fake_tab_context_sync_service.h"
#import "components/sync_tab_context/http_rpc_constants.h"
#import "components/sync_tab_context/tab_context_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/tab_context_sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

constexpr char kDisallowedUrl[] = "https://example.com/";
constexpr char kUserEmail[] = "user@gmail.com";
constexpr char kMismatchGaiaId[] = "mismatch_gaia_id";
NSString* const kInvalidUuid = @"not-a-valid-uuid";
NSString* const kTestHtml = @"<html><body></body></html>";
NSString* const kObfuscatedGaiaIdKey = @"obfuscatedGaiaId";
NSString* const kContainerIdKey = @"containerId";

// Builds a `sync_tab_context::FakeTabContextSyncService` for `profile`.
std::unique_ptr<KeyedService> BuildFakeTabContextSyncService(
    ProfileIOS* profile) {
  return std::make_unique<sync_tab_context::FakeTabContextSyncService>();
}

}  // namespace

class TabContextDecryptionTokenJavaScriptFeatureTest
    : public IOSChromeTestWithWebState {
 protected:
  void SetUp() override {
    IOSChromeTestWithWebState::SetUp();

    signin::IdentityManager* const identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    const AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, kUserEmail, signin::ConsentLevel::kSignin);
    primary_gaia_id_ = account_info.GetGaiaId().ToString();
  }

  // Configures `TabContextDecryptionTokenJavaScriptFeature` and loads
  // `kTestHtml` at `url` in `web_state()`.
  void LoadTestPage(const GURL& url) {
    web::test::OverrideJavaScriptFeatures(
        profile(), {TabContextDecryptionTokenJavaScriptFeature::GetInstance()});
    web::test::LoadHtml(kTestHtml, url, web_state());
  }

  TestProfileIOS::TestingFactories GetTestingFactories() const override {
    return {
        TestProfileIOS::TestingFactory{
            IdentityManagerFactory::GetInstance(),
            base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                    BuildIdentityManagerForTests)},
        TestProfileIOS::TestingFactory{
            TabContextSyncServiceFactory::GetInstance(),
            base::BindRepeating(&BuildFakeTabContextSyncService)},
    };
  }

  // Returns the `FakeTabContextSyncService` associated with `profile()`.
  sync_tab_context::FakeTabContextSyncService* GetService() {
    return static_cast<sync_tab_context::FakeTabContextSyncService*>(
        TabContextSyncServiceFactory::GetForProfile(profile()));
  }

  // Forwards `message` directly to the feature's
  // `ScriptMessageReceivedWithReply` handler.
  void SendScriptMessageWithReply(
      web::WebState* target_web_state,
      const web::ScriptMessage& message,
      base::OnceCallback<void(const base::Value*, NSString*)> callback) {
    TabContextDecryptionTokenJavaScriptFeature::GetInstance()
        ->ScriptMessageReceivedWithReply(target_web_state, message,
                                         std::move(callback));
  }

  // Sends a valid main-frame `ScriptMessage` for `gaia_id` and `container_id`
  // from the allowed origin to `target_web_state` and returns the decoded token
  // string, or `std::nullopt` if the reply was empty.
  std::optional<std::string> SendTokenRequestMessage(
      web::WebState* target_web_state,
      const std::string& gaia_id,
      NSString* container_id) {
    const GURL allowed_url(sync_tab_context::kDefaultTabContextAllowedOrigin);
    web::ScriptMessage message(
        web::ScriptMessageValue(@{
          kObfuscatedGaiaIdKey : base::SysUTF8ToNSString(gaia_id),
          kContainerIdKey : container_id,
        }),
        /*is_user_interacting=*/false,
        /*is_main_frame=*/true,
        /*request_url=*/allowed_url,
        /*security_origin=*/url::Origin::Create(allowed_url));

    bool reply_called = false;
    std::optional<std::string> decoded_token;
    SendScriptMessageWithReply(
        target_web_state, message,
        base::BindLambdaForTesting(
            [&reply_called, &decoded_token](const base::Value* reply,
                                            NSString* error) {
              reply_called = true;
              ASSERT_TRUE(reply);
              EXPECT_FALSE(error);
              if (reply->is_none()) {
                return;
              }
              ASSERT_TRUE(reply->is_list());
              std::string token;
              token.reserve(reply->GetList().size());
              for (const base::Value& byte_val : reply->GetList()) {
                ASSERT_TRUE(byte_val.is_int());
                token.push_back(static_cast<char>(byte_val.GetInt()));
              }
              decoded_token = std::move(token);
            }));
    EXPECT_TRUE(reply_called);
    return decoded_token;
  }

  // Invokes `chrome.getContainerDecryptionToken` in `target_web_state` and
  // returns the decoded token string, or `nil` if the callback received `null`.
  NSString* ExecuteGetContainerDecryptionToken(web::WebState* target_web_state,
                                               const std::string& gaia_id,
                                               NSString* container_id) {
    NSString* setup_script =
        [NSString stringWithFormat:
                      @"window.tokenDone = false;"
                       "window.tokenResult = null;"
                       "chrome.getContainerDecryptionToken(function(buffer) {"
                       "  if (buffer instanceof ArrayBuffer) {"
                       "    window.tokenResult = new TextDecoder().decode(new "
                       "Uint8Array(buffer));"
                       "  }"
                       "  window.tokenDone = true;"
                       "}, '%@', '%@');",
                      base::SysUTF8ToNSString(gaia_id), container_id];
    web::test::ExecuteJavaScriptForFeature(
        target_web_state, setup_script,
        TabContextDecryptionTokenJavaScriptFeature::GetInstance());

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          id done = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
              target_web_state, @"window.tokenDone;",
              TabContextDecryptionTokenJavaScriptFeature::GetInstance());
          return [done isEqual:@YES];
        }));

    id result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
        target_web_state, @"window.tokenResult;",
        TabContextDecryptionTokenJavaScriptFeature::GetInstance());
    if (!result || [result isKindOfClass:[NSNull class]]) {
      return nil;
    }
    return base::apple::ObjCCastStrict<NSString>(result);
  }

  base::test::ScopedFeatureList feature_list_{syncer::kSyncEncryptedTabContext};
  std::string primary_gaia_id_;
};

// Tests that `chrome.getContainerDecryptionToken` is not exposed on disallowed
// origins.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldNotExposeApiOnDisallowedOrigin) {
  LoadTestPage(GURL(kDisallowedUrl));

  id result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(), @"typeof window.chrome?.getContainerDecryptionToken;",
      TabContextDecryptionTokenJavaScriptFeature::GetInstance());
  EXPECT_NSEQ(@"undefined", result);
}

// Tests that `chrome.getContainerDecryptionToken` is exposed on the allowed
// origin and returns the token as an `ArrayBuffer` end-to-end when
// `TabContextSyncService` succeeds.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldExposeApiOnAllowedOriginAndReturnToken) {
  LoadTestPage(GURL(sync_tab_context::kDefaultTabContextAllowedOrigin));

  const sync_tab_context::ContainerId container_id =
      CHECK_DEREF(GetService()->CreateContainer());

  NSString* token = ExecuteGetContainerDecryptionToken(
      web_state(), primary_gaia_id_,
      base::SysUTF8ToNSString(container_id.value().AsLowercaseString()));
  ASSERT_TRUE(token);
  EXPECT_TRUE(
      GetService()->IsValidToken(container_id, base::SysNSStringToUTF8(token)));
}

// Tests that `ScriptMessageReceivedWithReply` replies with `null` when
// `containerId` is not a valid UUID.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldReturnNullOnInvalidUuid) {
  EXPECT_EQ(std::nullopt, SendTokenRequestMessage(web_state(), primary_gaia_id_,
                                                  kInvalidUuid));
}

// Tests that `ScriptMessageReceivedWithReply` replies with `null` when
// `TabContextSyncService` returns `std::nullopt`.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldReturnNullWhenServiceReturnsNullopt) {
  const base::Uuid unknown_container_uuid = base::Uuid::GenerateRandomV4();
  EXPECT_EQ(
      std::nullopt,
      SendTokenRequestMessage(
          web_state(), primary_gaia_id_,
          base::SysUTF8ToNSString(unknown_container_uuid.AsLowercaseString())));
}

// Tests that `ScriptMessageReceivedWithReply` replies with `null` when the Gaia
// ID does not match the primary account.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldReturnNullOnGaiaIdMismatch) {
  const sync_tab_context::ContainerId container_id =
      CHECK_DEREF(GetService()->CreateContainer());

  EXPECT_EQ(
      std::nullopt,
      SendTokenRequestMessage(
          web_state(), kMismatchGaiaId,
          base::SysUTF8ToNSString(container_id.value().AsLowercaseString())));
}

// Tests that `ScriptMessageReceivedWithReply` replies with `null` in an
// off-the-record profile.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldReturnNullInOffTheRecordProfile) {
  const sync_tab_context::ContainerId container_id =
      CHECK_DEREF(GetService()->CreateContainer());

  web::WebState::CreateParams otr_params(profile()->GetOffTheRecordProfile());
  std::unique_ptr<web::WebState> otr_web_state =
      web::WebState::Create(otr_params);

  EXPECT_EQ(
      std::nullopt,
      SendTokenRequestMessage(
          otr_web_state.get(), primary_gaia_id_,
          base::SysUTF8ToNSString(container_id.value().AsLowercaseString())));
}

// Tests that messages from subframes are rejected with an empty reply.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldIgnoreSubframeMessage) {
  const sync_tab_context::ContainerId container_id =
      CHECK_DEREF(GetService()->CreateContainer());

  const GURL allowed_url(sync_tab_context::kDefaultTabContextAllowedOrigin);
  web::ScriptMessage subframe_message(
      web::ScriptMessageValue(@{
        kObfuscatedGaiaIdKey : base::SysUTF8ToNSString(primary_gaia_id_),
        kContainerIdKey :
            base::SysUTF8ToNSString(container_id.value().AsLowercaseString()),
      }),
      /*is_user_interacting=*/false,
      /*is_main_frame=*/false,
      /*request_url=*/allowed_url,
      /*security_origin=*/url::Origin::Create(allowed_url));

  bool reply_called = false;
  SendScriptMessageWithReply(
      web_state(), subframe_message,
      base::BindLambdaForTesting(
          [&reply_called](const base::Value* reply, NSString* error) {
            reply_called = true;
            ASSERT_TRUE(reply);
            EXPECT_TRUE(reply->is_none());
            EXPECT_FALSE(error);
          }));
  EXPECT_TRUE(reply_called);
}

// Tests that messages from a disallowed origin are rejected with an empty
// reply.
TEST_F(TabContextDecryptionTokenJavaScriptFeatureTest,
       ShouldIgnoreDisallowedOriginMessage) {
  const sync_tab_context::ContainerId container_id =
      CHECK_DEREF(GetService()->CreateContainer());

  const GURL disallowed_url(kDisallowedUrl);
  web::ScriptMessage disallowed_origin_message(
      web::ScriptMessageValue(@{
        kObfuscatedGaiaIdKey : base::SysUTF8ToNSString(primary_gaia_id_),
        kContainerIdKey :
            base::SysUTF8ToNSString(container_id.value().AsLowercaseString()),
      }),
      /*is_user_interacting=*/false,
      /*is_main_frame=*/true,
      /*request_url=*/disallowed_url,
      /*security_origin=*/url::Origin::Create(disallowed_url));

  bool reply_called = false;
  SendScriptMessageWithReply(
      web_state(), disallowed_origin_message,
      base::BindLambdaForTesting(
          [&reply_called](const base::Value* reply, NSString* error) {
            reply_called = true;
            ASSERT_TRUE(reply);
            EXPECT_TRUE(reply->is_none());
            EXPECT_FALSE(error);
          }));
  EXPECT_TRUE(reply_called);
}
