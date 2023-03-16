// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/webauthn_handler.h"

#include "content/browser/devtools/protocol/web_authn.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::protocol {

class WebAuthnHandlerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    webauthn_handler_ = std::make_unique<WebAuthnHandler>();
  }

  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();
    webauthn_handler_.reset();
  }

 protected:
  std::unique_ptr<WebAuthnHandler> webauthn_handler_;
};

TEST_F(WebAuthnHandlerTest, EnableFailsGracefullyIfNoFrameHostSet) {
  Response response = webauthn_handler_->Enable(/*enable_ui=*/false);
  EXPECT_EQ(crdtp::DispatchCode::SERVER_ERROR, response.Code());
  EXPECT_EQ("The DevTools session is not attached to a frame",
            response.Message());
}

TEST_F(WebAuthnHandlerTest, DisableGracefullyIfNoFrameHostSet) {
  EXPECT_TRUE(webauthn_handler_->Disable().IsSuccess());
}

TEST_F(WebAuthnHandlerTest, DtorCleansUpObservers) {
  webauthn_handler_->SetRenderer(0, contents()->GetPrimaryMainFrame());
  Response response = webauthn_handler_->Enable(/*enable_ui=*/false);
  ASSERT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                     .SetTransport(WebAuthn::AuthenticatorTransportEnum::Usb)
                     .Build();
  String id;
  response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  ASSERT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  VirtualAuthenticatorManagerImpl* authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());

  VirtualAuthenticator* authenticator =
      authenticator_manager->GetAuthenticator(id);
  EXPECT_TRUE(authenticator->HasObserversForTest());

  webauthn_handler_.reset();
  EXPECT_FALSE(authenticator->HasObserversForTest());
}

}  // namespace content::protocol
