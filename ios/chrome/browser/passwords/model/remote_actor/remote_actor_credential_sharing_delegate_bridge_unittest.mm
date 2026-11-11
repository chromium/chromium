// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_credential_sharing_delegate_bridge.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

using CredentialOrError = password_manager::
    RemoteActorCredentialSharingController::Delegate::CredentialOrError;

constexpr char16_t kUsername[] = u"user@example.com";
constexpr char kOriginURL[] = "https://example.com";

}  // namespace

// Fake Objective-C delegate capturing calls forwarded by the bridge.
@interface FakeRemoteActorCredentialSharingDelegate
    : NSObject <RemoteActorCredentialSharingDelegate>

// Number of credentials received in
// `showCredentialSelectionWithCredentials:origin:callback:`.
@property(nonatomic, readonly) int credentialsCount;
// Username of the first credential received, if any.
@property(nonatomic, readonly) std::u16string firstUsername;
// Origin received in the most recent delegate call.
@property(nonatomic, readonly) url::Origin receivedOrigin;

// Runs the stored credential selection callback with `form`.
- (void)invokeCredentialCallbackWithForm:
    (const password_manager::PasswordForm&)form;

@end

@implementation FakeRemoteActorCredentialSharingDelegate {
  // Pending callback passed to
  // `showCredentialSelectionWithCredentials:origin:callback:`.
  RemoteActorCredentialSelectionCallback _credentialCallback;
}

- (void)
    showCredentialSelectionWithCredentials:
        (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
            credentials
                                    origin:(const url::Origin&)origin
                                  callback:
                                      (RemoteActorCredentialSelectionCallback)
                                          callback {
  _credentialsCount = static_cast<int>(credentials.size());
  if (!credentials.empty()) {
    _firstUsername = credentials[0]->username_value;
  }
  _receivedOrigin = origin;
  _credentialCallback = std::move(callback);
}

- (void)invokeCredentialCallbackWithForm:
    (const password_manager::PasswordForm&)form {
  if (_credentialCallback) {
    std::move(_credentialCallback).Run(form);
  }
}

@end

// Unit tests for `RemoteActorCredentialSharingDelegateBridge`.
class RemoteActorCredentialSharingDelegateBridgeTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    delegate_ = [[FakeRemoteActorCredentialSharingDelegate alloc] init];
    bridge_ =
        std::make_unique<RemoteActorCredentialSharingDelegateBridge>(delegate_);
  }

  FakeRemoteActorCredentialSharingDelegate* delegate_;
  std::unique_ptr<RemoteActorCredentialSharingDelegateBridge> bridge_;
};

// Tests that `ShowCredentialSelection` forwards credentials, origin, and
// callback to the Objective-C delegate.
TEST_F(RemoteActorCredentialSharingDelegateBridgeTest,
       TestShowCredentialSelectionForwardsToDelegate) {
  auto form = std::make_unique<password_manager::PasswordForm>();
  form->username_value = kUsername;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(std::move(form));

  url::Origin origin = url::Origin::Create(GURL(kOriginURL));
  base::test::TestFuture<CredentialOrError> future;

  bridge_->ShowCredentialSelection(std::move(credentials), origin,
                                   future.GetCallback());

  EXPECT_EQ(1, delegate_.credentialsCount);
  EXPECT_EQ(kUsername, delegate_.firstUsername);
  EXPECT_EQ(origin, delegate_.receivedOrigin);

  password_manager::PasswordForm selected_form;
  selected_form.username_value = kUsername;
  [delegate_ invokeCredentialCallbackWithForm:selected_form];

  ASSERT_TRUE(future.IsReady());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(kUsername, future.Get()->username_value);
}

// Tests that deallocating the Objective-C delegate clears the bridge's weak
// reference, so subsequent calls resolve the callback with an error.
TEST_F(RemoteActorCredentialSharingDelegateBridgeTest,
       TestDeallocatedDelegateResolvesCallbackWithError) {
  __weak FakeRemoteActorCredentialSharingDelegate* weak_delegate = delegate_;
  delegate_ = nil;
  ASSERT_FALSE(weak_delegate);

  url::Origin origin = url::Origin::Create(GURL(kOriginURL));
  base::test::TestFuture<CredentialOrError> future;
  bridge_->ShowCredentialSelection({}, origin, future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(password_manager::RemoteActorCredentialSharingResult::kOtherError,
            future.Get().error());
}
