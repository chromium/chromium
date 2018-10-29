// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/renderer.mojom.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/previews_state.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/mojo/blink_interface_registry_impl.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/frame_host_test_interface.mojom.h"
#include "content/test/test_render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/native_theme/native_theme_features.h"

using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

constexpr int32_t kSubframeRouteId = 20;
constexpr int32_t kSubframeWidgetRouteId = 21;
constexpr int32_t kFrameProxyRouteId = 22;
constexpr int32_t kEmbeddedSubframeRouteId = 23;

const char kParentFrameHTML[] = "Parent frame <iframe name='frame'></iframe>";

const char kAutoplayTestOrigin[] = "https://www.google.com";

}  // namespace

// RenderFrameImplTest creates a RenderFrameImpl that is a child of the
// main frame, and has its own RenderWidget. This behaves like an out
// of process frame even though it is in the same process as its parent.
class RenderFrameImplTest : public RenderViewTest {
 public:
  ~RenderFrameImplTest() override {}

  void SetUp() override {
    blink::WebRuntimeFeatures::EnableOverlayScrollbars(
        ui::IsOverlayScrollbarEnabled());
    RenderViewTest::SetUp();
    EXPECT_TRUE(GetMainRenderFrame()->is_main_frame_);

    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

    LoadHTML(kParentFrameHTML);
    LoadChildFrame();
  }

  void LoadChildFrame() {
    mojom::CreateFrameWidgetParams widget_params;
    widget_params.routing_id = kSubframeWidgetRouteId;
    widget_params.hidden = false;

    FrameReplicationState frame_replication_state;
    frame_replication_state.name = "frame";
    frame_replication_state.unique_name = "frame-uniqueName";

    RenderFrameImpl::FromWebFrame(
        view_->GetMainRenderFrame()->GetWebFrame()->FirstChild())
        ->OnSwapOut(kFrameProxyRouteId, false, frame_replication_state);

    service_manager::mojom::InterfaceProviderPtr stub_interface_provider;
    mojo::MakeRequest(&stub_interface_provider);
    RenderFrameImpl::CreateFrame(
        kSubframeRouteId, std::move(stub_interface_provider), MSG_ROUTING_NONE,
        MSG_ROUTING_NONE, kFrameProxyRouteId, MSG_ROUTING_NONE,
        base::UnguessableToken::Create(), frame_replication_state,
        &compositor_deps_, widget_params, FrameOwnerProperties(),
        /*has_committed_real_load=*/true);

    frame_ = static_cast<TestRenderFrame*>(
        RenderFrameImpl::FromRoutingID(kSubframeRouteId));
    EXPECT_FALSE(frame_->is_main_frame_);
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
     // Do this before shutting down V8 in RenderViewTest::TearDown().
     // http://crbug.com/328552
     __lsan_do_leak_check();
#endif
     RenderViewTest::TearDown();
  }

  void SetPreviewsState(RenderFrameImpl* frame, PreviewsState previews_state) {
    frame->previews_state_ = previews_state;
  }

  void SetEffectionConnectionType(RenderFrameImpl* frame,
                                  blink::WebEffectiveConnectionType type) {
    frame->effective_connection_type_ = type;
  }

  TestRenderFrame* GetMainRenderFrame() {
    return static_cast<TestRenderFrame*>(view_->GetMainRenderFrame());
  }

  TestRenderFrame* frame() { return frame_; }

  content::RenderWidget* frame_widget() const {
    return frame_->render_widget_.get();
  }

  static url::Origin GetOriginForFrame(TestRenderFrame* frame) {
    return url::Origin(frame->GetWebFrame()->GetSecurityOrigin());
  }

  static int32_t AutoplayFlagsForFrame(TestRenderFrame* frame) {
    return frame->render_view()->webview()->AutoplayFlagsForTest();
  }

#if defined(OS_ANDROID)
  void ReceiveOverlayRoutingToken(const base::UnguessableToken& token) {
    overlay_routing_token_ = token;
  }

  base::Optional<base::UnguessableToken> overlay_routing_token_;
#endif

 private:
  TestRenderFrame* frame_;
  FakeCompositorDependencies compositor_deps_;
};

class RenderFrameTestObserver : public RenderFrameObserver {
 public:
  explicit RenderFrameTestObserver(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame), visible_(false) {}

  ~RenderFrameTestObserver() override {}

  // RenderFrameObserver implementation.
  void WasShown() override { visible_ = true; }
  void WasHidden() override { visible_ = false; }
  void OnDestruct() override { delete this; }

  bool visible() { return visible_; }

 private:
  bool visible_;
};

// Verify that a frame with a RenderFrameProxy as a parent has its own
// RenderWidget.
TEST_F(RenderFrameImplTest, SubframeWidget) {
  EXPECT_TRUE(frame_widget());
  EXPECT_NE(frame_widget(), static_cast<RenderViewImpl*>(view_)->GetWidget());
}

// Verify a subframe RenderWidget properly processes its viewport being
// resized.
TEST_F(RenderFrameImplTest, FrameResize) {
  VisualProperties visual_properties;
  gfx::Size size(200, 200);
  visual_properties.screen_info = ScreenInfo();
  visual_properties.new_size = size;
  visual_properties.compositor_viewport_pixel_size = size;
  visual_properties.visible_viewport_size = size;
  visual_properties.top_controls_height = 0.f;
  visual_properties.browser_controls_shrink_blink_size = false;
  visual_properties.is_fullscreen_granted = false;

  WidgetMsg_SynchronizeVisualProperties resize_message(0, visual_properties);
  frame_widget()->OnMessageReceived(resize_message);

  EXPECT_EQ(frame_widget()->GetWebWidget()->Size(), blink::WebSize(size));
  EXPECT_EQ(view_->GetWebView()->Size(), blink::WebSize(size));
}

// Verify a subframe RenderWidget properly processes a WasShown message.
TEST_F(RenderFrameImplTest, FrameWasShown) {
  RenderFrameTestObserver observer(frame());

  WidgetMsg_WasShown was_shown_message(0, base::TimeTicks());
  frame_widget()->OnMessageReceived(was_shown_message);

  EXPECT_FALSE(frame_widget()->is_hidden());
  EXPECT_TRUE(observer.visible());
}

// Verify that a local subframe of a frame with a RenderWidget processes a
// WasShown message.
TEST_F(RenderFrameImplTest, LocalChildFrameWasShown) {
  service_manager::mojom::InterfaceProviderPtr stub_interface_provider;
  mojo::MakeRequest(&stub_interface_provider);

  // Create and initialize a local child frame of the simulated OOPIF, which
  // is a grandchild of the remote main frame.
  RenderFrameImpl* grandchild = RenderFrameImpl::Create(
      frame()->render_view(), kEmbeddedSubframeRouteId,
      std::move(stub_interface_provider), base::UnguessableToken::Create());
  blink::WebLocalFrame* parent_web_frame = frame()->GetWebFrame();

  parent_web_frame->CreateLocalChild(
      blink::WebTreeScopeType::kDocument, grandchild,
      grandchild->blink_interface_registry_.get());
  grandchild->in_frame_tree_ = true;
  grandchild->Initialize();

  EXPECT_EQ(grandchild->GetRenderWidget(), frame()->GetRenderWidget());

  RenderFrameTestObserver observer(grandchild);

  WidgetMsg_WasShown was_shown_message(0, base::TimeTicks());
  frame_widget()->OnMessageReceived(was_shown_message);

  EXPECT_FALSE(frame_widget()->is_hidden());
  EXPECT_TRUE(observer.visible());
}

// Ensure that a RenderFrameImpl does not crash if the RenderView receives
// a WasShown message after the frame's widget has been closed.
TEST_F(RenderFrameImplTest, FrameWasShownAfterWidgetClose) {
  WidgetMsg_Close close_message(0);
  frame_widget()->OnMessageReceived(close_message);

  WidgetMsg_WasShown was_shown_message(0, base::TimeTicks());
  // Test passes if this does not crash.
  RenderWidget* render_widget =
      static_cast<RenderViewImpl*>(view_)->GetWidget();
  render_widget->OnMessageReceived(was_shown_message);
}

// Test that LoFi state only updates for new main frame documents. Subframes
// inherit from the main frame and should not change at commit time.
TEST_F(RenderFrameImplTest, LoFiNotUpdatedOnSubframeCommits) {
  SetPreviewsState(GetMainRenderFrame(), SERVER_LOFI_ON);
  SetPreviewsState(frame(), SERVER_LOFI_ON);
  EXPECT_EQ(SERVER_LOFI_ON, GetMainRenderFrame()->GetPreviewsState());
  EXPECT_EQ(SERVER_LOFI_ON, frame()->GetPreviewsState());

  blink::WebHistoryItem item;
  item.Initialize();

  // The main frame's and subframe's LoFi states should stay the same on
  // same-document navigations.
  frame()->DidFinishSameDocumentNavigation(item, blink::kWebStandardCommit,
                                           false /* content_initiated */);
  EXPECT_EQ(SERVER_LOFI_ON, frame()->GetPreviewsState());
  GetMainRenderFrame()->DidFinishSameDocumentNavigation(
      item, blink::kWebStandardCommit, false /* content_initiated */);
  EXPECT_EQ(SERVER_LOFI_ON, GetMainRenderFrame()->GetPreviewsState());

  // The subframe's LoFi state should not be reset on commit.
  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  navigation_state->set_was_within_same_document(false);

  frame()->DidCommitProvisionalLoad(
      item, blink::kWebStandardCommit,
      blink::WebGlobalObjectReusePolicy::kCreateNew);
  EXPECT_EQ(SERVER_LOFI_ON, frame()->GetPreviewsState());

  // The main frame's LoFi state should be reset to off on commit.
  navigation_state = NavigationState::FromDocumentLoader(
      GetMainRenderFrame()->GetWebFrame()->GetDocumentLoader());
  navigation_state->set_was_within_same_document(false);

  // Calling didCommitProvisionalLoad is not representative of a full navigation
  // but serves the purpose of testing the LoFi state logic.
  GetMainRenderFrame()->DidCommitProvisionalLoad(
      item, blink::kWebStandardCommit,
      blink::WebGlobalObjectReusePolicy::kCreateNew);
  EXPECT_EQ(PREVIEWS_UNSPECIFIED, GetMainRenderFrame()->GetPreviewsState());
  // The subframe would be deleted here after a cross-document navigation. It
  // happens to be left around in this test because this does not simulate the
  // frame detach.
}

// Test that effective connection type only updates for new main frame
// documents.
TEST_F(RenderFrameImplTest, EffectiveConnectionType) {
  EXPECT_EQ(blink::WebEffectiveConnectionType::kTypeUnknown,
            frame()->GetEffectiveConnectionType());
  EXPECT_EQ(blink::WebEffectiveConnectionType::kTypeUnknown,
            GetMainRenderFrame()->GetEffectiveConnectionType());

  const struct {
    blink::WebEffectiveConnectionType type;
  } tests[] = {{blink::WebEffectiveConnectionType::kTypeUnknown},
               {blink::WebEffectiveConnectionType::kType2G},
               {blink::WebEffectiveConnectionType::kType4G}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SetEffectionConnectionType(GetMainRenderFrame(), tests[i].type);
    SetEffectionConnectionType(frame(), tests[i].type);

    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());
    EXPECT_EQ(tests[i].type,
              GetMainRenderFrame()->GetEffectiveConnectionType());

    blink::WebHistoryItem item;
    item.Initialize();

    // The main frame's and subframe's effective connection type should stay the
    // same on same-document navigations.
    frame()->DidFinishSameDocumentNavigation(item, blink::kWebStandardCommit,
                                             false /* content_initiated */);
    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());
    GetMainRenderFrame()->DidFinishSameDocumentNavigation(
        item, blink::kWebStandardCommit, false /* content_initiated */);
    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());

    // The subframe's effective connection type should not be reset on commit.
    NavigationState* navigation_state = NavigationState::FromDocumentLoader(
        frame()->GetWebFrame()->GetDocumentLoader());
    navigation_state->set_was_within_same_document(false);

    frame()->DidCommitProvisionalLoad(
        item, blink::kWebStandardCommit,
        blink::WebGlobalObjectReusePolicy::kCreateNew);
    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());

    // The main frame's effective connection type should be reset on commit.
    navigation_state = NavigationState::FromDocumentLoader(
        GetMainRenderFrame()->GetWebFrame()->GetDocumentLoader());
    navigation_state->set_was_within_same_document(false);

    GetMainRenderFrame()->DidCommitProvisionalLoad(
        item, blink::kWebStandardCommit,
        blink::WebGlobalObjectReusePolicy::kCreateNew);
    EXPECT_EQ(blink::WebEffectiveConnectionType::kTypeUnknown,
              GetMainRenderFrame()->GetEffectiveConnectionType());

    // The subframe would be deleted here after a cross-document navigation.
    // It happens to be left around in this test because this does not simulate
    // the frame detach.
  }
}

TEST_F(RenderFrameImplTest, SaveImageFromDataURL) {
  const IPC::Message* msg1 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_FALSE(msg1);
  render_thread_->sink().ClearMessages();

  const std::string image_data_url =
      "data:image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=";

  frame()->SaveImageFromDataURL(WebString::FromUTF8(image_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg2 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_TRUE(msg2);

  FrameHostMsg_SaveImageFromDataURL::Param param1;
  FrameHostMsg_SaveImageFromDataURL::Read(msg2, &param1);
  EXPECT_EQ(std::get<2>(param1), image_data_url);

  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  const std::string large_data_url(1024 * 1024 * 20 - 1, 'd');

  frame()->SaveImageFromDataURL(WebString::FromUTF8(large_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg3 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_TRUE(msg3);

  FrameHostMsg_SaveImageFromDataURL::Param param2;
  FrameHostMsg_SaveImageFromDataURL::Read(msg3, &param2);
  EXPECT_EQ(std::get<2>(param2), large_data_url);

  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  const std::string exceeded_data_url(1024 * 1024 * 20 + 1, 'd');

  frame()->SaveImageFromDataURL(WebString::FromUTF8(exceeded_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg4 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_FALSE(msg4);
}

// Tests that url download are throttled when reaching the limit.
TEST_F(RenderFrameImplTest, DownloadUrlLimit) {
  const IPC::Message* msg1 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_DownloadUrl::ID);
  EXPECT_FALSE(msg1);
  render_thread_->sink().ClearMessages();

  WebURLRequest request;
  request.SetURL(GURL("http://test/test.pdf"));
  request.SetRequestorOrigin(
      blink::WebSecurityOrigin::Create(GURL("http://test")));

  for (int i = 0; i < 10; ++i) {
    frame()->DownloadURL(
        request, blink::WebLocalFrameClient::CrossOriginRedirects::kNavigate,
        mojo::ScopedMessagePipeHandle());
    base::RunLoop().RunUntilIdle();
    const IPC::Message* msg2 = render_thread_->sink().GetFirstMessageMatching(
        FrameHostMsg_DownloadUrl::ID);
    EXPECT_TRUE(msg2);
    base::RunLoop().RunUntilIdle();
    render_thread_->sink().ClearMessages();
  }

  frame()->DownloadURL(
      request, blink::WebLocalFrameClient::CrossOriginRedirects::kNavigate,
      mojo::ScopedMessagePipeHandle());
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg3 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_DownloadUrl::ID);
  EXPECT_FALSE(msg3);
}

TEST_F(RenderFrameImplTest, ZoomLimit) {
  const double kMinZoomLevel = ZoomFactorToZoomLevel(kMinimumZoomFactor);
  const double kMaxZoomLevel = ZoomFactorToZoomLevel(kMaximumZoomFactor);

  // Verifies navigation to a URL with preset zoom level indeed sets the level.
  // Regression test for http://crbug.com/139559, where the level was not
  // properly set when it is out of the default zoom limits of WebView.
  CommonNavigationParams common_params;
  common_params.url = GURL("data:text/html,min_zoomlimit_test");
  common_params.navigation_type = FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
  GetMainRenderFrame()->SetHostZoomLevel(common_params.url, kMinZoomLevel);
  GetMainRenderFrame()->Navigate(common_params, RequestNavigationParams());
  base::RunLoop().RunUntilIdle();
  EXPECT_DOUBLE_EQ(kMinZoomLevel, view_->GetWebView()->ZoomLevel());

  // It should work even when the zoom limit is temporarily changed in the page.
  view_->GetWebView()->ZoomLimitsChanged(ZoomFactorToZoomLevel(1.0),
                                         ZoomFactorToZoomLevel(1.0));
  common_params.url = GURL("data:text/html,max_zoomlimit_test");
  GetMainRenderFrame()->SetHostZoomLevel(common_params.url, kMaxZoomLevel);
  GetMainRenderFrame()->Navigate(common_params, RequestNavigationParams());
  base::RunLoop().RunUntilIdle();
  EXPECT_DOUBLE_EQ(kMaxZoomLevel, view_->GetWebView()->ZoomLevel());
}

// Regression test for crbug.com/692557. It shouldn't crash if we inititate a
// text finding, and then delete the frame immediately before the text finding
// returns any text match.
TEST_F(RenderFrameImplTest, NoCrashWhenDeletingFrameDuringFind) {
  frame()->GetWebFrame()->FindForTesting(
      1, "foo", true /* match_case */, true /* forward */,
      false /* find_next */, true /* force */, false /* wrap_within_frame */);

  FrameMsg_Delete delete_message(0);
  frame()->OnMessageReceived(delete_message);
}

#if defined(OS_ANDROID)
// Verify that RFI defers token requests if the token hasn't arrived yet.
TEST_F(RenderFrameImplTest, TestOverlayRoutingTokenSendsLater) {
  ASSERT_FALSE(overlay_routing_token_.has_value());

  frame()->RequestOverlayRoutingToken(
      base::Bind(&RenderFrameImplTest::ReceiveOverlayRoutingToken,
                 base::Unretained(this)));
  ASSERT_FALSE(overlay_routing_token_.has_value());

  // The host should receive a request for it sent to the frame.
  const IPC::Message* msg = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_RequestOverlayRoutingToken::ID);
  EXPECT_TRUE(msg);

  // Send a token.
  base::UnguessableToken token = base::UnguessableToken::Create();
  FrameMsg_SetOverlayRoutingToken token_message(0, token);
  frame()->OnMessageReceived(token_message);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(overlay_routing_token_.has_value());
  ASSERT_EQ(overlay_routing_token_.value(), token);
}

// Verify that RFI sends tokens if they're already available.
TEST_F(RenderFrameImplTest, TestOverlayRoutingTokenSendsNow) {
  ASSERT_FALSE(overlay_routing_token_.has_value());
  base::UnguessableToken token = base::UnguessableToken::Create();
  FrameMsg_SetOverlayRoutingToken token_message(0, token);
  frame()->OnMessageReceived(token_message);

  // The frame now has a token.  We don't care if it sends the token before
  // returning or posts a message.
  base::RunLoop().RunUntilIdle();
  frame()->RequestOverlayRoutingToken(
      base::Bind(&RenderFrameImplTest::ReceiveOverlayRoutingToken,
                 base::Unretained(this)));
  ASSERT_TRUE(overlay_routing_token_.has_value());
  ASSERT_EQ(overlay_routing_token_.value(), token);

  // Since the token already arrived, a request for it shouldn't be sent.
  const IPC::Message* msg = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_RequestOverlayRoutingToken::ID);
  EXPECT_FALSE(msg);
}
#endif

TEST_F(RenderFrameImplTest, PreviewsStateAfterWillSendRequest) {
  const struct {
    PreviewsState frame_previews_state;
    WebURLRequest::PreviewsState initial_request_previews_state;
    WebURLRequest::PreviewsState expected_final_request_previews_state;
  } tests[] = {
      // With no previews enabled for the frame, no previews should be
      // activated.
      {PREVIEWS_UNSPECIFIED, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kPreviewsOff},

      // If the request already has a previews state set, then it shouldn't be
      // overridden.
      {SERVER_LOFI_ON, WebURLRequest::kPreviewsNoTransform,
       WebURLRequest::kPreviewsNoTransform},
      {SERVER_LOFI_ON, WebURLRequest::kPreviewsOff,
       WebURLRequest::kPreviewsOff},

      // Server Lo-Fi and Server Lite Pages should be enabled for the request
      // when they're enabled for the frame.
      {SERVER_LOFI_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLoFiOn},
      {SERVER_LITE_PAGE_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLitePageOn},
      {SERVER_LITE_PAGE_ON | SERVER_LOFI_ON,
       WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLitePageOn | WebURLRequest::kServerLoFiOn},

      // The CLIENT_LOFI_ON frame flag should be ignored at this point in the
      // request.
      {CLIENT_LOFI_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kPreviewsOff},
      {SERVER_LOFI_ON | CLIENT_LOFI_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLoFiOn},

      // A request that's using Client Lo-Fi should continue using Client Lo-Fi.
      {SERVER_LOFI_ON | CLIENT_LOFI_ON, WebURLRequest::kClientLoFiOn,
       WebURLRequest::kClientLoFiOn},
      {CLIENT_LOFI_ON, WebURLRequest::kClientLoFiOn,
       WebURLRequest::kClientLoFiOn},
      {SERVER_LITE_PAGE_ON, WebURLRequest::kClientLoFiOn,
       WebURLRequest::kClientLoFiOn},
  };

  for (const auto& test : tests) {
    SetPreviewsState(frame(), test.frame_previews_state);

    WebURLRequest request;
    request.SetURL(GURL("http://example.com"));
    request.SetPreviewsState(test.initial_request_previews_state);

    frame()->WillSendRequest(request);

    EXPECT_EQ(test.expected_final_request_previews_state,
              request.GetPreviewsState())
        << (&test - tests);
  }
}

TEST_F(RenderFrameImplTest, GetPreviewsStateForFrame) {
  SetPreviewsState(frame(), CLIENT_LOFI_ON | SERVER_LOFI_ON);
  EXPECT_EQ(WebURLRequest::kClientLoFiOn | WebURLRequest::kServerLoFiOn,
            frame()->GetPreviewsStateForFrame());

  SetPreviewsState(frame(), PREVIEWS_OFF);
  EXPECT_EQ(WebURLRequest::kPreviewsOff, frame()->GetPreviewsStateForFrame());

  SetPreviewsState(frame(), PREVIEWS_OFF | PREVIEWS_NO_TRANSFORM);
  EXPECT_EQ(WebURLRequest::kPreviewsOff | WebURLRequest::kPreviewsNoTransform,
            frame()->GetPreviewsStateForFrame());

  SetPreviewsState(frame(), CLIENT_LOFI_ON | PREVIEWS_OFF);
  EXPECT_DCHECK_DEATH(frame()->GetPreviewsStateForFrame());
}

TEST_F(RenderFrameImplTest, AutoplayFlags) {
  // Add autoplay flags to the page.
  GetMainRenderFrame()->AddAutoplayFlags(
      url::Origin::Create(GURL(kAutoplayTestOrigin)),
      blink::mojom::kAutoplayFlagHighMediaEngagement);

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, kAutoplayTestOrigin);

  // Check the flags have been set correctly.
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(GetMainRenderFrame()));

  // Navigate the child frame.
  LoadChildFrame();

  // Check the flags are set on both frames.
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(GetMainRenderFrame()));
  EXPECT_EQ(blink::mojom::kAutoplayFlagHighMediaEngagement,
            AutoplayFlagsForFrame(frame()));

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, "https://www.example.com");
  LoadChildFrame();

  // Check the flags have been cleared.
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone,
            AutoplayFlagsForFrame(GetMainRenderFrame()));
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone, AutoplayFlagsForFrame(frame()));
}

TEST_F(RenderFrameImplTest, AutoplayFlags_WrongOrigin) {
  // Add autoplay flags to the page.
  GetMainRenderFrame()->AddAutoplayFlags(
      url::Origin(), blink::mojom::kAutoplayFlagHighMediaEngagement);

  // Navigate the top frame.
  LoadHTMLWithUrlOverride(kParentFrameHTML, kAutoplayTestOrigin);

  // Check the flags have been not been set.
  EXPECT_EQ(blink::mojom::kAutoplayFlagNone,
            AutoplayFlagsForFrame(GetMainRenderFrame()));
}

TEST_F(RenderFrameImplTest, FileUrlPathAlias) {
  const struct {
    const char* original;
    const char* transformed;
  } kTestCases[] = {
      {"file:///alias", "file:///replacement"},
      {"file:///alias/path/to/file", "file:///replacement/path/to/file"},
      {"file://alias/path/to/file", "file://alias/path/to/file"},
      {"file:///notalias/path/to/file", "file:///notalias/path/to/file"},
      {"file:///root/alias/path/to/file", "file:///root/alias/path/to/file"},
      {"file:///", "file:///"},
  };
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFileUrlPathAlias, "/alias=/replacement");

  for (const auto& test_case : kTestCases) {
    WebURLRequest request;
    request.SetURL(GURL(test_case.original));
    GetMainRenderFrame()->WillSendRequest(request);
    EXPECT_EQ(test_case.transformed, request.Url().GetString().Utf8());
  }
}

// RenderFrameRemoteInterfacesTest ------------------------------------

namespace {

constexpr char kTestFirstURL[] = "http://foo.com/1";
constexpr char kTestSecondURL[] = "http://foo.com/2";
// constexpr char kTestCrossOriginURL[] = "http://bar.com/";
constexpr char kAboutBlankURL[] = "about:blank";

constexpr char kFrameEventDidCreateNewFrame[] = "did-create-new-frame";
constexpr char kFrameEventDidCreateNewDocument[] = "did-create-new-document";
constexpr char kFrameEventDidCreateDocumentElement[] =
    "did-create-document-element";
constexpr char kFrameEventWillCommitProvisionalLoad[] =
    "will-commit-provisional-load";
constexpr char kFrameEventDidCommitProvisionalLoad[] =
    "did-commit-provisional-load";
constexpr char kFrameEventDidCommitSameDocumentLoad[] =
    "did-commit-same-document-load";
constexpr char kFrameEventAfterCommit[] = "after-commit";

constexpr char kNoDocumentMarkerURL[] = "data:,No document.";

// A simple testing implementation of mojom::InterfaceProvider that binds
// interface requests only for one hard-coded kind of interface.
class TestSimpleInterfaceProviderImpl
    : public service_manager::mojom::InterfaceProvider {
 public:
  using BinderCallback =
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;

  // Incoming interface requests for |interface_name| will invoke |binder|.
  // Everything else is ignored.
  TestSimpleInterfaceProviderImpl(const std::string& interface_name,
                                  BinderCallback binder_callback)
      : binding_(this),
        interface_name_(interface_name),
        binder_callback_(binder_callback) {}

  void BindAndFlush(service_manager::mojom::InterfaceProviderRequest request) {
    ASSERT_FALSE(binding_.is_bound());
    binding_.Bind(std::move(request));
    binding_.FlushForTesting();
  }

 private:
  // mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override {
    if (interface_name == interface_name_)
      binder_callback_.Run(std::move(handle));
  }

  mojo::Binding<service_manager::mojom::InterfaceProvider> binding_;

  std::string interface_name_;
  BinderCallback binder_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSimpleInterfaceProviderImpl);
};

// Used to annotate the source of an interface request.
struct SourceAnnotation {
  // The URL of the active document in the frame, at the time the interface was
  // requested by the RenderFrame.
  GURL document_url;

  // The RenderFrameObserver event in response to which the interface is
  // requested by the RenderFrame.
  std::string render_frame_event;

  bool operator==(const SourceAnnotation& rhs) const {
    return document_url == rhs.document_url &&
           render_frame_event == rhs.render_frame_event;
  }
};

std::ostream& operator<<(std::ostream& os, const SourceAnnotation& a) {
  return os << "[" << a.document_url << ", " << a.render_frame_event << "]";
}

class FrameHostTestInterfaceImpl : public mojom::FrameHostTestInterface {
 public:
  FrameHostTestInterfaceImpl() : binding_(this) {}
  ~FrameHostTestInterfaceImpl() override {}

  void BindAndFlush(mojom::FrameHostTestInterfaceRequest request) {
    binding_.Bind(std::move(request));
    binding_.WaitForIncomingMethodCall();
  }

  const base::Optional<SourceAnnotation>& ping_source() const {
    return ping_source_;
  }

 protected:
  void Ping(const GURL& url, const std::string& event) override {
    ping_source_ = SourceAnnotation{url, event};
  }

 private:
  mojo::Binding<mojom::FrameHostTestInterface> binding_;
  base::Optional<SourceAnnotation> ping_source_;

  DISALLOW_COPY_AND_ASSIGN(FrameHostTestInterfaceImpl);
};

// RenderFrameObserver that issues FrameHostTestInterface interface requests
// through the RenderFrame's |remote_interfaces_| in response to observing
// important milestones in a frame's lifecycle.
class FrameHostTestInterfaceRequestIssuer : public RenderFrameObserver {
 public:
  explicit FrameHostTestInterfaceRequestIssuer(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame) {}

  void RequestTestInterfaceOnFrameEvent(const std::string& event) {
    mojom::FrameHostTestInterfacePtr ptr;
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&ptr));

    blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
    ptr->Ping(
        !document.IsNull() ? GURL(document.Url()) : GURL(kNoDocumentMarkerURL),
        event);
  }

 private:
  // RenderFrameObserver:
  void OnDestruct() override {}

  void DidCreateDocumentElement() override {
    RequestTestInterfaceOnFrameEvent(kFrameEventDidCreateDocumentElement);
  }

  void DidCreateNewDocument() override {
    RequestTestInterfaceOnFrameEvent(kFrameEventDidCreateNewDocument);
  }

  void WillCommitProvisionalLoad() override {
    RequestTestInterfaceOnFrameEvent(kFrameEventWillCommitProvisionalLoad);
  }

  void DidStartProvisionalLoad(blink::WebDocumentLoader* document_loader,
                               bool is_content_initiated) override {}

  void DidFailProvisionalLoad(const blink::WebURLError& error) override {}

  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    RequestTestInterfaceOnFrameEvent(is_same_document_navigation
                                         ? kFrameEventDidCommitSameDocumentLoad
                                         : kFrameEventDidCommitProvisionalLoad);
  }

  DISALLOW_COPY_AND_ASSIGN(FrameHostTestInterfaceRequestIssuer);
};

// RenderFrameObserver that can be used to wait for the next commit in a frame.
class FrameCommitWaiter : public RenderFrameObserver {
 public:
  explicit FrameCommitWaiter(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame) {}

  void Wait() {
    if (did_commit_)
      return;
    run_loop_.Run();
  }

 private:
  // RenderFrameObserver:
  void OnDestruct() override {}

  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    did_commit_ = true;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool did_commit_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameCommitWaiter);
};

// Testing ContentRendererClient implementation that fires the |callback|
// whenever a new frame is created.
class FrameCreationObservingRendererClient : public ContentRendererClient {
 public:
  using FrameCreatedCallback = base::RepeatingCallback<void(TestRenderFrame*)>;

  FrameCreationObservingRendererClient() {}
  ~FrameCreationObservingRendererClient() override {}

  void set_callback(FrameCreatedCallback callback) {
    callback_ = std::move(callback);
  }

  void reset_callback() { callback_.Reset(); }

 protected:
  void RenderFrameCreated(RenderFrame* render_frame) override {
    ContentRendererClient::RenderFrameCreated(render_frame);
    if (callback_)
      callback_.Run(static_cast<TestRenderFrame*>(render_frame));
  }

 private:
  FrameCreatedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(FrameCreationObservingRendererClient);
};

// Expects observing the creation of a new frame, and creates an instance of
// FrameHostTestInterfaceRequestIssuerRenderFrame for that new frame to exercise
// its RemoteInterfaceProvider interface.
class ScopedNewFrameInterfaceProviderExerciser {
 public:
  explicit ScopedNewFrameInterfaceProviderExerciser(
      FrameCreationObservingRendererClient* frame_creation_observer,
      const base::Optional<GURL>& url_override_for_first_load = base::nullopt)
      : frame_creation_observer_(frame_creation_observer),
        url_override_for_first_load_(url_override_for_first_load) {
    frame_creation_observer_->set_callback(base::BindRepeating(
        &ScopedNewFrameInterfaceProviderExerciser::OnFrameCreated,
        base::Unretained(this)));
  }

  ~ScopedNewFrameInterfaceProviderExerciser() {
    frame_creation_observer_->reset_callback();
  }

  void ExpectNewFrameAndWaitForLoad(const GURL& expected_loaded_url) {
    ASSERT_NE(nullptr, frame_);
    frame_commit_waiter_->Wait();

    ASSERT_FALSE(frame_->current_history_item().IsNull());
    ASSERT_FALSE(frame_->GetWebFrame()->GetDocument().IsNull());
    EXPECT_EQ(expected_loaded_url,
              GURL(frame_->GetWebFrame()->GetDocument().Url()));

    interface_request_for_first_document_ =
        frame_->TakeLastInterfaceProviderRequest();
  }

  service_manager::mojom::InterfaceProviderRequest
  interface_request_for_initial_empty_document() {
    return std::move(interface_request_for_initial_empty_document_);
  };

  service_manager::mojom::InterfaceProviderRequest
  interface_request_for_first_document() {
    return std::move(interface_request_for_first_document_);
  }

 private:
  void OnFrameCreated(TestRenderFrame* frame) {
    ASSERT_EQ(nullptr, frame_);
    frame_ = frame;
    frame_commit_waiter_.emplace(frame);

    if (url_override_for_first_load_.has_value()) {
      frame_->SetURLOverrideForNextWebURLRequest(
          std::move(url_override_for_first_load_).value());
    }

    // The FrameHostTestInterfaceRequestIssuer needs to stay alive even after
    // this method returns, so that it continues to observe RenderFrame
    // lifecycle events and request test interfaces in response.
    test_request_issuer_.emplace(frame);
    test_request_issuer_->RequestTestInterfaceOnFrameEvent(
        kFrameEventDidCreateNewFrame);

    interface_request_for_initial_empty_document_ =
        frame->TakeLastInterfaceProviderRequest();
    EXPECT_TRUE(frame->current_history_item().IsNull());
  }

  FrameCreationObservingRendererClient* frame_creation_observer_;
  TestRenderFrame* frame_ = nullptr;
  base::Optional<GURL> url_override_for_first_load_;
  GURL first_committed_url_;

  base::Optional<FrameCommitWaiter> frame_commit_waiter_;
  base::Optional<FrameHostTestInterfaceRequestIssuer> test_request_issuer_;

  service_manager::mojom::InterfaceProviderRequest
      interface_request_for_initial_empty_document_;
  service_manager::mojom::InterfaceProviderRequest
      interface_request_for_first_document_;

  DISALLOW_COPY_AND_ASSIGN(ScopedNewFrameInterfaceProviderExerciser);
};

// Extracts all interface requests for FrameHostTestInterface pending on the
// specified |interface_provider_request|, and returns a list of the source
// annotations that are provided in the pending Ping() call for each of these
// FrameHostTestInterface requests.
void ExpectPendingInterfaceRequestsFromSources(
    service_manager::mojom::InterfaceProviderRequest interface_provider_request,
    std::vector<SourceAnnotation> expected_sources) {
  std::vector<SourceAnnotation> sources;
  ASSERT_TRUE(interface_provider_request.is_pending());
  TestSimpleInterfaceProviderImpl provider(
      mojom::FrameHostTestInterface::Name_,
      base::BindLambdaForTesting(
          [&sources](mojo::ScopedMessagePipeHandle handle) {
            FrameHostTestInterfaceImpl impl;
            impl.BindAndFlush(
                mojom::FrameHostTestInterfaceRequest(std::move(handle)));
            ASSERT_TRUE(impl.ping_source().has_value());
            sources.push_back(impl.ping_source().value());
          }));
  provider.BindAndFlush(std::move(interface_provider_request));
  EXPECT_THAT(sources, ::testing::ElementsAreArray(expected_sources));
}

}  // namespace

class RenderFrameRemoteInterfacesTest : public RenderViewTest {
 public:
  RenderFrameRemoteInterfacesTest() {}
  ~RenderFrameRemoteInterfacesTest() override {}

 protected:
  void SetUp() override {
    RenderViewTest::SetUp();
    LoadHTML("Nothing to see here.");
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
    // Do this before shutting down V8 in RenderViewTest::TearDown().
    // http://crbug.com/328552
    __lsan_do_leak_check();
#endif
    RenderViewTest::TearDown();
  }

  FrameCreationObservingRendererClient* frame_creation_observer() {
    DCHECK(frame_creation_observer_);
    return frame_creation_observer_;
  }

  TestRenderFrame* GetMainRenderFrame() {
    return static_cast<TestRenderFrame*>(view_->GetMainRenderFrame());
  }

  ContentRendererClient* CreateContentRendererClient() override {
    frame_creation_observer_ = new FrameCreationObservingRendererClient();
    return frame_creation_observer_;
  }

 private:
  // Owned by RenderViewTest.
  FrameCreationObservingRendererClient* frame_creation_observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameRemoteInterfacesTest);
};

// Expect that |remote_interfaces_| is bound before the first committed load in
// a child frame, and then re-bound on the first commit.
TEST_F(RenderFrameRemoteInterfacesTest, ChildFrameAtFirstCommittedLoad) {
  constexpr struct {
    const char* main_frame_url_override;
    const char* child_frame_url;
  } kTestCases[] = {
      {kTestFirstURL, kAboutBlankURL},
      {kTestSecondURL, "data:text/html,Child"},
      {kAboutBlankURL, kAboutBlankURL},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "main_frame_url = " << test_case.main_frame_url_override
                 << ", child_frame_url = " << test_case.child_frame_url);

    ScopedNewFrameInterfaceProviderExerciser child_frame_exerciser(
        frame_creation_observer());
    const std::string html = base::StringPrintf("<iframe src=\"%s\"></iframe>",
                                                test_case.child_frame_url);
    LoadHTMLWithUrlOverride(html.c_str(), test_case.main_frame_url_override);

    const GURL child_frame_url(test_case.child_frame_url);
    ASSERT_NO_FATAL_FAILURE(
        child_frame_exerciser.ExpectNewFrameAndWaitForLoad(child_frame_url));

    // TODO(https://crbug.com/792410): It is unfortunate how many internal
    // details of frame/document creation this encodes. Need to decouple.
    const GURL initial_empty_url(kAboutBlankURL);
    ExpectPendingInterfaceRequestsFromSources(
        child_frame_exerciser.interface_request_for_initial_empty_document(),
        {{GURL(kNoDocumentMarkerURL), kFrameEventDidCreateNewFrame},
         {initial_empty_url, kFrameEventDidCreateNewDocument},
         {initial_empty_url, kFrameEventDidCreateDocumentElement},
         {initial_empty_url, kFrameEventWillCommitProvisionalLoad},
         // TODO(https://crbug.com/555773): It seems strange that the new
         // document is created and DidCreateNewDocument is invoked *before* the
         // provisional load would have even committed.
         {child_frame_url, kFrameEventDidCreateNewDocument}});
    ExpectPendingInterfaceRequestsFromSources(
        child_frame_exerciser.interface_request_for_first_document(),
        {{child_frame_url, kFrameEventDidCommitProvisionalLoad},
         {child_frame_url, kFrameEventDidCreateDocumentElement}});
  }
}

// Expect that |remote_interfaces_| is bound before the first committed load in
// the main frame of an opened window, and then re-bound on the first commit.
TEST_F(RenderFrameRemoteInterfacesTest,
       MainFrameOfOpenedWindowAtFirstCommittedLoad) {
  constexpr struct {
    const char* main_frame_url_override;
    const char* new_window_url;
  } kTestCases[] = {
      {kTestFirstURL, kAboutBlankURL},
      {kTestSecondURL, "data:text/html,NewWindow"},
      {kAboutBlankURL, kAboutBlankURL},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "main_frame_url = " << test_case.main_frame_url_override
                 << ", new_window_url = " << test_case.new_window_url);

    const GURL new_window_url(test_case.new_window_url);
    ScopedNewFrameInterfaceProviderExerciser main_frame_exerciser(
        frame_creation_observer(), new_window_url);
    const std::string html =
        base::StringPrintf("<script>window.open(\"%s\", \"_blank\")</script>",
                           test_case.new_window_url);
    LoadHTMLWithUrlOverride(html.c_str(), test_case.main_frame_url_override);
    ASSERT_NO_FATAL_FAILURE(
        main_frame_exerciser.ExpectNewFrameAndWaitForLoad(new_window_url));

    // The URL of the initial empty document is "" for opened windows, in
    // contrast to child frames, where it is "about:blank". See
    // Document::Document and Document::SetURL for more details.
    //
    // Furthermore, for main frames, InitializeCoreFrame is invoked first, and
    // RenderFrameImpl::Initialize is invoked second, in contrast to child
    // frames where it is vice versa. ContentRendererClient::RenderFrameCreated
    // is invoked from RenderFrameImpl::Initialize, so we miss the events
    // related to initial empty document that is created from
    // InitializeCoreFrame, and there is already a document when
    // RenderFrameCreated is invoked.
    //
    // TODO(https://crbug.com/792410): It is unfortunate how many internal
    // details of frame/document creation this encodes. Need to decouple.
    const GURL initial_empty_url;
    ExpectPendingInterfaceRequestsFromSources(
        main_frame_exerciser.interface_request_for_initial_empty_document(),
        {{initial_empty_url, kFrameEventDidCreateNewFrame},
         {initial_empty_url, kFrameEventWillCommitProvisionalLoad},
         {new_window_url, kFrameEventDidCreateNewDocument}});
    ExpectPendingInterfaceRequestsFromSources(
        main_frame_exerciser.interface_request_for_first_document(),
        {{new_window_url, kFrameEventDidCommitProvisionalLoad},
         {new_window_url, kFrameEventDidCreateDocumentElement}});
  }
}

// Expect that |remote_interfaces_| is not bound to a new pipe if the first
// committed load in the child frame has the same security origin as that of the
// initial empty document.
//
// In this case, the LocalDOMWindow object associated with the initial empty
// document will be re-used for the newly committed document. Here, we must
// continue using the InterfaceProvider connection created for the initial empty
// document to support the following use-case:
//  1) Parent frame dynamically injects an <iframe>.
//  2) The parent frame calls `child.contentDocument.write(...)` to inject
//     Javascript that may stash objects on the child frame's global object
//     (LocalDOMWindow). Internally, these objects may be using Mojo services
//     exposed by the RenderFrameHost. The InterfaceRequests for these services
//     might still be en-route to the RemnderFrameHost's InterfaceProvider.
//  3) The `child` frame commits the first real load, and it is same-origin.
//  4) The global object in the child frame's browsing context is re-used.
//  5) Javascript objects stashed on the global object should continue to work.
//
// TODO(https://crbug.com/778318): Once the Window object inheritance is fixed,
// add a similar test for: <iframe src="javascript:'html'"></iframe>.
TEST_F(RenderFrameRemoteInterfacesTest,
       ChildFrameReusingWindowOfInitialDocument) {
  const GURL main_frame_url(kTestFirstURL);
  const GURL initial_empty_url(kAboutBlankURL);
  const GURL child_frame_url(kTestSecondURL);

  // Override the URL for the first navigation in the newly created frame to
  // |child_frame_url|.
  ScopedNewFrameInterfaceProviderExerciser child_frame_exerciser(
      frame_creation_observer(), child_frame_url);

  constexpr char kHTML[] = "<iframe srcdoc=\"Foo\"></iframe>";
  LoadHTMLWithUrlOverride(kHTML, main_frame_url.spec().c_str());

  ASSERT_NO_FATAL_FAILURE(
      child_frame_exerciser.ExpectNewFrameAndWaitForLoad(child_frame_url));

  ExpectPendingInterfaceRequestsFromSources(
      child_frame_exerciser.interface_request_for_initial_empty_document(),
      {{GURL(kNoDocumentMarkerURL), kFrameEventDidCreateNewFrame},
       {initial_empty_url, kFrameEventDidCreateNewDocument},
       {initial_empty_url, kFrameEventDidCreateDocumentElement},
       {initial_empty_url, kFrameEventWillCommitProvisionalLoad},
       {child_frame_url, kFrameEventDidCreateNewDocument},
       {child_frame_url, kFrameEventDidCommitProvisionalLoad},
       {child_frame_url, kFrameEventDidCreateDocumentElement}});

  auto request = child_frame_exerciser.interface_request_for_first_document();
  ASSERT_FALSE(request.is_pending());
}

// Expect that |remote_interfaces_| is bound to a new pipe on cross-document
// navigations.
TEST_F(RenderFrameRemoteInterfacesTest, ReplacedOnNonSameDocumentNavigation) {
  LoadHTMLWithUrlOverride("", kTestFirstURL);

  auto interface_provider_request_for_first_document =
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest();

  FrameHostTestInterfaceRequestIssuer requester(GetMainRenderFrame());
  requester.RequestTestInterfaceOnFrameEvent(kFrameEventAfterCommit);

  LoadHTMLWithUrlOverride("", kTestSecondURL);

  auto interface_provider_request_for_second_document =
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest();

  ASSERT_TRUE(interface_provider_request_for_first_document.is_pending());
  ExpectPendingInterfaceRequestsFromSources(
      std::move(interface_provider_request_for_first_document),
      {{GURL(kTestFirstURL), kFrameEventAfterCommit},
       {GURL(kTestFirstURL), kFrameEventWillCommitProvisionalLoad},
       {GURL(kTestSecondURL), kFrameEventDidCreateNewDocument}});

  ASSERT_TRUE(interface_provider_request_for_second_document.is_pending());
  ExpectPendingInterfaceRequestsFromSources(
      std::move(interface_provider_request_for_second_document),
      {{GURL(kTestSecondURL), kFrameEventDidCommitProvisionalLoad},
       {GURL(kTestSecondURL), kFrameEventDidCreateDocumentElement}});
}

// Expect that |remote_interfaces_| is not bound to a new pipe on same-document
// navigations, i.e. the existing InterfaceProvider connection is continued to
// be used.
TEST_F(RenderFrameRemoteInterfacesTest, ReusedOnSameDocumentNavigation) {
  LoadHTMLWithUrlOverride("", kTestFirstURL);

  auto interface_provider_request =
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest();

  FrameHostTestInterfaceRequestIssuer requester(GetMainRenderFrame());
  OnSameDocumentNavigation(GetMainFrame(), true /* is_new_navigation */);

  EXPECT_FALSE(
      GetMainRenderFrame()->TakeLastInterfaceProviderRequest().is_pending());

  ASSERT_TRUE(interface_provider_request.is_pending());
  ExpectPendingInterfaceRequestsFromSources(
      std::move(interface_provider_request),
      {{GURL(kTestFirstURL), kFrameEventDidCommitSameDocumentLoad}});
}

}  // namespace content
