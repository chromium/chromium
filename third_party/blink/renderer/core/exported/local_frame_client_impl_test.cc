/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/exported/local_frame_client_impl.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace blink {
namespace {

class LocalFrameMockWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  ~LocalFrameMockWebFrameClient() override = default;

  MOCK_METHOD0(UserAgentOverride, WebString());
};

class LocalFrameClientImplTest : public testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(web_frame_client_, UserAgentOverride())
        .WillByDefault(Return(WebString()));

    helper_.Initialize(&web_frame_client_);
  }

  void TearDown() override {
    // Tearing down the WebView by resetting the helper will call
    // UserAgentOverride() in order to store the information for detached
    // requests.
    EXPECT_CALL(WebLocalFrameClient(), UserAgentOverride());
    helper_.Reset();
  }

  WebString UserAgent() {
    // The test always returns the same user agent .
    std::string user_agent = GetLocalFrameClient().UserAgent().Utf8();
    return WebString::FromUTF8(user_agent.c_str(), user_agent.length());
  }

  WebLocalFrameImpl* MainFrame() { return helper_.LocalMainFrame(); }
  Document& GetDocument() { return *MainFrame()->GetFrame()->GetDocument(); }
  LocalFrameMockWebFrameClient& WebLocalFrameClient() {
    return web_frame_client_;
  }
  LocalFrameClient& GetLocalFrameClient() {
    return *ToLocalFrameClientImpl(MainFrame()->GetFrame()->Client());
  }

 private:
  LocalFrameMockWebFrameClient web_frame_client_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(LocalFrameClientImplTest, UserAgentOverride) {
  const WebString default_user_agent = UserAgent();
  const WebString override_user_agent = WebString::FromUTF8("dummy override");

  // Override the user agent and make sure we get it back.
  EXPECT_CALL(WebLocalFrameClient(), UserAgentOverride())
      .WillOnce(Return(override_user_agent));
  EXPECT_TRUE(override_user_agent.Equals(UserAgent()));
  Mock::VerifyAndClearExpectations(&WebLocalFrameClient());

  // Remove the override and make sure we get the original back.
  EXPECT_CALL(WebLocalFrameClient(), UserAgentOverride())
      .WillOnce(Return(WebString()));
  EXPECT_TRUE(default_user_agent.Equals(UserAgent()));
}

}  // namespace
}  // namespace blink
