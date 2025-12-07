// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/ios_webauthn_credentials_delegate_factory.h"

#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace webauthn {

constexpr std::string kFrameId1 = "frame1";
constexpr std::string kFrameId2 = "frame2";

class IOSWebAuthnCredentialsDelegateFactoryTest : public PlatformTest {
 protected:
  void SetUp() override {
    web_state_.SetWebFramesManager(
        content_world(), std::make_unique<web::FakeWebFramesManager>());

    factory_ = IOSWebAuthnCredentialsDelegateFactory::GetFactory(&web_state_);
    ASSERT_TRUE(factory_);
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
      factory()->GetDelegateForFrame(kFrameId1);

  EXPECT_TRUE(delegate);
}

// Tests that the factory returns the same delegate for the same frame.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest, SameDelegateForSameFrame) {
  IOSWebAuthnCredentialsDelegate* delegate1 =
      factory()->GetDelegateForFrame(kFrameId1);

  IOSWebAuthnCredentialsDelegate* delegate2 =
      factory()->GetDelegateForFrame(kFrameId1);

  EXPECT_EQ(delegate1, delegate2);
}

// Tests that the factory returns different delegates for different frames.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest,
       DifferentDelegateForDifferentFrame) {
  IOSWebAuthnCredentialsDelegate* delegate1 =
      factory()->GetDelegateForFrame(kFrameId1);

  IOSWebAuthnCredentialsDelegate* delegate2 =
      factory()->GetDelegateForFrame(kFrameId2);

  EXPECT_NE(delegate1, delegate2);
}

// Tests that the delegate is destroyed when the associated frame is removed.
TEST_F(IOSWebAuthnCredentialsDelegateFactoryTest, DelegateDestroyedWithFrame) {
  auto fake_frame = web::FakeWebFrame::Create(kFrameId1, /*is_main_frame=*/true,
                                              GURL::EmptyGURL());
  web_frames_manager()->AddWebFrame(std::move(fake_frame));

  // Getting the delegate should create it.
  IOSWebAuthnCredentialsDelegate* old_delegate =
      factory()->GetDelegateForFrame(kFrameId1);
  EXPECT_TRUE(old_delegate);

  // Removing the frame should destroy the delegate.
  web_frames_manager()->RemoveWebFrame(kFrameId1);

  // Create another delegate for a different frame to make it less likely
  // that the memory address of `old_delegate` is reused.
  factory()->GetDelegateForFrame(kFrameId2);

  // Getting the delegate again for the frame with ID `kFrameId1` should create
  // a new one.
  IOSWebAuthnCredentialsDelegate* new_delegate =
      factory()->GetDelegateForFrame(kFrameId1);
  EXPECT_TRUE(new_delegate);
  EXPECT_NE(old_delegate, new_delegate);
}

}  // namespace webauthn
