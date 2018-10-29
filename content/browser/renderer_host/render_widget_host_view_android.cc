// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include <utility>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_hittest.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/web_contents_accessibility_android.h"
#include "content/browser/android/content_feature_list.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/android/overscroll_controller_android.h"
#include "content/browser/android/selection/selection_popup_controller.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/android/text_suggestion_host_android.h"
#include "content/browser/bad_message.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/delegated_frame_host_client_android.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_metadata_util.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_manager_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

namespace {

static const char kAsyncReadBackString[] = "Compositing.CopyFromSurfaceTime";
static const base::TimeDelta kClickCountInterval =
    base::TimeDelta::FromSecondsD(0.5);
static const float kClickCountRadiusSquaredDIP = 25;

std::unique_ptr<ui::TouchSelectionController> CreateSelectionController(
    ui::TouchSelectionControllerClient* client,
    bool has_view_tree) {
  DCHECK(client);
  DCHECK(has_view_tree);
  ui::TouchSelectionController::Config config;
  config.max_tap_duration = base::TimeDelta::FromMilliseconds(
      gfx::ViewConfiguration::GetLongPressTimeoutInMs());
  config.tap_slop = gfx::ViewConfiguration::GetTouchSlopInDips();
  config.enable_adaptive_handle_orientation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAdaptiveSelectionHandleOrientation);
  config.enable_longpress_drag_selection =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLongpressDragSelection);
  config.hide_active_handle =
      base::FeatureList::IsEnabled(
          content::android::kEnhancedSelectionInsertionHandle) &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_P;
  return std::make_unique<ui::TouchSelectionController>(client, config);
}

gfx::RectF GetSelectionRect(const ui::TouchSelectionController& controller) {
  gfx::RectF rect = controller.GetRectBetweenBounds();
  if (rect.IsEmpty())
    return rect;

  rect.Union(controller.GetStartHandleRect());
  rect.Union(controller.GetEndHandleRect());
  return rect;
}

void RecordToolTypeForActionDown(const ui::MotionEventAndroid& event) {
  ui::MotionEventAndroid::Action action = event.GetAction();
  if (action == ui::MotionEventAndroid::Action::DOWN ||
      action == ui::MotionEventAndroid::Action::POINTER_DOWN ||
      action == ui::MotionEventAndroid::Action::BUTTON_PRESS) {
    UMA_HISTOGRAM_ENUMERATION(
        "Event.AndroidActionDown.ToolType",
        static_cast<int>(event.GetToolType(0)),
        static_cast<int>(ui::MotionEventAndroid::ToolType::LAST) + 1);
  }
}

void WakeUpGpu(GpuProcessHost* host) {
  if (host && host->gpu_host()->wake_up_gpu_before_drawing()) {
    host->gpu_service()->WakeUpGpu();
  }
}

}  // namespace

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    gfx::NativeView parent_native_view)
    : RenderWidgetHostViewBase(widget_host),
      begin_frame_source_(nullptr),
      outstanding_begin_frame_requests_(0),
      is_showing_(!widget_host->is_hidden()),
      is_window_visible_(true),
      is_window_activity_started_(true),
      is_in_vr_(false),
      ime_adapter_android_(nullptr),
      selection_popup_controller_(nullptr),
      text_suggestion_host_(nullptr),
      gesture_listener_manager_(nullptr),
      view_(ui::ViewAndroid::LayoutType::MATCH_PARENT),
      gesture_provider_(ui::GetGestureProviderConfig(
                            ui::GestureProviderConfigType::CURRENT_PLATFORM),
                        this),
      stylus_text_selector_(this),
      using_browser_compositor_(CompositorImpl::IsInitialized()),
      synchronous_compositor_client_(nullptr),
      observing_root_window_(false),
      prev_top_shown_pix_(0.f),
      prev_top_controls_translate_(0.f),
      prev_bottom_shown_pix_(0.f),
      prev_bottom_controls_translate_(0.f),
      page_scale_(1.f),
      min_page_scale_(1.f),
      max_page_scale_(1.f),
      mouse_wheel_phase_handler_(this),
      weak_ptr_factory_(this) {
  // Set the layer which will hold the content layer for this view. The content
  // layer is managed by the DelegatedFrameHost.
  view_.SetLayer(cc::Layer::Create());
  view_.set_event_handler(this);

  if (using_browser_compositor_) {
    delegated_frame_host_client_ =
        std::make_unique<DelegatedFrameHostClientAndroid>(this);
    delegated_frame_host_ = std::make_unique<ui::DelegatedFrameHostAndroid>(
        &view_, CompositorImpl::GetHostFrameSinkManager(),
        delegated_frame_host_client_.get(), host()->GetFrameSinkId(),
        features::IsSurfaceSynchronizationEnabled());
    if (is_showing_) {
      delegated_frame_host_->WasShown(
          local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
          GetCompositorViewportPixelSize());
    }

    // Let the page-level input event router know about our frame sink ID
    // for surface-based hit testing.
    if (ShouldRouteEvents()) {
      host()->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
          GetFrameSinkId(), this);
    }
  }

  host()->SetView(this);
  touch_selection_controller_client_manager_ =
      std::make_unique<TouchSelectionControllerClientManagerAndroid>(
          this, CompositorImpl::GetHostFrameSinkManager());

  UpdateNativeViewTree(parent_native_view);
  // This RWHVA may have been created speculatively. We should give any
  // existing RWHVAs priority for receiving input events, otherwise a
  // speculative RWHVA could be sent input events intended for the currently
  // showing RWHVA.
  if (parent_native_view)
    parent_native_view->MoveToBack(&view_);

  CreateOverscrollControllerIfPossible();

  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  UpdateNativeViewTree(nullptr);
  view_.set_event_handler(nullptr);
  DCHECK(!ime_adapter_android_);
  DCHECK(!delegated_frame_host_);
}

void RenderWidgetHostViewAndroid::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void RenderWidgetHostViewAndroid::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewAndroid::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
    const base::Optional<viz::LocalSurfaceId>& child_allocated_local_surface_id,
    const base::Optional<base::TimeTicks>&
        child_local_surface_id_allocation_time) {
  if (child_allocated_local_surface_id) {
    local_surface_id_allocator_.UpdateFromChild(
        *child_allocated_local_surface_id,
        *child_local_surface_id_allocation_time);
  } else {
    local_surface_id_allocator_.GenerateId();
  }
  if (delegated_frame_host_) {
    delegated_frame_host_->EmbedSurface(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize(), deadline_policy);

    // TODO(ericrk): This can be removed once surface synchronization is
    // enabled. https://crbug.com/835102
    delegated_frame_host_->PixelSizeWillChange(
        GetCompositorViewportPixelSize());
  }

  return host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  default_bounds_ = gfx::Rect(default_bounds_.origin(), size);
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  default_bounds_ = rect;
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  if (!delegated_frame_host_)
    return false;

  if (!view_.parent())
    return false;

  if (current_surface_size_.IsEmpty())
    return false;

  return delegated_frame_host_->HasSavedFrame();
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() const {
  return &view_;
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::GotFocus() {
  host()->GotFocus();
  OnFocusInternal();
}

void RenderWidgetHostViewAndroid::LostFocus() {
  host()->LostFocus();
  LostFocusInternal();
}

void RenderWidgetHostViewAndroid::OnRenderFrameMetadataChangedBeforeActivation(
    const cc::RenderFrameMetadata& metadata) {
  if (!features::IsSurfaceSynchronizationEnabled())
    return;

  bool is_mobile_optimized = IsMobileOptimizedFrame(
      metadata.page_scale_factor, metadata.min_page_scale_factor,
      metadata.max_page_scale_factor, metadata.scrollable_viewport_size,
      metadata.root_layer_size);

  gesture_provider_.SetDoubleTapSupportForPageEnabled(!is_mobile_optimized);

  float dip_scale = view_.GetDipScale();
  gfx::SizeF root_layer_size_dip = metadata.root_layer_size;
  gfx::SizeF scrollable_viewport_size_dip = metadata.scrollable_viewport_size;
  gfx::Vector2dF root_scroll_offset_dip =
      metadata.root_scroll_offset.value_or(gfx::Vector2dF());
  if (IsUseZoomForDSFEnabled()) {
    float pix_to_dip = 1 / dip_scale;
    root_layer_size_dip.Scale(pix_to_dip);
    scrollable_viewport_size_dip.Scale(pix_to_dip);
    root_scroll_offset_dip.Scale(pix_to_dip);
  }

  float to_pix = IsUseZoomForDSFEnabled() ? 1.f : dip_scale;
  // Note that the height of browser control is not affected by page scale
  // factor. Thus, |top_content_offset| in CSS pixels is also in DIPs.
  float top_content_offset =
      metadata.top_controls_height * metadata.top_controls_shown_ratio;
  float top_shown_pix = top_content_offset * to_pix;

  if (ime_adapter_android_) {
    ime_adapter_android_->UpdateFrameInfo(metadata.selection.start, dip_scale,
                                          top_shown_pix);
  }

  auto* wcax = GetWebContentsAccessibilityAndroid();
  if (wcax)
    wcax->UpdateFrameInfo(metadata.page_scale_factor);

  if (!gesture_listener_manager_)
    return;

  UpdateTouchSelectionController(metadata.selection, metadata.page_scale_factor,
                                 metadata.top_controls_height,
                                 metadata.top_controls_shown_ratio,
                                 scrollable_viewport_size_dip);

  // ViewAndroid::content_offset() must be in dip.
  float top_content_offset_dip = IsUseZoomForDSFEnabled()
                                     ? top_content_offset / dip_scale
                                     : top_content_offset;
  view_.UpdateFrameInfo({scrollable_viewport_size_dip, top_content_offset_dip});
  bool controls_changed = UpdateControls(
      view_.GetDipScale(), metadata.top_controls_height,
      metadata.top_controls_shown_ratio, metadata.bottom_controls_height,
      metadata.bottom_controls_shown_ratio);

  SetContentBackgroundColor(metadata.has_transparent_background
                                ? SK_ColorTRANSPARENT
                                : metadata.root_background_color);

  if (overscroll_controller_) {
    overscroll_controller_->OnFrameMetadataUpdated(
        metadata.page_scale_factor, metadata.device_scale_factor,
        metadata.scrollable_viewport_size, metadata.root_layer_size,
        metadata.root_scroll_offset.value_or(gfx::Vector2dF()),
        metadata.root_overflow_y_hidden);
  }

  // All offsets and sizes except |top_shown_pix| are in dip.
  gesture_listener_manager_->UpdateScrollInfo(
      root_scroll_offset_dip, metadata.page_scale_factor,
      metadata.min_page_scale_factor, metadata.max_page_scale_factor,
      root_layer_size_dip, scrollable_viewport_size_dip, top_content_offset_dip,
      top_shown_pix, controls_changed);

  page_scale_ = metadata.page_scale_factor;
  min_page_scale_ = metadata.min_page_scale_factor;
  max_page_scale_ = metadata.max_page_scale_factor;
  current_surface_size_ = metadata.viewport_size_in_pixels;
}

void RenderWidgetHostViewAndroid::Focus() {
  if (view_.HasFocus())
    GotFocus();
  else
    view_.RequestFocus();
}

void RenderWidgetHostViewAndroid::OnFocusInternal() {
  if (overscroll_controller_)
    overscroll_controller_->Enable();
}

void RenderWidgetHostViewAndroid::LostFocusInternal() {
  if (overscroll_controller_)
    overscroll_controller_->Disable();
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  return view_.HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  return !using_browser_compositor_ ||
         (delegated_frame_host_ &&
          delegated_frame_host_->CanCopyFromCompositingSurface());
}

void RenderWidgetHostViewAndroid::Show() {
  if (is_showing_)
    return;

  is_showing_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  is_showing_ = false;
  HideInternal();
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // |view_.parent()| being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_showing_ && view_.parent();
}

void RenderWidgetHostViewAndroid::SelectWordAroundCaretAck(bool did_select,
                                                           int start_adjust,
                                                           int end_adjust) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->OnSelectWordAroundCaretAck(
      did_select, start_adjust, end_adjust);
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  if (!view_.parent())
    return default_bounds_;

  gfx::Size size(view_.GetSize());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOSKOverscroll)) {
    size.Enlarge(0, view_.GetSystemWindowInsetBottom() / view_.GetDipScale());
  }

  return gfx::Rect(size);
}

gfx::Size RenderWidgetHostViewAndroid::GetVisibleViewportSize() const {
  if (!view_.parent())
    return default_bounds_.size();

  return view_.GetSize();
}

gfx::Size RenderWidgetHostViewAndroid::GetCompositorViewportPixelSize() const {
  if (!view_.parent()) {
    if (default_bounds_.IsEmpty()) return gfx::Size();

    float scale_factor = view_.GetDipScale();
    return gfx::Size(default_bounds_.right() * scale_factor,
                     default_bounds_.bottom() * scale_factor);
  }

  return view_.GetPhysicalBackingSize();
}

bool RenderWidgetHostViewAndroid::DoBrowserControlsShrinkRendererSize() const {
  auto* delegate_view = GetRenderViewHostDelegateView();
  return delegate_view ? delegate_view->DoBrowserControlsShrinkRendererSize()
                       : false;
}

float RenderWidgetHostViewAndroid::GetTopControlsHeight() const {
  auto* delegate_view = GetRenderViewHostDelegateView();
  return delegate_view ? delegate_view->GetTopControlsHeight() : 0.f;
}

float RenderWidgetHostViewAndroid::GetBottomControlsHeight() const {
  auto* delegate_view = GetRenderViewHostDelegateView();
  return delegate_view ? delegate_view->GetBottomControlsHeight() : 0.f;
}

int RenderWidgetHostViewAndroid::GetMouseWheelMinimumGranularity() const {
  auto* window = view_.GetWindowAndroid();
  if (!window)
    return 0;

  // On Android, mouse wheel MotionEvents specify the number of ticks and how
  // many pixels each tick scrolls. This multiplier is specified by device
  // metrics (See WindowAndroid.getMouseWheelScrollFactor) so the minimum
  // granularity will be the size of this tick multiplier.
  return window->mouse_wheel_scroll_factor() / view_.GetDipScale();
}

void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& cursor) {
  CursorInfo cursor_info;
  cursor.GetCursorInfo(&cursor_info);
  view_.OnCursorChanged(cursor_info.type, cursor_info.custom_image,
                        cursor_info.hotspot);
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

// -----------------------------------------------------------------------------
// TextInputManager::Observer implementations.
void RenderWidgetHostViewAndroid::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_change_state) {
  DCHECK_EQ(text_input_manager_, text_input_manager);
  // If there are no active widgets, the TextInputState.type should be reported
  // as none.
  const TextInputState& state =
      GetTextInputManager()->GetActiveWidget()
          ? *GetTextInputManager()->GetTextInputState()
          : TextInputState();

  if (!ime_adapter_android_)
    return;

  ime_adapter_android_->UpdateState(state);
}

void RenderWidgetHostViewAndroid::OnImeCompositionRangeChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);
  const TextInputManager::CompositionRangeInfo* info =
      text_input_manager_->GetCompositionRangeInfo();
  if (!info)
    return;

  std::vector<gfx::RectF> character_bounds;
  for (const gfx::Rect& rect : info->character_bounds)
    character_bounds.emplace_back(rect);

  if (ime_adapter_android_)
    ime_adapter_android_->SetCharacterBounds(character_bounds);
}

void RenderWidgetHostViewAndroid::OnImeCancelComposition(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);
  if (ime_adapter_android_)
    ime_adapter_android_->CancelComposition();
}

void RenderWidgetHostViewAndroid::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  DCHECK_EQ(text_input_manager_, text_input_manager);

  if (!selection_popup_controller_)
    return;

  RenderWidgetHostImpl* focused_widget = GetFocusedWidget();
  if (!focused_widget || !focused_widget->GetView())
    return;

  const TextInputManager::TextSelection& selection =
      *text_input_manager_->GetTextSelection(focused_widget->GetView());

  selection_popup_controller_->OnSelectionChanged(
      base::UTF16ToUTF8(selection.selected_text()));
}

void RenderWidgetHostViewAndroid::SetNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::SetNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  if (needs_begin_frames)
    AddBeginFrameRequest(PERSISTENT_BEGIN_FRAME);
  else
    ClearBeginFrameRequest(PERSISTENT_BEGIN_FRAME);
}

void RenderWidgetHostViewAndroid::SetWantsAnimateOnlyBeginFrames() {
  wants_animate_only_begin_frames_ = true;
}

viz::FrameSinkId RenderWidgetHostViewAndroid::GetRootFrameSinkId() {
  if (view_.GetWindowAndroid() && view_.GetWindowAndroid()->GetCompositor())
    return view_.GetWindowAndroid()->GetCompositor()->GetFrameSinkId();
  return viz::FrameSinkId();
}

viz::SurfaceId RenderWidgetHostViewAndroid::GetCurrentSurfaceId() const {
  return delegated_frame_host_ ? delegated_frame_host_->SurfaceId()
                               : viz::SurfaceId();
}

bool RenderWidgetHostViewAndroid::TransformPointToLocalCoordSpaceLegacy(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  if (!delegated_frame_host_)
    return false;

  float scale_factor = view_.GetDipScale();
  DCHECK_GT(scale_factor, 0);
  // Transformations use physical pixels rather than DIP, so conversion
  // is necessary.
  gfx::PointF point_in_pixels = gfx::ConvertPointToPixel(scale_factor, point);

  viz::SurfaceId surface_id = delegated_frame_host_->SurfaceId();
  if (!surface_id.is_valid())
    return false;

  if (original_surface == surface_id)
    return true;

  *transformed_point = point_in_pixels;
  viz::SurfaceHittest hittest(nullptr,
                              GetFrameSinkManager()->surface_manager());
  if (!hittest.TransformPointToTargetSurface(original_surface, surface_id,
                                             transformed_point))
    return false;

  *transformed_point = gfx::ConvertPointToDIP(scale_factor, *transformed_point);
  return true;
}

bool RenderWidgetHostViewAndroid::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point,
    viz::EventSource source) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  viz::SurfaceId surface_id = GetCurrentSurfaceId();
  if (!surface_id.is_valid())
    return false;

  // In TransformPointToLocalCoordSpace() there is a Point-to-Pixel conversion,
  // but it is not necessary here because the final target view is responsible
  // for converting before computing the final transform.
  return target_view->TransformPointToLocalCoordSpace(
      point, surface_id, transformed_point, source);
}

base::WeakPtr<RenderWidgetHostViewAndroid>
RenderWidgetHostViewAndroid::GetWeakPtrAndroid() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventAndroid& event) {
  std::unique_ptr<blink::WebGestureEvent> web_event;
  if (event.scale() < 0.f) {
    // Negative scale indicates zoom reset.
    float delta = min_page_scale_ / page_scale_;
    web_event = ui::CreateWebGestureEventFromGestureEventAndroid(
        ui::GestureEventAndroid(event.type(), event.location(),
                                event.screen_location(), event.time(), delta, 0,
                                0, 0, 0, /*target_viewport*/ false,
                                /*synthetic_scroll*/ false,
                                /*prevent_boosting*/ false));
  } else {
    web_event = ui::CreateWebGestureEventFromGestureEventAndroid(event);
  }
  if (!web_event)
    return false;
  SendGestureEvent(*web_event);
  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchEvent(
    const ui::MotionEventAndroid& event) {
  RecordToolTypeForActionDown(event);

  if (event.GetAction() == ui::MotionEventAndroid::Action::DOWN) {
    if (ime_adapter_android_)
      ime_adapter_android_->UpdateOnTouchDown();
    if (gesture_listener_manager_)
      gesture_listener_manager_->UpdateOnTouchDown();
  }

  if (event.for_touch_handle())
    return OnTouchHandleEvent(event);

  if (!host() || !host()->delegate())
    return false;

  ComputeEventLatencyOSTouchHistograms(event);

  // Receiving any other touch event before the double-tap timeout expires
  // cancels opening the spellcheck menu.
  if (text_suggestion_host_)
    text_suggestion_host_->StopSuggestionMenuTimer();

  // If a browser-based widget consumes the touch event, it's critical that
  // touch event interception be disabled. This avoids issues with
  // double-handling for embedder-detected gestures like side swipe.
  if (OnTouchHandleEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  if (stylus_text_selector_.OnTouchEvent(event)) {
    RequestDisallowInterceptTouchEvent();
    return true;
  }

  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(event);
  if (!result.succeeded)
    return false;

  blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
      event, result.moved_beyond_slop_region /* may_cause_scrolling */,
      false /* hovering */);
  if (web_event.GetType() == blink::WebInputEvent::kUndefined)
    return false;

  ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteTouchEvent(this, &web_event,
                                                               latency_info);
  } else {
    host()->ForwardTouchEventWithLatencyInfo(web_event, latency_info);
  }

  // Send a proactive BeginFrame for this vsync to reduce scroll latency for
  // scroll-inducing touch events. Note that Android's Choreographer ensures
  // that BeginFrame requests made during Action::MOVE dispatch will be honored
  // in the same vsync phase.
  if (observing_root_window_ && result.moved_beyond_slop_region)
    AddBeginFrameRequest(BEGIN_FRAME);

  return true;
}

bool RenderWidgetHostViewAndroid::OnTouchHandleEvent(
    const ui::MotionEvent& event) {
  return touch_selection_controller_ &&
         touch_selection_controller_->WillHandleTouchEvent(event);
}

int RenderWidgetHostViewAndroid::GetTouchHandleHeight() {
  if (!touch_selection_controller_)
    return 0;
  return static_cast<int>(touch_selection_controller_->GetTouchHandleHeight());
}

void RenderWidgetHostViewAndroid::ResetGestureDetection() {
  const ui::MotionEvent* current_down_event =
      gesture_provider_.GetCurrentDownEvent();
  if (!current_down_event) {
    // A hard reset ensures prevention of any timer-based events that might fire
    // after a touch sequence has ended.
    gesture_provider_.ResetDetection();
    return;
  }

  std::unique_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
  if (gesture_provider_.OnTouchEvent(*cancel_event).succeeded) {
    bool causes_scrolling = false;
    ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
    latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
    blink::WebTouchEvent web_event = ui::CreateWebTouchEventFromMotionEvent(
        *cancel_event, causes_scrolling /* may_cause_scrolling */,
        false /* hovering */);
    if (ShouldRouteEvents()) {
      host()->delegate()->GetInputEventRouter()->RouteTouchEvent(
          this, &web_event, latency_info);
    } else {
      host()->ForwardTouchEventWithLatencyInfo(web_event, latency_info);
    }
  }
}

void RenderWidgetHostViewAndroid::OnDidNavigateMainFrameToNewPage() {
  if (view_.parent())
    view_.parent()->MoveToFront(&view_);
  ResetGestureDetection();
}

void RenderWidgetHostViewAndroid::SetDoubleTapSupportEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void RenderWidgetHostViewAndroid::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}

void RenderWidgetHostViewAndroid::FocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_screen) {
  if (ime_adapter_android_)
    ime_adapter_android_->FocusedNodeChanged(is_editable_node);
}

void RenderWidgetHostViewAndroid::RenderProcessGone(
    base::TerminationStatus status, int error_code) {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  host()->ViewDestroyed();
  UpdateNativeViewTree(nullptr);
  delegated_frame_host_.reset();

  if (GetTextInputManager() && GetTextInputManager()->HasObserver(this))
    GetTextInputManager()->RemoveObserver(this);

  for (auto& observer : destruction_observers_)
    observer.RenderWidgetHostViewDestroyed(this);
  destruction_observers_.Clear();
  RenderWidgetHostViewBase::Destroy();
  delete this;
}

void RenderWidgetHostViewAndroid::SetTooltipText(
    const base::string16& tooltip_text) {
  // Tooltips don't makes sense on Android.
}

void RenderWidgetHostViewAndroid::UpdateBackgroundColor() {
  DCHECK(RenderWidgetHostViewBase::GetBackgroundColor());

  SkColor color = *RenderWidgetHostViewBase::GetBackgroundColor();
  view_.OnBackgroundColorChanged(color);
}

bool RenderWidgetHostViewAndroid::HasFallbackSurface() const {
  return delegated_frame_host_ && delegated_frame_host_->HasFallbackSurface();
}

void RenderWidgetHostViewAndroid::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::CopyFromSurface");
  if (!IsSurfaceAvailableForCopy()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  if (!using_browser_compositor_) {
    SynchronousCopyContents(src_subrect, output_size, std::move(callback));
    UMA_HISTOGRAM_TIMES("Compositing.CopyFromSurfaceTimeSynchronous",
                        base::TimeTicks::Now() - start_time);
    return;
  }

  DCHECK(delegated_frame_host_);
  delegated_frame_host_->CopyFromCompositingSurface(
      src_subrect, output_size,
      base::BindOnce(
          [](base::OnceCallback<void(const SkBitmap&)> callback,
             base::TimeTicks start_time, const SkBitmap& bitmap) {
            TRACE_EVENT0(
                "cc", "RenderWidgetHostViewAndroid::CopyFromSurface finished");
            UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                                base::TimeTicks::Now() - start_time);
            std::move(callback).Run(bitmap);
          },
          std::move(callback), start_time));
}

void RenderWidgetHostViewAndroid::EnsureSurfaceSynchronizedForLayoutTest() {
  ++latest_capture_sequence_number_;
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseInfiniteDeadline(),
                              base::nullopt, base::nullopt);
}

uint32_t RenderWidgetHostViewAndroid::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void RenderWidgetHostViewAndroid::OnInterstitialPageAttached() {
  if (view_.parent())
    view_.parent()->MoveToFront(&view_);
}

void RenderWidgetHostViewAndroid::OnInterstitialPageGoingAway() {
  sync_compositor_.reset();
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAndroid::CreateSyntheticGestureTarget() {
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetAndroid(host(), &view_));
}

bool RenderWidgetHostViewAndroid::ShouldRouteEvents() const {
  DCHECK(host());
  return using_browser_compositor_ && host()->delegate() &&
         host()->delegate()->GetInputEventRouter();
}

void RenderWidgetHostViewAndroid::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources);
}

void RenderWidgetHostViewAndroid::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(using_browser_compositor_);
  renderer_compositor_frame_sink_->DidPresentCompositorFrame(presentation_token,
                                                             feedback);
}

void RenderWidgetHostViewAndroid::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  renderer_compositor_frame_sink_->ReclaimResources(resources);
}

void RenderWidgetHostViewAndroid::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  if (!delegated_frame_host_) {
    DCHECK(!using_browser_compositor_);
    // We don't expect RendererCompositorFrameSink on Android WebView.
    // (crbug.com/721102)
    bad_message::ReceivedBadMessage(host()->GetProcess(),
                                    bad_message::RWH_BAD_FRAME_SINK_REQUEST);
    return;
  }
  delegated_frame_host_->CompositorFrameSinkChanged();
  renderer_compositor_frame_sink_ = renderer_compositor_frame_sink;
}

void RenderWidgetHostViewAndroid::EvictFrameIfNecessary() {
  if (!host()->delegate()->IsFullscreenForCurrentTab() ||
      current_surface_size_ == view_.GetPhysicalBackingSize() ||
      !base::FeatureList::IsEnabled(
          features::kHideIncorrectlySizedFullscreenFrames)) {
    return;
  }
  // When we're in a fullscreen and and doing a resize we show black
  // instead of the incorrectly-sized frame. However when we are just
  // adjusting the height we keep the frames because it is a less jarring
  // experience for the user instead frames shown as black.
  bool is_width_same =
      current_surface_size_.width() == view_.GetPhysicalBackingSize().width();
  if (!is_width_same) {
    EvictDelegatedFrame();
    SetContentBackgroundColor(SK_ColorBLACK);
  }
}

void RenderWidgetHostViewAndroid::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list) {
  if (!delegated_frame_host_) {
    DCHECK(!using_browser_compositor_);
    return;
  }

  DCHECK(!frame.render_pass_list.empty());

  viz::RenderPass* root_pass = frame.render_pass_list.back().get();
  current_surface_size_ = root_pass->output_rect.size();
  bool is_transparent = root_pass->has_transparent_background;

  viz::CompositorFrameMetadata metadata = frame.metadata.Clone();

  delegated_frame_host_->SubmitCompositorFrame(
      local_surface_id, std::move(frame), std::move(hit_test_region_list));

  // As the metadata update may trigger view invalidation, always call it after
  // any potential compositor scheduling.
  OnFrameMetadataUpdated(std::move(metadata), is_transparent);
}

void RenderWidgetHostViewAndroid::OnDidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  if (!delegated_frame_host_) {
    // We are not using the browser compositor and there's no DisplayScheduler
    // that needs to be notified about the BeginFrameAck, so we can drop it.
    DCHECK(!using_browser_compositor_);
    return;
  }

  delegated_frame_host_->DidNotProduceFrame(ack);
}

void RenderWidgetHostViewAndroid::ClearCompositorFrame() {
  EvictDelegatedFrame();
}

void RenderWidgetHostViewAndroid::ResetFallbackToFirstNavigationSurface() {
  if (delegated_frame_host_)
    delegated_frame_host_->ResetFallbackToFirstNavigationSurface();
}

bool RenderWidgetHostViewAndroid::RequestRepaintForTesting() {
  return SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                     base::nullopt, base::nullopt);
}

void RenderWidgetHostViewAndroid::SynchronousFrameMetadata(
    viz::CompositorFrameMetadata metadata) {
  if (!view_.parent())
    return;

  bool is_mobile_optimized = IsMobileOptimizedFrame(
      metadata.page_scale_factor, metadata.min_page_scale_factor,
      metadata.max_page_scale_factor, metadata.scrollable_viewport_size,
      metadata.root_layer_size);

  if (host() && host()->input_router()) {
    host()->input_router()->NotifySiteIsMobileOptimized(is_mobile_optimized);
  }

  if (host() && metadata.frame_token)
    host()->DidProcessFrame(metadata.frame_token);

  // This is a subset of OnSwapCompositorFrame() used in the synchronous
  // compositor flow.
  OnFrameMetadataUpdated(metadata.Clone(), false);

  // DevTools ScreenCast support for Android WebView.
  RenderFrameHost* frame_host = RenderViewHost::From(host())->GetMainFrame();
  if (frame_host) {
    RenderFrameDevToolsAgentHost::SignalSynchronousSwapCompositorFrame(
        frame_host, std::move(metadata));
  }
}

void RenderWidgetHostViewAndroid::SetSynchronousCompositorClient(
      SynchronousCompositorClient* client) {
  synchronous_compositor_client_ = client;
  if (!sync_compositor_ && synchronous_compositor_client_) {
    sync_compositor_ = SynchronousCompositorHost::Create(this);
  }
}

void RenderWidgetHostViewAndroid::OnOverscrollRefreshHandlerAvailable() {
  DCHECK(!overscroll_controller_);
  CreateOverscrollControllerIfPossible();
}

bool RenderWidgetHostViewAndroid::SupportsAnimation() const {
  // The synchronous (WebView) compositor does not have a proper browser
  // compositor with which to drive animations.
  return using_browser_compositor_;
}

void RenderWidgetHostViewAndroid::SetNeedsAnimate() {
  DCHECK(view_.GetWindowAndroid());
  DCHECK(using_browser_compositor_);
  view_.GetWindowAndroid()->SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::PointF& position) {
  MoveCaret(gfx::Point(position.x(), position.y()));
}

void RenderWidgetHostViewAndroid::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->MoveRangeSelectionExtent(extent);
}

void RenderWidgetHostViewAndroid::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->SelectBetweenCoordinates(base, extent);
}

void RenderWidgetHostViewAndroid::OnSelectionEvent(
    ui::SelectionEventType event) {
  if (!selection_popup_controller_)
    return;
  DCHECK(touch_selection_controller_);
  // If a selection drag has started, it has taken over the active touch
  // sequence. Immediately cancel gesture detection and any downstream touch
  // listeners (e.g., web content) to communicate this transfer.
  if (event == ui::SELECTION_HANDLES_SHOWN &&
      gesture_provider_.GetCurrentDownEvent()) {
    ResetGestureDetection();
  }
  selection_popup_controller_->OnSelectionEvent(
      event, GetSelectionRect(*touch_selection_controller_));
}

void RenderWidgetHostViewAndroid::OnDragUpdate(const gfx::PointF& position) {
  if (!selection_popup_controller_)
    return;
  selection_popup_controller_->OnDragUpdate(position);
}

ui::TouchSelectionControllerClient*
RenderWidgetHostViewAndroid::GetSelectionControllerClientManagerForTesting() {
  return touch_selection_controller_client_manager_.get();
}

void RenderWidgetHostViewAndroid::SetSelectionControllerClientForTesting(
    std::unique_ptr<ui::TouchSelectionControllerClient> client) {
  touch_selection_controller_client_for_test_.swap(client);

  touch_selection_controller_ = CreateSelectionController(
      touch_selection_controller_client_for_test_.get(), !!view_.parent());
}

std::unique_ptr<ui::TouchHandleDrawable>
RenderWidgetHostViewAndroid::CreateDrawable() {
  if (!using_browser_compositor_) {
    if (!sync_compositor_)
      return nullptr;
    return std::unique_ptr<ui::TouchHandleDrawable>(
        sync_compositor_->client()->CreateDrawable());
  }
  if (!selection_popup_controller_)
    return nullptr;
  return selection_popup_controller_->CreateTouchHandleDrawable();
}

void RenderWidgetHostViewAndroid::DidScroll() {}

void RenderWidgetHostViewAndroid::SynchronousCopyContents(
    const gfx::Rect& src_subrect_dip,
    const gfx::Size& dst_size_in_pixel,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  // Note: When |src_subrect| is empty, a conversion from the view size must
  // be made instead of using |current_frame_size_|. The latter sometimes also
  // includes extra height for the toolbar UI, which is not intended for
  // capture.
  const gfx::Rect src_subrect_in_pixel = gfx::ConvertRectToPixel(
      view_.GetDipScale(), src_subrect_dip.IsEmpty()
                               ? gfx::Rect(GetVisibleViewportSize())
                               : src_subrect_dip);

  // TODO(crbug/698974): [BUG] Current implementation does not support read-back
  // of regions that do not originate at (0,0).
  const gfx::Size& input_size_in_pixel = src_subrect_in_pixel.size();
  DCHECK(!input_size_in_pixel.IsEmpty());

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = input_size_in_pixel;
  else
    output_size_in_pixel = dst_size_in_pixel;
  int output_width = output_size_in_pixel.width();
  int output_height = output_size_in_pixel.height();

  if (!sync_compositor_) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(output_width, output_height));
  SkCanvas canvas(bitmap);
  canvas.scale(
      (float)output_width / (float)input_size_in_pixel.width(),
      (float)output_height / (float)input_size_in_pixel.height());
  sync_compositor_->DemandDrawSw(&canvas);
  std::move(callback).Run(bitmap);
}

WebContentsAccessibilityAndroid*
RenderWidgetHostViewAndroid::GetWebContentsAccessibilityAndroid() const {
  return static_cast<WebContentsAccessibilityAndroid*>(
      web_contents_accessibility_);
}

void RenderWidgetHostViewAndroid::OnFrameMetadataUpdated(
    const viz::CompositorFrameMetadata& metadata,
    bool is_transparent) {
  if (features::IsSurfaceSynchronizationEnabled())
    return;

  bool is_mobile_optimized = IsMobileOptimizedFrame(
      metadata.page_scale_factor, metadata.min_page_scale_factor,
      metadata.max_page_scale_factor, metadata.scrollable_viewport_size,
      metadata.root_layer_size);

  gesture_provider_.SetDoubleTapSupportForPageEnabled(!is_mobile_optimized);

  float dip_scale = view_.GetDipScale();
  gfx::SizeF root_layer_size_dip = metadata.root_layer_size;
  gfx::SizeF scrollable_viewport_size_dip = metadata.scrollable_viewport_size;
  gfx::Vector2dF root_scroll_offset_dip = metadata.root_scroll_offset;
  if (IsUseZoomForDSFEnabled()) {
    float pix_to_dip = 1 / dip_scale;
    root_layer_size_dip.Scale(pix_to_dip);
    scrollable_viewport_size_dip.Scale(pix_to_dip);
    root_scroll_offset_dip.Scale(pix_to_dip);
  }

  float to_pix = IsUseZoomForDSFEnabled() ? 1.f : dip_scale;
  // Note that the height of browser control is not affected by page scale
  // factor. Thus, |top_content_offset| in CSS pixels is also in DIPs.
  float top_content_offset =
      metadata.top_controls_height * metadata.top_controls_shown_ratio;
  float top_shown_pix = top_content_offset * to_pix;

  if (ime_adapter_android_) {
    ime_adapter_android_->UpdateFrameInfo(metadata.selection.start, dip_scale,
                                          top_shown_pix);
  }

  auto* wcax = GetWebContentsAccessibilityAndroid();
  if (wcax)
    wcax->UpdateFrameInfo(metadata.page_scale_factor);

  if (!gesture_listener_manager_)
    return;

  if (overscroll_controller_) {
    overscroll_controller_->OnFrameMetadataUpdated(
        metadata.page_scale_factor, metadata.device_scale_factor,
        metadata.scrollable_viewport_size, metadata.root_layer_size,
        metadata.root_scroll_offset, metadata.root_overflow_y_hidden);
  }

  UpdateTouchSelectionController(metadata.selection, metadata.page_scale_factor,
                                 metadata.top_controls_height,
                                 metadata.top_controls_shown_ratio,
                                 scrollable_viewport_size_dip);
  // ViewAndroid::content_offset() must be in dip
  float top_content_offset_dip = IsUseZoomForDSFEnabled()
                                     ? top_content_offset / dip_scale
                                     : top_content_offset;
  view_.UpdateFrameInfo({scrollable_viewport_size_dip, top_content_offset_dip});
  bool controls_changed = UpdateControls(
      view_.GetDipScale(), metadata.top_controls_height,
      metadata.top_controls_shown_ratio, metadata.bottom_controls_height,
      metadata.bottom_controls_shown_ratio);

  // All offsets and sizes except |top_shown_pix| are in dip.
  gesture_listener_manager_->UpdateScrollInfo(
      root_scroll_offset_dip, metadata.page_scale_factor,
      metadata.min_page_scale_factor, metadata.max_page_scale_factor,
      root_layer_size_dip, scrollable_viewport_size_dip, top_content_offset_dip,
      top_shown_pix, controls_changed);

  SetContentBackgroundColor(is_transparent ? SK_ColorTRANSPARENT
                                           : metadata.root_background_color);

  page_scale_ = metadata.page_scale_factor;
  min_page_scale_ = metadata.min_page_scale_factor;
  max_page_scale_ = metadata.max_page_scale_factor;

  EvictFrameIfNecessary();
}

void RenderWidgetHostViewAndroid::UpdateTouchSelectionController(
    const viz::Selection<gfx::SelectionBound>& selection,
    float page_scale_factor,
    float top_controls_height,
    float top_controls_shown_ratio,
    const gfx::SizeF& scrollable_viewport_size_dip) {
  if (!touch_selection_controller_)
    return;

  DCHECK(touch_selection_controller_client_manager_);
  touch_selection_controller_client_manager_->UpdateClientSelectionBounds(
      selection.start, selection.end, this, nullptr);

  // Set parameters for adaptive handle orientation.
  gfx::SizeF viewport_size(scrollable_viewport_size_dip);
  viewport_size.Scale(page_scale_factor);
  gfx::RectF viewport_rect(0.0f, top_controls_height * top_controls_shown_ratio,
                           viewport_size.width(), viewport_size.height());
  touch_selection_controller_->OnViewportChanged(viewport_rect);
}

bool RenderWidgetHostViewAndroid::UpdateControls(
    float dip_scale,
    float top_controls_height,
    float top_controls_shown_ratio,
    float bottom_controls_height,
    float bottom_controls_shown_ratio) {
  float to_pix = IsUseZoomForDSFEnabled() ? 1.f : dip_scale;
  float top_controls_pix = top_controls_height * to_pix;
  // |top_content_offset| is in physical pixels if --use-zoom-for-dsf is
  // enabled. Otherwise, it is in DIPs.
  // Note that the height of browser control is not affected by page scale
  // factor. Thus, |top_content_offset| in CSS pixels is also in DIPs.
  float top_content_offset = top_controls_height * top_controls_shown_ratio;
  float top_shown_pix = top_content_offset * to_pix;
  float top_translate = top_shown_pix - top_controls_pix;
  bool top_changed =
      !cc::MathUtil::IsFloatNearlyTheSame(top_shown_pix, prev_top_shown_pix_);
  // TODO(mthiesse, https://crbug.com/853686): Remove the IsInVR check once
  // there are no use cases for ignoring the initial update.
  if (top_changed || (!controls_initialized_ && IsInVR()))
    view_.OnTopControlsChanged(top_translate, top_shown_pix);
  prev_top_shown_pix_ = top_shown_pix;
  prev_top_controls_translate_ = top_translate;

  float bottom_controls_pix = bottom_controls_height * to_pix;
  float bottom_shown_pix = bottom_controls_pix * bottom_controls_shown_ratio;
  bool bottom_changed = !cc::MathUtil::IsFloatNearlyTheSame(
      bottom_shown_pix, prev_bottom_shown_pix_);
  float bottom_translate = bottom_controls_pix - bottom_shown_pix;
  if (bottom_changed || (!controls_initialized_ && IsInVR()))
    view_.OnBottomControlsChanged(bottom_translate, bottom_shown_pix);
  prev_bottom_shown_pix_ = bottom_shown_pix;
  prev_bottom_controls_translate_ = bottom_translate;
  controls_initialized_ = true;
  return top_changed || bottom_changed;
}

void RenderWidgetHostViewAndroid::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  SynchronizeVisualProperties(
      cc::DeadlinePolicy::UseDefaultDeadline(), metadata.local_surface_id,
      metadata.local_surface_id_allocation_time_from_child);
  // We've just processed new RenderFrameMetadata and potentially embedded a
  // new surface for that data. Check if we need to evict it.
  EvictFrameIfNecessary();
}

void RenderWidgetHostViewAndroid::ShowInternal() {
  bool show = is_showing_ && is_window_activity_started_ && is_window_visible_;
  if (!show)
    return;

  if (!host() || !host()->is_hidden())
    return;

  view_.GetLayer()->SetHideLayerAndSubtree(false);

  if (overscroll_controller_)
    overscroll_controller_->Enable();

  if (delegated_frame_host_ &&
      delegated_frame_host_->IsPrimarySurfaceEvicted()) {
    ui::WindowAndroidCompositor* compositor =
        view_.GetWindowAndroid() ? view_.GetWindowAndroid()->GetCompositor()
                                 : nullptr;
    SynchronizeVisualProperties(
        compositor && compositor->IsDrawingFirstVisibleFrame()
            ? cc::DeadlinePolicy::UseSpecifiedDeadline(
                  ui::DelegatedFrameHostAndroid::FirstFrameTimeoutFrames())
            : cc::DeadlinePolicy::UseDefaultDeadline(),
        base::nullopt, base::nullopt);
  }

  host()->WasShown(false /* record_presentation_time */);

  if (delegated_frame_host_) {
    delegated_frame_host_->WasShown(
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        GetCompositorViewportPixelSize());
  }

  if (view_.parent() && view_.GetWindowAndroid()) {
    StartObservingRootWindow();
    AddBeginFrameRequest(BEGIN_FRAME);
  }
}

void RenderWidgetHostViewAndroid::HideInternal() {
  DCHECK(!is_showing_ || !is_window_activity_started_ || !is_window_visible_)
      << "Hide called when the widget should be shown.";

  // Only preserve the frontbuffer if the activity was stopped while the
  // window is still visible. This avoids visual artificts when transitioning
  // between activities.
  bool hide_frontbuffer = is_window_activity_started_ || !is_window_visible_;

  // Only stop observing the root window if the widget has been explicitly
  // hidden and the frontbuffer is being cleared. This allows window visibility
  // notifications to eventually clear the frontbuffer.
  bool stop_observing_root_window = !is_showing_ && hide_frontbuffer;

  if (hide_frontbuffer) {
    view_.GetLayer()->SetHideLayerAndSubtree(true);
    if (delegated_frame_host_)
      delegated_frame_host_->WasHidden();
  }

  if (stop_observing_root_window) {
    DCHECK(!is_showing_);
    StopObservingRootWindow();
  }

  if (!host() || host()->is_hidden())
    return;

  if (overscroll_controller_)
    overscroll_controller_->Disable();

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host()->WasHidden();
}

void RenderWidgetHostViewAndroid::SetBeginFrameSource(
    viz::BeginFrameSource* begin_frame_source) {
  if (begin_frame_source_ == begin_frame_source)
    return;

  if (begin_frame_source_ && outstanding_begin_frame_requests_)
    begin_frame_source_->RemoveObserver(this);
  begin_frame_source_ = begin_frame_source;
  if (begin_frame_source_ && outstanding_begin_frame_requests_)
    begin_frame_source_->AddObserver(this);
}

void RenderWidgetHostViewAndroid::AddBeginFrameRequest(
    BeginFrameRequestType request) {
  uint32_t prior_requests = outstanding_begin_frame_requests_;
  outstanding_begin_frame_requests_ = prior_requests | request;

  // Note that if we don't currently have a BeginFrameSource, outstanding begin
  // frame requests will be pushed if/when we get one during
  // |StartObservingRootWindow()| or when the DelegatedFrameHostAndroid sets it.
  viz::BeginFrameSource* source = begin_frame_source_;
  if (source && outstanding_begin_frame_requests_ && !prior_requests)
    source->AddObserver(this);
}

void RenderWidgetHostViewAndroid::ClearBeginFrameRequest(
    BeginFrameRequestType request) {
  uint32_t prior_requests = outstanding_begin_frame_requests_;
  outstanding_begin_frame_requests_ = prior_requests & ~request;

  viz::BeginFrameSource* source = begin_frame_source_;
  if (source && !outstanding_begin_frame_requests_ && prior_requests)
    source->RemoveObserver(this);
}

void RenderWidgetHostViewAndroid::StartObservingRootWindow() {
  DCHECK(view_.parent());
  DCHECK(view_.GetWindowAndroid());
  DCHECK(is_showing_);
  if (observing_root_window_)
    return;

  observing_root_window_ = true;
  SendBeginFramePaused();
  view_.GetWindowAndroid()->AddObserver(this);
  // When using browser compositor, DelegatedFrameHostAndroid provides the BFS.
  if (!using_browser_compositor_)
    SetBeginFrameSource(view_.GetWindowAndroid()->GetBeginFrameSource());

  ui::WindowAndroidCompositor* compositor =
      view_.GetWindowAndroid()->GetCompositor();
  if (compositor) {
    delegated_frame_host_->AttachToCompositor(compositor);
  }
}

void RenderWidgetHostViewAndroid::StopObservingRootWindow() {
  if (!(view_.GetWindowAndroid())) {
    DCHECK(!observing_root_window_);
    return;
  }

  if (!observing_root_window_)
    return;

  // Reset window state variables to their defaults.
  is_window_activity_started_ = true;
  is_window_visible_ = true;
  observing_root_window_ = false;
  SendBeginFramePaused();
  view_.GetWindowAndroid()->RemoveObserver(this);
  if (!using_browser_compositor_)
    SetBeginFrameSource(nullptr);
  // If the DFH has already been destroyed, it will have cleaned itself up.
  // This happens in some WebView cases.
  if (delegated_frame_host_)
    delegated_frame_host_->DetachFromCompositor();
  DCHECK(!begin_frame_source_);
}

void RenderWidgetHostViewAndroid::SendBeginFrame(viz::BeginFrameArgs args) {
  TRACE_EVENT2("cc", "RenderWidgetHostViewAndroid::SendBeginFrame",
               "frame_number", args.sequence_number, "frame_time_us",
               args.frame_time.ToInternalValue());

  // Synchronous compositor does not use deadline-based scheduling.
  // TODO(brianderson): Replace this hardcoded deadline after Android
  // switches to Surfaces and the Browser's commit isn't in the critical path.
  args.deadline = sync_compositor_ ? base::TimeTicks()
  : args.frame_time + (args.interval * 0.6);
  if (sync_compositor_) {
    sync_compositor_->BeginFrame(view_.GetWindowAndroid(), args);
  } else if (renderer_compositor_frame_sink_) {
    renderer_compositor_frame_sink_->OnBeginFrame(args);
  }
}

bool RenderWidgetHostViewAndroid::Animate(base::TimeTicks frame_time) {
  bool needs_animate = false;
  if (overscroll_controller_) {
    needs_animate |=
        overscroll_controller_->Animate(frame_time, view_.parent()->GetLayer());
  }
  // TODO(wjmaclean): Investigate how animation here does or doesn't affect
  // an OOPIF client.
  if (touch_selection_controller_)
    needs_animate |= touch_selection_controller_->Animate(frame_time);
  return needs_animate;
}

void RenderWidgetHostViewAndroid::RequestDisallowInterceptTouchEvent() {
  if (view_.parent())
    view_.RequestDisallowInterceptTouchEvent();
}

void RenderWidgetHostViewAndroid::TransformPointToRootSurface(
    gfx::PointF* point) {
  *point += gfx::Vector2d(
      0, DoBrowserControlsShrinkRendererSize() ? GetTopControlsHeight() : 0);
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  const bool event_consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  gesture_provider_.OnTouchEventAck(
      touch.event.unique_touch_event_id, event_consumed,
      InputEventAckStateIsSetNonBlocking(ack_result));
  if (touch.event.touch_start_or_first_touch_move && event_consumed &&
      host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()
        ->delegate()
        ->GetInputEventRouter()
        ->OnHandledTouchStartOrFirstTouchMove(
            touch.event.unique_touch_event_id);
  }
}

void RenderWidgetHostViewAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (overscroll_controller_)
    overscroll_controller_->OnGestureEventAck(event, ack_result);
  mouse_wheel_phase_handler_.GestureEventAck(event, ack_result);

  ForwardTouchpadZoomEventIfNecessary(event, ack_result);

  if (!gesture_listener_manager_)
    return;
  gesture_listener_manager_->GestureEventAck(event, ack_result);
}

RenderViewHostDelegateView*
RenderWidgetHostViewAndroid::GetRenderViewHostDelegateView() const {
  RenderWidgetHostDelegate* delegate = host()->delegate();
  return delegate ? delegate->GetDelegateView() : nullptr;
}

InputEventAckState RenderWidgetHostViewAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (overscroll_controller_ &&
      blink::WebInputEvent::IsGestureEventType(input_event.GetType())) {
    blink::WebGestureEvent gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    if (overscroll_controller_->WillHandleGestureEvent(gesture_event)) {
      // Terminate an active fling when a GSU generated from the fling progress
      // (GSU with inertial state) is consumed by the overscroll_controller_ and
      // overscrolling mode is not |OVERSCROLL_NONE|. The early fling
      // termination generates a GSE which completes the overscroll action.
      if (gesture_event.GetType() ==
              blink::WebInputEvent::kGestureScrollUpdate &&
          gesture_event.data.scroll_update.inertial_phase ==
              blink::WebGestureEvent::kMomentumPhase) {
        host_->StopFling();
      }

      return INPUT_EVENT_ACK_STATE_CONSUMED;
    }
  }

  if (gesture_listener_manager_ &&
      gesture_listener_manager_->FilterInputEvent(input_event)) {
    return INPUT_EVENT_ACK_STATE_CONSUMED;
  }

  if (!host())
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  if (input_event.GetType() == blink::WebInputEvent::kGestureTapDown ||
      input_event.GetType() == blink::WebInputEvent::kTouchStart) {
    GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                             false /* force_create */, base::Bind(&WakeUpGpu));
  }

  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

InputEventAckState RenderWidgetHostViewAndroid::FilterChildGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  if (overscroll_controller_ &&
      overscroll_controller_->WillHandleGestureEvent(gesture_event))
    return INPUT_EVENT_ACK_STATE_CONSUMED;
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

BrowserAccessibilityManager*
    RenderWidgetHostViewAndroid::CreateBrowserAccessibilityManager(
        BrowserAccessibilityDelegate* delegate, bool for_root_frame) {
  return new BrowserAccessibilityManagerAndroid(
      BrowserAccessibilityManagerAndroid::GetEmptyDocument(),
      for_root_frame && host() ? GetWebContentsAccessibilityAndroid() : nullptr,
      delegate);
}

bool RenderWidgetHostViewAndroid::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::UnlockMouse() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host())
    return;

  RenderWidgetHostImpl* target_host = host();

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (host()->delegate())
    target_host = host()->delegate()->GetFocusedRenderWidgetHost(host());
  if (!target_host)
    return;

  // Receiving a key event before the double-tap timeout expires cancels opening
  // the spellcheck menu. If the suggestion menu is open, we close the menu.
  if (text_suggestion_host_)
    text_suggestion_host_->OnKeyEvent();

  ui::LatencyInfo latency_info;
  if (event.GetType() == blink::WebInputEvent::kRawKeyDown ||
      event.GetType() == blink::WebInputEvent::kChar) {
    latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  }
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  target_host->ForwardKeyboardEventWithLatencyInfo(event, latency_info);
}

void RenderWidgetHostViewAndroid::SendMouseEvent(
    const ui::MotionEventAndroid& motion_event,
    int action_button) {
  blink::WebInputEvent::Type webMouseEventType =
      ui::ToWebMouseEventType(motion_event.GetAction());

  if (webMouseEventType == blink::WebInputEvent::kUndefined)
    return;

  if (webMouseEventType == blink::WebInputEvent::kMouseDown)
    UpdateMouseState(action_button, motion_event.GetX(0), motion_event.GetY(0));

  int click_count = 0;

  if (webMouseEventType == blink::WebInputEvent::kMouseDown ||
      webMouseEventType == blink::WebInputEvent::kMouseUp)
    click_count = (action_button == ui::MotionEventAndroid::BUTTON_PRIMARY)
                      ? left_click_count_
                      : 1;

  blink::WebMouseEvent mouse_event = WebMouseEventBuilder::Build(
      motion_event, webMouseEventType, click_count, action_button);

  if (!host() || !host()->delegate())
    return;

  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteMouseEvent(
        this, &mouse_event, ui::LatencyInfo());
  } else {
    host()->ForwardMouseEvent(mouse_event);
  }
}

void RenderWidgetHostViewAndroid::UpdateMouseState(int action_button,
                                                   float mousedown_x,
                                                   float mousedown_y) {
  if (action_button != ui::MotionEventAndroid::BUTTON_PRIMARY) {
    // Reset state if middle or right button was pressed.
    left_click_count_ = 0;
    prev_mousedown_timestamp_ = base::TimeTicks();
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  const base::TimeDelta time_delay = current_time - prev_mousedown_timestamp_;
  const gfx::Point mousedown_point(mousedown_x, mousedown_y);
  const float distance_squared =
      (mousedown_point - prev_mousedown_point_).LengthSquared();
  if (left_click_count_ > 2 || time_delay > kClickCountInterval ||
      distance_squared > kClickCountRadiusSquaredDIP) {
    left_click_count_ = 0;
  }
  left_click_count_++;
  prev_mousedown_timestamp_ = current_time;
  prev_mousedown_point_ = mousedown_point;
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (!host() || !host()->delegate())
    return;

  ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  blink::WebMouseWheelEvent wheel_event(event);
  bool should_route_events = ShouldRouteEvents();
  mouse_wheel_phase_handler_.AddPhaseIfNeededAndScheduleEndEvent(
      wheel_event, should_route_events);

  if (should_route_events) {
    host()->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
        this, &wheel_event, latency_info);
  } else {
    host()->ForwardWheelEventWithLatencyInfo(wheel_event, latency_info);
  }
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_controller_)
    overscroll_controller_->Enable();

  if (!host() || !host()->delegate() ||
      event.GetType() == blink::WebInputEvent::kUndefined) {
    return;
  }

  // We let the touch selection controller see gesture events here, since they
  // may be routed and not make it to FilterInputEvent().
  if (touch_selection_controller_ &&
      event.SourceDevice() ==
          blink::WebGestureDevice::kWebGestureDeviceTouchscreen) {
    switch (event.GetType()) {
      case blink::WebInputEvent::kGestureLongPress:
        touch_selection_controller_->HandleLongPressEvent(
            event.TimeStamp(), event.PositionInWidget());
        break;

      case blink::WebInputEvent::kGestureTap:
        touch_selection_controller_->HandleTapEvent(event.PositionInWidget(),
                                                    event.data.tap.tap_count);
        break;

      case blink::WebInputEvent::kGestureScrollBegin:
        touch_selection_controller_->OnScrollBeginEvent();
        break;

      default:
        break;
    }
  }

  ui::LatencyInfo latency_info =
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(event);
  if (event.SourceDevice() ==
      blink::WebGestureDevice::kWebGestureDeviceTouchscreen) {
    if (event.GetType() == blink::WebInputEvent::kGestureScrollBegin) {
      // If there is a current scroll going on and a new scroll that isn't
      // wheel based, send a synthetic wheel event with kPhaseEnded to cancel
      // the current scroll.
      mouse_wheel_phase_handler_.DispatchPendingWheelEndEvent();
    } else if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd) {
      // Make sure that the next wheel event will have phase = |kPhaseBegan|.
      // This is for maintaining the correct phase info when some of the wheel
      // events get ignored while a touchscreen scroll is going on.
      mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();
    }

  } else if (event.GetType() == blink::WebInputEvent::kGestureFlingStart &&
             event.SourceDevice() ==
                 blink::WebGestureDevice::kWebGestureDeviceTouchpad) {
    // Ignore the pending wheel end event to avoid sending a wheel event with
    // kPhaseEnded before a GFS.
    mouse_wheel_phase_handler_.IgnorePendingWheelEndEvent();
  }
  if (ShouldRouteEvents()) {
    blink::WebGestureEvent gesture_event(event);
    host()->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &gesture_event, latency_info);
  } else {
    host()->ForwardGestureEventWithLatencyInfo(event, latency_info);
  }
}

bool RenderWidgetHostViewAndroid::ShowSelectionMenu(
    const ContextMenuParams& params) {
  if (!selection_popup_controller_ || is_in_vr_)
    return false;

  return selection_popup_controller_->ShowSelectionMenu(params,
                                                        GetTouchHandleHeight());
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host() && host()->delegate())
    host()->delegate()->MoveCaret(point);
}

void RenderWidgetHostViewAndroid::ShowContextMenuAtPoint(
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (host())
    host()->ShowContextMenuAtPoint(point, source_type);
}

void RenderWidgetHostViewAndroid::DismissTextHandles() {
  if (touch_selection_controller_)
    touch_selection_controller_->HideAndDisallowShowingAutomatically();
}

void RenderWidgetHostViewAndroid::SetTextHandlesTemporarilyHidden(
    bool hide_handles) {
  if (!touch_selection_controller_ ||
      handles_hidden_by_selection_ui_ == hide_handles)
    return;
  handles_hidden_by_selection_ui_ = hide_handles;
  SetTextHandlesHiddenInternal();
}

base::Optional<SkColor> RenderWidgetHostViewAndroid::GetCachedBackgroundColor()
    const {
  return RenderWidgetHostViewBase::GetBackgroundColor();
}

void RenderWidgetHostViewAndroid::SetIsInVR(bool is_in_vr) {
  if (is_in_vr_ == is_in_vr)
    return;
  is_in_vr_ = is_in_vr;
  // TODO(crbug.com/851054): support touch selection handles in VR.
  SetTextHandlesHiddenInternal();

  gesture_provider_.UpdateConfig(ui::GetGestureProviderConfig(
      is_in_vr_ ? ui::GestureProviderConfigType::CURRENT_PLATFORM_VR
                : ui::GestureProviderConfigType::CURRENT_PLATFORM));

  if (is_in_vr_ && controls_initialized_) {
    // TODO(mthiesse, https://crbug.com/825765): See the TODO in
    // RenderWidgetHostViewAndroid::OnFrameMetadataUpdated. RWHVA isn't
    // initialized with VR state so the initial frame metadata top controls
    // height can be dropped when a new RWHVA is created.
    view_.OnTopControlsChanged(prev_top_controls_translate_,
                               prev_top_shown_pix_);
    view_.OnBottomControlsChanged(prev_bottom_controls_translate_,
                                  prev_bottom_shown_pix_);
  }
}

bool RenderWidgetHostViewAndroid::IsInVR() const {
  return is_in_vr_;
}

void RenderWidgetHostViewAndroid::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (sync_compositor_)
    sync_compositor_->DidOverscroll(params);

  if (!view_.parent() || !is_showing_)
    return;

  if (overscroll_controller_)
    overscroll_controller_->OnOverscrolled(params);
}

void RenderWidgetHostViewAndroid::DidStopFlinging() {
  if (!gesture_listener_manager_)
    return;
  gesture_listener_manager_->DidStopFlinging();
}

const viz::FrameSinkId& RenderWidgetHostViewAndroid::GetFrameSinkId() const {
  if (!delegated_frame_host_)
    return viz::FrameSinkIdAllocator::InvalidFrameSinkId();

  return delegated_frame_host_->GetFrameSinkId();
}

void RenderWidgetHostViewAndroid::UpdateNativeViewTree(
    gfx::NativeView parent_native_view) {
  bool will_build_tree = parent_native_view != nullptr;
  bool has_view_tree = view_.parent() != nullptr;

  // Allows same parent view to be set again.
  DCHECK(!will_build_tree || !has_view_tree ||
         parent_native_view == view_.parent());

  StopObservingRootWindow();

  bool resize = false;
  if (will_build_tree != has_view_tree) {
    touch_selection_controller_.reset();
    if (has_view_tree) {
      view_.RemoveObserver(this);
      view_.RemoveFromParent();
      view_.GetLayer()->RemoveFromParent();
    }
    if (will_build_tree) {
      view_.AddObserver(this);
      parent_native_view->AddChild(&view_);
      parent_native_view->GetLayer()->AddChild(view_.GetLayer());
    }

    // TODO(yusufo) : Get rid of the below conditions and have a better handling
    // for resizing after crbug.com/628302 is handled.
    bool is_size_initialized = !will_build_tree ||
                               view_.GetSize().width() != 0 ||
                               view_.GetSize().height() != 0;
    if (has_view_tree || is_size_initialized)
      resize = true;
    has_view_tree = will_build_tree;
  }

  if (!has_view_tree) {
    sync_compositor_.reset();
    return;
  }

  if (is_showing_ && view_.GetWindowAndroid())
    StartObservingRootWindow();

  if (resize) {
    SynchronizeVisualProperties(
        cc::DeadlinePolicy::UseSpecifiedDeadline(
            ui::DelegatedFrameHostAndroid::ResizeTimeoutFrames()),
        base::nullopt, base::nullopt);
  }

  if (!touch_selection_controller_) {
    ui::TouchSelectionControllerClient* client =
        touch_selection_controller_client_manager_.get();
    if (touch_selection_controller_client_for_test_)
      client = touch_selection_controller_client_for_test_.get();

    touch_selection_controller_ = CreateSelectionController(client, true);
  }

  CreateOverscrollControllerIfPossible();
}

MouseWheelPhaseHandler*
RenderWidgetHostViewAndroid::GetMouseWheelPhaseHandler() {
  return &mouse_wheel_phase_handler_;
}

void RenderWidgetHostViewAndroid::EvictDelegatedFrame() {
  if (!delegated_frame_host_)
    return;

  delegated_frame_host_->EvictDelegatedFrame();
  current_surface_size_.SetSize(0, 0);
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewAndroid::GetTouchSelectionControllerClientManager() {
  return touch_selection_controller_client_manager_.get();
}

const viz::LocalSurfaceId& RenderWidgetHostViewAndroid::GetLocalSurfaceId()
    const {
  if (!delegated_frame_host_)
    return viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
  return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

base::TimeTicks RenderWidgetHostViewAndroid::GetLocalSurfaceIdAllocationTime()
    const {
  if (!delegated_frame_host_)
    return base::TimeTicks();
  return local_surface_id_allocator_.allocation_time();
}

void RenderWidgetHostViewAndroid::OnRenderWidgetInit() {
  if (sync_compositor_)
    sync_compositor_->InitMojo();
}

bool RenderWidgetHostViewAndroid::OnMouseEvent(
    const ui::MotionEventAndroid& event) {
  RecordToolTypeForActionDown(event);
  SendMouseEvent(event, event.GetActionButton());
  return true;
}

bool RenderWidgetHostViewAndroid::OnMouseWheelEvent(
    const ui::MotionEventAndroid& event) {
  SendMouseWheelEvent(WebMouseWheelEventBuilder::Build(event));
  return true;
}

void RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::ET_GESTURE_PINCH_BEGIN ||
       gesture.type() == ui::ET_GESTURE_PINCH_UPDATE ||
       gesture.type() == ui::ET_GESTURE_PINCH_END) &&
      !IsPinchToZoomEnabled()) {
    return;
  }

  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);
  // TODO(jdduke): Remove this workaround after Android fixes UiAutomator to
  // stop providing shift meta values to synthetic MotionEvents. This prevents
  // unintended shift+click interpretation of all accessibility clicks.
  // See crbug.com/443247.
  if (web_gesture.GetType() == blink::WebInputEvent::kGestureTap &&
      web_gesture.GetModifiers() == blink::WebInputEvent::kShiftKey) {
    web_gesture.SetModifiers(blink::WebInputEvent::kNoModifiers);
  }
  SendGestureEvent(web_gesture);
}

bool RenderWidgetHostViewAndroid::RequiresDoubleTapGestureEvents() const {
  return true;
}

void RenderWidgetHostViewAndroid::OnSizeChanged() {
  if (ime_adapter_android_)
    ime_adapter_android_->UpdateAfterViewSizeChanged();
}

void RenderWidgetHostViewAndroid::OnPhysicalBackingSizeChanged() {
  EvictFrameIfNecessary();
  SynchronizeVisualProperties(
      cc::DeadlinePolicy::UseSpecifiedDeadline(
          ui::DelegatedFrameHostAndroid::ResizeTimeoutFrames()),
      base::nullopt, base::nullopt);
}

void RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged(bool visible) {
  TRACE_EVENT1("browser",
               "RenderWidgetHostViewAndroid::OnRootWindowVisibilityChanged",
               "visible", visible);
  DCHECK(observing_root_window_);
  if (is_window_visible_ == visible)
    return;

  is_window_visible_ = visible;

  if (visible)
    ShowInternal();
  else
    HideInternal();
}

void RenderWidgetHostViewAndroid::OnAttachedToWindow() {
  if (!view_.parent())
    return;

  if (is_showing_)
    StartObservingRootWindow();
  DCHECK(view_.GetWindowAndroid());
  if (view_.GetWindowAndroid()->GetCompositor())
    OnAttachCompositor();
}

void RenderWidgetHostViewAndroid::OnDetachedFromWindow() {
  StopObservingRootWindow();
  OnDetachCompositor();
}

void RenderWidgetHostViewAndroid::OnAttachCompositor() {
  DCHECK(view_.parent());
  CreateOverscrollControllerIfPossible();
  if (observing_root_window_ && using_browser_compositor_) {
    ui::WindowAndroidCompositor* compositor =
        view_.GetWindowAndroid()->GetCompositor();
    delegated_frame_host_->AttachToCompositor(compositor);
  }
}

void RenderWidgetHostViewAndroid::OnDetachCompositor() {
  DCHECK(view_.parent());
  overscroll_controller_.reset();
  if (using_browser_compositor_)
    delegated_frame_host_->DetachFromCompositor();
}

void RenderWidgetHostViewAndroid::OnBeginFrame(
    const viz::BeginFrameArgs& args) {
  TRACE_EVENT0("cc,benchmark", "RenderWidgetHostViewAndroid::OnBeginFrame");
  if (!host()) {
    OnDidNotProduceFrame(
        viz::BeginFrameAck(args.source_id, args.sequence_number, false));
    return;
  }

  // In sync mode, we disregard missed frame args to ensure that
  // SynchronousCompositorBrowserFilter::SyncStateAfterVSync will be called
  // during WindowAndroid::WindowBeginFrameSource::OnVSync() observer iteration.
  if (sync_compositor_ && args.type == viz::BeginFrameArgs::MISSED) {
    OnDidNotProduceFrame(
        viz::BeginFrameAck(args.source_id, args.sequence_number, false));
    return;
  }

  bool webview_fling = sync_compositor_ && is_currently_scrolling_viewport_;
  if (!webview_fling) {
    host_->ProgressFlingIfNeeded(args.frame_time);
  } else if (sync_compositor_->on_compute_scroll_called()) {
    // On Android webview progress the fling only when |OnComputeScroll| is
    // called since in some cases Apps override |OnComputeScroll| to cancel
    // fling animation.
    host_->ProgressFlingIfNeeded(args.frame_time);
  }

  // Update |last_begin_frame_args_| before handling
  // |outstanding_begin_frame_requests_| to prevent the BeginFrameSource from
  // sending the same MISSED args in infinite recursion.
  last_begin_frame_args_ = args;

  if ((outstanding_begin_frame_requests_ & BEGIN_FRAME) ||
      (outstanding_begin_frame_requests_ & PERSISTENT_BEGIN_FRAME)) {
    ClearBeginFrameRequest(BEGIN_FRAME);
    SendBeginFrame(args);
  } else {
    OnDidNotProduceFrame(
        viz::BeginFrameAck(args.source_id, args.sequence_number, false));
  }
}

const viz::BeginFrameArgs& RenderWidgetHostViewAndroid::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

bool RenderWidgetHostViewAndroid::WantsAnimateOnlyBeginFrames() const {
  return wants_animate_only_begin_frames_;
}

void RenderWidgetHostViewAndroid::SendBeginFramePaused() {
  bool paused = begin_frame_paused_ || !observing_root_window_;

  if (!using_browser_compositor_) {
    if (sync_compositor_)
      sync_compositor_->SetBeginFramePaused(paused);
  } else if (renderer_compositor_frame_sink_) {
    renderer_compositor_frame_sink_->OnBeginFramePausedChanged(paused);
  }
}

void RenderWidgetHostViewAndroid::OnBeginFrameSourcePausedChanged(bool paused) {
  if (paused != begin_frame_paused_) {
    begin_frame_paused_ = paused;
    SendBeginFramePaused();
  }
}

void RenderWidgetHostViewAndroid::OnAnimate(base::TimeTicks begin_frame_time) {
  if (Animate(begin_frame_time))
    SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::OnActivityStopped() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStopped");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = false;
  HideInternal();
}

void RenderWidgetHostViewAndroid::OnActivityStarted() {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAndroid::OnActivityStarted");
  DCHECK(observing_root_window_);
  is_window_activity_started_ = true;
  ShowInternal();
}

void RenderWidgetHostViewAndroid::OnLostResources() {
  EvictDelegatedFrame();
}

void RenderWidgetHostViewAndroid::SetTextHandlesHiddenForStylus(
    bool hide_handles) {
  if (!touch_selection_controller_ || handles_hidden_by_stylus_ == hide_handles)
    return;
  handles_hidden_by_stylus_ = hide_handles;
  SetTextHandlesHiddenInternal();
}

void RenderWidgetHostViewAndroid::SetTextHandlesHiddenInternal() {
  if (!touch_selection_controller_)
    return;
  // TODO(crbug.com/851054): support touch selection handles in VR.
  touch_selection_controller_->SetTemporarilyHidden(
      is_in_vr_ || handles_hidden_by_stylus_ ||
      handles_hidden_by_selection_ui_);
}

void RenderWidgetHostViewAndroid::OnStylusSelectBegin(float x0,
                                                      float y0,
                                                      float x1,
                                                      float y1) {
  SetTextHandlesHiddenForStylus(true);
  // TODO(ajith.v) Refactor the event names as this is not really handle drag,
  // but currently we use same for long press drag selection as well.
  OnSelectionEvent(ui::SELECTION_HANDLE_DRAG_STARTED);
  SelectBetweenCoordinates(gfx::PointF(x0, y0), gfx::PointF(x1, y1));
}

void RenderWidgetHostViewAndroid::OnStylusSelectUpdate(float x, float y) {
  MoveRangeSelectionExtent(gfx::PointF(x, y));
}

void RenderWidgetHostViewAndroid::OnStylusSelectEnd(float x, float y) {
  SetTextHandlesHiddenForStylus(false);
  // TODO(ajith.v) Refactor the event names as this is not really handle drag,
  // but currently we use same for long press drag selection as well.
  OnSelectionEvent(ui::SELECTION_HANDLE_DRAG_STOPPED);
}

void RenderWidgetHostViewAndroid::OnStylusSelectTap(base::TimeTicks time,
                                                    float x,
                                                    float y) {
  // Treat the stylus tap as a long press, activating either a word selection or
  // context menu depending on the targetted content.
  blink::WebGestureEvent long_press = WebGestureEventBuilder::Build(
      blink::WebInputEvent::kGestureLongPress, time, x, y);
  SendGestureEvent(long_press);
}

void RenderWidgetHostViewAndroid::ComputeEventLatencyOSTouchHistograms(
      const ui::MotionEvent& event) {
  base::TimeTicks event_time = event.GetEventTime();
  base::TimeDelta delta = base::TimeTicks::Now() - event_time;
  switch (event.GetAction()) {
    case ui::MotionEvent::Action::DOWN:
    case ui::MotionEvent::Action::POINTER_DOWN:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_PRESSED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    case ui::MotionEvent::Action::MOVE:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_MOVED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    case ui::MotionEvent::Action::UP:
    case ui::MotionEvent::Action::POINTER_UP:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_RELEASED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    default:
      return;
  }
}

void RenderWidgetHostViewAndroid::CreateOverscrollControllerIfPossible() {
  // an OverscrollController is already set
  if (overscroll_controller_)
    return;

  RenderWidgetHostDelegate* delegate = host()->delegate();
  if (!delegate)
    return;

  RenderViewHostDelegateView* delegate_view = delegate->GetDelegateView();
  // render_widget_host_unittest.cc uses an object called
  // MockRenderWidgetHostDelegate that does not have a DelegateView
  if (!delegate_view)
    return;

  ui::OverscrollRefreshHandler* overscroll_refresh_handler =
      delegate_view->GetOverscrollRefreshHandler();
  if (!overscroll_refresh_handler)
    return;

  if (!view_.parent())
    return;

  // If window_android is null here, this is bad because we don't listen for it
  // being set, so we won't be able to construct the OverscrollController at the
  // proper time.
  ui::WindowAndroid* window_android = view_.GetWindowAndroid();
  if (!window_android)
    return;

  ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();
  if (!compositor)
    return;

  overscroll_controller_ = std::make_unique<OverscrollControllerAndroid>(
      overscroll_refresh_handler, compositor, view_.GetDipScale());
}

void RenderWidgetHostViewAndroid::SetOverscrollControllerForTesting(
    ui::OverscrollRefreshHandler* overscroll_refresh_handler) {
  overscroll_controller_ = std::make_unique<OverscrollControllerAndroid>(
      overscroll_refresh_handler, view_.GetWindowAndroid()->GetCompositor(),
      view_.GetDipScale());
}

void RenderWidgetHostViewAndroid::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewChildFrame());
  DCHECK(!static_cast<RenderWidgetHostViewBase*>(view)
              ->IsRenderWidgetHostViewGuest());
  base::Optional<SkColor> color = view->GetBackgroundColor();
  if (color)
    SetBackgroundColor(*color);

  RenderWidgetHostViewAndroid* view_android =
      static_cast<RenderWidgetHostViewAndroid*>(view);
  if (!delegated_frame_host_ || !view_android->delegated_frame_host_)
    return;
  delegated_frame_host_->TakeFallbackContentFrom(
      view_android->delegated_frame_host_.get());
  host()->GetContentRenderingTimeoutFrom(view_android->host());
}

void RenderWidgetHostViewAndroid::OnSynchronizedDisplayPropertiesChanged() {
  SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                              base::nullopt, base::nullopt);
}

base::Optional<SkColor> RenderWidgetHostViewAndroid::GetBackgroundColor()
    const {
  return default_background_color_;
}

void RenderWidgetHostViewAndroid::DidNavigate() {
  if (!delegated_frame_host_) {
    RenderWidgetHostViewBase::DidNavigate();
    return;
  }

  if (is_first_navigation_) {
    SynchronizeVisualProperties(
        cc::DeadlinePolicy::UseExistingDeadline(),
        local_surface_id_allocator_.GetCurrentLocalSurfaceId(),
        local_surface_id_allocator_.allocation_time());
  } else {
    SynchronizeVisualProperties(cc::DeadlinePolicy::UseExistingDeadline(),
                                base::nullopt, base::nullopt);
  }
  delegated_frame_host_->DidNavigate();
  is_first_navigation_ = false;
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewAndroid::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  if (!features::IsSurfaceSynchronizationEnabled())
    return RenderWidgetHostViewBase::DidUpdateVisualProperties(metadata);

  base::OnceCallback<void()> allocation_task = base::BindOnce(
      &RenderWidgetHostViewAndroid::OnDidUpdateVisualPropertiesComplete,
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

void RenderWidgetHostViewAndroid::WasEvicted() {
  local_surface_id_allocator_.GenerateId();
}

}  // namespace content
