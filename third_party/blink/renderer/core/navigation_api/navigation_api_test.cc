// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

// static
HistoryItem* MakeHistoryItemFor(const KURL& url, const String& key) {
  HistoryItem* item = MakeGarbageCollected<HistoryItem>();
  item->SetURL(url);
  item->SetDocumentSequenceNumber(1234);
  item->SetNavigationApiKey(key);
  // The |item| has a unique default item sequence number. Reusing an item
  // sequence number will suppress the naivgate event, so don't overwrite it.
  return item;
}

class NavigationApiTest : public testing::Test {
 public:
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }
  test::TaskEnvironment task_environment_;
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

TEST_F(NavigationApiTest, BrowserInitiatedSameDocumentBackForward) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL(
          "https://example.com/navigation-api/onnavigate-preventDefault.html"),
      test::CoreTestDataPath("navigation-api/onnavigate-preventDefault.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      "https://example.com/navigation-api/onnavigate-preventDefault.html");

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();
  const KURL& url = document_loader->Url();
  const String& key = document_loader->GetHistoryItem()->GetNavigationApiKey();

  // Emulate a same-document back-forward navigation initiated by browser UI.
  // It should be uncancelable, even though the onnavigate handler will try.
  auto result1 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, nullptr /* initiator_origin */,
      false /* is_synchronously_committed */, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      true /* is_browser_initiated */, /*has_ua_visual_transition,=*/false,
      std::nullopt);
  EXPECT_EQ(result1, mojom::blink::CommitResult::Ok);

  // Now that there's been a user activation, the onnavigate handler should be
  // able to cancel the navigation (which will consume the user activation).
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  auto result2 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, nullptr /* initiator_origin */,
      false /* is_synchronously_committed */, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      true /* is_browser_initiated */, /*has_ua_visual_transition,=*/false,
      std::nullopt);
  EXPECT_EQ(result2, mojom::blink::CommitResult::Aborted);

  // Having consumed the user activation, the onnavigate handler should not be
  // able to cancel the next navigation.
  auto result3 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, nullptr /* initiator_origin */,
      false /* is_synchronously_committed */, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      true /* is_browser_initiated */, /*has_ua_visual_transition,=*/false,
      std::nullopt);
  EXPECT_EQ(result3, mojom::blink::CommitResult::Ok);
}

TEST_F(NavigationApiTest, BrowserInitiatedSameDocumentBackForwardWindowStop) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL(
          "https://example.com/navigation-api/onnavigate-window-stop.html"),
      test::CoreTestDataPath("navigation-api/onnavigate-window-stop.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      "https://example.com/navigation-api/onnavigate-window-stop.html");

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();
  const KURL& url = document_loader->Url();
  const String& key = document_loader->GetHistoryItem()->GetNavigationApiKey();

  // Emulate a same-document back-forward navigation initiated by browser UI.
  // It should be uncancelable, even though the onnavigate handler will try.
  auto result1 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, nullptr /* initiator_origin */,
      false /* is_synchronously_committed */, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      true /* is_browser_initiated */, /*has_ua_visual_transition,=*/false,
      std::nullopt);
  EXPECT_EQ(result1, mojom::blink::CommitResult::Ok);

  // Now that there's been a user activation, the onnavigate handler should be
  // able to cancel the navigation (which will consume the user activation).
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  auto result2 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, nullptr /* initiator_origin */,
      false /* is_synchronously_committed */, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      true /* is_browser_initiated */, /*has_ua_visual_transition,=*/false,
      std::nullopt);
  EXPECT_EQ(result2, mojom::blink::CommitResult::Aborted);

  // Having consumed the user activation, the onnavigate handler should not be
  // able to cancel the next navigation.
  auto result3 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, nullptr /* initiator_origin */,
      false /* is_synchronously_committed */, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      true /* is_browser_initiated */, /*has_ua_visual_transition,=*/false,
      std::nullopt);
  EXPECT_EQ(result3, mojom::blink::CommitResult::Ok);
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
