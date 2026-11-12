// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

#import <memory>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync/test/test_sync_service.h"
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

// Optional dependencies of the mediator's sharing controller. Unset ones are
// null, which makes the controller fail before querying any password store.
struct MediatorDependencies {
  std::string gaia_id = "gaia_id";
  raw_ptr<signin::IdentityManager> identity_manager = nullptr;
  raw_ptr<syncer::SyncService> sync_service = nullptr;
  raw_ptr<password_manager::PasswordStoreInterface> account_store = nullptr;
  raw_ptr<password_manager::PasswordManagerClient> client = nullptr;
  raw_ptr<password_manager::RemoteActorCredentialSharingService>
      sharing_service = nullptr;
};

// Sharing service that hands each share's completion callback to the test.
class FakeRemoteActorCredentialSharingService
    : public password_manager::RemoteActorCredentialSharingService {
 public:
  void SharePassword(const ShareParameters& params,
                     SharePasswordCallback callback) override {
    share_callback = std::move(callback);
  }

  SharePasswordCallback share_callback;
};

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
    mediator_ = CreateMediator(kTestDomain);
    account_store_->Init();
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(consumer_);
    EXPECT_OCMOCK_VERIFY(delegate_);
    [mediator_ disconnect];
    account_store_ = nullptr;
    PlatformTest::TearDown();
  }

  // Returns a mediator for a request targeting `domain` built with
  // `dependencies`.
  RemoteActorCredentialBottomSheetMediator* CreateMediator(
      std::string_view domain,
      const MediatorDependencies& dependencies = {}) {
    constexpr char kTestTaskID[] = "task_id";
    remote_actor::proto::RemoteActorPasswordUploadRequest request;
    request.set_gaia_id(dependencies.gaia_id);
    request.set_domain(std::string(domain));
    request.set_task_id(kTestTaskID);
    RemoteActorCredentialBottomSheetMediator* mediator =
        [[RemoteActorCredentialBottomSheetMediator alloc]
            initWithIdentityManager:dependencies.identity_manager
                        syncService:dependencies.sync_service
               profilePasswordStore:nullptr
               accountPasswordStore:dependencies.account_store
                     sharingService:dependencies.sharing_service
              passwordManagerClient:dependencies.client
                      faviconLoader:&favicon_loader_
                            request:request];
    mediator.delegate = delegate_;
    return mediator;
  }

  // Returns a mediator for a signed-in user with account storage active, whose
  // sharing controller queries the account store and shares the selected
  // credential through `sharing_service`, if any.
  RemoteActorCredentialBottomSheetMediator*
  CreateMediatorWithSignedInSyncingUser(
      password_manager::RemoteActorCredentialSharingService* sharing_service =
          nullptr) {
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
    return CreateMediator(
        kTestDomain, {.gaia_id = account.GetGaiaId().ToString(),
                      .identity_manager = identity_test_env_.identity_manager(),
                      .sync_service = &sync_service_,
                      .account_store = account_store_.get(),
                      .client = &password_manager_client_,
                      .sharing_service = sharing_service});
  }

  // Starts a mediator with a saved credential, selects it and completes the
  // share successfully, expecting the loading state and then the checkmark.
  void ShareCredentialAndWaitForConfirmation() {
    base::test::TestFuture<void> added_future;
    account_store_->AddLogin(password_manager::FromPasswordForm(
                                 *CreateTestForm(kUsername1, kPassword1)),
                             added_future.GetCallback());
    ASSERT_TRUE(added_future.Wait());
    mediator_ = CreateMediatorWithSignedInSyncingUser(&sharing_service_);

    OCMExpect([consumer_ setDomain:[OCMArg any]]);
    mediator_.consumer = consumer_;

    base::test::TestFuture<void> suggestions_future;
    base::RepeatingClosure suggestions_callback =
        suggestions_future.GetRepeatingCallback();
    OCMExpect([consumer_ setSuggestions:[OCMArg any]]).andDo(^(NSInvocation*) {
      suggestions_callback.Run();
    });
    OCMExpect([consumer_ setFaviconAttributes:[OCMArg any]]);
    [mediator_ start];
    ASSERT_TRUE(suggestions_future.Wait());

    OCMExpect([consumer_ setLoading:YES]);
    [mediator_ didSelectCredentialAtIndex:0];
    ASSERT_TRUE(sharing_service_.share_callback);
    EXPECT_OCMOCK_VERIFY(consumer_);

    OCMExpect([consumer_ showConfirmationState]);
    std::move(sharing_service_.share_callback).Run(/*success=*/true);
    EXPECT_OCMOCK_VERIFY(consumer_);
  }

  // Starts `mediator_` and waits until it shows the no saved passwords state.
  void StartAndWaitForNoSavedPasswords() {
    base::test::TestFuture<void> shown_future;
    base::RepeatingClosure shown_callback = shown_future.GetRepeatingCallback();
    OCMExpect([consumer_ showNoSavedPasswords]).andDo(^(NSInvocation*) {
      shown_callback.Run();
    });
    [mediator_ start];
    ASSERT_TRUE(shown_future.Wait());
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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  // Signed in with everything synced by default, so account storage is active.
  syncer::TestSyncService sync_service_;
  scoped_refptr<password_manager::TestPasswordStore> account_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore(true));
  password_manager::StubPasswordManagerClient password_manager_client_;
  FakeRemoteActorCredentialSharingService sharing_service_;
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
  base::test::TestFuture<void> finished_future;
  base::RepeatingClosure finished_callback =
      finished_future.GetRepeatingCallback();
  OCMExpect(
      [delegate_ mediator:mediator_
          didFinishWithResult:RemoteActorCredentialSharingResult::kOtherError])
      .andDo(^(NSInvocation*) {
        finished_callback.Run();
      });

  [mediator_ start];
  EXPECT_TRUE(finished_future.Wait());
}

// Tests that a successful share shows the checkmark in place of the spinner and
// only notifies the delegate once the checkmark has been shown.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestSharingSuccessShowsConfirmationBeforeFinishing) {
  ASSERT_NO_FATAL_FAILURE(ShareCredentialAndWaitForConfirmation());
  EXPECT_OCMOCK_VERIFY(delegate_);

  OCMExpect([delegate_ mediator:mediator_
            didFinishWithResult:RemoteActorCredentialSharingResult::kSuccess]);
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests that leaving the sheet while the checkmark is shown reports a
// cancellation, and that the pending success is not reported afterwards.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestDidCancelDuringConfirmationReportsCancellation) {
  ASSERT_NO_FATAL_FAILURE(ShareCredentialAndWaitForConfirmation());

  OCMExpect([delegate_ mediator:mediator_
            didFinishWithResult:RemoteActorCredentialSharingResult::
                                    kUserCancelledDialog]);
  [mediator_ didCancel];
  EXPECT_OCMOCK_VERIFY(delegate_);

  // The delegate is not notified a second time.
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests that disconnecting while the checkmark is shown reports the success
// instead of a cancellation.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestDisconnectDuringConfirmationReportsSuccess) {
  ASSERT_NO_FATAL_FAILURE(ShareCredentialAndWaitForConfirmation());

  OCMExpect([delegate_ mediator:mediator_
            didFinishWithResult:RemoteActorCredentialSharingResult::kSuccess]);
  [mediator_ disconnect];
}

// Tests that receiving the final sharing result stops the loading state.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestSharingResultStopsLoading) {
  OCMExpect([consumer_ setDomain:base::SysUTF8ToNSString(kTestDomain)]);
  mediator_.consumer = consumer_;

  base::test::TestFuture<void> finished_future;
  base::RepeatingClosure finished_callback =
      finished_future.GetRepeatingCallback();
  OCMExpect([consumer_ setLoading:NO]);
  OCMExpect(
      [delegate_ mediator:mediator_
          didFinishWithResult:RemoteActorCredentialSharingResult::kOtherError])
      .andDo(^(NSInvocation*) {
        finished_callback.Run();
      });

  [mediator_ start];
  EXPECT_TRUE(finished_future.Wait());
}

// Tests that the mediator sends the domain to the consumer as soon as it is
// set, before any credential is received.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestSetConsumerSendsDomain) {
  OCMExpect([consumer_ setDomain:base::SysUTF8ToNSString(kTestDomain)]);

  mediator_.consumer = consumer_;
}

// Tests that a request holding a full origin shows the host only, without a
// duplicated scheme.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestSetConsumerSendsHostForFullOrigin) {
  [mediator_ disconnect];
  mediator_ = CreateMediator(kTestOrigin);
  OCMExpect([consumer_ setDomain:base::SysUTF8ToNSString(kTestDomain)]);

  mediator_.consumer = consumer_;
}

// Tests that a request holding an HTTP origin with a non-default port shows
// the port.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestSetConsumerSendsPortForHttpOrigin) {
  [mediator_ disconnect];
  mediator_ = CreateMediator("http://instacart.com:8080");
  OCMExpect([consumer_ setDomain:@"instacart.com:8080"]);

  mediator_.consumer = consumer_;
}

// Tests that a request that does not hold a valid origin shows an empty domain.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestSetConsumerSendsEmptyDomainForInvalidOrigin) {
  [mediator_ disconnect];
  mediator_ = CreateMediator("https://");
  OCMExpect([consumer_ setDomain:@""]);

  mediator_.consumer = consumer_;
}

// Tests that the mediator exposes every provided credential as a suggestion,
// and notifies the consumer.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestParsesCredentialsAndNotifiesConsumer) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));
  credentials.push_back(CreateTestForm(kUsername2, kPassword2));

  ShowCredentialSelection(std::move(credentials));

  EXPECT_FALSE(selection_future_.IsReady());

  OCMExpect([consumer_ setDomain:base::SysUTF8ToNSString(kTestDomain)]);
  OCMExpect([consumer_
      setSuggestions:[OCMArg checkWithBlock:^BOOL(
                                 NSArray<FormSuggestion*>* suggestions) {
        return suggestions.count == 2 &&
               [suggestions[0].value
                   isEqualToString:base::SysUTF16ToNSString(kUsername1)] &&
               [suggestions[1].value
                   isEqualToString:base::SysUTF16ToNSString(kUsername2)];
      }]]);
  OCMExpect([consumer_ setFaviconAttributes:[OCMArg isNotNil]]);

  mediator_.consumer = consumer_;
  EXPECT_NSEQ(consumer_, mediator_.consumer);
}

// Tests that a consumer set before credentials arrive is notified once the
// credentials are received.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestConsumerSetBeforeCredentialsIsNotifiedOnArrival) {
  OCMExpect([consumer_ setDomain:[OCMArg any]]);
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setSuggestions:[OCMArg any]]);
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

  OCMExpect([consumer_ setDomain:[OCMArg any]]);
  OCMExpect([consumer_ setSuggestions:[OCMArg any]]);
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

// Tests that selecting a credential puts the consumer in the loading state.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestDidSelectCredentialShowsLoading) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm(kUsername1, kPassword1));
  ShowCredentialSelection(std::move(credentials));

  OCMExpect([consumer_ setDomain:[OCMArg any]]);
  OCMExpect([consumer_ setSuggestions:[OCMArg any]]);
  OCMExpect([consumer_ setFaviconAttributes:[OCMArg isNotNil]]);
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setLoading:YES]);
  [mediator_ didSelectCredentialAtIndex:0];

  EXPECT_TRUE(selection_future_.Wait());
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

// Tests that finding no saved passwords shows the no saved passwords state
// instead of finishing, and that leaving the sheet then reports
// `kNoPasswordsFound`.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestNoPasswordsFoundShowsNoSavedPasswords) {
  mediator_ = CreateMediatorWithSignedInSyncingUser();
  OCMExpect([consumer_ setDomain:base::SysUTF8ToNSString(kTestDomain)]);
  mediator_.consumer = consumer_;

  ASSERT_NO_FATAL_FAILURE(StartAndWaitForNoSavedPasswords());
  EXPECT_OCMOCK_VERIFY(delegate_);

  OCMExpect([delegate_ mediator:mediator_
            didFinishWithResult:RemoteActorCredentialSharingResult::
                                    kNoPasswordsFound]);
  [mediator_ didCancel];
}
