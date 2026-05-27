// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_webauthn_credentials_delegate_factory.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/test/test_future.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace webauthn {

constexpr char kFrameId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kFrameId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr char kRemoteFrameId1[] = "cccccccccccccccccccccccccccccccc";
constexpr char kRemoteFrameId2[] = "dddddddddddddddddddddddddddddddd";
constexpr char kRemoteFrameId1New[] = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";

class IOSWebAuthnCredentialsDelegateFactoryTest : public PlatformTest {
 protected:
  void SetUp() override {
    web_state_.SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());

    autofill::ChildFrameRegistrar::GetOrCreateForWebState(&web_state_);
    RegisterFrame(kFrameId1, kRemoteFrameId1);
    RegisterFrame(kFrameId2, kRemoteFrameId2);

    factory_ = IOSWebAuthnCredentialsDelegateFactory::GetFactory(&web_state_);
    ASSERT_TRUE(factory_);
  }

  void RegisterFrame(const std::string& local_frame_id,
                     const std::string& remote_frame_id) {
    autofill::LocalFrameToken local_token(
        *autofill::DeserializeJavaScriptFrameId(local_frame_id));
    autofill::RemoteFrameToken remote_token(
        *autofill::DeserializeJavaScriptFrameId(remote_frame_id));
    autofill::ChildFrameRegistrar::FromWebState(&web_state_)
        ->RegisterMapping(remote_token, local_token);
  }

  web::FakeWebFramesManager* web_frames_manager() {
    return static_cast<web::FakeWebFramesManager*>(
        web_state_.GetWebFramesManager(content_world()));
  }

  IOSWebAuthnCredentialsDelegateFactory* factory() { return factory_; }

  web::ContentWorld content_world() {
    return autofill::AutofillJavaScriptFeature::GetInstance()
        ->GetSupportedContentWorld();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  raw_ptr<IOSWebAuthnCredentialsDelegateFactory> factory_;
};

// Tests that the factory creates a delegate for a given frame.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest, FactoryCreatesDelegate) {
  IOSWebAuthnCredentialsDelegate* delegate =
      factory()->GetDelegateForFrameId(kFrameId1);

  EXPECT_TRUE(delegate);
}

// Tests that the factory doesn't create a delegate for an empty frame ID.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest,
       ReturnsNullptrForEmptyFrameId) {
  IOSWebAuthnCredentialsDelegate* delegate =
      factory()->GetDelegateForFrameId("");

  EXPECT_FALSE(delegate);
}

// Tests that the factory returns the same delegate for the same frame.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest, SameDelegateForSameFrame) {
  IOSWebAuthnCredentialsDelegate* delegate1 =
      factory()->GetDelegateForFrameId(kFrameId1);

  IOSWebAuthnCredentialsDelegate* delegate2 =
      factory()->GetDelegateForFrameId(kFrameId1);

  EXPECT_EQ(delegate1, delegate2);
}

// Tests that the factory returns different delegates for different frames.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest,
       DifferentDelegateForDifferentFrame) {
  IOSWebAuthnCredentialsDelegate* delegate1 =
      factory()->GetDelegateForFrameId(kFrameId1);

  IOSWebAuthnCredentialsDelegate* delegate2 =
      factory()->GetDelegateForFrameId(kFrameId2);

  EXPECT_NE(delegate1, delegate2);
}

// Tests that the delegate is destroyed when the associated frame is removed.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest, DelegateDestroyedWithFrame) {
  auto fake_frame = web::FakeWebFrame::Create(kFrameId1, /*is_main_frame=*/true,
                                              GURL::EmptyGURL());
  web_frames_manager()->AddWebFrame(std::move(fake_frame));

  // Getting the delegate should create it.
  IOSWebAuthnCredentialsDelegate* old_delegate =
      factory()->GetDelegateForFrameId(kFrameId1);
  EXPECT_TRUE(old_delegate);

  // Removing the frame should destroy the delegate.
  web_frames_manager()->RemoveWebFrame(kFrameId1);

  // Create another delegate for a different frame to make it less likely
  // that the memory address of `old_delegate` is reused.
  factory()->GetDelegateForFrameId(kFrameId2);

  // Re-register the mapping for local frame token with a new remote frame token
  // to simulate a new frame being registered.
  RegisterFrame(kFrameId1, kRemoteFrameId1New);

  // Getting the delegate again for the frame with ID `kFrameId1` should create
  // a new one.
  IOSWebAuthnCredentialsDelegate* new_delegate =
      factory()->GetDelegateForFrameId(kFrameId1);
  EXPECT_TRUE(new_delegate);
  EXPECT_NE(old_delegate, new_delegate);
}

// Tests that the factory returns the same delegate for a remote frame token.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest, DelegateForRemoteFrameToken) {
  autofill::RemoteFrameToken remote_token(
      *autofill::DeserializeJavaScriptFrameId(kRemoteFrameId1));

  base::test::TestFuture<IOSWebAuthnCredentialsDelegate*> future;
  factory()->GetDelegateForRemoteFrameToken(remote_token, future.GetCallback());
  IOSWebAuthnCredentialsDelegate* delegate_from_remote = future.Get();
  EXPECT_TRUE(delegate_from_remote);

  IOSWebAuthnCredentialsDelegate* delegate_from_local =
      factory()->GetDelegateForFrameId(kFrameId1);
  EXPECT_EQ(delegate_from_remote, delegate_from_local);
}

// Tests that the factory returns nullptr for empty/missing remote frame token.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest,
       ReturnsNullptrForMissingRemoteFrameToken) {
  base::test::TestFuture<IOSWebAuthnCredentialsDelegate*> future;
  factory()->GetDelegateForRemoteFrameToken(autofill::RemoteFrameToken(),
                                            future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get());
}

}  // namespace webauthn
