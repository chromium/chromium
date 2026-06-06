// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/surface_embed/surface_embed_connector_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/input/cursor_manager.h"
#include "content/browser/compositor/surface_utils.h"
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

  RenderWidgetHostViewChildFrame* GetView(
      SurfaceEmbedConnectorImpl* connector) {
    return connector->view_;
  }

 protected:
  WebContentsImpl* GetParentWebContents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  struct ConnectorTestContext {
    ConnectorTestContext() = default;
    ConnectorTestContext(ConnectorTestContext&& other) {
      parent_shell = other.parent_shell;
      other.parent_shell = nullptr;
      parent_web_contents = other.parent_web_contents;
      other.parent_web_contents = nullptr;
      child_web_contents = std::move(other.child_web_contents);
      rwhvcf = other.rwhvcf;
      other.rwhvcf = nullptr;
      connector = other.connector;
      other.connector = nullptr;
    }
    ConnectorTestContext& operator=(ConnectorTestContext&& other) {
      if (this != &other) {
        parent_web_contents = nullptr;
        if (parent_shell) {
          Shell* shell = parent_shell;
          parent_shell = nullptr;
          shell->Close();
        }
        parent_shell = other.parent_shell;
        other.parent_shell = nullptr;
        parent_web_contents = other.parent_web_contents;
        other.parent_web_contents = nullptr;
        child_web_contents = std::move(other.child_web_contents);
        rwhvcf = other.rwhvcf;
        other.rwhvcf = nullptr;
        connector = other.connector;
        other.connector = nullptr;
      }
      return *this;
    }
    ~ConnectorTestContext() {
      parent_web_contents = nullptr;
      if (parent_shell) {
        Shell* shell = parent_shell;
        parent_shell = nullptr;
        shell->Close();
      }
    }

    raw_ptr<Shell> parent_shell = nullptr;
    raw_ptr<WebContents> parent_web_contents = nullptr;
    std::unique_ptr<WebContents> child_web_contents;
    raw_ptr<RenderWidgetHostViewChildFrame> rwhvcf = nullptr;
    raw_ptr<SurfaceEmbedConnectorImpl> connector = nullptr;
  };

  ConnectorTestContext SetupConnectorTest(
      MockSurfaceEmbedConnectorDelegate* delegate) {
    ConnectorTestContext context;

    context.parent_shell =
        Shell::CreateNewWindow(GetParentWebContents()->GetBrowserContext(),
                               GURL(), nullptr, Shell::GetShellDefaultSize());
    WebContentsImpl* parent_web_contents_impl =
        static_cast<WebContentsImpl*>(context.parent_shell->web_contents());
    context.parent_web_contents = parent_web_contents_impl;

    WebContents::CreateParams create_params(
        GetParentWebContents()->GetBrowserContext());
    context.child_web_contents = WebContents::Create(create_params);
    WebContentsImpl* child_web_contents_impl =
        static_cast<WebContentsImpl*>(context.child_web_contents.get());

    EXPECT_TRUE(NavigateToURL(parent_web_contents_impl, GURL("about:blank")));
    SurfaceEmbedConnector::Attach(
        child_web_contents_impl,
        parent_web_contents_impl->GetPrimaryMainFrame(), delegate);

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

  bool HasKeepSurfaceAlive(SurfaceEmbedConnectorImpl* connector) const {
    return !!connector->keep_surface_alive_;
  }

  bool HasParentWCObserver(SurfaceEmbedConnectorImpl* connector) const {
    return !!connector->parent_wc_observer_;
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
  EXPECT_TRUE(HasParentWCObserver(connector));

  context.parent_web_contents = nullptr;
  Shell* shell = context.parent_shell;
  context.parent_shell = nullptr;
  shell->Close();

  // Verify connector handles missing parent gracefully where checks exist.
  EXPECT_EQ(connector->GetParentWebContentsView(), nullptr);
  EXPECT_EQ(connector->GetParentRenderViewHostDelegateView(), nullptr);
  EXPECT_EQ(connector->GetInputEventRouter(), nullptr);
  EXPECT_EQ(connector->GetTextInputManager(), nullptr);
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

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, Detach) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);

  auto* child_impl =
      static_cast<WebContentsImpl*>(context.child_web_contents.get());

  auto* connector = child_impl->GetSurfaceEmbedConnector();
  connector->OnVisibilityChanged(
      blink::mojom::FrameVisibility::kRenderedInViewport);
  EXPECT_EQ(context.child_web_contents->GetVisibility(),
            content::Visibility::VISIBLE);

  // Detach should set it back to kNotRendered, although we cannot directly
  // observe the connector after detachment since it is destroyed. We ensure
  // calling Detach does not trigger failed CHECKs in performance_manager
  // (e.g., in FrameVisibilityDecorator::OnViewportIntersectionChanged and
  // FrameNodeImpl::SetViewportIntersection) and correctly clears the connector.
  context.connector = nullptr;  // Clear raw_ptr to avoid DanglingPtr crash.
  context.rwhvcf = nullptr;     // Clear raw_ptr to avoid DanglingPtr crash.
  SurfaceEmbedConnector::Detach(context.child_web_contents.get());
  EXPECT_FALSE(context.child_web_contents->GetSurfaceEmbedConnector());
  EXPECT_EQ(context.child_web_contents->GetVisibility(),
            content::Visibility::HIDDEN);
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
  SurfaceEmbedConnector::Attach(
      parent_impl, grandparent_impl->GetPrimaryMainFrame(), &parent_delegate);

  // Setup Child
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);
  WebContentsImpl* child_impl =
      static_cast<WebContentsImpl*>(child_web_contents.get());
  EXPECT_TRUE(NavigateToURL(parent_impl, GURL("about:blank")));
  SurfaceEmbedConnector::Attach(child_impl, parent_impl->GetPrimaryMainFrame(),
                                &child_delegate);
  auto* child_connector = static_cast<SurfaceEmbedConnectorImpl*>(
      child_impl->GetSurfaceEmbedConnector());

  // Verify that the child's root render widget host view is the grandparent's
  EXPECT_NE(grandparent_impl->GetRenderWidgetHostView(), nullptr);
  EXPECT_EQ(child_connector->GetRootRenderWidgetHostView(),
            grandparent_impl->GetRenderWidgetHostView());
  EXPECT_EQ(child_connector->GetParentRenderWidgetHostView(),
            parent_impl->GetRenderWidgetHostView());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       HiddenPluginCanBeCaptured) {
  MockSurfaceEmbedConnectorDelegate delegate;

  // Use shell()->web_contents() as parent since it is attached to a window and
  // has a compositor.
  auto* parent_impl = GetParentWebContents();
  EXPECT_TRUE(NavigateToURL(parent_impl, GURL("about:blank")));

  WebContents::CreateParams create_params(parent_impl->GetBrowserContext());
  auto child_web_contents = WebContents::Create(create_params);
  auto* child_impl = static_cast<WebContentsImpl*>(child_web_contents.get());

  SurfaceEmbedConnector::Attach(child_impl, parent_impl->GetPrimaryMainFrame(),
                                &delegate);
  auto* connector = static_cast<SurfaceEmbedConnectorImpl*>(
      child_impl->GetSurfaceEmbedConnector());
  auto* rwhvcf = static_cast<RenderWidgetHostViewChildFrame*>(
      child_impl->GetRenderWidgetHostView());

  connector->SetView(rwhvcf, false);

  // We need to wait for the surface ID to be valid.
  blink::FrameVisualProperties visual_properties;
  display::ScreenInfo screen_info;
  screen_info.device_scale_factor = 2.0f;
  visual_properties.screen_infos = display::ScreenInfos(screen_info);
  visual_properties.local_surface_id =
      viz::LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(2, 3));
  connector->SynchronizeVisualProperties(visual_properties, false);

  // Ensure the plugin is hidden.
  connector->OnVisibilityChanged(blink::mojom::FrameVisibility::kNotRendered);

  // Increment the capturer count, which should keep the surface alive.
  auto capturer = child_impl->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/true, /*stay_awake=*/true,
      /*is_activity=*/true);

  // Attempt to take a screenshot. It should not fail with kNotImplemented.
  // It may still fail for other reasons depending on test harness
  // initialization, but it will have bypassed the IsSurfaceAvailableForCopy
  // check.
  base::test::TestFuture<const content::CopyFromSurfaceResult&> future;
  child_impl->GetRenderWidgetHostView()->CopyFromSurface(
      gfx::Rect(), gfx::Size(), base::Milliseconds(1), future.GetCallback());

  // We expect it either to succeed, or fail with a different error (like
  // kTimeout or kFrameGone), but definitely NOT kNotImplemented which is
  // returned when IsSurfaceAvailableForCopy is false.
  if (!future.Get().has_value()) {
    EXPECT_NE(future.Get().error(), CopyFromSurfaceError::kNotImplemented);
  }
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       HideSetsKeepSurfaceAliveFalse) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);

  context.rwhvcf->Hide();
  EXPECT_FALSE(HasKeepSurfaceAlive(context.connector.get()));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       ResizeIgnoredForEmbedded) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);
  auto* child_impl =
      static_cast<WebContentsImpl*>(context.child_web_contents.get());

  // Navigate the child to ensure it has a RenderFrame and a view.
  EXPECT_TRUE(
      NavigateToURL(context.child_web_contents.get(), GURL("about:blank")));
  // Re-fetch rwhvcf after navigation as it might have changed.
  context.rwhvcf = static_cast<RenderWidgetHostViewChildFrame*>(
      child_impl->GetRenderWidgetHostView());
  ASSERT_TRUE(context.rwhvcf);
  ASSERT_EQ(context.rwhvcf->FrameConnectorForTesting(),
            context.connector.get());

  // Initialize screen_infos_ to ensure a valid device scale factor.
  display::ScreenInfo screen_info;
  screen_info.device_scale_factor = 1.0f;
  SetScreenInfos(context.connector, display::ScreenInfos(screen_info));

  // Set an initial size via the connector (the "correct" way for embedded).
  gfx::Rect initial_bounds(0, 0, 100, 100);
  context.connector->SetRectInParentView(initial_bounds);
  context.connector->SetLocalFrameSize(initial_bounds.size());
  EXPECT_EQ(initial_bounds, context.connector->GetRectInParentViewInDip());
  EXPECT_EQ(initial_bounds.size(), context.rwhvcf->GetViewBounds().size());

  // Attempt to resize via WebContents::Resize (the "ignored" way for embedded).
  gfx::Rect ignored_bounds(10, 10, 200, 200);
  child_impl->Resize(ignored_bounds);

  // The size should remain unchanged.
  EXPECT_EQ(initial_bounds.size(), context.rwhvcf->GetViewBounds().size());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       KeepSurfaceAlivePersistsAcrossInvalidSurfaceState) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);
  auto* connector = context.connector.get();

  // Ensure we start with an invalid surface ID.
  ASSERT_FALSE(context.rwhvcf->GetCurrentSurfaceId().is_valid());

  // Request to keep surface alive.
  connector->SetKeepSurfaceAlive(true);

  // The intent should be saved, but we cannot actually keep it alive yet
  // because the surface ID is invalid.
  EXPECT_TRUE(connector->IsKeepingAlive());
  EXPECT_FALSE(HasKeepSurfaceAlive(connector));

  // Now make the surface ID valid by synchronizing visual properties.
  blink::FrameVisualProperties visual_properties;
  display::ScreenInfo screen_info;
  screen_info.device_scale_factor = 1.0f;
  visual_properties.screen_infos = display::ScreenInfos(screen_info);
  visual_properties.local_surface_id =
      viz::LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(2, 3));
  connector->SynchronizeVisualProperties(visual_properties, false);

  // The surface ID should now be valid.
  ASSERT_TRUE(context.rwhvcf->GetCurrentSurfaceId().is_valid());

  // The keep-alive should have been automatically refreshed and now be active.
  EXPECT_TRUE(connector->IsKeepingAlive());
  EXPECT_TRUE(HasKeepSurfaceAlive(connector));
}

// Tests that GetFocusedWebContents, GetFocusedFrameTree, GetFocusedFrame,
// ContainsOrIsFocusedWebContents, and GetFocusedRenderWidgetHost return correct
// values when a child WebContents is embedded via SurfaceEmbed.
IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       FocusBehaviorWithSurfaceEmbed) {
  MockSurfaceEmbedConnectorDelegate connector_delegate;

  // Use shell's WebContents as the parent (has proper view and focused frame).
  WebContentsImpl* parent_impl = GetParentWebContents();
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(parent_impl,
                            embedded_test_server()->GetURL("/title1.html")));
  // Wait for the focused frame to be set after navigation.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return parent_impl->GetFocusedFrame() != nullptr; }));

  WebContents::CreateParams create_params(parent_impl->GetBrowserContext());
  std::unique_ptr<WebContents> child_wc = WebContents::Create(create_params);
  auto* child_impl = static_cast<WebContentsImpl*>(child_wc.get());

  SurfaceEmbedConnector::Attach(child_impl, parent_impl->GetPrimaryMainFrame(),
                                &connector_delegate);

  RenderWidgetHostImpl* parent_main_rwh =
      parent_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
  RenderWidgetHostImpl* child_rwh =
      child_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();

  // Initially, focus is on the parent. The child does NOT contain focus.
  EXPECT_FALSE(child_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(nullptr, child_impl->GetFocusedWebContents());
  EXPECT_EQ(nullptr, child_impl->GetFocusedFrameTree());
  EXPECT_EQ(nullptr, child_impl->GetFocusedFrame());
  EXPECT_EQ(nullptr, child_impl->GetFocusedRenderWidgetHost(child_rwh));

  // The parent should report itself as focused.
  EXPECT_TRUE(parent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(parent_impl, parent_impl->GetFocusedWebContents());
  EXPECT_EQ(&parent_impl->GetPrimaryFrameTree(),
            parent_impl->GetFocusedFrameTree());
  EXPECT_EQ(parent_impl->GetPrimaryMainFrame(), parent_impl->GetFocusedFrame());
  EXPECT_EQ(parent_main_rwh,
            parent_impl->GetFocusedRenderWidgetHost(parent_main_rwh));

  // Move focus to the child.
  child_impl->SetAsFocusedWebContentsIfNecessary();

  // Wait for async focus propagation.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return child_impl->GetFocusedFrame() != nullptr; }));

  EXPECT_TRUE(child_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(child_impl, child_impl->GetFocusedWebContents());
  EXPECT_EQ(&child_impl->GetPrimaryFrameTree(),
            child_impl->GetFocusedFrameTree());
  EXPECT_EQ(child_impl->GetPrimaryMainFrame(), child_impl->GetFocusedFrame());
  EXPECT_EQ(child_rwh, child_impl->GetFocusedRenderWidgetHost(child_rwh));

  // The parent should report the child as focused.
  EXPECT_TRUE(parent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(child_impl, parent_impl->GetFocusedWebContents());
  EXPECT_EQ(&child_impl->GetPrimaryFrameTree(),
            parent_impl->GetFocusedFrameTree());
  EXPECT_EQ(child_impl->GetPrimaryMainFrame(), parent_impl->GetFocusedFrame());
  EXPECT_EQ(child_rwh,
            parent_impl->GetFocusedRenderWidgetHost(parent_main_rwh));

  // Move focus back to the parent.
  parent_impl->SetAsFocusedWebContentsIfNecessary();

  // The child should no longer be focused.
  EXPECT_FALSE(child_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(nullptr, child_impl->GetFocusedWebContents());
  EXPECT_EQ(nullptr, child_impl->GetFocusedFrameTree());
  EXPECT_EQ(nullptr, child_impl->GetFocusedFrame());
  EXPECT_EQ(nullptr, child_impl->GetFocusedRenderWidgetHost(child_rwh));

  // The parent should be focused again.
  EXPECT_TRUE(parent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(parent_impl, parent_impl->GetFocusedWebContents());
  EXPECT_EQ(&parent_impl->GetPrimaryFrameTree(),
            parent_impl->GetFocusedFrameTree());
  EXPECT_EQ(parent_impl->GetPrimaryMainFrame(), parent_impl->GetFocusedFrame());
  EXPECT_EQ(parent_main_rwh,
            parent_impl->GetFocusedRenderWidgetHost(parent_main_rwh));
}

// Tests focus behavior with multi-level surface embed nesting:
// grandparent -> parent -> child. Uses FocusOwningWebContents() to move focus
// and verifies GetFocusedWebContents, GetFocusedFrame,
// ContainsOrIsFocusedWebContents, and GetFocusedRenderWidgetHost at each level.
IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       FocusBehaviorMultiLevelSurfaceEmbed) {
  MockSurfaceEmbedConnectorDelegate child_connector_delegate;
  MockSurfaceEmbedConnectorDelegate parent_connector_delegate;

  // Use shell's WebContents as the grandparent (root, has proper view).
  WebContentsImpl* grandparent_impl = GetParentWebContents();
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(grandparent_impl,
                            embedded_test_server()->GetURL("/title1.html")));
  // Wait for the focused frame to be set after navigation.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return grandparent_impl->GetFocusedFrame() != nullptr; }));

  WebContents::CreateParams create_params(
      grandparent_impl->GetBrowserContext());
  std::unique_ptr<WebContents> parent_wc = WebContents::Create(create_params);
  auto* parent_impl = static_cast<WebContentsImpl*>(parent_wc.get());
  ASSERT_TRUE(NavigateToURL(parent_impl, GURL("about:blank")));
  SurfaceEmbedConnector::Attach(parent_impl,
                                grandparent_impl->GetPrimaryMainFrame(),
                                &parent_connector_delegate);

  std::unique_ptr<WebContents> child_wc = WebContents::Create(create_params);
  auto* child_impl = static_cast<WebContentsImpl*>(child_wc.get());
  SurfaceEmbedConnector::Attach(child_impl, parent_impl->GetPrimaryMainFrame(),
                                &child_connector_delegate);

  RenderWidgetHostImpl* grandparent_rwh =
      grandparent_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
  RenderWidgetHostImpl* parent_rwh =
      parent_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
  RenderWidgetHostImpl* child_rwh =
      child_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();

  // Initially grandparent is focused. Parent and child should return nullptr.
  EXPECT_TRUE(grandparent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_FALSE(parent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_FALSE(child_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(grandparent_impl, grandparent_impl->GetFocusedWebContents());
  EXPECT_EQ(nullptr, parent_impl->GetFocusedWebContents());
  EXPECT_EQ(nullptr, child_impl->GetFocusedWebContents());
  EXPECT_EQ(grandparent_impl->GetPrimaryMainFrame(),
            grandparent_impl->GetFocusedFrame());
  EXPECT_EQ(nullptr, parent_impl->GetFocusedFrame());
  EXPECT_EQ(nullptr, child_impl->GetFocusedFrame());
  EXPECT_EQ(grandparent_rwh,
            grandparent_impl->GetFocusedRenderWidgetHost(grandparent_rwh));
  EXPECT_EQ(nullptr, parent_impl->GetFocusedRenderWidgetHost(parent_rwh));
  EXPECT_EQ(nullptr, child_impl->GetFocusedRenderWidgetHost(child_rwh));

  // Focus the child using FocusOwningWebContents.
  child_impl->FocusOwningWebContents(child_rwh);

  // Wait for async focus propagation.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return child_impl->GetFocusedFrame() != nullptr; }));

  EXPECT_TRUE(grandparent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_TRUE(parent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_TRUE(child_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(child_impl, grandparent_impl->GetFocusedWebContents());
  EXPECT_EQ(child_impl, parent_impl->GetFocusedWebContents());
  EXPECT_EQ(child_impl, child_impl->GetFocusedWebContents());
  EXPECT_EQ(child_impl->GetPrimaryMainFrame(),
            grandparent_impl->GetFocusedFrame());
  EXPECT_EQ(child_impl->GetPrimaryMainFrame(), parent_impl->GetFocusedFrame());
  EXPECT_EQ(child_impl->GetPrimaryMainFrame(), child_impl->GetFocusedFrame());
  EXPECT_EQ(child_rwh,
            grandparent_impl->GetFocusedRenderWidgetHost(grandparent_rwh));
  EXPECT_EQ(child_rwh, parent_impl->GetFocusedRenderWidgetHost(parent_rwh));
  EXPECT_EQ(child_rwh, child_impl->GetFocusedRenderWidgetHost(child_rwh));

  // Focus the parent (intermediate level).
  parent_impl->FocusOwningWebContents(parent_rwh);

  // Wait for async focus propagation.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return parent_impl->GetFocusedFrame() != nullptr; }));

  EXPECT_TRUE(grandparent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_TRUE(parent_impl->ContainsOrIsFocusedWebContents());
  EXPECT_FALSE(child_impl->ContainsOrIsFocusedWebContents());
  EXPECT_EQ(parent_impl, grandparent_impl->GetFocusedWebContents());
  EXPECT_EQ(parent_impl, parent_impl->GetFocusedWebContents());
  EXPECT_EQ(nullptr, child_impl->GetFocusedWebContents());
  EXPECT_EQ(parent_impl->GetPrimaryMainFrame(),
            grandparent_impl->GetFocusedFrame());
  EXPECT_EQ(parent_impl->GetPrimaryMainFrame(), parent_impl->GetFocusedFrame());
  EXPECT_EQ(nullptr, child_impl->GetFocusedFrame());
  EXPECT_EQ(parent_rwh,
            grandparent_impl->GetFocusedRenderWidgetHost(grandparent_rwh));
  EXPECT_EQ(parent_rwh, parent_impl->GetFocusedRenderWidgetHost(parent_rwh));
  EXPECT_EQ(nullptr, child_impl->GetFocusedRenderWidgetHost(child_rwh));
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, UpdateCursor) {
  MockSurfaceEmbedConnectorDelegate delegate;
  auto context = SetupConnectorTest(&delegate);

  // Ensure the view is set.
  if (!GetView(context.connector.get())) {
    context.connector->SetView(context.rwhvcf, false);
  }
  ASSERT_TRUE(GetView(context.connector.get()));

  RenderWidgetHostViewBase* root_view =
      context.connector->GetRootRenderWidgetHostView();
  ASSERT_TRUE(root_view);

  input::CursorManager* cursor_manager = root_view->GetCursorManager();

  // Verify that updating the cursor works.
  context.connector->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kHand));

  if (cursor_manager) {
    ui::Cursor cursor;
    EXPECT_TRUE(cursor_manager->GetCursorForTesting(
        GetView(context.connector.get()), cursor));
    EXPECT_EQ(ui::mojom::CursorType::kHand, cursor.type());
  }

  // Verify that updating the cursor does not crash when there is no root view
  // (e.g. parent is destroyed).
  auto* connector_ptr = context.connector.get();
  context.connector = nullptr;  // Clear raw_ptr to avoid DanglingPtr check.
  context.rwhvcf = nullptr;     // Clear raw_ptr to avoid DanglingPtr check.

  context.parent_web_contents = nullptr;
  Shell* shell = context.parent_shell;
  context.parent_shell = nullptr;
  if (shell) {
    shell->Close();
  }

  // This should not crash.
  connector_ptr->UpdateCursor(ui::Cursor(ui::mojom::CursorType::kPointer));
}

}  // namespace content
