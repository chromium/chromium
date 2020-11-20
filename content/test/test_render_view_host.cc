// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host.h"

#include <memory>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/drop_data_util.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_state.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/page/drag.mojom.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

#if defined(USE_AURA)
#include "ui/aura/test/test_window_delegate.h"
#endif

namespace content {

void InitNavigateParams(FrameHostMsg_DidCommitProvisionalLoad_Params* params,
                        int nav_entry_id,
                        bool did_create_new_entry,
                        const GURL& url,
                        ui::PageTransition transition) {
  params->nav_entry_id = nav_entry_id;
  params->url = url;
  params->origin = url::Origin::Create(url);
  params->referrer = Referrer();
  params->transition = transition;
  params->redirects = std::vector<GURL>();
  params->should_update_history = false;
  params->did_create_new_entry = did_create_new_entry;
  params->gesture = NavigationGestureUser;
  params->method = "GET";
  params->page_state = PageState::CreateFromURL(url);
}

TestRenderWidgetHostView::TestRenderWidgetHostView(RenderWidgetHost* rwh)
    : RenderWidgetHostViewBase(rwh), is_showing_(false), is_occluded_(false) {
#if defined(OS_ANDROID)
  frame_sink_id_ = AllocateFrameSinkId();
  GetHostFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kYes);
#else
  default_background_color_ = SK_ColorWHITE;
  // Not all tests initialize or need an image transport factory.
  if (ImageTransportFactory::GetInstance()) {
    frame_sink_id_ = AllocateFrameSinkId();
    GetHostFrameSinkManager()->RegisterFrameSinkId(
        frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kYes);
#if DCHECK_IS_ON()
    GetHostFrameSinkManager()->SetFrameSinkDebugLabel(
        frame_sink_id_, "TestRenderWidgetHostView");
#endif
  }
#endif

  host()->SetView(this);

  if (host()->delegate() && host()->delegate()->GetInputEventRouter() &&
      GetFrameSinkId().is_valid()) {
    host()->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }

#if defined(USE_AURA)
  window_.reset(new aura::Window(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate()));
  window_->set_owned_by_parent(false);
  window_->Init(ui::LayerType::LAYER_NOT_DRAWN);
#endif
}

TestRenderWidgetHostView::~TestRenderWidgetHostView() {
  viz::HostFrameSinkManager* manager = GetHostFrameSinkManager();
  if (manager)
    manager->InvalidateFrameSinkId(frame_sink_id_);
}

gfx::NativeView TestRenderWidgetHostView::GetNativeView() {
#if defined(USE_AURA)
  return window_.get();
#else
  return nullptr;
#endif
}

gfx::NativeViewAccessible TestRenderWidgetHostView::GetNativeViewAccessible() {
  return nullptr;
}

ui::TextInputClient* TestRenderWidgetHostView::GetTextInputClient() {
  return &text_input_client_;
}

bool TestRenderWidgetHostView::HasFocus() {
  return true;
}

void TestRenderWidgetHostView::Show() {
  is_showing_ = true;
  is_occluded_ = false;
}

void TestRenderWidgetHostView::Hide() {
  is_showing_ = false;
}

bool TestRenderWidgetHostView::IsShowing() {
  return is_showing_;
}

void TestRenderWidgetHostView::WasUnOccluded() {
  is_occluded_ = false;
}

void TestRenderWidgetHostView::WasOccluded() {
  is_occluded_ = true;
}

void TestRenderWidgetHostView::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
}

uint32_t TestRenderWidgetHostView::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void TestRenderWidgetHostView::RenderProcessGone() {
  delete this;
}

void TestRenderWidgetHostView::Destroy() { delete this; }

gfx::Rect TestRenderWidgetHostView::GetViewBounds() {
  return gfx::Rect();
}

#if defined(OS_MAC)
void TestRenderWidgetHostView::SetActive(bool active) {
  // <viettrungluu@gmail.com>: Do I need to do anything here?
}

void TestRenderWidgetHostView::SpeakSelection() {
}

void TestRenderWidgetHostView::SetWindowFrameInScreen(const gfx::Rect& rect) {}
#endif

gfx::Rect TestRenderWidgetHostView::GetBoundsInRootWindow() {
  return gfx::Rect();
}

void TestRenderWidgetHostView::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  base::Optional<SkColor> color = view->GetBackgroundColor();
  if (color)
    SetBackgroundColor(*color);
}

blink::mojom::PointerLockResult TestRenderWidgetHostView::LockMouse(bool) {
  return blink::mojom::PointerLockResult::kUnknownError;
}

blink::mojom::PointerLockResult TestRenderWidgetHostView::ChangeMouseLock(
    bool) {
  return blink::mojom::PointerLockResult::kUnknownError;
}

void TestRenderWidgetHostView::UnlockMouse() {
}

const viz::FrameSinkId& TestRenderWidgetHostView::GetFrameSinkId() const {
  return frame_sink_id_;
}

const viz::LocalSurfaceId& TestRenderWidgetHostView::GetLocalSurfaceId() const {
  return viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
}

viz::SurfaceId TestRenderWidgetHostView::GetCurrentSurfaceId() const {
  return viz::SurfaceId();
}

void TestRenderWidgetHostView::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  // TODO(fsamuel): Once surface synchronization is turned on, the fallback
  // surface should be set here.
}

void TestRenderWidgetHostView::OnFrameTokenChanged(uint32_t frame_token) {
  OnFrameTokenChangedForView(frame_token);
}

std::unique_ptr<SyntheticGestureTarget>
TestRenderWidgetHostView::CreateSyntheticGestureTarget() {
  NOTIMPLEMENTED();
  return nullptr;
}

void TestRenderWidgetHostView::UpdateBackgroundColor() {}

TestRenderViewHost::TestRenderViewHost(
    SiteInstance* instance,
    std::unique_ptr<RenderWidgetHostImpl> widget,
    RenderViewHostDelegate* delegate,
    int32_t routing_id,
    int32_t main_frame_routing_id,
    bool swapped_out)
    : RenderViewHostImpl(instance,
                         std::move(widget),
                         delegate,
                         routing_id,
                         main_frame_routing_id,
                         swapped_out,
                         false /* has_initialized_audio_host */),
      delete_counter_(nullptr) {
  // TestRenderWidgetHostView installs itself into this->view_ in its
  // constructor, and deletes itself when TestRenderWidgetHostView::Destroy() is
  // called.
  new TestRenderWidgetHostView(GetWidget());
}

TestRenderViewHost::~TestRenderViewHost() {
  if (delete_counter_)
    ++*delete_counter_;
}

bool TestRenderViewHost::CreateTestRenderView(
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int proxy_route_id,
    bool window_was_created_with_opener) {
  return CreateRenderView(opener_frame_token, proxy_route_id,
                          window_was_created_with_opener);
}

bool TestRenderViewHost::CreateRenderView(
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int proxy_route_id,
    bool window_was_created_with_opener) {
  DCHECK(!IsRenderViewLive());
  GetWidget()->set_renderer_initialized(true);
  DCHECK(IsRenderViewLive());
  opener_frame_token_ = opener_frame_token;
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(GetMainFrame());
  if (main_frame && is_active()) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        stub_interface_provider_remote;
    main_frame->BindInterfaceProviderReceiver(
        stub_interface_provider_remote.InitWithNewPipeAndPassReceiver());

    mojo::AssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
    mojo::AssociatedRemote<blink::mojom::Widget> blink_widget;
    auto blink_widget_receiver =
        blink_widget.BindNewEndpointAndPassDedicatedReceiver();
    GetWidget()->BindWidgetInterfaces(
        blink_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
        blink_widget.Unbind());

    mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> frame_widget_host;
    mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget;
    auto frame_widget_receiver =
        frame_widget.BindNewEndpointAndPassDedicatedReceiver();
    GetWidget()->BindFrameWidgetInterfaces(
        frame_widget_host.BindNewEndpointAndPassDedicatedReceiver(),
        frame_widget.Unbind());

    main_frame->SetRenderFrameCreated(true);
  }

  return true;
}

MockRenderProcessHost* TestRenderViewHost::GetProcess() {
  return static_cast<MockRenderProcessHost*>(RenderViewHostImpl::GetProcess());
}

void TestRenderViewHost::SimulateWasHidden() {
  GetWidget()->WasHidden();
}

void TestRenderViewHost::SimulateWasShown() {
  GetWidget()->WasShown({} /* record_tab_switch_time_request */);
}

blink::web_pref::WebPreferences
TestRenderViewHost::TestComputeWebPreferences() {
  return static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(this))
      ->ComputeWebPreferences();
}

bool TestRenderViewHost::IsTestRenderViewHost() const {
  return true;
}

void TestRenderViewHost::TestStartDragging(const DropData& drop_data,
                                           SkBitmap bitmap) {
  StoragePartitionImpl* storage_partition =
      static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition());
  GetWidget()->StartDragging(
      DropDataToDragData(drop_data,
                         storage_partition->GetNativeFileSystemManager(),
                         GetProcess()->GetID()),
      blink::kDragOperationEvery, std::move(bitmap), gfx::Vector2d(),
      blink::mojom::DragEventSourceInfo::New());
}

void TestRenderViewHost::TestOnUpdateStateWithFile(
    const base::FilePath& file_path) {
  PageState state = PageState::CreateForTesting(GURL("http://www.google.com"),
                                                false, "data", &file_path);
  static_cast<RenderFrameHostImpl*>(GetMainFrame())->UpdateState(state);
}

RenderViewHostImplTestHarness::RenderViewHostImplTestHarness()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  std::vector<ui::ScaleFactor> scale_factors;
  scale_factors.push_back(ui::SCALE_FACTOR_100P);
  scoped_set_supported_scale_factors_.reset(
      new ui::test::ScopedSetSupportedScaleFactors(scale_factors));
}

RenderViewHostImplTestHarness::~RenderViewHostImplTestHarness() {
}

TestRenderViewHost* RenderViewHostImplTestHarness::test_rvh() {
  return contents()->GetRenderViewHost();
}

TestRenderViewHost* RenderViewHostImplTestHarness::pending_test_rvh() {
  return contents()->GetPendingMainFrame() ?
      contents()->GetPendingMainFrame()->GetRenderViewHost() :
      nullptr;
}

TestRenderViewHost* RenderViewHostImplTestHarness::active_test_rvh() {
  return static_cast<TestRenderViewHost*>(active_rvh());
}

TestRenderFrameHost* RenderViewHostImplTestHarness::main_test_rfh() {
  return contents()->GetMainFrame();
}

TestWebContents* RenderViewHostImplTestHarness::contents() {
  return static_cast<TestWebContents*>(web_contents());
}

}  // namespace content
