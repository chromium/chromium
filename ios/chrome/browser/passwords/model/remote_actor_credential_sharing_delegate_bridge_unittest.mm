// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor_credential_sharing_delegate_bridge.h"

#import <memory>
#import <utility>
#import <vector>

#import "base/test/test_future.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

using CredentialOrError = password_manager::
    RemoteActorCredentialSharingController::Delegate::CredentialOrError;
using AuthResult = password_manager::RemoteActorCredentialSharingController::
    Delegate::AuthResult;

}  // namespace

// Fake Objective-C delegate capturing calls forwarded by the bridge.
@interface FakeRemoteActorCredentialSharingDelegate
    : NSObject <RemoteActorCredentialSharingDelegate>

// Number of credentials received in `showCredentialSelection:origin:callback:`.
@property(nonatomic, assign) size_t credentialsCount;
// Username of the first credential received, if any.
@property(nonatomic, assign) std::u16string firstUsername;
// Origin received in the most recent delegate call.
@property(nonatomic, assign) url::Origin receivedOrigin;
// Whether `showCredentialSelection:origin:callback:` was called.
@property(nonatomic, assign) BOOL showCredentialSelectionCalled;
// Whether `authenticateIfNeededForOrigin:callback:` was called.
@property(nonatomic, assign) BOOL authenticateIfNeededCalled;

// Runs the stored credential selection callback with `form`.
- (void)invokeCredentialCallbackWithForm:
    (const password_manager::PasswordForm&)form;

// Runs the stored authentication callback with success.
- (void)invokeAuthCallbackWithSuccess;

@end

@implementation FakeRemoteActorCredentialSharingDelegate {
  // Pending callback passed to `showCredentialSelection:origin:callback:`.
  base::OnceCallback<void(CredentialOrError)> _credentialCallback;

  // Pending callback passed to `authenticateIfNeededForOrigin:callback:`.
  base::OnceCallback<void(AuthResult)> _authCallback;
}

- (void)showCredentialSelection:
            (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
                credentials
                         origin:(const url::Origin&)origin
                       callback:(base::OnceCallback<void(CredentialOrError)>)
                                    callback {
  _showCredentialSelectionCalled = YES;
  _credentialsCount = credentials.size();
  if (!credentials.empty()) {
    _firstUsername = credentials[0]->username_value;
  }
  _receivedOrigin = origin;
  _credentialCallback = std::move(callback);
}

- (void)authenticateIfNeededForOrigin:(const url::Origin&)origin
                             callback:(base::OnceCallback<void(AuthResult)>)
                                          callback {
  _authenticateIfNeededCalled = YES;
  _receivedOrigin = origin;
  _authCallback = std::move(callback);
}

- (void)invokeCredentialCallbackWithForm:
    (const password_manager::PasswordForm&)form {
  if (_credentialCallback) {
    std::move(_credentialCallback).Run(form);
  }
}

- (void)invokeAuthCallbackWithSuccess {
  if (_authCallback) {
    std::move(_authCallback).Run(base::ok());
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
  form->username_value = u"user@example.com";
  form->password_value = password_manager::PasswordString(u"secret");
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(std::move(form));

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  base::test::TestFuture<CredentialOrError> future;

  bridge_->ShowCredentialSelection(std::move(credentials), origin,
                                   future.GetCallback());

  EXPECT_TRUE(delegate_.showCredentialSelectionCalled);
  EXPECT_EQ(1u, delegate_.credentialsCount);
  EXPECT_EQ(u"user@example.com", delegate_.firstUsername);
  EXPECT_EQ(origin, delegate_.receivedOrigin);

  password_manager::PasswordForm selected_form;
  selected_form.username_value = u"user@example.com";
  [delegate_ invokeCredentialCallbackWithForm:selected_form];

  ASSERT_TRUE(future.IsReady());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(u"user@example.com", future.Get()->username_value);
}

// Tests that `AuthenticateIfNeeded` forwards origin and callback to the
// Objective-C delegate.
TEST_F(RemoteActorCredentialSharingDelegateBridgeTest,
       TestAuthenticateIfNeededForwardsToDelegate) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  base::test::TestFuture<AuthResult> future;

  bridge_->AuthenticateIfNeeded(origin, future.GetCallback());

  EXPECT_TRUE(delegate_.authenticateIfNeededCalled);
  EXPECT_EQ(origin, delegate_.receivedOrigin);

  [delegate_ invokeAuthCallbackWithSuccess];

  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().has_value());
}

// Tests that when the Objective-C delegate is nil, callbacks are resolved with
// appropriate failure results rather than dropped.
TEST_F(RemoteActorCredentialSharingDelegateBridgeTest,
       TestNilDelegateResolvesCallbacksWithError) {
  RemoteActorCredentialSharingDelegateBridge nil_bridge(nil);
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  base::test::TestFuture<CredentialOrError> credential_future;
  nil_bridge.ShowCredentialSelection({}, origin,
                                     credential_future.GetCallback());
  ASSERT_TRUE(credential_future.IsReady());
  ASSERT_FALSE(credential_future.Get().has_value());
  EXPECT_EQ(password_manager::RemoteActorCredentialSharingResult::
                kUserCancelledDialog,
            credential_future.Get().error());

  base::test::TestFuture<AuthResult> auth_future;
  nil_bridge.AuthenticateIfNeeded(origin, auth_future.GetCallback());
  ASSERT_TRUE(auth_future.IsReady());
  ASSERT_FALSE(auth_future.Get().has_value());
  EXPECT_EQ(password_manager::RemoteActorCredentialSharingResult::
                kAuthenticatorFailed,
            auth_future.Get().error());
}
