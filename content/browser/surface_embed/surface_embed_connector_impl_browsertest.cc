// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/surface_embed/surface_embed_connector_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "cc/input/touch_action.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

class MockSurfaceEmbedConnectorDelegate
    : public SurfaceEmbedConnector::Delegate {
 public:
  MockSurfaceEmbedConnectorDelegate() = default;
  ~MockSurfaceEmbedConnectorDelegate() = default;

  MOCK_METHOD(void, SetFrameSinkId, (const viz::FrameSinkId&), (override));
  MOCK_METHOD(void,
              UpdateLocalSurfaceIdFromChild,
              (const viz::LocalSurfaceId&),
              (override));
  MOCK_METHOD(void, DetachedByHost, (), (override));
  MOCK_METHOD(bool, IsAttachedForTesting, (), (const, override));
  MOCK_METHOD(void, ChildProcessGone, (), (override));
  MOCK_METHOD(void, RequestFocus, (), (override));
};

}  // namespace

class SurfaceEmbedConnectorImplBrowserTest : public ContentBrowserTest {
 public:
  SurfaceEmbedConnectorImplBrowserTest() = default;
  ~SurfaceEmbedConnectorImplBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  void SetScreenInfos(SurfaceEmbedConnectorImpl* connector,
                      const display::ScreenInfos& screen_infos) {
    connector->screen_infos_ = screen_infos;
  }

  void SetLocalSurfaceId(SurfaceEmbedConnectorImpl* connector,
                         const viz::LocalSurfaceId& local_surface_id) {
    connector->local_surface_id_ = local_surface_id;
  }

 protected:
  WebContentsImpl* GetParentWebContents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  struct ConnectorTestContext {
    std::unique_ptr<WebContents> child_web_contents;
    raw_ptr<RenderWidgetHostViewChildFrame> rwhvcf;
    std::unique_ptr<WebContents> parent_web_contents;
    raw_ptr<SurfaceEmbedConnectorImpl> connector;
  };

  ConnectorTestContext SetupConnectorTest(
      MockSurfaceEmbedConnectorDelegate* delegate) {
    ConnectorTestContext context;

    WebContents::CreateParams create_params(
        GetParentWebContents()->GetBrowserContext());
    context.parent_web_contents = WebContents::Create(create_params);
    WebContentsImpl* parent_web_contents_impl =
        static_cast<WebContentsImpl*>(context.parent_web_contents.get());

    context.child_web_contents = WebContents::Create(create_params);
    WebContentsImpl* child_web_contents_impl =
        static_cast<WebContentsImpl*>(context.child_web_contents.get());

    SurfaceEmbedConnector::Attach(child_web_contents_impl,
                                  parent_web_contents_impl, delegate);

    context.connector = static_cast<SurfaceEmbedConnectorImpl*>(
        child_web_contents_impl->GetSurfaceEmbedConnector());
    context.rwhvcf = static_cast<RenderWidgetHostViewChildFrame*>(
        child_web_contents_impl->GetRenderWidgetHostView());

    EXPECT_TRUE(context.rwhvcf);

    return context;
  }

  void SetViewportIntersectionState(
      SurfaceEmbedConnectorImpl* connector,
      const blink::mojom::ViewportIntersectionState& intersection_state) {
    connector->intersection_state_ = intersection_state;
  }
};

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, BasicConnection) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);
  auto* connector = context.connector.get();
  auto* parent_impl =
      static_cast<WebContentsImpl*>(context.parent_web_contents.get());

  // Verify initial state and basic getters.
  EXPECT_EQ(connector->GetParentWebContentsView(), parent_impl->GetView());
  EXPECT_EQ(connector->GetParentRenderViewHostDelegateView(),
            parent_impl->GetDelegateView());
  EXPECT_EQ(connector->GetInputEventRouter(),
            parent_impl->GetInputEventRouter());

  // Verify delegate access.
  EXPECT_EQ(connector->GetDelegate(), &delegate);

  // Verify TextInputManager is forwarded.
  EXPECT_EQ(connector->GetTextInputManager(),
            parent_impl->GetTextInputManager());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       ParentDestruction) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);
  auto* connector = context.connector.get();
  auto* parent_impl =
      static_cast<WebContentsImpl*>(context.parent_web_contents.get());

  EXPECT_EQ(connector->GetParentWebContentsView(), parent_impl->GetView());

  // Destroy the parent.
  context.parent_web_contents.reset();

  // Verify connector handles missing parent gracefully where checks exist.
  EXPECT_EQ(connector->GetParentWebContentsView(), nullptr);
  EXPECT_EQ(connector->GetParentRenderViewHostDelegateView(), nullptr);

  // Note: GetInputEventRouter() and GetTextInputManager() in
  // SurfaceEmbedConnectorImpl currently do not check for null parent, so we
  // don't test them here to avoid crash.
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, ConstGetters) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);
  auto* connector = context.connector.get();
  auto* parent_impl =
      static_cast<WebContentsImpl*>(context.parent_web_contents.get());

  const SurfaceEmbedConnectorImpl& const_connector = *connector;

  // Verify getters can be called on a const reference.
  EXPECT_EQ(const_connector.GetParentWebContentsView(), parent_impl->GetView());
  EXPECT_EQ(const_connector.GetParentRenderViewHostDelegateView(),
            parent_impl->GetDelegateView());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, Attach) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);
  auto* parent_impl =
      static_cast<WebContentsImpl*>(context.parent_web_contents.get());

  // Verify the connector is attached to the child WebContents.
  auto* connector = context.child_web_contents->GetSurfaceEmbedConnector();
  ASSERT_TRUE(connector);

  // Verify properties.
  EXPECT_EQ(connector->GetDelegate(), &delegate);
  EXPECT_EQ(static_cast<SurfaceEmbedConnectorImpl*>(connector)
                ->GetParentWebContentsView(),
            parent_impl->GetView());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       FrameConnectorImplementation) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);
  auto* connector = context.connector.get();

  // Verify FrameConnector implementation defaults.
  //
  // TODO(cammie): Many of these expectations will need to change when the stub
  // implementations in SurfaceEmbedConnectorImpl are replaced with real ones.
  // For example, methods like HasSize(), IsInert(), and LockPointer() currently
  // return hardcoded default values (false, false, and kUnknownError
  // respectively). These tests should be updated to verify the actual behavior
  // once implemented.

  EXPECT_EQ(connector->GetParentRenderWidgetHostView(), nullptr);
  EXPECT_TRUE(
      NavigateToURL(context.parent_web_contents.get(), GURL("about:blank")));
  EXPECT_NE(connector->GetParentRenderWidgetHostView(), nullptr);
  EXPECT_EQ(connector->GetRootRenderWidgetHostView(),
            connector->GetParentRenderWidgetHostView());

  // RenderProcessGone just forwards to the delegate's ChildProcessGone.
  EXPECT_CALL(delegate, ChildProcessGone()).Times(1);
  connector->RenderProcessGone();

  // These return void, just call them to ensure no crash.
  connector->FirstSurfaceActivation(viz::SurfaceInfo());
  connector->SendIntrinsicSizingInfoToParent(nullptr);

  // We should construct a proper FrameVisualProperties to avoid crashes when
  // accessing screen_infos.
  blink::FrameVisualProperties visual_properties;
  display::ScreenInfo screen_info;
  screen_info.device_scale_factor = 2.0f;
  visual_properties.screen_infos = display::ScreenInfos(screen_info);
  visual_properties.local_frame_size = gfx::Size(100, 200);
  visual_properties.rect_in_local_root = gfx::Rect(10, 20, 300, 400);
  visual_properties.capture_sequence_number = 5u;
  visual_properties.css_zoom_factor = 1.25;
  visual_properties.local_surface_id =
      viz::LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(2, 3));

  connector->SynchronizeVisualProperties(visual_properties, false);

  connector->UpdateCursor(ui::Cursor());

  EXPECT_EQ(connector->HasFocus(),
            FrameConnector::RootViewFocusState::kNullView);

  connector->FocusRootView();  // void

  EXPECT_EQ(connector->LockPointer(false),
            blink::mojom::PointerLockResult::kUnknownError);
  EXPECT_EQ(connector->ChangePointerLock(false),
            blink::mojom::PointerLockResult::kUnknownError);

  connector->UnlockPointer();  // void

  EXPECT_TRUE(connector->HasSize());

  EXPECT_EQ(connector->GetScreenInfos().current().device_scale_factor, 2.0f);
  EXPECT_EQ(connector->GetLocalSurfaceId(), visual_properties.local_surface_id);

  // Just check it returns valid reference
  connector->GetIntersectionState();

  EXPECT_EQ(connector->GetCaptureSequenceNumber(), 5u);

  EXPECT_EQ(connector->GetRectInParentViewInDip(), gfx::Rect(5, 10, 150, 200));
  EXPECT_EQ(connector->GetLocalFrameSizeInDip(), gfx::Size(50, 100));
  EXPECT_EQ(connector->GetLocalFrameSizeInPixels(), gfx::Size(100, 200));

  EXPECT_EQ(connector->GetCssZoomFactor(), 1.25);

  connector->EnableAutoResize(gfx::Size(), gfx::Size());  // void
  connector->DisableAutoResize();                         // void

  EXPECT_FALSE(connector->IsInert());
  EXPECT_EQ(connector->InheritedEffectiveTouchAction(), cc::TouchAction::kAuto);

  // IsHidden() defaults to false initially.
  EXPECT_FALSE(connector->IsHidden());

  EXPECT_FALSE(connector->IsThrottled());
  EXPECT_FALSE(connector->IsSubtreeThrottled());
  EXPECT_FALSE(connector->IsDisplayLocked());

  connector->DidUpdateVisualProperties(cc::RenderFrameMetadata());  // void
  connector->SetVisibilityForChildViews(true);                      // void

  // Test updating local frame size separately.
  connector->SetLocalFrameSize(gfx::Size(400, 400));
  EXPECT_EQ(connector->GetLocalFrameSizeInPixels(), gfx::Size(400, 400));
  EXPECT_EQ(connector->GetLocalFrameSizeInDip(), gfx::Size(200, 200));

  // Test updating rect in parent view.
  connector->SetRectInParentView(gfx::Rect(100, 100, 200, 200));
  EXPECT_EQ(connector->GetRectInParentViewInDip(), gfx::Rect(50, 50, 100, 100));

  connector->OnVisibilityChanged(
      blink::mojom::FrameVisibility::kRenderedInViewport);

  blink::mojom::ViewportIntersectionState intersection;
  intersection.viewport_intersection = gfx::Rect(0, 0, 100, 100);
  SetViewportIntersectionState(connector, intersection);

  EXPECT_TRUE(connector->IsVisible());
  EXPECT_FALSE(connector->IsHidden());

  connector->OnVisibilityChanged(blink::mojom::FrameVisibility::kNotRendered);
  EXPECT_FALSE(connector->IsVisible());
  EXPECT_FALSE(connector->IsHidden());

  connector->DelegateWasShown();

  EXPECT_EQ(connector->EmbedderVisibility(), Visibility::VISIBLE);

  EXPECT_EQ(connector->GetParentViewInput(),
            connector->GetParentRenderWidgetHostView());
  EXPECT_EQ(connector->GetRootViewInput(),
            connector->GetParentRenderWidgetHostView());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, SetView) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);

  EXPECT_CALL(delegate, SetFrameSinkId(testing::_)).Times(1);
  FrameConnector* original_connector =
      context.rwhvcf->FrameConnectorForTesting();

  context.connector->SetView(context.rwhvcf, false);

  EXPECT_EQ(context.rwhvcf->FrameConnectorForTesting(),
            context.connector.get());

  context.connector->SetView(nullptr, false);

  EXPECT_EQ(context.rwhvcf->FrameConnectorForTesting(), nullptr);
  context.rwhvcf->SetFrameConnector(original_connector);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       SetViewReplacesViewAndVisibility) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);

  FrameConnector* original_connector =
      context.rwhvcf->FrameConnectorForTesting();

  EXPECT_CALL(delegate, SetFrameSinkId(testing::_)).Times(3);

  context.connector->SetView(context.rwhvcf, false);

  // Call it again to hit the `if (view_)` replacement path and coverage.
  context.connector->SetView(context.rwhvcf, false);

  // Now test SetView when visibility is not kRenderedInViewport.
  context.connector->OnVisibilityChanged(
      blink::mojom::FrameVisibility::kNotRendered);
  context.connector->SetView(context.rwhvcf, false);

  context.connector->SetView(nullptr, false);
  context.rwhvcf->SetFrameConnector(original_connector);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       ChildNavigationReplacesView) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);

  // TODO(crbug.com/479743223): Remove mocked screen info once visual data
  // plumbs through the connector.
  // Initialize screen_infos_ to prevent crashes during navigation.
  display::ScreenInfo screen_info;
  screen_info.display_id = 1;
  screen_info.device_scale_factor = 1.0f;
  screen_info.rect = gfx::Rect(800, 600);
  screen_info.available_rect = gfx::Rect(800, 600);
  SetScreenInfos(context.connector, display::ScreenInfos(screen_info));
  SetLocalSurfaceId(context.connector,
                    viz::LocalSurfaceId(1, base::UnguessableToken::Create()));

  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(context.parent_web_contents.get(), GURL("about:blank")));

  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(context.child_web_contents.get(), url_a));

  auto* old_view = context.child_web_contents->GetRenderWidgetHostView();
  EXPECT_TRUE(old_view);

  // Clear the raw_ptr in context since the old view will be destroyed
  // during the cross-process navigation, which triggers DanglingPtr checks.
  context.rwhvcf = nullptr;

  // Force the old view to fetch the updated screen_infos_ from the connector.
  // This ensures the speculative RenderWidgetHostView created during navigation
  // inherits a valid ScreenInfos object, preventing crashes.
  static_cast<RenderWidgetHostViewBase*>(old_view)->UpdateScreenInfo();

  // The new view creation is asynchronous.
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(context.child_web_contents.get(), url_b));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return context.child_web_contents->GetRenderWidgetHostView() != old_view;
  }));

  auto* new_view = context.child_web_contents->GetRenderWidgetHostView();
  EXPECT_NE(old_view, new_view);
  ASSERT_TRUE(new_view);

  // Verify that the connector was registered with the new view.
  ASSERT_TRUE(static_cast<RenderWidgetHostViewBase*>(new_view)
                  ->IsRenderWidgetHostViewChildFrame());
  auto* new_rwhvcf = static_cast<RenderWidgetHostViewChildFrame*>(new_view);
  EXPECT_EQ(new_rwhvcf->FrameConnectorForTesting(), context.connector);

  // Clean up.
  context.connector->SetView(nullptr, false);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       PropagateLocalSurfaceId) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);

  cc::RenderFrameMetadata metadata;
  metadata.local_surface_id =
      viz::LocalSurfaceId(1, base::UnguessableToken::Create());

  EXPECT_CALL(delegate,
              UpdateLocalSurfaceIdFromChild(*metadata.local_surface_id))
      .Times(1);

  // SurfaceEmbedConnectorImpl inherits from
  // FrameConnector so we can call
  // DidUpdateVisualProperties directly.
  context.connector->DidUpdateVisualProperties(metadata);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       MultiLevelEmbeddingGetRootRenderWidgetHostView) {
  MockSurfaceEmbedConnectorDelegate child_delegate;
  MockSurfaceEmbedConnectorDelegate parent_delegate;

  // Setup Grandparent (Root)
  WebContents::CreateParams create_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> grandparent_web_contents =
      WebContents::Create(create_params);
  WebContentsImpl* grandparent_impl =
      static_cast<WebContentsImpl*>(grandparent_web_contents.get());
  EXPECT_TRUE(NavigateToURL(grandparent_impl, GURL("about:blank")));

  // Setup Parent
  std::unique_ptr<WebContents> parent_web_contents =
      WebContents::Create(create_params);
  WebContentsImpl* parent_impl =
      static_cast<WebContentsImpl*>(parent_web_contents.get());
  SurfaceEmbedConnector::Attach(parent_impl, grandparent_impl,
                                &parent_delegate);

  // Setup Child
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);
  WebContentsImpl* child_impl =
      static_cast<WebContentsImpl*>(child_web_contents.get());
  SurfaceEmbedConnector::Attach(child_impl, parent_impl, &child_delegate);

  auto* child_connector = static_cast<SurfaceEmbedConnectorImpl*>(
      child_impl->GetSurfaceEmbedConnector());

  // Verify that the child's root render widget host view is the grandparent's
  EXPECT_NE(grandparent_impl->GetRenderWidgetHostView(), nullptr);
  EXPECT_EQ(child_connector->GetRootRenderWidgetHostView(),
            grandparent_impl->GetRenderWidgetHostView());
  EXPECT_EQ(child_connector->GetParentRenderWidgetHostView(),
            parent_impl->GetRenderWidgetHostView());
}

}  // namespace content
