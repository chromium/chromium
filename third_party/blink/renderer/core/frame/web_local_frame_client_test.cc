// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test makes assertions about the order of various callbacks in the (very
// large) WebLocalFrameClient interface.

#include "third_party/blink/public/web/web_local_frame_client.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using blink::url_test_helpers::ToKURL;

namespace blink {

namespace {

class CallTrackingTestWebLocalFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  void DidCreateDocumentLoader(WebDocumentLoader* loader) override {
    calls_.push_back("DidCreateDocumentLoader");
    TestWebFrameClient::DidCreateDocumentLoader(loader);
  }

  void DidCommitNavigation(
      WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker,
      const ParsedPermissionsPolicy& permissions_policy_header,
      const DocumentPolicyFeatureState& document_policy_header) override {
    calls_.push_back("DidCommitNavigation");
    TestWebFrameClient::DidCommitNavigation(
        commit_type, should_reset_browser_interface_broker,
        permissions_policy_header, document_policy_header);
  }

  void DidCreateDocumentElement() override {
    calls_.push_back("DidCreateDocumentElement");
    TestWebFrameClient::DidCreateDocumentElement();
  }

  void RunScriptsAtDocumentElementAvailable() override {
    calls_.push_back("RunScriptsAtDocumentElementAvailable");
    TestWebFrameClient::RunScriptsAtDocumentElementAvailable();
  }

  void DidDispatchDOMContentLoadedEvent() override {
    calls_.push_back("DidDispatchDOMContentLoadedEvent");
    TestWebFrameClient::DidDispatchDOMContentLoadedEvent();
  }

  void RunScriptsAtDocumentReady() override {
    calls_.push_back("RunScriptsAtDocumentReady");
    TestWebFrameClient::RunScriptsAtDocumentReady();
  }

  void RunScriptsAtDocumentIdle() override {
    calls_.push_back("RunScriptsAtDocumentIdle");
    TestWebFrameClient::RunScriptsAtDocumentIdle();
  }

  void DidHandleOnloadEvents() override {
    calls_.push_back("DidHandleOnloadEvents");
    TestWebFrameClient::DidHandleOnloadEvents();
  }

  void DidFinishLoad() override {
    calls_.push_back("DidFinishLoad");
    TestWebFrameClient::DidFinishLoad();
  }

  Vector<String> TakeCalls() { return std::exchange(calls_, {}); }

 private:
  Vector<String> calls_;
};

TEST(WebLocalFrameClientTest, Basic) {
  test::TaskEnvironment task_environment;
  CallTrackingTestWebLocalFrameClient client;
  frame_test_helpers::WebViewHelper web_view_helper;

  // Initialize() should populate the main frame with the initial empty document
  // and nothing more than that.
  web_view_helper.Initialize(&client);
  EXPECT_THAT(client.TakeCalls(),
              testing::ElementsAre("DidCreateDocumentLoader",
                                   "DidCreateDocumentElement",
                                   "RunScriptsAtDocumentElementAvailable"));

  frame_test_helpers::LoadHTMLString(web_view_helper.LocalMainFrame(),
                                     "<p>Hello world!</p>",
                                     ToKURL("https://example.com/"));
  EXPECT_THAT(client.TakeCalls(),
              testing::ElementsAre(
                  // TODO(https://crbug.com/1057229): RunScriptsAtDocumentIdle
                  // really should not be here, but there might be a bug where a
                  // truly empty initial document doesn't fire document_idle due
                  // to an early return in FrameLoader::FinishedParsing()...
                  "RunScriptsAtDocumentIdle", "DidCreateDocumentLoader",
                  "DidCommitNavigation", "DidCreateDocumentElement",
                  "RunScriptsAtDocumentElementAvailable",
                  "DidDispatchDOMContentLoadedEvent",
                  "RunScriptsAtDocumentReady", "RunScriptsAtDocumentIdle",
                  "DidHandleOnloadEvents", "DidFinishLoad"));
}

// TODO(dcheng): Add test cases for iframes (i.e. iframe with no source, iframe
// with explicit source of about:blank, et cetera)

// TODO(dcheng): Add Javascript URL tests too.

}  // namespace

}  // namespace blink
