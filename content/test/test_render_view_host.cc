// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_visibility_state.h"
#include "content/test/test_page_broadcast.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

#if defined(USE_AURA)
#include "ui/aura/test/test_window_delegate.h"
#endif

namespace content {

TestRenderWidgetHostView::TestRenderWidgetHostView(RenderWidgetHost* rwh)
    : RenderWidgetHostViewBase(rwh),
      is_showing_(false),
      is_occluded_(false),
      cursor_manager_(this) {
#if BUILDFLAG(IS_ANDROID)
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

  SetIsFrameSinkIdOwner(true);

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
    manager->InvalidateFrameSinkId(frame_sink_id_, this);
}

gfx::NativeView TestRenderWidgetHostView::GetNativeView() {
#if defined(USE_AURA)
  return window_.get();
#else
  return gfx::NativeView();
#endif
}

gfx::NativeViewAccessible TestRenderWidgetHostView::GetNativeViewAccessible() {
  return nullptr;
}

ui::TextInputClient* TestRenderWidgetHostView::GetTextInputClient() {
#if !BUILDFLAG(IS_IOS)
  return &text_input_client_;
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

bool TestRenderWidgetHostView::HasFocus() {
  return true;
}

void TestRenderWidgetHostView::ShowWithVisibility(
    PageVisibilityState page_visibility) {
  page_visibility_ = page_visibility;
  OnShowWithPageVisibility(page_visibility_);
  is_showing_ = true;
  is_occluded_ = false;
}

void TestRenderWidgetHostView::Hide() {
  if (!host()->is_hidden())
    host()->WasHidden();
  is_showing_ = false;
}

bool TestRenderWidgetHostView::IsShowing() {
  return is_showing_;
}

void TestRenderWidgetHostView::WasUnOccluded() {
  // Can't be unoccluded unless the page is visible.
  page_visibility_ = PageVisibilityState::kVisible;
  OnShowWithPageVisibility(page_visibility_);
  is_occluded_ = false;
}

void TestRenderWidgetHostView::WasOccluded() {
  if (!host()->is_hidden())
    host()->WasHidden();
  is_occluded_ = true;
}

void TestRenderWidgetHostView::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
}

uint32_t TestRenderWidgetHostView::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void TestRenderWidgetHostView::UpdateCursor(const ui::Cursor& cursor) {
  last_cursor_ = cursor;
}

void TestRenderWidgetHostView::RenderProcessGone() {
  delete this;
}

void TestRenderWidgetHostView::Destroy() {
  // Call this here in case any observers need access to the `this` before
  // this derived class runs its destructor.
  NotifyObserversAboutShutdown();

  delete this;
}

gfx::Rect TestRenderWidgetHostView::GetViewBounds() {
  return gfx::Rect();
}

#if BUILDFLAG(IS_MAC)
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

uint64_t TestRenderWidgetHostView::GetNSViewId() const {
  return 0;
}
#endif

gfx::Rect TestRenderWidgetHostView::GetBoundsInRootWindow() {
  return gfx::Rect();
}

const viz::LocalSurfaceId&
TestRenderWidgetHostView::IncrementSurfaceIdForNavigation() {
  static constexpr viz::LocalSurfaceId kInvalidId;
  return kInvalidId;
}

void TestRenderWidgetHostView::ClearFallbackSurfaceForCommitPending() {
  clear_fallback_surface_for_commit_pending_called_ = true;
}

void TestRenderWidgetHostView::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  take_fallback_content_from_called_ = true;
  CopyBackgroundColorIfPresentFrom(*view);
}

blink::mojom::PointerLockResult TestRenderWidgetHostView::LockPointer(bool) {
  return blink::mojom::PointerLockResult::kUnknownError;
}

blink::mojom::PointerLockResult TestRenderWidgetHostView::ChangePointerLock(
    bool) {
  return blink::mojom::PointerLockResult::kUnknownError;
}

void TestRenderWidgetHostView::UnlockPointer() {}

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

void TestRenderWidgetHostView::ClearFallbackSurfaceCalled() {
  clear_fallback_surface_for_commit_pending_called_ = false;
  take_fallback_content_from_called_ = false;
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
    display_feature_ = std::nullopt;
}

void TestRenderWidgetHostView::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  // Should only be called if the view was not already shown.
  EXPECT_TRUE(!is_showing_ || is_occluded_);
  switch (page_visibility_) {
    case PageVisibilityState::kVisible:
      // May or may not include a visible_time_request.
      break;
    case PageVisibilityState::kHiddenButPainting:
      EXPECT_FALSE(visible_time_request);
      break;
    case PageVisibilityState::kHidden:
      ADD_FAILURE();
      break;
  }
  if (host()->is_hidden()) {
    // Do not pass on `visible_time_request` because there is no compositing to
    // measure.
    host()->WasShown({});
  }
}

void TestRenderWidgetHostView::
    RequestSuccessfulPresentationTimeFromHostOrDelegate(
        blink::mojom::RecordContentToVisibleTimeRequestPtr
            visible_time_request) {
  // Should only be called if the view was already shown.
#if !BUILDFLAG(IS_ANDROID)
  // TODO(jonross): Update the constructor to determine showing state
  // `is_showing_ = !host()->is_hidden()` this will match production code. Also
  // update various tests not prepared for this to also match production.
  //
  // In tests TestRenderViewHostFactory::CreateRenderViewHost creates all hosts
  // as visible. Which leads to newly created views being attached to already
  // visible hosts. On Android we begin tracking content-to-visible-time when
  // recreating the main render frame. This leads to requests while already
  // visible in tests.
  EXPECT_TRUE(is_showing_);
#endif
  EXPECT_FALSE(is_occluded_);
  EXPECT_EQ(page_visibility_, PageVisibilityState::kVisible);
  EXPECT_TRUE(visible_time_request);
}

void TestRenderWidgetHostView::
    CancelSuccessfulPresentationTimeRequestForHostAndDelegate() {
  // Should only be called if the view was already shown.
  EXPECT_TRUE(is_showing_);
  EXPECT_FALSE(is_occluded_);
  EXPECT_EQ(page_visibility_, PageVisibilityState::kHiddenButPainting);
}

std::optional<DisplayFeature> TestRenderWidgetHostView::GetDisplayFeature() {
  return display_feature_;
}

ui::Compositor* TestRenderWidgetHostView::GetCompositor() {
  return compositor_;
}

input::CursorManager* TestRenderWidgetHostView::GetCursorManager() {
  return &cursor_manager_;
}

TestRenderWidgetHostViewChildFrame::TestRenderWidgetHostViewChildFrame(
    RenderWidgetHost* rwh)
    : RenderWidgetHostViewChildFrame(
          rwh,
          display::ScreenInfos(display::ScreenInfo())) {
  Init();
}

void TestRenderWidgetHostViewChildFrame::Reset() {
  last_gesture_seen_ = blink::WebInputEvent::Type::kUndefined;
}

void TestRenderWidgetHostViewChildFrame::SetCompositor(
    ui::Compositor* compositor) {
  compositor_ = compositor;
}

ui::Compositor* TestRenderWidgetHostViewChildFrame::GetCompositor() {
  return compositor_;
}

void TestRenderWidgetHostViewChildFrame::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo&) {
  last_gesture_seen_ = event.GetType();
}

TestRenderViewHost::TestRenderViewHost(
    FrameTree* frame_tree,
    SiteInstanceGroup* group,
    const StoragePartitionConfig& storage_partition_config,
    std::unique_ptr<RenderWidgetHostImpl> widget,
    RenderViewHostDelegate* delegate,
    int32_t routing_id,
    int32_t main_frame_routing_id,
    scoped_refptr<BrowsingContextState> main_browsing_context_state,
    CreateRenderViewHostCase create_case)
    : RenderViewHostImpl(frame_tree,
                         group,
                         storage_partition_config,
                         std::move(widget),
                         delegate,
                         routing_id,
                         main_frame_routing_id,
                         false /* has_initialized_audio_host */,
                         std::move(main_browsing_context_state),
                         create_case),
      delete_counter_(nullptr) {
  if (frame_tree->is_fenced_frame()) {
    // TestRenderWidgetHostViewChildFrame deletes itself in
    // RenderWidgetHostViewChildFrame::Destroy.
    new TestRenderWidgetHostViewChildFrame(GetWidget());
  } else {
    // TestRenderWidgetHostView installs itself into this->view_ in
    // its constructor, and deletes itself when
    // TestRenderWidgetHostView::Destroy() is called.
    new TestRenderWidgetHostView(GetWidget());
  }
}

TestRenderViewHost::~TestRenderViewHost() {
  if (delete_counter_)
    ++*delete_counter_;
}

bool TestRenderViewHost::CreateTestRenderView() {
  return CreateRenderView(std::nullopt, MSG_ROUTING_NONE, false);
}

bool TestRenderViewHost::CreateRenderView(
    const std::optional<blink::FrameToken>& opener_frame_token,
    int proxy_route_id,
    bool window_was_created_with_opener) {
  DCHECK(!IsRenderViewLive());
  // Mark the `blink::WebView` as live, though there's nothing to do here since
  // we don't yet use mojo to talk to the RenderView.
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

  if (!GetWidget()->view_is_frame_sink_id_owner()) {
    main_frame->NotifyWillCreateRenderWidgetOnCommit();
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
    mojo::AssociatedRemote<blink::mojom::RemoteFrame> remote_frame;
    std::ignore = remote_frame.BindNewEndpointAndPassDedicatedReceiver();
    proxy_host->BindRemoteFrameInterfaces(
        remote_frame.Unbind(),
        mojo::AssociatedRemote<blink::mojom::RemoteFrameHost>()
            .BindNewEndpointAndPassDedicatedReceiver());

    mojo::AssociatedRemote<blink::mojom::RemoteMainFrame> remote_main_frame;
    std::ignore = remote_main_frame.BindNewEndpointAndPassDedicatedReceiver();
    proxy_host->BindRemoteMainFrameInterfaces(
        remote_main_frame.Unbind(),
        mojo::AssociatedRemote<blink::mojom::RemoteMainFrameHost>()
            .BindNewEndpointAndPassDedicatedReceiver());

    proxy_host->SetRenderFrameProxyCreated(true);
  }

  mojo::AssociatedRemote<blink::mojom::PageBroadcast> broadcast_remote;
  page_broadcast_ = std::make_unique<TestPageBroadcast>(
      broadcast_remote.BindNewEndpointAndPassDedicatedReceiver());
  BindPageBroadcast(broadcast_remote.Unbind());

  opener_frame_token_ = opener_frame_token;
  DCHECK(IsRenderViewLive());
  return true;
}

MockRenderProcessHost* TestRenderViewHost::GetProcess() const {
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
  GetMainRenderFrameHost()->StartDragging(
      DropDataToDragData(
          drop_data, storage_partition->GetFileSystemAccessManager(),
          GetProcess()->GetID(),
          ChromeBlobStorageContext::GetFor(GetProcess()->GetBrowserContext())),
      blink::kDragOperationEvery, std::move(bitmap), gfx::Vector2d(),
      gfx::Rect(), blink::mojom::DragEventSourceInfo::New());
}

void TestRenderViewHost::TestOnUpdateStateWithFile(
    const base::FilePath& file_path) {
  auto state = blink::PageState::CreateForTesting(GURL("http://www.google.com"),
                                                  false, "data", &file_path);
  GetMainRenderFrameHost()->UpdateState(state);
}

RenderViewHostImplTestHarness::RenderViewHostImplTestHarness()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

RenderViewHostImplTestHarness::~RenderViewHostImplTestHarness() = default;

TestRenderViewHost* RenderViewHostImplTestHarness::test_rvh() {
  return contents()->GetRenderViewHost();
}

TestRenderFrameHost* RenderViewHostImplTestHarness::main_test_rfh() {
  return contents()->GetPrimaryMainFrame();
}

TestWebContents* RenderViewHostImplTestHarness::contents() {
  return static_cast<TestWebContents*>(web_contents());
}

}  // namespace content
