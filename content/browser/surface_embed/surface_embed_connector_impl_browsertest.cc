// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/surface_embed/surface_embed_connector_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "cc/input/touch_action.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
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
};

}  // namespace

class SurfaceEmbedConnectorImplBrowserTest : public ContentBrowserTest {
 public:
  SurfaceEmbedConnectorImplBrowserTest() = default;
  ~SurfaceEmbedConnectorImplBrowserTest() override = default;

 protected:
  WebContentsImpl* GetParentWebContents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  std::unique_ptr<SurfaceEmbedConnectorImpl> CreateConnector(
      WebContents* child_web_contents,
      WebContents* parent_web_contents,
      SurfaceEmbedConnector::Delegate* delegate) {
    return base::WrapUnique(new SurfaceEmbedConnectorImpl(
        child_web_contents, parent_web_contents, delegate));
  }
};

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, BasicConnection) {
  // Create the child WebContents.
  WebContents::CreateParams create_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);

  MockSurfaceEmbedConnectorDelegate delegate;

  // Create the connector.
  auto connector = CreateConnector(child_web_contents.get(),
                                   GetParentWebContents(), &delegate);

  // Verify initial state and basic getters.
  EXPECT_EQ(connector->GetParentWebContentsView(),
            GetParentWebContents()->GetView());
  EXPECT_EQ(connector->GetParentRenderViewHostDelegateView(),
            GetParentWebContents()->GetDelegateView());
  EXPECT_EQ(connector->GetInputEventRouter(),
            GetParentWebContents()->GetInputEventRouter());

  // Verify delegate access.
  EXPECT_EQ(connector->GetDelegate(), &delegate);

  // Verify TextInputManager is forwarded.
  EXPECT_EQ(connector->GetTextInputManager(),
            GetParentWebContents()->GetTextInputManager());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       ParentDestruction) {
  // Create a separate parent WebContents so we can destroy it during the
  // test.
  WebContents::CreateParams parent_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> parent = WebContents::Create(parent_params);
  WebContentsImpl* parent_impl = static_cast<WebContentsImpl*>(parent.get());

  // Create the child WebContents.
  WebContents::CreateParams child_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child = WebContents::Create(child_params);

  MockSurfaceEmbedConnectorDelegate delegate;
  auto connector = CreateConnector(child.get(), parent_impl, &delegate);

  EXPECT_EQ(connector->GetParentWebContentsView(), parent_impl->GetView());

  // Destroy the parent.
  parent.reset();

  // Verify connector handles missing parent gracefully where checks exist.
  EXPECT_EQ(connector->GetParentWebContentsView(), nullptr);
  EXPECT_EQ(connector->GetParentRenderViewHostDelegateView(), nullptr);

  // Note: GetInputEventRouter() and GetTextInputManager() in
  // SurfaceEmbedConnectorImpl currently do not check for null parent, so we
  // don't test them here to avoid crash.
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, ConstGetters) {
  // Create the child WebContents.
  WebContents::CreateParams create_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);

  MockSurfaceEmbedConnectorDelegate delegate;

  // Create the connector.
  auto connector = CreateConnector(child_web_contents.get(),
                                   GetParentWebContents(), &delegate);

  const SurfaceEmbedConnectorImpl& const_connector = *connector;

  // Verify getters can be called on a const reference.
  EXPECT_EQ(const_connector.GetParentWebContentsView(),
            GetParentWebContents()->GetView());
  EXPECT_EQ(const_connector.GetParentRenderViewHostDelegateView(),
            GetParentWebContents()->GetDelegateView());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest, Attach) {
  // Create the child WebContents.
  WebContents::CreateParams create_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);

  MockSurfaceEmbedConnectorDelegate delegate;

  // Attach the connector.
  SurfaceEmbedConnector::Attach(child_web_contents.get(),
                                GetParentWebContents(), &delegate);

  // Verify the connector is attached to the child WebContents.
  auto* connector = child_web_contents->GetSurfaceEmbedConnector();
  ASSERT_TRUE(connector);

  // Verify properties.
  EXPECT_EQ(connector->GetDelegate(), &delegate);
  EXPECT_EQ(static_cast<SurfaceEmbedConnectorImpl*>(connector)
                ->GetParentWebContentsView(),
            GetParentWebContents()->GetView());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedConnectorImplBrowserTest,
                       FrameConnectorImplementation) {
  // Create the child WebContents.
  WebContents::CreateParams create_params(
      GetParentWebContents()->GetBrowserContext());
  std::unique_ptr<WebContents> child_web_contents =
      WebContents::Create(create_params);
  WebContentsImpl* child_web_contents_impl =
      static_cast<WebContentsImpl*>(child_web_contents.get());

  MockSurfaceEmbedConnectorDelegate delegate;

  // Create the connector.
  auto connector = CreateConnector(child_web_contents_impl,
                                   GetParentWebContents(), &delegate);

  // Verify FrameConnector implementation defaults.
  //
  // TODO(cammie): Many of these expectations will need to change when the stub
  // implementations are replaced with real ones.
  //
  // We can't verify SetView as it doesn't have a getter, and
  // RenderWidgetHostViewChildFrame cannot be easily instantiated in this
  // test environment.
  connector->SetView(nullptr, false);

  EXPECT_EQ(connector->GetParentRenderWidgetHostView(), nullptr);
  EXPECT_EQ(connector->GetRootRenderWidgetHostView(), nullptr);

  // These return void, just call them to ensure no crash.
  connector->RenderProcessGone();
  connector->FirstSurfaceActivation(viz::SurfaceInfo());
  connector->SendIntrinsicSizingInfoToParent(nullptr);
  connector->SynchronizeVisualProperties(blink::FrameVisualProperties(), false);
  connector->UpdateCursor(ui::Cursor());

  EXPECT_EQ(connector->HasFocus(),
            FrameConnector::RootViewFocusState::kNullView);

  connector->FocusRootView();  // void

  EXPECT_EQ(connector->LockPointer(false),
            blink::mojom::PointerLockResult::kUnknownError);
  EXPECT_EQ(connector->ChangePointerLock(false),
            blink::mojom::PointerLockResult::kUnknownError);

  connector->UnlockPointer();  // void

  EXPECT_FALSE(connector->HasSize());

  // Just check they return valid references/values
  connector->GetScreenInfos();
  connector->GetLocalSurfaceId();
  connector->GetIntersectionState();

  EXPECT_EQ(connector->GetCaptureSequenceNumber(), 0u);

  connector->GetRectInParentViewInDip();
  connector->GetLocalFrameSizeInDip();
  connector->GetLocalFrameSizeInPixels();

  EXPECT_EQ(connector->GetCssZoomFactor(), 1.0);

  connector->EnableAutoResize(gfx::Size(), gfx::Size());  // void
  connector->DisableAutoResize();                         // void

  EXPECT_FALSE(connector->IsInert());
  EXPECT_EQ(connector->InheritedEffectiveTouchAction(), cc::TouchAction::kAuto);
  EXPECT_FALSE(connector->IsHidden());
  EXPECT_FALSE(connector->IsThrottled());
  EXPECT_FALSE(connector->IsSubtreeThrottled());
  EXPECT_FALSE(connector->IsDisplayLocked());

  connector->DidUpdateVisualProperties(cc::RenderFrameMetadata());  // void
  connector->SetVisibilityForChildViews(true);                      // void
  connector->SetLocalFrameSize(gfx::Size());                        // void
  connector->SetRectInParentView(gfx::Rect());                      // void
  connector->OnVisibilityChanged(
      blink::mojom::FrameVisibility::kRenderedInViewport);  // void

  EXPECT_TRUE(connector->IsVisible());

  connector->DelegateWasShown();  // void

  EXPECT_EQ(connector->EmbedderVisibility(), Visibility::VISIBLE);

  EXPECT_EQ(connector->GetParentViewInput(), nullptr);
  EXPECT_EQ(connector->GetRootViewInput(), nullptr);
}

}  // namespace content
