// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_loader.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/chrome_client_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {

class FrameLoaderSimTest : public SimTest {
 public:
  FrameLoaderSimTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }
};

// Ensure that the load event progress is progressed through BeforeUnload only
// if the event is uncanceled.
TEST_F(FrameLoaderSimTest, LoadEventProgressBeforeUnloadCanceled) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest request_a("https://example.com/subframe-a.html", "text/html");
  SimRequest request_b("https://example.com/subframe-b.html", "text/html");
  SimRequest request_c("https://example.com/subframe-c.html", "text/html");
  SimRequest request_unload("https://example.com/next-page.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe src="subframe-a.html"></iframe>
  )HTML");

  request_a.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe src="subframe-b.html"></iframe>
      <a id="link" href="next-page.html">Next Page</a>
  )HTML");
  request_b.Complete(R"HTML(
      <!DOCTYPE html>
      <script>
        window.onbeforeunload = (e) => {
          e.returnValue = '';
          e.preventDefault();
        };
      </script>
      <iframe src="subframe-c.html"></iframe>
  )HTML");
  request_c.Complete(R"HTML(
      <!DOCTYPE html>
  )HTML");
  Compositor().BeginFrame();

  auto* main_frame = To<LocalFrame>(GetDocument().GetPage()->MainFrame());
  auto* frame_a = To<LocalFrame>(main_frame->Tree().FirstChild());
  auto* frame_b = To<LocalFrame>(frame_a->Tree().FirstChild());
  auto* frame_c = To<LocalFrame>(frame_b->Tree().FirstChild());

  ASSERT_FALSE(main_frame->GetDocument()->BeforeUnloadStarted());
  ASSERT_FALSE(frame_a->GetDocument()->BeforeUnloadStarted());
  ASSERT_FALSE(frame_b->GetDocument()->BeforeUnloadStarted());
  ASSERT_FALSE(frame_c->GetDocument()->BeforeUnloadStarted());

  // We'll only allow canceling a beforeunload if there's a sticky user
  // activation present so simulate a user gesture.
  LocalFrame::NotifyUserActivation(
      frame_b, mojom::UserActivationNotificationType::kTest);

  auto& chrome_client =
      To<ChromeClientImpl>(WebView().GetPage()->GetChromeClient());

  // Simulate the user canceling the navigation away. Since the navigation was
  // "canceled", we expect that each of the frames should remain in their state
  // before the beforeunload was dispatched.
  {
    chrome_client.SetBeforeUnloadConfirmPanelResultForTesting(false);

    // Note: We can't perform a navigation to check this because the
    // beforeunload event is dispatched from content's RenderFrameImpl, Blink
    // tests mock this out using a WebFrameTestProxy which doesn't check
    // beforeunload before navigating.
    base::TimeTicks before_unload_dialog_opened_time;
    base::TimeTicks before_unload_dialog_closed_time;
    ASSERT_FALSE(frame_a->Loader().ShouldClose(
        /*is_reload=*/false, /*force_to_proceed=*/false,
        before_unload_dialog_opened_time, before_unload_dialog_closed_time));

    EXPECT_FALSE(main_frame->GetDocument()->BeforeUnloadStarted());
    EXPECT_FALSE(frame_a->GetDocument()->BeforeUnloadStarted());
    EXPECT_FALSE(frame_b->GetDocument()->BeforeUnloadStarted());
    EXPECT_FALSE(frame_c->GetDocument()->BeforeUnloadStarted());
  }

  // Now test the opposite, the user allowing the navigation away.
  {
    chrome_client.SetBeforeUnloadConfirmPanelResultForTesting(true);
    base::TimeTicks before_unload_dialog_opened_time;
    base::TimeTicks before_unload_dialog_closed_time;
    ASSERT_TRUE(frame_a->Loader().ShouldClose(
        /*is_reload=*/false, /*force_to_proceed=*/false,
        before_unload_dialog_opened_time, before_unload_dialog_closed_time));

    // The navigation was in frame a so it shouldn't affect the parent.
    EXPECT_FALSE(main_frame->GetDocument()->BeforeUnloadStarted());
    EXPECT_TRUE(frame_a->GetDocument()->BeforeUnloadStarted());
    EXPECT_TRUE(frame_b->GetDocument()->BeforeUnloadStarted());
    EXPECT_TRUE(frame_c->GetDocument()->BeforeUnloadStarted());
  }
}

class FrameLoaderJavaScriptUrlWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    // The initial page is not loaded via a call to `BeginNavigation()`, and
    // javascript: URLs should always be handled internally in Blink, so this
    // should never be reached in tests.
    ASSERT_TRUE(false);
  }
};

class FrameLoaderJavaScriptUrlTest : public SimTest {
  std::unique_ptr<frame_test_helpers::TestWebFrameClient>
  CreateWebFrameClientForMainFrame() override {
    return std::make_unique<FrameLoaderJavaScriptUrlWebFrameClient>();
  }
};

// This is mostly a differential test, to verify that JavaScriptUrlTargetBlank
// and CtrlClickJavaScriptUrlTargetBlank don't unexpectedly pass. That is, if
// this test starts failing, any pass results for the aforementioned tests
// should be considered highly suspicious.
TEST_F(FrameLoaderJavaScriptUrlTest, Click) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <html><body>
      <a href="javascript:'moo'"></a>
      </body></html>
  )HTML");

  // Generate and dispatch a click event.
  MouseEventInit* mouse_initializer = MouseEventInit::Create();
  mouse_initializer->setView(&Window());
  mouse_initializer->setButton(1);

  Event* event =
      MouseEvent::Create(nullptr, event_type_names::kClick, mouse_initializer);
  Element* anchor = GetDocument().QuerySelector(AtomicString("a"));
  anchor->DispatchSimulatedClick(event);

  // Navigations to JavaScript URLs should queue a task:
  // https://whatwg.org/c/browsing-the-web.html#beginning-navigation:navigate-to-a-javascript:-url
  EXPECT_TRUE(GetDocument().HasPendingJavaScriptUrlsForTest());

  base::RunLoop run_loop;
  GetDocument()
      .GetFrame()
      ->GetTaskRunner(TaskType::kNetworking)
      ->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ("moo", GetDocument().documentElement()->innerText());
}

// Clicking an anchor with href="javascript:..." and target="_blank" should not
// run the JavaScript URL.
TEST_F(FrameLoaderJavaScriptUrlTest, JavaScriptUrlTargetBlank) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <html><body>
      <a href="javascript:'moo'" target="_blank"></a>
      </body></html>
  )HTML");

  // Generate and dispatch a click event.
  MouseEventInit* mouse_initializer = MouseEventInit::Create();
  mouse_initializer->setView(&Window());
  mouse_initializer->setButton(1);

  Event* event =
      MouseEvent::Create(nullptr, event_type_names::kClick, mouse_initializer);
  Element* anchor = GetDocument().QuerySelector(AtomicString("a"));
  anchor->DispatchSimulatedClick(event);

  // No task should be queued, since this navigation attempt should be ignored.
  EXPECT_FALSE(GetDocument().HasPendingJavaScriptUrlsForTest());
}

// Ctrl+clicking an anchor with href="javascript:..." and target="_blank" should
// not run the JavaScript URL. Regression test for crbug.com/41490237.
TEST_F(FrameLoaderJavaScriptUrlTest, CtrlClickJavaScriptUrlTargetBlank) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <html><body>
      <a href="javascript:'moo'" target="_blank"></a>
      </body></html>
  )HTML");

  // Generate and dispatch a ctrl+click event.
  MouseEventInit* mouse_initializer = MouseEventInit::Create();
  mouse_initializer->setView(&Window());
  mouse_initializer->setButton(1);
  mouse_initializer->setCtrlKey(true);

  Event* event =
      MouseEvent::Create(nullptr, event_type_names::kClick, mouse_initializer);
  Element* anchor = GetDocument().QuerySelector(AtomicString("a"));
  anchor->DispatchSimulatedClick(event);

  // No task should be queued, since this navigation attempt should be ignored.
  EXPECT_FALSE(GetDocument().HasPendingJavaScriptUrlsForTest());
}

class FrameLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(FrameLoaderTest, PolicyContainerIsStoredOnCommitNavigation) {
  WebViewImpl* web_view_impl = web_view_helper_.Initialize();

  const KURL& url = KURL(NullUrl(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
  MockPolicyContainerHost mock_policy_container_host;
  InitiatorStateToken initiator_state_token;
  params->initiator_state_token = initiator_state_token;
  params->policy_container = std::make_unique<WebPolicyContainer>(
      WebPolicyContainerPolicies{
          network::ConnectionAllowlists(),
          network::mojom::CrossOriginEmbedderPolicyValue::kNone,
          network::IntegrityPolicy(),
          network::IntegrityPolicy(),
          network::mojom::ReferrerPolicy::kAlways,
          std::vector<WebContentSecurityPolicy>(),
      },
      mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(
      *mojom::blink::PolicyContainerPolicies::New(
          network::ConnectionAllowlists(),
          network::CrossOriginEmbedderPolicy(
              network::mojom::CrossOriginEmbedderPolicyValue::kNone),
          network::IntegrityPolicy(), network::IntegrityPolicy(),
          network::mojom::ReferrerPolicy::kAlways,
          Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
          /*is_credentialless=*/false, network::mojom::WebSandboxFlags::kNone,
          network::mojom::blink::IPAddressSpace::kUnknown,
          /*can_navigate_top_without_user_gesture=*/true,
          /*cross_origin_isolation_enabled_by_dip=*/false),
      local_frame->DomWindow()->GetPolicyContainer()->GetPolicies());
  EXPECT_EQ(initiator_state_token,
            local_frame->DomWindow()->GetInitiatorStateToken());
}

TEST_F(FrameLoaderSimTest, DirectLaunchSchemeBlocked) {
  const String kScheme("direct-launch-scheme");
  SchemeRegistry::RegisterURLSchemeAsDirectLaunch(kScheme);

  SimRequest main_request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(StrCat({"<a id='link' href='", kScheme,
                                ":https://example.com/dest.html'>link</a>"}));

  auto* anchor = To<HTMLAnchorElement>(GetElementById("link"));
  ASSERT_NE(anchor, nullptr);

  anchor->click();

  // Verify navigation was synchronously blocked in Blink, logged a security
  // error, and did not initiate a provisional load.
  EXPECT_TRUE(ConsoleMessages().Contains(
      StrCat({"Not allowed to navigate to direct-launch scheme '", kScheme,
              "' from web contexts."})));
  EXPECT_FALSE(GetDocument().GetFrame()->Loader().HasProvisionalNavigation());
  EXPECT_EQ(GetDocument().Url(), KURL("https://example.com/test.html"));

  SchemeRegistry::RemoveURLSchemeAsDirectLaunchForTest(kScheme);
}

TEST_F(FrameLoaderSimTest, DirectLaunchSchemeTargetBlankBlocked) {
  const String kScheme("direct-launch-scheme");
  SchemeRegistry::RegisterURLSchemeAsDirectLaunch(kScheme);

  SimRequest main_request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(StrCat({"<a id='link' target='_blank' href='", kScheme,
                                "://https://example.com/dest.html'>link</a>"}));

  auto* anchor = To<HTMLAnchorElement>(GetElementById("link"));
  ASSERT_NE(anchor, nullptr);

  anchor->click();

  // Verify navigation to new window was synchronously blocked in Blink and
  // logged a security error.
  EXPECT_TRUE(ConsoleMessages().Contains(
      StrCat({"Not allowed to navigate to direct-launch scheme '", kScheme,
              "' from web contexts."})));

  SchemeRegistry::RemoveURLSchemeAsDirectLaunchForTest(kScheme);
}

class UserGestureNavigationTestWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    last_has_user_gesture_ = info->url_request.HasUserGesture();
  }

  std::optional<bool> last_has_user_gesture() const {
    return last_has_user_gesture_;
  }

 private:
  std::optional<bool> last_has_user_gesture_;
};

TEST_F(FrameLoaderTest, StartNavigationDoesNotSpoofTargetUserGesture) {
  UserGestureNavigationTestWebFrameClient client;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(&client);

  LocalFrame* target_frame = helper.LocalMainFrame()->GetFrame();

  // Target frame has transient user activation.
  LocalFrame::NotifyUserActivation(
      target_frame, mojom::UserActivationNotificationType::kTest);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(target_frame));

  // Initiator frame load request without user activation.
  ResourceRequest resource_request(KURL("https://example.com/foo.html"));
  resource_request.SetHasUserGesture(false);
  FrameLoadRequest request(nullptr, resource_request);

  target_frame->Loader().StartNavigation(request);

  ASSERT_TRUE(client.last_has_user_gesture().has_value());
  EXPECT_FALSE(client.last_has_user_gesture().value());
}

TEST_F(FrameLoaderTest, StartNavigationPropagatesInitiatorUserGesture) {
  UserGestureNavigationTestWebFrameClient client;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(&client);

  LocalFrame* target_frame = helper.LocalMainFrame()->GetFrame();

  // Target frame has NO transient user activation.
  ASSERT_FALSE(LocalFrame::HasTransientUserActivation(target_frame));

  // Initiator frame load request with user activation.
  ResourceRequest resource_request(KURL("https://example.com/foo.html"));
  resource_request.SetHasUserGesture(true);
  FrameLoadRequest request(nullptr, resource_request);

  target_frame->Loader().StartNavigation(request);

  ASSERT_TRUE(client.last_has_user_gesture().has_value());
  EXPECT_TRUE(client.last_has_user_gesture().value());
}

TEST_F(FrameLoaderTest, FrameLoadRequestCapturesInitiatorUserGesture) {
  UserGestureNavigationTestWebFrameClient client;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(&client);

  LocalFrame* initiator_frame = helper.LocalMainFrame()->GetFrame();

  // 1. Without user activation on initiator.
  ASSERT_FALSE(LocalFrame::HasTransientUserActivation(initiator_frame));
  FrameLoadRequest request1(
      initiator_frame->DomWindow(),
      ResourceRequest(KURL("https://example.com/foo.html")));
  EXPECT_FALSE(request1.GetResourceRequest().HasUserGesture());

  // 2. With user activation on initiator.
  LocalFrame::NotifyUserActivation(
      initiator_frame, mojom::UserActivationNotificationType::kTest);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(initiator_frame));
  FrameLoadRequest request2(
      initiator_frame->DomWindow(),
      ResourceRequest(KURL("https://example.com/foo.html")));
  EXPECT_TRUE(request2.GetResourceRequest().HasUserGesture());
}

TEST_F(FrameLoaderSimTest, AutoplayFlagsMergeAndNavigationLifecycle) {
  auto origin_a = SecurityOrigin::Create(KURL("https://example.com/"));
  auto origin_b = SecurityOrigin::Create(KURL("https://other.test/"));

  // Calling AddAutoplayFlags with a different origin replaces pending flags,
  // and calling again with the same origin merges the flags.
  LocalFrame* main_frame = To<LocalFrame>(GetDocument().GetPage()->MainFrame());
  main_frame->Loader().AddAutoplayFlags(origin_b,
                                        mojom::blink::kAutoplayFlagForceAllow);
  main_frame->Loader().AddAutoplayFlags(
      origin_a, mojom::blink::kAutoplayFlagHighMediaEngagement);
  main_frame->Loader().AddAutoplayFlags(
      origin_a, mojom::blink::kAutoplayFlagUserException);

  SimRequest main_request("https://example.com/main.html", "text/html");
  SimRequest child_request("https://other.test/child.html", "text/html");
  LoadURL("https://example.com/main.html");
  main_request.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe src="https://other.test/child.html"></iframe>
  )HTML");

  // Pending flags for origin_a should now be applied to the Page.
  const int32_t expected_main_flags =
      mojom::blink::kAutoplayFlagHighMediaEngagement |
      mojom::blink::kAutoplayFlagUserException;
  EXPECT_EQ(GetDocument().GetPage()->AutoplayFlags(), expected_main_flags);

  // Subframe navigation adds subframe flags without clearing main frame flags.
  LocalFrame* child_frame = To<LocalFrame>(main_frame->Tree().FirstChild());
  ASSERT_NE(child_frame, nullptr);
  child_frame->Loader().AddAutoplayFlags(origin_b,
                                         mojom::blink::kAutoplayFlagForceAllow);
  child_request.Complete("<!DOCTYPE html>");
  EXPECT_EQ(GetDocument().GetPage()->AutoplayFlags(),
            expected_main_flags | mojom::blink::kAutoplayFlagForceAllow);

  // Same-document navigation on the main frame preserves Page flags and applies
  // any pending flags.
  GetDocument()
      .GetFrame()
      ->Loader()
      .GetDocumentLoader()
      ->RunURLAndHistoryUpdateSteps(
          KURL("https://example.com/main.html#section"), nullptr,
          mojom::blink::SameDocumentNavigationType::kFragment,
          /*data=*/nullptr, WebFrameLoadType::kStandard, FirePopstate::kYes,
          /*should_skip_screenshot=*/true, UserNavigationInvolvement::kNone,
          /*has_transient_user_activation=*/false,
          PerformanceTimelineEntryIdInfo::kNone,
          /*is_browser_initiated=*/false);
  EXPECT_EQ(GetDocument().GetPage()->AutoplayFlags(),
            expected_main_flags | mojom::blink::kAutoplayFlagForceAllow);

  // A subsequent same-origin cross-document main frame navigation without new
  // pending flags clears Page flags (since the pending origin was wiped on the
  // previous commit).
  SimRequest reload_request("https://example.com/main.html", "text/html");
  LoadURL("https://example.com/main.html");
  reload_request.Complete("<!DOCTYPE html>");
  EXPECT_EQ(GetDocument().GetPage()->AutoplayFlags(),
            mojom::blink::kAutoplayFlagNone);
}

TEST_F(FrameLoaderSimTest, AutoplayFlagsFileOriginWithBlockedLocalAccess) {
  GetDocument().GetSettings()->SetAllowUniversalAccessFromFileURLs(false);
  GetDocument().GetSettings()->SetAllowFileAccessFromFileURLs(false);

  const KURL file_url("file:///test.html");
  // Simulate distinct `SecurityOrigin` instances deserialized from
  // `url.mojom.Origin` (which lack `block_local_access_from_local_origin_`).
  auto mojo_origin_1 = SecurityOrigin::Create(file_url);
  auto mojo_origin_2 = SecurityOrigin::CreateFromValidTuple("file", "", 0);
  LocalFrame* main_frame = To<LocalFrame>(GetDocument().GetPage()->MainFrame());
  main_frame->Loader().AddAutoplayFlags(
      mojo_origin_1, mojom::blink::kAutoplayFlagHighMediaEngagement);
  main_frame->Loader().AddAutoplayFlags(
      mojo_origin_2, mojom::blink::kAutoplayFlagUserException);

  main_frame->Loader().CommitNavigation(
      WebNavigationParams::CreateWithEmptyHTMLForTesting(file_url), nullptr);

  const SecurityOrigin* committed_origin =
      main_frame->DomWindow()->GetSecurityOrigin();
  EXPECT_TRUE(committed_origin->block_local_access_from_local_origin());
  EXPECT_FALSE(committed_origin->IsSameOriginWith(mojo_origin_1.get()));
  EXPECT_EQ(GetDocument().GetPage()->AutoplayFlags(),
            mojom::blink::kAutoplayFlagHighMediaEngagement |
                mojom::blink::kAutoplayFlagUserException);
}

}  // namespace blink
