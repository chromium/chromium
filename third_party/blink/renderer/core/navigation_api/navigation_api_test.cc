// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class NavigationApiTest : public testing::Test {
 public:
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }
};

class BeginNavigationClient : public frame_test_helpers::TestWebFrameClient {
 public:
  void BeginNavigation(
      std::unique_ptr<blink::WebNavigationInfo> info) override {
    begin_navigation_called_ = true;
  }

  bool BeginNavigationCalled() const { return begin_navigation_called_; }

 private:
  bool begin_navigation_called_ = false;
};

TEST_F(NavigationApiTest, NavigateEventCtrlClick) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL(
          "https://example.com/navigation-api/onnavigate-preventDefault.html"),
      test::CoreTestDataPath("navigation-api/onnavigate-preventDefault.html"));

  BeginNavigationClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      "https://example.com/navigation-api/onnavigate-preventDefault.html",
      &client);
  ASSERT_FALSE(client.BeginNavigationCalled());

  // Emulate a navigation as started by a ctrl+click.
  FrameLoadRequest request(nullptr, ResourceRequest(BlankURL()));
  request.SetNavigationPolicy(kNavigationPolicyNewBackgroundTab);
  web_view_helper.LocalMainFrame()->GetFrame()->Loader().StartNavigation(
      request);

  // The navigate event should not have fired for a ctrl+click.
  // If the navigate event handler was executed, the navigation will have been
  // cancelled, so check whether the begin navigation count was called.
  EXPECT_TRUE(client.BeginNavigationCalled());
}

TEST_F(NavigationApiTest, BrowserInitiatedSameDocumentBackForwardUncancelable) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL(
          "https://example.com/navigation-api/onnavigate-preventDefault.html"),
      test::CoreTestDataPath("navigation-api/onnavigate-preventDefault.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      "https://example.com/navigation-api/onnavigate-preventDefault.html");

  // Emulate a same-document back-forward navigation initiated by browser UI.
  // It should be uncancelable, even though the onnavigate handler will try.
  auto& frame_loader = web_view_helper.LocalMainFrame()->GetFrame()->Loader();
  HistoryItem* item = frame_loader.GetDocumentLoader()->GetHistoryItem();
  auto result = frame_loader.GetDocumentLoader()->CommitSameDocumentNavigation(
      item->Url(), WebFrameLoadType::kBackForward, item,
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true,
      /*soft_navigation_heuristics_task_id=*/absl::nullopt);

  EXPECT_EQ(result, mojom::blink::CommitResult::Ok);
}

TEST_F(NavigationApiTest, DispatchNavigateEventAfterPurgeMemory) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.com/foo.html"),
      test::CoreTestDataPath("foo.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("https://example.com/foo.html");
  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  frame->ForciblyPurgeV8Memory();

  KURL dest_url = url_test_helpers::ToKURL("https://example.com/foo.html#frag");
  // Should not crash.
  frame->DomWindow()->navigation()->DispatchNavigateEvent(
      MakeGarbageCollected<NavigateEventDispatchParams>(
          dest_url, NavigateEventType::kFragment, WebFrameLoadType::kStandard));
}

TEST_F(NavigationApiTest, UpdateForNavigationAfterPurgeMemory) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.com/foo.html"),
      test::CoreTestDataPath("foo.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("https://example.com/foo.html");
  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  frame->ForciblyPurgeV8Memory();

  HistoryItem* item = frame->Loader().GetDocumentLoader()->GetHistoryItem();
  // Should not crash.
  frame->DomWindow()->navigation()->UpdateForNavigation(
      *item, WebFrameLoadType::kStandard);
}

TEST_F(NavigationApiTest, InformAboutCanceledNavigationAfterPurgeMemory) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.com/foo.html"),
      test::CoreTestDataPath("foo.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("https://example.com/foo.html");

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  KURL dest_url = url_test_helpers::ToKURL("https://example.com/foo.html#frag");
  // DispatchNavigateEvent() will ensure NavigationApi::ongoing_navigate_event_
  // is non-null.
  frame->DomWindow()->navigation()->DispatchNavigateEvent(
      MakeGarbageCollected<NavigateEventDispatchParams>(
          dest_url, NavigateEventType::kFragment, WebFrameLoadType::kStandard));
  // Purging memory will invalidate the v8::Context then call
  // FrameLoader::StopAllLoaders(), which will in turn call
  // NavigationApi::InformAboutCanceledNavigation. InformAboutCanceledNavigation
  // shouldn't crash due to the invalid v8::Context.
  frame->ForciblyPurgeV8Memory();
}

}  // namespace blink
