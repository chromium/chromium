// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/page_state/page_state.h"
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
  window_ = std::make_unique<aura::Window>(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate());
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

void TestRenderWidgetHostView::UpdateCursor(const WebCursor& cursor) {
  last_cursor_ = cursor;
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

void TestRenderWidgetHostView::ShowSharePicker(
    const std::string& title,
    const std::string& text,
    const std::string& url,
    const std::vector<std::string>& file_paths,
    blink::mojom::ShareService::ShareCallback callback) {}
#endif

gfx::Rect TestRenderWidgetHostView::GetBoundsInRootWindow() {
  return gfx::Rect();
}

void TestRenderWidgetHostView::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  CopyBackgroundColorIfPresentFrom(*view);
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

void TestRenderWidgetHostView::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  OnFrameTokenChangedForView(frame_token, activation_time);
}

std::unique_ptr<SyntheticGestureTarget>
TestRenderWidgetHostView::CreateSyntheticGestureTarget() {
  NOTIMPLEMENTED();
  return nullptr;
}

void TestRenderWidgetHostView::UpdateBackgroundColor() {}

void TestRenderWidgetHostView::SetDisplayFeatureForTesting(
    const DisplayFeature* display_feature) {
  if (display_feature)
    display_feature_ = *display_feature;
  else
    display_feature_ = absl::nullopt;
}

absl::optional<DisplayFeature> TestRenderWidgetHostView::GetDisplayFeature() {
  return display_feature_;
}

TestRenderViewHost::TestRenderViewHost(
    FrameTree* frame_tree,
    SiteInstance* instance,
    std::unique_ptr<RenderWidgetHostImpl> widget,
    RenderViewHostDelegate* delegate,
    int32_t routing_id,
    int32_t main_frame_routing_id,
    bool swapped_out)
    : RenderViewHostImpl(frame_tree,
                         instance,
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

bool TestRenderViewHost::CreateTestRenderView() {
  return CreateRenderView(absl::nullopt, MSG_ROUTING_NONE, false);
}

bool TestRenderViewHost::CreateRenderView(
    const absl::optional<blink::FrameToken>& opener_frame_token,
    int proxy_route_id,
    bool window_was_created_with_opener) {
  DCHECK(!IsRenderViewLive());
  // Mark the RenderView as live, though there's nothing to do here since we
  // don't yet use mojo to talk to the RenderView.
  renderer_view_created_ = true;

  // When the RenderViewHost has a main frame host attached, the RenderView
  // in the renderer creates the main frame along with it. We mimic that here by
  // creating the mojo connections and calling RenderFrameCreated().
  RenderFrameHostImpl* main_frame = nullptr;
  RenderFrameProxyHost* proxy_host = nullptr;
  if (main_frame_routing_id_ != MSG_ROUTING_NONE) {
    main_frame = RenderFrameHostImpl::FromID(GetProcess()->GetID(),
                                             main_frame_routing_id_);
  } else {
    proxy_host =
        RenderFrameProxyHost::FromID(GetProcess()->GetID(), proxy_route_id);
  }

  DCHECK_EQ(!!main_frame, is_active());
  if (main_frame) {
    // Pretend that we started a renderer process and created the renderer Frame
    // with its Widget. We bind all the mojom interfaces, but they all just talk
    // into the void.
    RenderWidgetHostImpl* main_frame_widget = main_frame->GetRenderWidgetHost();
    main_frame_widget->BindWidgetInterfaces(
        mojo::PendingAssociatedRemote<blink::mojom::WidgetHost>()
            .InitWithNewEndpointAndPassReceiver(),
        TestRenderWidgetHost::CreateStubWidgetRemote());
    main_frame_widget->BindFrameWidgetInterfaces(
        mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>()
            .InitWithNewEndpointAndPassReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());
    main_frame->SetMojomFrameRemote(
        TestRenderFrameHost::CreateStubFrameRemote());

    // This also initializes the RenderWidgetHost attached to the frame.
    main_frame->RenderFrameCreated();
  } else {
    // Pretend that mojo connections of the RemoteFrame is transferred to
    // renderer process and bound in blink.
    ignore_result(proxy_host->BindRemoteMainFrameReceiverForTesting());
    proxy_host->SetRenderFrameProxyCreated(true);
  }

  opener_frame_token_ = opener_frame_token;
  DCHECK(IsRenderViewLive());
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
                         storage_partition->GetFileSystemAccessManager(),
                         GetProcess()->GetID()),
      blink::kDragOperationEvery, std::move(bitmap), gfx::Vector2d(),
      blink::mojom::DragEventSourceInfo::New());
}

void TestRenderViewHost::TestOnUpdateStateWithFile(
    const base::FilePath& file_path) {
  auto state = blink::PageState::CreateForTesting(GURL("http://www.google.com"),
                                                  false, "data", &file_path);
  static_cast<RenderFrameHostImpl*>(GetMainFrame())->UpdateState(state);
}

RenderViewHostImplTestHarness::RenderViewHostImplTestHarness()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  std::vector<ui::ScaleFactor> scale_factors;
  scale_factors.push_back(ui::SCALE_FACTOR_100P);
  scoped_set_supported_scale_factors_ =
      std::make_unique<ui::test::ScopedSetSupportedScaleFactors>(scale_factors);
}

RenderViewHostImplTestHarness::~RenderViewHostImplTestHarness() {
}

TestRenderViewHost* RenderViewHostImplTestHarness::test_rvh() {
  return contents()->GetRenderViewHost();
}

TestRenderViewHost* RenderViewHostImplTestHarness::pending_test_rvh() {
  return contents()->GetSpeculativePrimaryMainFrame()
             ? contents()->GetSpeculativePrimaryMainFrame()->GetRenderViewHost()
             : nullptr;
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
