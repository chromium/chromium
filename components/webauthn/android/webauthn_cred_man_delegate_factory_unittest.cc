// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"

#include "base/memory/raw_ptr.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {

class WebAuthnCredManDelegateFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  WebAuthnCredManDelegateFactoryTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    factory_ = WebAuthnCredManDelegateFactory::GetFactory(web_contents());
    content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
        ->InitializeRenderFrameIfNeeded();
  }

  WebAuthnCredManDelegateFactory* factory() { return factory_; }

 private:
  raw_ptr<WebAuthnCredManDelegateFactory> factory_;
};

// Verify that a delegate created for a given RenderFrameHost is returned when
// the same RenderFrameHost is used a second time.
TEST_F(WebAuthnCredManDelegateFactoryTest, GetDelegateReturnsSameDelegate) {
  WebAuthnCredManDelegate* delegate =
      factory()->GetRequestDelegate(web_contents()->GetPrimaryMainFrame());
  EXPECT_NE(delegate, nullptr);
  EXPECT_EQ(delegate, factory()->GetRequestDelegate(
                          web_contents()->GetPrimaryMainFrame()));
}

// Verify that calling GetRequestDelegate with different RenderFrameHosts
// returns different delegates.
TEST_F(WebAuthnCredManDelegateFactoryTest,
       GetDelegateReturnsDifferentDelegate) {
  content::RenderFrameHost* child_frame =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("child_frame");
  content::RenderFrameHostTester::For(child_frame)
      ->InitializeRenderFrameIfNeeded();

  WebAuthnCredManDelegate* delegate =
      factory()->GetRequestDelegate(web_contents()->GetPrimaryMainFrame());
  EXPECT_NE(delegate, nullptr);
  EXPECT_NE(delegate, factory()->GetRequestDelegate(child_frame));
}

}  // namespace webauthn
