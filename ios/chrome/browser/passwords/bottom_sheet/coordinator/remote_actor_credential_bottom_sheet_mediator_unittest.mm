// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_consumer.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_credential_sharing_delegate_bridge.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

using CredentialOrError = password_manager::
    RemoteActorCredentialSharingController::Delegate::CredentialOrError;
using password_manager::RemoteActorCredentialSharingResult;

constexpr char kTestOrigin[] = "https://instacart.com";
constexpr char kTestDomain[] = "instacart.com";
constexpr char16_t kUsername1[] = u"camille.c.walsh@gmail.com";
constexpr char16_t kPassword1[] = u"secret1";
constexpr char16_t kUsername2[] = u"camille.walsh@work.com";
constexpr char16_t kPassword2[] = u"secret2";

// Creates a `PasswordForm` for `username` and `password`.
std::unique_ptr<password_manager::PasswordForm> CreateTestForm(
    std::u16string username,
    std::u16string password) {
  constexpr char kTestSignonRealm[] = "https://instacart.com/";
  auto form = std::make_unique<password_manager::PasswordForm>();
  form->url = GURL(kTestOrigin);
  form->signon_realm = kTestSignonRealm;
  form->username_value = std::move(username);
  form->password_value = password_manager::PasswordString(std::move(password));
  return form;
}

}  // namespace

// Tests for `RemoteActorCredentialBottomSheetMediator`.
class RemoteActorCredentialBottomSheetMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    consumer_ = OCMStrictProtocolMock(
        @protocol(RemoteActorCredentialBottomSheetConsumer));
    delegate_ = OCMStrictProtocolMock(
        @protocol(RemoteActorCredentialBottomSheetMediatorDelegate));
    constexpr char kTestGaiaID[] = "gaia_id";
    constexpr char kTestTaskID[] = "task_id";
    remote_actor::proto::RemoteActorPasswordUploadRequest request;
    request.set_gaia_id(kTestGaiaID);
    request.set_domain(kTestDomain);
    request.set_task_id(kTestTaskID);
    mediator_ = [[RemoteActorCredentialBottomSheetMediator alloc]
        initWithIdentityManager:nullptr
                    syncService:nullptr
           profilePasswordStore:nullptr
           accountPasswordStore:nullptr
                 sharingService:nullptr
          passwordManagerClient:nullptr
                  faviconLoader:&favicon_loader_
                        request:request];
    mediator_.delegate = delegate_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(consumer_);
    EXPECT_OCMOCK_VERIFY(delegate_);
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  // Returns `mediator_` as its privately adopted
  // `RemoteActorCredentialSharingDelegate`, to simulate controller callbacks.
  id<RemoteActorCredentialSharingDelegate> sharing_delegate() {
    CHECK([mediator_
        conformsToProtocol:@protocol(RemoteActorCredentialSharingDelegate)]);
    return static_cast<id<RemoteActorCredentialSharingDelegate>>(mediator_);
  }

  // Forwards `credentials` to the mediator as if retrieved by the controller
  // and records the selection result in `selection_future_`.
  void ShowCredentialSelection(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          credentials) {
    [sharing_delegate()
        showCredentialSelectionWithCredentials:std::move(credentials)
                                        origin:url::Origin::Create(
                                                   GURL(kTestOrigin))
                                      callback:selection_future_.GetCallback()];
  }

  base::test::TaskEnvironment task_environment_;
  TestFaviconLoader favicon_loader_;
  id<RemoteActorCredentialBottomSheetConsumer> consumer_;
  id<RemoteActorCredentialBottomSheetMediatorDelegate> delegate_;
  RemoteActorCredentialBottomSheetMediator* mediator_;
  base::test::TestFuture<CredentialOrError> selection_future_;
};

// Tests that starting the mediator without a `PasswordManagerClient` notifies
// the delegate that the flow finished with `kOtherError`.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestStartWithoutPasswordManagerClientFails) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  OCMExpect(
      [delegate_ mediator:mediator_
          didFinishWithResult:RemoteActorCredentialSharingResult::kOtherError])
      .andDo(^(NSInvocation*) {
        quit_closure.Run();
      });

  [mediator_ start];
  run_loop.Run();
}

// Tests that the mediator exposes every provided credential as a suggestion
// along with the domain, and notifies the consumer.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestParsesCredentialsAndNotifiesConsumer) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));
  credentials.push_back(CreateTestForm(kUsername2, kPassword2));

  ShowCredentialSelection(std::move(credentials));

  EXPECT_FALSE(selection_future_.IsReady());

  OCMExpect([consumer_
      setSuggestions:[OCMArg checkWithBlock:^BOOL(
                                 NSArray<FormSuggestion*>* suggestions) {
        return suggestions.count == 2 &&
               [suggestions[0].value
                   isEqualToString:base::SysUTF16ToNSString(kUsername1)] &&
               [suggestions[1].value
                   isEqualToString:base::SysUTF16ToNSString(kUsername2)];
      }]
           andDomain:base::SysUTF8ToNSString(kTestDomain)]);
  OCMExpect([consumer_ setFaviconAttributes:[OCMArg isNotNil]]);

  mediator_.consumer = consumer_;
  EXPECT_NSEQ(consumer_, mediator_.consumer);
}

// Tests that a consumer set before credentials arrive is notified once the
// credentials are received.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestConsumerSetBeforeCredentialsIsNotifiedOnArrival) {
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setSuggestions:[OCMArg any] andDomain:[OCMArg any]]);
  OCMExpect([consumer_ setFaviconAttributes:[OCMArg isNotNil]]);

  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));
  [sharing_delegate()
      showCredentialSelectionWithCredentials:std::move(credentials)
                                      origin:url::Origin::Create(
                                                 GURL(kTestOrigin))
                                    callback:base::DoNothing()];
}

// Tests that an opaque origin falls back to the default globe favicon.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestOpaqueOriginUsesFallbackFavicon) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));

  [sharing_delegate()
      showCredentialSelectionWithCredentials:std::move(credentials)
                                      origin:url::Origin()
                                    callback:base::DoNothing()];

  OCMExpect([consumer_ setSuggestions:[OCMArg any] andDomain:[OCMArg any]]);
  OCMExpect([consumer_ setFaviconAttributes:[OCMArg isNotNil]]);

  mediator_.consumer = consumer_;
}

// Tests that selecting a credential by index returns the matching form to the
// controller.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestDidSelectCredentialAtIndexReturnsCredential) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));
  credentials.push_back(CreateTestForm(kUsername2, kPassword2));
  ShowCredentialSelection(std::move(credentials));

  [mediator_ didSelectCredentialAtIndex:1];

  const CredentialOrError& result = selection_future_.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kUsername2, result->username_value);
  EXPECT_EQ(kPassword2, result->password_value);
}

// Tests that canceling returns `kUserCancelledDialog` to the controller.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestDidCancelReportsCancellation) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));
  ShowCredentialSelection(std::move(credentials));

  [mediator_ didCancel];

  const CredentialOrError& result = selection_future_.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(RemoteActorCredentialSharingResult::kUserCancelledDialog,
            result.error());
}

// Tests that canceling when no selection callback is pending notifies the
// delegate that the flow finished with `kUserCancelledDialog`.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestDidCancelWithoutPendingCallbackNotifiesDelegate) {
  OCMExpect([delegate_ mediator:mediator_
            didFinishWithResult:RemoteActorCredentialSharingResult::
                                    kUserCancelledDialog]);

  [mediator_ didCancel];
}

// Tests that disconnecting while a selection is pending resolves the callback
// with `kUserCancelledDialog`.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestDisconnectReportsCancellation) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));
  ShowCredentialSelection(std::move(credentials));

  [mediator_ disconnect];

  const CredentialOrError& result = selection_future_.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(RemoteActorCredentialSharingResult::kUserCancelledDialog,
            result.error());
}
