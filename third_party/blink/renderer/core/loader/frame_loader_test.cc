// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/page/chrome_client_impl.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class UserAgentOverrideWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  UserAgentOverrideWebFrameClient() = default;

  WebString UserAgentOverride() override { return WebString("foo"); }
};

}  // namespace

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
    ASSERT_FALSE(frame_a->Loader().ShouldClose());

    EXPECT_FALSE(main_frame->GetDocument()->BeforeUnloadStarted());
    EXPECT_FALSE(frame_a->GetDocument()->BeforeUnloadStarted());
    EXPECT_FALSE(frame_b->GetDocument()->BeforeUnloadStarted());
    EXPECT_FALSE(frame_c->GetDocument()->BeforeUnloadStarted());
  }

  // Now test the opposite, the user allowing the navigation away.
  {
    chrome_client.SetBeforeUnloadConfirmPanelResultForTesting(true);
    ASSERT_TRUE(frame_a->Loader().ShouldClose());

    // The navigation was in frame a so it shouldn't affect the parent.
    EXPECT_FALSE(main_frame->GetDocument()->BeforeUnloadStarted());
    EXPECT_TRUE(frame_a->GetDocument()->BeforeUnloadStarted());
    EXPECT_TRUE(frame_b->GetDocument()->BeforeUnloadStarted());
    EXPECT_TRUE(frame_c->GetDocument()->BeforeUnloadStarted());
  }
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

  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(FrameLoaderTest, PolicyContainerIsStoredOnCommitNavigation) {
  WebViewImpl* web_view_impl = web_view_helper_.Initialize();

  const KURL& url = KURL(NullURL(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBufferForTesting(
          SharedBuffer::Create(), url);
  MockPolicyContainerHost mock_policy_container_host;
  params->policy_container = std::make_unique<WebPolicyContainer>(
      WebPolicyContainerPolicies{
          network::mojom::CrossOriginEmbedderPolicyValue::kNone,
          network::mojom::ReferrerPolicy::kAlways,
          WebVector<WebContentSecurityPolicy>(),
      },
      mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(*mojom::blink::PolicyContainerPolicies::New(
                network::mojom::CrossOriginEmbedderPolicyValue::kNone,
                network::mojom::ReferrerPolicy::kAlways,
                Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
                /*anonymous=*/false, network::mojom::WebSandboxFlags::kNone,
                network::mojom::blink::IPAddressSpace::kUnknown,
                /*can_navigate_top_without_user_gesture=*/true),
            local_frame->DomWindow()->GetPolicyContainer()->GetPolicies());
}

class UserAgentOverrideFrameLoaderTest : public FrameLoaderTest {
 public:
  void SetUp() override {
    FrameLoaderTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kUserAgentOverrideExperiment);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UserAgentOverrideFrameLoaderTest, UserAgentOverrideIframeNavigation) {
  frame_test_helpers::WebViewHelper web_view_helper;
  UserAgentOverrideWebFrameClient client;
  WebViewImpl* web_view = web_view_helper.Initialize(&client);

  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      R"HTML(
      <!DOCTYPE html>
      <iframe src="foo.html"></iframe>
  )HTML",
      url_test_helpers::ToKURL("https://example.com/"));

  // Manually reset ro avoid UAF
  web_view_helper.Reset();
  // Test passes if there's no crash.
}

}  // namespace blink
