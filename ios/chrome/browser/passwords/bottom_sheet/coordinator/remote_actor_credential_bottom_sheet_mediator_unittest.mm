// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

#import <memory>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_credential_sharing_delegate_bridge.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

constexpr char kTestOrigin[] = "https://instacart.com";

// Creates a test `PasswordForm`.
std::unique_ptr<password_manager::PasswordForm> CreateTestForm() {
  constexpr char kTestSignonRealm[] = "https://instacart.com/";
  constexpr char16_t kUsername[] = u"camille.c.walsh@gmail.com";
  constexpr char16_t kPassword[] = u"secret";
  auto form = std::make_unique<password_manager::PasswordForm>();
  form->url = GURL(kTestOrigin);
  form->signon_realm = kTestSignonRealm;
  form->username_value = kUsername;
  form->password_value = password_manager::PasswordString(kPassword);
  return form;
}

}  // namespace

// Tests for `RemoteActorCredentialBottomSheetMediator`.
class RemoteActorCredentialBottomSheetMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    delegate_ = OCMStrictProtocolMock(
        @protocol(RemoteActorCredentialBottomSheetMediatorDelegate));
    constexpr char kTestDomain[] = "instacart.com";
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
                  faviconLoader:nullptr
                        request:request];
    mediator_.delegate = delegate_;
  }

  void TearDown() override {
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

  base::test::TaskEnvironment task_environment_;
  id<RemoteActorCredentialBottomSheetMediatorDelegate> delegate_;
  RemoteActorCredentialBottomSheetMediator* mediator_;
};

// Tests that starting the mediator when required dependencies are unavailable
// notifies the delegate that the flow finished unsuccessfully.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestStartWithMissingDependenciesFails) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  OCMExpect([delegate_ mediator:mediator_ didFinishWithSuccess:NO])
      .andDo(^(NSInvocation*) {
        quit_closure.Run();
      });

  [mediator_ start];
  run_loop.Run();
}

// Tests that the mediator requests presentation when credentials are provided.
TEST_F(RemoteActorCredentialBottomSheetMediatorTest,
       TestRequestsPresentationWhenCredentialsProvided) {
  OCMExpect([delegate_ mediatorDidRequestPresentation:mediator_]);
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(CreateTestForm());

  [sharing_delegate()
      showCredentialSelectionWithCredentials:std::move(credentials)
                                      origin:url::Origin::Create(
                                                 GURL(kTestOrigin))
                                    callback:base::DoNothing()];
}
