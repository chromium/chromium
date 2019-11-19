// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/webauthn_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace protocol {

class WebAuthnHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    webauthn_handler_ = std::make_unique<WebAuthnHandler>();
  }

  void TearDown() override { webauthn_handler_.reset(); }

 protected:
  WebAuthnHandler* webauthn_handler() { return webauthn_handler_.get(); }

 private:
  std::unique_ptr<WebAuthnHandler> webauthn_handler_;
};

TEST_F(WebAuthnHandlerTest, EnableFailsGracefullyIfNoFrameHostSet) {
  Response response = webauthn_handler()->Enable();
  EXPECT_EQ(DispatchResponse::Status::kError, response.status());
  EXPECT_STREQ("The DevTools session is not attached to a frame",
               response.errorMessage().c_str());
}

TEST_F(WebAuthnHandlerTest, DisableGracefullyIfNoFrameHostSet) {
  Response response = webauthn_handler()->Disable();
  EXPECT_EQ(DispatchResponse::Status::kSuccess, response.status());
}

}  // namespace protocol
}  // namespace content
