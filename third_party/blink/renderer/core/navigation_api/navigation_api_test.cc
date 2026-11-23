// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

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
  FrameLoadRequest request(nullptr, ResourceRequest(BlankUrl()));
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
      std::nullopt, false /* should_skip_screenshot */);
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
      std::nullopt, false /* should_skip_screenshot */);
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
      std::nullopt, false /* should_skip_screenshot */);
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
      std::nullopt, false /* should_skip_screenshot */);
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
      std::nullopt, false /* should_skip_screenshot */);
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
      std::nullopt, false /* should_skip_screenshot */);
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

TEST_F(
    NavigationApiTest,
    BrowserInitiatedSameDocumentBackForwardDoesNotAutofocusCapabilityElement) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.com/foo.html"),
      test::CoreTestDataPath("foo.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("https://example.com/foo.html");

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  Document* document = frame->GetDocument();
  document->body()->SetInnerHTMLWithoutTrustedTypes(
      "<geolocation id='geo' autofocus></geolocation>");
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  document->View()->UpdateAllLifecyclePhasesForTest();

  ClassicScript::CreateUnspecifiedScript(
      "navigation.addEventListener('navigate', e => e.intercept());")
      ->RunScript(frame->DomWindow());

  Element* geo = document->getElementById(AtomicString("geo"));
  ASSERT_TRUE(geo);
  EXPECT_EQ(document->FocusedElement(), nullptr);

  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();
  const KURL& url = document_loader->Url();
  const String& key = document_loader->GetHistoryItem()->GetNavigationApiKey();

  // Emulate a browser-initiated same-document back-forward navigation without
  // transient user activation (UserNavigationInvolvement::kBrowserUI). Even
  // though NavigateEvent.userInitiated is true, intercepting the navigation
  // must not focus the autofocused capability element without user activation.
  auto result1 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true, /*has_ua_visual_transition=*/false,
      std::nullopt, /*should_skip_screenshot=*/false);
  EXPECT_EQ(result1, mojom::blink::CommitResult::Ok);
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document->FocusedElement(), nullptr);

  // When initiated by user activation (UserNavigationInvolvement::kActivation),
  // intercepting the navigation should focus the autofocused capability
  // element.
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kTest);
  auto result2 = document_loader->CommitSameDocumentNavigation(
      url, WebFrameLoadType::kBackForward, MakeHistoryItemFor(url, key),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/true, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kFromTrustedEvent,
      /*is_browser_initiated=*/false, /*has_ua_visual_transition=*/false,
      std::nullopt, /*should_skip_screenshot=*/false);
  EXPECT_EQ(result2, mojom::blink::CommitResult::Ok);
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document->FocusedElement(), geo);
}

TEST_F(NavigationApiTest,
       InterceptedSameDocumentNavigationPreservesInitiatorUserActivation) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.com/foo.html"),
      test::CoreTestDataPath("foo.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("https://example.com/foo.html");

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  Document* document = frame->GetDocument();
  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();

  ClassicScript::CreateUnspecifiedScript(
      "navigation.addEventListener('navigate', e => e.intercept());")
      ->RunScript(frame->DomWindow());

  // Grant transient user activation to the target frame, then commit a
  // cross-origin-initiated same-document navigation that started WITHOUT user
  // activation. The committed navigation must not inherit the target frame's
  // transient user activation.
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kTest);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(frame));

  scoped_refptr<const SecurityOrigin> cross_origin_initiator =
      SecurityOrigin::CreateFromString("https://attacker.example.com");
  KURL bar1_url = url_test_helpers::ToKURL("https://example.com/foo.html#bar1");
  auto result1 = document_loader->CommitSameDocumentNavigation(
      bar1_url, WebFrameLoadType::kStandard, /*history_item=*/nullptr,
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, cross_origin_initiator.get(),
      /*is_synchronously_committed=*/true, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/false, /*has_ua_visual_transition=*/false,
      /*task_state_id=*/std::nullopt, /*should_skip_screenshot=*/false);
  EXPECT_EQ(result1, mojom::blink::CommitResult::Ok);
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document_loader->Url(), bar1_url);
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(frame));
  EXPECT_FALSE(document_loader->LastNavigationHadTransientUserActivation());
  EXPECT_FALSE(document_loader->LastNavigationHadTrustedInitiator());

  // Conversely, when the target frame has NO transient user activation, a
  // cross-origin-initiated same-document navigation that started WITH user
  // activation must preserve `has_transient_user_activation = true`.
  LocalFrame::ConsumeTransientUserActivation(frame);
  ASSERT_FALSE(LocalFrame::HasTransientUserActivation(frame));

  KURL bar2_url = url_test_helpers::ToKURL("https://example.com/foo.html#bar2");
  auto result2 = document_loader->CommitSameDocumentNavigation(
      bar2_url, WebFrameLoadType::kStandard, /*history_item=*/nullptr,
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/true, cross_origin_initiator.get(),
      /*is_synchronously_committed=*/true, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/false, /*has_ua_visual_transition=*/false,
      /*task_state_id=*/std::nullopt, /*should_skip_screenshot=*/false);
  EXPECT_EQ(result2, mojom::blink::CommitResult::Ok);
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document_loader->Url(), bar2_url);
  EXPECT_TRUE(document_loader->LastNavigationHadTransientUserActivation());
}

TEST_F(NavigationApiTest,
       InterceptedDeferredPrecommitNavigationPreservesInitialUserActivation) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.com/foo.html"),
      test::CoreTestDataPath("foo.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("https://example.com/foo.html");

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  Document* document = frame->GetDocument();
  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();

  ClassicScript::CreateUnspecifiedScript(R"(
    navigation.addEventListener('navigate', e => {
      e.intercept({
        precommitHandler() {
          return new Promise(resolve => {
            window.resolvePrecommit = resolve;
          });
        }
      });
    });
  )")
      ->RunScript(frame->DomWindow());

  ASSERT_FALSE(LocalFrame::HasTransientUserActivation(frame));

  scoped_refptr<const SecurityOrigin> cross_origin_initiator =
      SecurityOrigin::CreateFromString("https://attacker.example.com");
  KURL deferred_url =
      url_test_helpers::ToKURL("https://example.com/foo.html#deferred");
  auto result = document_loader->CommitSameDocumentNavigation(
      deferred_url, WebFrameLoadType::kStandard, /*history_item=*/nullptr,
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, cross_origin_initiator.get(),
      /*is_synchronously_committed=*/true, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/false, /*has_ua_visual_transition=*/false,
      /*task_state_id=*/std::nullopt, /*should_skip_screenshot=*/false);
  EXPECT_EQ(result, mojom::blink::CommitResult::Ok);
  // Precommit handler is still pending, so the URL has not yet updated.
  EXPECT_EQ(document_loader->Url(),
            url_test_helpers::ToKURL("https://example.com/foo.html"));

  // Grant transient user activation to the target frame while precommitHandler
  // is pending before CommitNow() runs.
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kTest);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(frame));

  ClassicScript::CreateUnspecifiedScript("window.resolvePrecommit();")
      ->RunScript(frame->DomWindow());
  document->GetAgent().PerformMicrotaskCheckpoint();

  EXPECT_EQ(document_loader->Url(), deferred_url);
  EXPECT_FALSE(document_loader->LastNavigationHadTransientUserActivation());
}

TEST_F(NavigationApiTest,
       InterceptedCrossDocumentAndHistoryApiPreserveUserActivation) {
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.com/foo.html"),
      test::CoreTestDataPath("foo.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("https://example.com/foo.html");

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  Document* document = frame->GetDocument();
  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();

  ClassicScript::CreateUnspecifiedScript(
      "navigation.addEventListener('navigate', e => e.intercept());")
      ->RunScript(frame->DomWindow());

  // 1. Intercepted same-origin cross-document FrameLoadRequest with and without
  // transient user activation.
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kTest);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(frame));

  KURL next1_url = url_test_helpers::ToKURL("https://example.com/next1.html");
  FrameLoadRequest request_with_gesture(frame->DomWindow(),
                                        ResourceRequest(next1_url));
  frame->Loader().StartNavigation(request_with_gesture);
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document_loader->Url(), next1_url);
  EXPECT_TRUE(document_loader->LastNavigationHadTransientUserActivation());

  LocalFrame::ConsumeTransientUserActivation(frame);
  ASSERT_FALSE(LocalFrame::HasTransientUserActivation(frame));

  KURL next2_url = url_test_helpers::ToKURL("https://example.com/next2.html");
  FrameLoadRequest request_without_gesture(frame->DomWindow(),
                                           ResourceRequest(next2_url));
  frame->Loader().StartNavigation(request_without_gesture);
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document_loader->Url(), next2_url);
  EXPECT_FALSE(document_loader->LastNavigationHadTransientUserActivation());

  // 2. Intercepted history.pushState() with and without transient user
  // activation.
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kTest);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(frame));

  ClassicScript::CreateUnspecifiedScript(
      "history.pushState(null, '', '/pushed1.html');")
      ->RunScript(frame->DomWindow());
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document_loader->Url(),
            url_test_helpers::ToKURL("https://example.com/pushed1.html"));
  EXPECT_TRUE(document_loader->LastNavigationHadTransientUserActivation());

  LocalFrame::ConsumeTransientUserActivation(frame);
  ASSERT_FALSE(LocalFrame::HasTransientUserActivation(frame));

  ClassicScript::CreateUnspecifiedScript(
      "history.pushState(null, '', '/pushed2.html');")
      ->RunScript(frame->DomWindow());
  document->GetAgent().PerformMicrotaskCheckpoint();
  EXPECT_EQ(document_loader->Url(),
            url_test_helpers::ToKURL("https://example.com/pushed2.html"));
  EXPECT_FALSE(document_loader->LastNavigationHadTransientUserActivation());
}

}  // namespace blink
