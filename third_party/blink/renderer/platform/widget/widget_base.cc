// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/ukm_manager.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/input/pointer_lock_context.mojom-blink.h"
#include "third_party/blink/public/mojom/page/record_content_to_visible_time_request.mojom-blink.h"
#include "third_party/blink/public/mojom/widget/visual_properties.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_widget_scheduler.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view.h"
#include "third_party/blink/renderer/platform/widget/compositing/widget_compositor.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/input/ime_event_guard.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"
#include "ui/base/ime/mojom/text_input_state.mojom-blink.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/presentation_feedback.h"

namespace blink {

namespace {

static const int kInvalidNextPreviousFlagsValue = -1;

void OnDidPresentForceDrawFrame(
    mojom::blink::Widget::ForceRedrawCallback callback,
    const gfx::PresentationFeedback& feedback) {
  std::move(callback).Run();
}

bool IsDateTimeInput(ui::TextInputType type) {
  return type == ui::TEXT_INPUT_TYPE_DATE ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL ||
         type == ui::TEXT_INPUT_TYPE_MONTH ||
         type == ui::TEXT_INPUT_TYPE_TIME || type == ui::TEXT_INPUT_TYPE_WEEK;
}

ui::TextInputType ConvertWebTextInputType(blink::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX))
      << "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

ui::TextInputMode ConvertWebTextInputMode(blink::WebTextInputMode mode) {
  // Check the mode is in the range representable by ui::TextInputMode.
  DCHECK_LE(mode, static_cast<int>(ui::TEXT_INPUT_MODE_MAX))
      << "blink::WebTextInputMode and ui::TextInputMode not synchronized";
  return static_cast<ui::TextInputMode>(mode);
}

unsigned OrientationTypeToAngle(mojom::blink::ScreenOrientation type) {
  unsigned angle;
  // FIXME(ostap): This relationship between orientationType and
  // orientationAngle is temporary. The test should be able to specify
  // the angle in addition to the orientation type.
  switch (type) {
    case mojom::blink::ScreenOrientation::kLandscapePrimary:
      angle = 90;
      break;
    case mojom::blink::ScreenOrientation::kLandscapeSecondary:
      angle = 270;
      break;
    case mojom::blink::ScreenOrientation::kPortraitSecondary:
      angle = 180;
      break;
    default:
      angle = 0;
  }
  return angle;
}

}  // namespace

WidgetBase::WidgetBase(
    WidgetBaseClient* client,
    CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget,
    bool hidden,
    bool never_composited)
    : client_(client),
      widget_host_(std::move(widget_host)),
      receiver_(this, std::move(widget)),
      next_previous_flags_(kInvalidNextPreviousFlagsValue),
      use_zoom_for_dsf_(Platform::Current()->IsUseZoomForDSFEnabled()),
      is_hidden_(hidden),
      never_composited_(never_composited) {
  if (auto* main_thread_scheduler =
          scheduler::WebThreadScheduler::MainThreadScheduler()) {
    render_widget_scheduling_state_ =
        main_thread_scheduler->NewRenderWidgetSchedulingState();
  }
}

WidgetBase::~WidgetBase() {
  // Ensure Shutdown was called.
  DCHECK(!layer_tree_view_);
}

void WidgetBase::InitializeCompositing(
    scheduler::WebThreadScheduler* main_thread_scheduler,
    cc::TaskGraphRunner* task_graph_runner,
    bool for_child_local_root_frame,
    const ScreenInfo& screen_info,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory,
    const cc::LayerTreeSettings* settings) {
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      main_thread_scheduler->CompositorTaskRunner();
  if (!main_thread_task_runner)
    main_thread_task_runner = base::ThreadTaskRunnerHandle::Get();

  auto* compositing_thread_scheduler =
      scheduler::WebThreadScheduler::CompositorThreadScheduler();
  layer_tree_view_ = std::make_unique<LayerTreeView>(
      this, main_thread_task_runner,
      compositing_thread_scheduler
          ? compositing_thread_scheduler->DefaultTaskRunner()
          : nullptr,
      task_graph_runner, main_thread_scheduler);

  base::Optional<cc::LayerTreeSettings> default_settings;
  if (!settings) {
    default_settings = GenerateLayerTreeSettings(
        compositing_thread_scheduler, for_child_local_root_frame,
        screen_info.rect.size(), screen_info.device_scale_factor);
    settings = &default_settings.value();
  }
  screen_info_ = screen_info;
  layer_tree_view_->Initialize(*settings, std::move(ukm_recorder_factory));

  FrameWidget* frame_widget = client_->FrameWidget();

  scheduler::WebThreadScheduler* compositor_thread_scheduler =
      scheduler::WebThreadScheduler::CompositorThreadScheduler();
  scoped_refptr<base::SingleThreadTaskRunner> compositor_input_task_runner;
  // Use the compositor thread task runner unless this is a popup or other such
  // non-frame widgets. The |compositor_thread_scheduler| can be null in tests
  // without a compositor thread.
  if (frame_widget && compositor_thread_scheduler) {
    compositor_input_task_runner =
        compositor_thread_scheduler->DefaultTaskRunner();
  }

  // We only use an external input handler for frame widgets because only
  // frames use the compositor for input handling. Other kinds of widgets
  // (e.g.  popups, plugins) must forward their input directly through
  // WidgetBaseInputHandler.
  bool uses_input_handler = frame_widget;
  widget_input_handler_manager_ = WidgetInputHandlerManager::Create(
      weak_ptr_factory_.GetWeakPtr(), never_composited_,
      std::move(compositor_input_task_runner), main_thread_scheduler,
      uses_input_handler);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAllowPreCommitInput))
    widget_input_handler_manager_->AllowPreCommitInput();

  UpdateScreenInfo(screen_info);
}

void WidgetBase::Shutdown(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) {
  if (!cleanup_runner)
    cleanup_runner = base::ThreadTaskRunnerHandle::Get();

  // The |input_event_queue_| is refcounted and will live while an event is
  // being handled. This drops the connection back to this WidgetBase which
  // is being destroyed.
  if (widget_input_handler_manager_)
    widget_input_handler_manager_->ClearClient();

  // The LayerTreeHost may already be in the call stack, if this WidgetBase
  // is being destroyed during an animation callback for instance. We can not
  // delete it here and unwind the stack back up to it, or it will crash. So
  // we post the deletion to another task, but disconnect the LayerTreeHost
  // (via the LayerTreeView) from the destroying WidgetBase. The
  // LayerTreeView owns the LayerTreeHost, and is its client, so they are kept
  // alive together for a clean call stack.
  if (layer_tree_view_) {
    layer_tree_view_->Disconnect();
    cleanup_runner->DeleteSoon(FROM_HERE, std::move(layer_tree_view_));
  }

  // The |widget_input_handler_manager_| needs to outlive the LayerTreeHost,
  // which is destroyed asynchronously by DeleteSoon(). This needs to be a
  // NonNestableTask as it needs to occur after DeleteSoon.
  cleanup_runner->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce([](scoped_refptr<WidgetInputHandlerManager> manager) {},
                     std::move(widget_input_handler_manager_)));

  if (widget_compositor_) {
    widget_compositor_->Shutdown();
    widget_compositor_ = nullptr;
  }
}

cc::LayerTreeHost* WidgetBase::LayerTreeHost() const {
  return layer_tree_view_->layer_tree_host();
}

cc::AnimationHost* WidgetBase::AnimationHost() const {
  return layer_tree_view_->animation_host();
}

scheduler::WebRenderWidgetSchedulingState*
WidgetBase::RendererWidgetSchedulingState() const {
  return render_widget_scheduling_state_.get();
}

void WidgetBase::ForceRedraw(
    mojom::blink::Widget::ForceRedrawCallback callback) {
  LayerTreeHost()->RequestPresentationTimeForNextFrame(
      base::BindOnce(&OnDidPresentForceDrawFrame, std::move(callback)));
  LayerTreeHost()->SetNeedsCommitWithForcedRedraw();

  // ScheduleAnimationForWebTests() which is implemented by WebWidgetTestProxy,
  // providing the additional control over the lifecycle of compositing required
  // by web tests. This will be a no-op on production.
  client_->ScheduleAnimationForWebTests();
}

void WidgetBase::GetWidgetInputHandler(
    mojo::PendingReceiver<mojom::blink::WidgetInputHandler> request,
    mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host) {
  widget_input_handler_manager_->AddInterface(std::move(request),
                                              std::move(host));
}

void WidgetBase::UpdateVisualProperties(
    const VisualProperties& visual_properties_from_browser) {
  TRACE_EVENT0("renderer", "WidgetBase::UpdateVisualProperties");

  // UpdateVisualProperties is used to receive properties from the browser
  // process for this WidgetBase. There are roughly 4 types of
  // VisualProperties.
  // TODO(danakj): Splitting these 4 types of properties apart and making them
  // more explicit could be super useful to understanding this code.
  // 1. Unique to each WidgetBase. Computed by the RenderWidgetHost and passed
  //    to the WidgetBase which consumes it here.
  //    Example: new_size.
  // 2. Global properties, which are given to each WidgetBase (to maintain
  //    the requirement that a WidgetBase is updated atomically). These
  //    properties are usually the same for every WidgetBase, except when
  //    device emulation changes them in the main frame WidgetBase only.
  //    Example: screen_info.
  // 3. Computed in the renderer of the main frame WebFrameWidgetBase (in blink
  //    usually). Passed down through the waterfall dance to child frame
  //    WebFrameWidgetBase. Here that step is performed by passing the value
  //    along to all RemoteFrame objects that are below this WebFrameWidgetBase
  //    in the frame tree. The main frame (top level) WebFrameWidgetBase ignores
  //    this value from its RenderWidgetHost since it is controlled in the
  //    renderer. Child frame WebFrameWidgetBases consume the value from their
  //    RenderWidgetHost. Example: page_scale_factor.
  // 4. Computed independently in the renderer for each WidgetBase (in blink
  //    usually). Passed down from the parent to the child WidgetBases through
  //    the waterfall dance, but the value only travels one step - the child
  //    frame WebFrameWidgetBase would compute values for grandchild
  //    WebFrameWidgetBases independently. Here the value is passed to child
  //    frame RenderWidgets by passing the value along to all RemoteFrame
  //    objects that are below this WebFrameWidgetBase in the frame tree. Each
  //    WidgetBase consumes this value when it is received from its
  //    RenderWidgetHost. Example: compositor_viewport_pixel_rect.
  // For each of these properties:
  //   If the WebView also knows these properties, each WebFrameWidgetBase
  //   will pass them along to the WebView as it receives it, even if there
  //   are multiple WebFrameWidgetBases related to the same WebView.
  //   However when the main frame in the renderer is the source of truth,
  //   then child widgets must not clobber that value! In all cases child frames
  //   do not need to update state in the WebView when a local main frame is
  //   present as it always sets the value first.
  //   TODO(danakj): This does create a race if there are multiple
  //   UpdateVisualProperties updates flowing through the WebFrameWidgetBase
  //   tree at the same time, and it seems that only one WebFrameWidgetBase for
  //   each WebView should be responsible for this update.
  //
  //   TODO(danakj): A more explicit API to give values from here to RenderView
  //   and/or WebView would be nice. Also a more explicit API to give values to
  //   the RemoteFrame in one go, instead of setting each property
  //   independently, causing an update IPC from the
  //   RenderFrameProxy/RemoteFrame for each one.
  //
  //   See also:
  //   https://docs.google.com/document/d/1G_fR1D_0c1yke8CqDMddoKrDGr3gy5t_ImEH4hKNIII/edit#

  blink::VisualProperties visual_properties = visual_properties_from_browser;
  // Web tests can override the device scale factor in the renderer.
  if (auto scale_factor = client_->GetDeviceScaleFactorForTesting()) {
    visual_properties.screen_info.device_scale_factor = scale_factor;
    visual_properties.compositor_viewport_pixel_rect =
        gfx::Rect(gfx::ScaleToCeiledSize(
            visual_properties.new_size,
            visual_properties.screen_info.device_scale_factor));
  }

  // Inform the rendering thread of the color space indicating the presence of
  // HDR capabilities. The HDR bit happens to be globally true/false for all
  // browser windows (on Windows OS) and thus would be the same for all
  // RenderWidgets, so clobbering each other works out since only the HDR bit is
  // used. See https://crbug.com/803451 and
  // https://chromium-review.googlesource.com/c/chromium/src/+/852912/15#message-68bbd3e25c3b421a79cd028b2533629527d21fee
  Platform::Current()->SetRenderingColorSpace(
      visual_properties.screen_info.display_color_spaces
          .GetScreenInfoColorSpace());

  LayerTreeHost()->SetBrowserControlsParams(
      visual_properties.browser_controls_params);

  client_->UpdateVisualProperties(visual_properties);

  // FrameWidgets have custom code for external page scale factor.
  if (!client_->FrameWidget()) {
    LayerTreeHost()->SetExternalPageScaleFactor(
        visual_properties.page_scale_factor,
        visual_properties.is_pinch_gesture_active);
  }
}

void WidgetBase::UpdateScreenRects(const gfx::Rect& widget_screen_rect,
                                   const gfx::Rect& window_screen_rect,
                                   UpdateScreenRectsCallback callback) {
  if (!client_->UpdateScreenRects(widget_screen_rect, window_screen_rect)) {
    widget_screen_rect_ = widget_screen_rect;
    window_screen_rect_ = window_screen_rect;
  }
  std::move(callback).Run();
}

void WidgetBase::WasHidden() {
  // A provisional frame widget will never be hidden since that would require it
  // to be shown first. A frame must be attached to the frame tree before
  // changing visibility.
  DCHECK(!IsForProvisionalFrame());

  TRACE_EVENT0("renderer", "WidgetBase::WasHidden");

  SetHidden(true);

  tab_switch_time_recorder_.TabWasHidden();

  client_->WasHidden();
}

void WidgetBase::WasShown(base::TimeTicks show_request_timestamp,
                          bool was_evicted,
                          mojom::blink::RecordContentToVisibleTimeRequestPtr
                              record_tab_switch_time_request) {
  // The frame must be attached to the frame tree (which makes it no longer
  // provisional) before changing visibility.
  DCHECK(!IsForProvisionalFrame());

  TRACE_EVENT_WITH_FLOW0("renderer", "WidgetBase::WasShown", this,
                         TRACE_EVENT_FLAG_FLOW_IN);

  SetHidden(false);

  if (record_tab_switch_time_request) {
    LayerTreeHost()->RequestPresentationTimeForNextFrame(
        tab_switch_time_recorder_.TabWasShown(
            false /* has_saved_frames */,
            record_tab_switch_time_request->event_start_time,
            record_tab_switch_time_request->destination_is_loaded,
            record_tab_switch_time_request->show_reason_tab_switching,
            record_tab_switch_time_request->show_reason_unoccluded,
            record_tab_switch_time_request->show_reason_bfcache_restore,
            show_request_timestamp));
  }

  client_->WasShown(was_evicted);
}

void WidgetBase::ApplyViewportChanges(
    const cc::ApplyViewportChangesArgs& args) {
  client_->ApplyViewportChanges(args);
}

void WidgetBase::RecordManipulationTypeCounts(cc::ManipulationInfo info) {
  client_->RecordManipulationTypeCounts(info);
}

void WidgetBase::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  client_->SendOverscrollEventFromImplSide(overscroll_delta,
                                           scroll_latched_element_id);
}

void WidgetBase::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  client_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void WidgetBase::OnDeferMainFrameUpdatesChanged(bool defer) {
  // LayerTreeHost::CreateThreaded() will defer main frame updates immediately
  // until it gets a LocalSurfaceIdAllocation. That's before the
  // |widget_input_handler_manager_| is created, so it can be null here.
  // TODO(schenney): To avoid ping-ponging between defer main frame states
  // during initialization, and requiring null checks here, we should probably
  // pass the LocalSurfaceIdAllocation to the compositor while it is
  // initialized so that it doesn't have to immediately switch into deferred
  // mode without being requested to.
  if (!widget_input_handler_manager_)
    return;

  // The input handler wants to know about the mainframe update status to
  // enable/disable input and for metrics.
  widget_input_handler_manager_->OnDeferMainFrameUpdatesChanged(defer);
}

void WidgetBase::OnDeferCommitsChanged(bool defer) {
  // The input handler wants to know about the commit status for metric purposes
  // and to enable/disable input.
  widget_input_handler_manager_->OnDeferCommitsChanged(defer);
}

void WidgetBase::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void WidgetBase::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::LayerTreeFrameSink.
  DCHECK(!never_composited_);

  client_->RequestNewLayerTreeFrameSink(std::move(callback));
}

void WidgetBase::DidCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "WidgetBase::DidCommitAndDrawCompositorFrame");

  client_->DidCommitAndDrawCompositorFrame();
}

void WidgetBase::DidObserveFirstScrollDelay(
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  client_->DidObserveFirstScrollDelay(first_scroll_delay,
                                      first_scroll_timestamp);
}

void WidgetBase::WillCommitCompositorFrame() {
  client_->BeginCommitCompositorFrame();
}

void WidgetBase::DidCommitCompositorFrame(base::TimeTicks commit_start_time) {
  client_->EndCommitCompositorFrame(commit_start_time);
}

void WidgetBase::DidCompletePageScaleAnimation() {
  client_->DidCompletePageScaleAnimation();
}

void WidgetBase::RecordStartOfFrameMetrics() {
  client_->RecordStartOfFrameMetrics();
}

void WidgetBase::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    cc::ActiveFrameSequenceTrackers trackers) {
  client_->RecordEndOfFrameMetrics(frame_begin_time, trackers);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
WidgetBase::GetBeginMainFrameMetrics() {
  return client_->GetBeginMainFrameMetrics();
}

void WidgetBase::BeginUpdateLayers() {
  client_->BeginUpdateLayers();
}

void WidgetBase::EndUpdateLayers() {
  client_->EndUpdateLayers();
}

void WidgetBase::WillBeginMainFrame() {
  TRACE_EVENT0("gpu", "WidgetBase::WillBeginMainFrame");
  client_->SetSuppressFrameRequestsWorkaroundFor704763Only(true);
  client_->WillBeginMainFrame();
  UpdateSelectionBounds();

  // The UpdateTextInputState can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState();
}

void WidgetBase::SetCompositorVisible(bool visible) {
  if (never_composited_)
    return;

  if (visible)
    was_shown_time_ = base::TimeTicks::Now();
  else
    first_update_visual_state_after_hidden_ = true;
  layer_tree_view_->SetVisible(visible);
}

void WidgetBase::UpdateVisualState() {
  // When recording main frame metrics set the lifecycle reason to
  // kBeginMainFrame, because this is the calller of UpdateLifecycle
  // for the main frame. Otherwise, set the reason to kTests, which is
  // the only other reason this method is called.
  DocumentUpdateReason lifecycle_reason =
      ShouldRecordBeginMainFrameMetrics()
          ? DocumentUpdateReason::kBeginMainFrame
          : DocumentUpdateReason::kTest;
  client_->UpdateLifecycle(WebLifecycleUpdate::kAll, lifecycle_reason);
  client_->SetSuppressFrameRequestsWorkaroundFor704763Only(false);
  if (first_update_visual_state_after_hidden_) {
    client_->RecordTimeToFirstActivePaint(base::TimeTicks::Now() -
                                          was_shown_time_);
    first_update_visual_state_after_hidden_ = false;
  }
}

void WidgetBase::BeginMainFrame(base::TimeTicks frame_time) {
  base::TimeTicks raf_aligned_input_start_time;
  if (ShouldRecordBeginMainFrameMetrics()) {
    raf_aligned_input_start_time = base::TimeTicks::Now();
  }
  widget_input_handler_manager_->input_event_queue()->DispatchRafAlignedInput(
      frame_time);
  if (ShouldRecordBeginMainFrameMetrics()) {
    client_->RecordDispatchRafAlignedInputTime(raf_aligned_input_start_time);
  }
  client_->BeginMainFrame(frame_time);
}

bool WidgetBase::ShouldRecordBeginMainFrameMetrics() {
  // We record metrics only when running in multi-threaded mode, not
  // single-thread mode for testing.
  return Thread::CompositorThread();
}

void WidgetBase::AddPresentationCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  layer_tree_view_->AddPresentationCallback(frame_token, std::move(callback));
}

void WidgetBase::SetCursor(const ui::Cursor& cursor) {
  if (input_handler_.DidChangeCursor(cursor)) {
    widget_host_->SetCursor(cursor);
  }
}

void WidgetBase::SetToolTipText(const String& tooltip_text, TextDirection dir) {
  widget_host_->SetToolTipText(tooltip_text.IsEmpty() ? "" : tooltip_text,
                               ToBaseTextDirection(dir));
}

void WidgetBase::ShowVirtualKeyboard() {
  UpdateTextInputStateInternal(true, false);
}

void WidgetBase::UpdateTextInputState() {
  UpdateTextInputStateInternal(false, false);
}

bool WidgetBase::CanComposeInline() {
  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return true;
  return frame_widget->Client()->CanComposeInline();
}

void WidgetBase::UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                              bool reply_to_request) {
  TRACE_EVENT0("renderer", "WidgetBase::UpdateTextInputStateInternal");
  if (ime_event_guard_) {
    DCHECK(!reply_to_request);
    if (show_virtual_keyboard)
      ime_event_guard_->set_show_virtual_keyboard(true);
    return;
  }
  ui::TextInputType new_type = GetTextInputType();
  if (IsDateTimeInput(new_type))
    return;  // Not considered as a text input field in WebKit/Chromium.

  FrameWidget* frame_widget = client_->FrameWidget();

  blink::WebTextInputInfo new_info;
  ui::mojom::VirtualKeyboardVisibilityRequest last_vk_visibility_request =
      ui::mojom::VirtualKeyboardVisibilityRequest::NONE;
  bool always_hide_ime = false;
  if (frame_widget) {
    new_info = frame_widget->TextInputInfo();
    // This will be used to decide whether or not to show VK when VK policy is
    // manual.
    last_vk_visibility_request =
        frame_widget->GetLastVirtualKeyboardVisibilityRequest();

    // Check whether the keyboard should always be hidden for the currently
    // focused element.
    always_hide_ime = frame_widget->ShouldSuppressKeyboardForFocusedElement();
  }
  const ui::TextInputMode new_mode =
      ConvertWebTextInputMode(new_info.input_mode);
  const ui::mojom::VirtualKeyboardPolicy new_vk_policy =
      new_info.virtual_keyboard_policy;
  bool new_can_compose_inline = CanComposeInline();

  // Only sends text input params if they are changed or if the ime should be
  // shown.
  if (show_virtual_keyboard || reply_to_request ||
      text_input_type_ != new_type || text_input_mode_ != new_mode ||
      text_input_info_ != new_info || !new_info.ime_text_spans.empty() ||
      can_compose_inline_ != new_can_compose_inline ||
      always_hide_ime_ != always_hide_ime || vk_policy_ != new_vk_policy ||
      (new_vk_policy == ui::mojom::VirtualKeyboardPolicy::MANUAL &&
       (last_vk_visibility_request !=
        ui::mojom::VirtualKeyboardVisibilityRequest::NONE))) {
    ui::mojom::blink::TextInputStatePtr params =
        ui::mojom::blink::TextInputState::New();
    params->type = new_type;
    params->mode = new_mode;
    params->action = new_info.action;
    params->flags = new_info.flags;
    params->vk_policy = new_vk_policy;
    params->last_vk_visibility_request = last_vk_visibility_request;
    if (!new_info.ime_text_spans.empty()) {
      params->ime_text_spans_info =
          frame_widget->GetImeTextSpansInfo(new_info.ime_text_spans);
    }
    if (frame_widget) {
      frame_widget->GetEditContextBoundsInWindow(
          &params->edit_context_control_bounds,
          &params->edit_context_selection_bounds);
    }
#if defined(OS_ANDROID)
    if (next_previous_flags_ == kInvalidNextPreviousFlagsValue) {
      // Due to a focus change, values will be reset by the frame.
      // That case we only need fresh NEXT/PREVIOUS information.
      // Also we won't send WidgetHostMsg_TextInputStateChanged if next/previous
      // focusable status is changed.
      if (frame_widget) {
        next_previous_flags_ =
            frame_widget->ComputeWebTextInputNextPreviousFlags();
      } else {
        // For safety in case GetInputMethodController() is null, because -1 is
        // invalid value to send to browser process.
        next_previous_flags_ = 0;
      }
    }
#else
    next_previous_flags_ = 0;
#endif
    params->flags |= next_previous_flags_;
    params->value = new_info.value;
    params->selection =
        gfx::Range(new_info.selection_start, new_info.selection_end);
    if (new_info.composition_start != -1) {
      params->composition =
          gfx::Range(new_info.composition_start, new_info.composition_end);
    }
    params->can_compose_inline = new_can_compose_inline;
    // TODO(changwan): change instances of show_ime_if_needed to
    // show_virtual_keyboard.
    params->show_ime_if_needed = show_virtual_keyboard;
    params->always_hide_ime = always_hide_ime;
    params->reply_to_request = reply_to_request;
    widget_host_->TextInputStateChanged(std::move(params));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    text_input_mode_ = new_mode;
    vk_policy_ = new_vk_policy;
    can_compose_inline_ = new_can_compose_inline;
    always_hide_ime_ = always_hide_ime;
    text_input_flags_ = new_info.flags;
    // Reset the show/hide state in the InputMethodController.
    if (frame_widget) {
      if (last_vk_visibility_request !=
          ui::mojom::VirtualKeyboardVisibilityRequest::NONE) {
        // Reset the visibility state.
        frame_widget->ResetVirtualKeyboardVisibilityRequest();
      }
    }

#if defined(OS_ANDROID)
    // If we send a new TextInputStateChanged message, we must also deliver a
    // new RenderFrameMetadata, as the IME will need this info to be updated.
    // TODO(ericrk): Consider folding the above IPC into RenderFrameMetadata.
    // https://crbug.com/912309
    LayerTreeHost()->RequestForceSendMetadata();
#endif
  }
}

void WidgetBase::ClearTextInputState() {
  text_input_info_ = blink::WebTextInputInfo();
  text_input_type_ = ui::TextInputType::TEXT_INPUT_TYPE_NONE;
  text_input_mode_ = ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT;
  can_compose_inline_ = false;
  text_input_flags_ = 0;
  next_previous_flags_ = kInvalidNextPreviousFlagsValue;
}

void WidgetBase::ShowVirtualKeyboardOnElementFocus() {
#if defined(OS_CHROMEOS)
  // On ChromeOS, virtual keyboard is triggered only when users leave the
  // mouse button or the finger and a text input element is focused at that
  // time. Focus event itself shouldn't trigger virtual keyboard.
  UpdateTextInputState();
#else
  ShowVirtualKeyboard();
#endif

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  client_->FocusChangeComplete();
#endif
}

void WidgetBase::ProcessTouchAction(cc::TouchAction touch_action) {
  if (!input_handler_.ProcessTouchAction(touch_action))
    return;
  widget_input_handler_manager_->ProcessTouchAction(touch_action);
}

void WidgetBase::SetFocus(bool enable) {
  has_focus_ = enable;
  client_->FocusChanged(enable);
}

void WidgetBase::BindWidgetCompositor(
    mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver) {
  if (widget_compositor_)
    widget_compositor_->Shutdown();

  widget_compositor_ = base::MakeRefCounted<WidgetCompositor>(
      weak_ptr_factory_.GetWeakPtr(),
      LayerTreeHost()->GetTaskRunnerProvider()->MainThreadTaskRunner(),
      LayerTreeHost()->GetTaskRunnerProvider()->ImplThreadTaskRunner(),
      std::move(receiver));
}

void WidgetBase::UpdateCompositionInfo(bool immediate_request) {
  if (!monitor_composition_info_ && !immediate_request)
    return;  // Do not calculate composition info if not requested.

  TRACE_EVENT0("renderer", "WidgetBase::UpdateCompositionInfo");
  gfx::Range range;
  Vector<gfx::Rect> character_bounds;

  if (GetTextInputType() == ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
    // Composition information is only available on editable node.
    range = gfx::Range::InvalidRange();
  } else {
    GetCompositionRange(&range);
    GetCompositionCharacterBounds(&character_bounds);
  }

  if (!immediate_request &&
      !ShouldUpdateCompositionInfo(range, character_bounds)) {
    return;
  }
  composition_character_bounds_ = character_bounds;
  composition_range_ = range;

  if (mojom::blink::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->ImeCompositionRangeChanged(composition_range_,
                                     composition_character_bounds_);
  }
}

void WidgetBase::ForceTextInputStateUpdate() {
#if defined(OS_ANDROID)
  UpdateSelectionBounds();
  UpdateTextInputStateInternal(false, true /* reply_to_request */);
#endif
}

void WidgetBase::RequestCompositionUpdates(bool immediate_request,
                                           bool monitor_updates) {
  monitor_composition_info_ = monitor_updates;
  if (!immediate_request)
    return;
  UpdateCompositionInfo(true /* immediate request */);
}

void WidgetBase::GetCompositionRange(gfx::Range* range) {
  *range = gfx::Range::InvalidRange();
  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget ||
      frame_widget->Client()->ShouldDispatchImeEventsToPepper())
    return;
  *range = frame_widget->CompositionRange();
}

void WidgetBase::GetCompositionCharacterBounds(Vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();

  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget ||
      frame_widget->Client()->ShouldDispatchImeEventsToPepper())
    return;

  frame_widget->GetCompositionCharacterBoundsInWindow(bounds);
}

bool WidgetBase::ShouldUpdateCompositionInfo(const gfx::Range& range,
                                             const Vector<gfx::Rect>& bounds) {
  if (!range.IsValid())
    return false;
  if (composition_range_ != range)
    return true;
  if (bounds.size() != composition_character_bounds_.size())
    return true;
  for (size_t i = 0; i < bounds.size(); ++i) {
    if (bounds[i] != composition_character_bounds_[i])
      return true;
  }
  return false;
}

void WidgetBase::SetHidden(bool hidden) {
  // A provisional frame widget will never be shown or hidden, as the frame must
  // be attached to the frame tree before changing visibility.
  DCHECK(!IsForProvisionalFrame());

  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it and ensure
  // throttled acks are released in case frame production ceases.
  is_hidden_ = hidden;

  if (auto* scheduler_state = RendererWidgetSchedulingState())
    scheduler_state->SetHidden(hidden);

  // If the renderer was hidden, resolve any pending synthetic gestures so they
  // aren't blocked waiting for a compositor frame to be generated.
  if (is_hidden_)
    FlushInputProcessedCallback();

  SetCompositorVisible(!is_hidden_);
}

ui::TextInputType WidgetBase::GetTextInputType() {
  return ConvertWebTextInputType(client_->GetTextInputType());
}

void WidgetBase::UpdateSelectionBounds() {
  TRACE_EVENT0("renderer", "WidgetBase::UpdateSelectionBounds");
  if (ime_event_guard_)
    return;
#if defined(USE_AURA)
  // TODO(mohsen): For now, always send explicit selection IPC notifications for
  // Aura beucause composited selection updates are not working for webview tags
  // which regresses IME inside webview. Remove this when composited selection
  // updates are fixed for webviews. See, http://crbug.com/510568.
  bool send_ipc = true;
#else
  // With composited selection updates, the selection bounds will be reported
  // directly by the compositor, in which case explicit IPC selection
  // notifications should be suppressed.
  bool send_ipc = !RuntimeEnabledFeatures::CompositedSelectionUpdateEnabled();
#endif
  if (send_ipc) {
    bool is_anchor_first = false;
    base::i18n::TextDirection focus_dir =
        base::i18n::TextDirection::UNKNOWN_DIRECTION;
    base::i18n::TextDirection anchor_dir =
        base::i18n::TextDirection::UNKNOWN_DIRECTION;

    FrameWidget* frame_widget = client_->FrameWidget();
    if (!frame_widget)
      return;
    if (frame_widget->GetSelectionBoundsInWindow(
            &selection_focus_rect_, &selection_anchor_rect_, &focus_dir,
            &anchor_dir, &is_anchor_first)) {
      widget_host_->SelectionBoundsChanged(selection_anchor_rect_, anchor_dir,
                                           selection_focus_rect_, focus_dir,
                                           is_anchor_first);
    }
  }
  UpdateCompositionInfo(false /* not an immediate request */);
}

void WidgetBase::MouseCaptureLost() {
  client_->MouseCaptureLost();
}

void WidgetBase::SetEditCommandsForNextKeyEvent(
    Vector<mojom::blink::EditCommandPtr> edit_commands) {
  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return;
  frame_widget->SetEditCommandsForNextKeyEvent(std::move(edit_commands));
}

void WidgetBase::CursorVisibilityChange(bool is_visible) {
  client_->SetCursorVisibilityState(is_visible);
}

void WidgetBase::SetMouseCapture(bool capture) {
  if (mojom::blink::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->SetMouseCapture(capture);
  }
}

void WidgetBase::ImeSetComposition(
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  if (!ShouldHandleImeEvents())
    return;

  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return;
  if (frame_widget->Client()->ShouldDispatchImeEventsToPepper()) {
    frame_widget->Client()->ImeSetCompositionForPepper(
        text,
        std::vector<ui::ImeTextSpan>(ime_text_spans.begin(),
                                     ime_text_spans.end()),
        replacement_range, selection_start, selection_end);
    return;
  }

  ImeEventGuard guard(weak_ptr_factory_.GetWeakPtr());
  if (!frame_widget->SetComposition(text, ime_text_spans, replacement_range,
                                    selection_start, selection_end)) {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    if (mojom::blink::WidgetInputHandlerHost* host =
            widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
      host->ImeCancelComposition();
    }
  }
  UpdateCompositionInfo(false /* not an immediate request */);
}

void WidgetBase::ImeCommitText(const String& text,
                               const Vector<ui::ImeTextSpan>& ime_text_spans,
                               const gfx::Range& replacement_range,
                               int relative_cursor_pos) {
  if (!ShouldHandleImeEvents())
    return;

  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return;
  if (frame_widget->Client()->ShouldDispatchImeEventsToPepper()) {
    frame_widget->Client()->ImeCommitTextForPepper(
        text,
        std::vector<ui::ImeTextSpan>(ime_text_spans.begin(),
                                     ime_text_spans.end()),
        replacement_range, relative_cursor_pos);
    return;
  }

  ImeEventGuard guard(weak_ptr_factory_.GetWeakPtr());
  input_handler_.set_handling_input_event(true);
  frame_widget->CommitText(text, ime_text_spans, replacement_range,
                           relative_cursor_pos);
  input_handler_.set_handling_input_event(false);
  UpdateCompositionInfo(false /* not an immediate request */);
}

void WidgetBase::ImeFinishComposingText(bool keep_selection) {
  if (!ShouldHandleImeEvents())
    return;

  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return;
  if (frame_widget->Client()->ShouldDispatchImeEventsToPepper()) {
    frame_widget->Client()->ImeFinishComposingTextForPepper(keep_selection);
    return;
  }

  ImeEventGuard guard(weak_ptr_factory_.GetWeakPtr());
  input_handler_.set_handling_input_event(true);
  frame_widget->FinishComposingText(keep_selection);
  input_handler_.set_handling_input_event(false);
  UpdateCompositionInfo(false /* not an immediate request */);
}

void WidgetBase::QueueSyntheticEvent(
    std::unique_ptr<WebCoalescedInputEvent> event) {
  FrameWidget* frame_widget = client_->FrameWidget();
  if (frame_widget)
    frame_widget->Client()->WillQueueSyntheticEvent(*event);

  // TODO(acomminos): If/when we add support for gesture event attribution on
  //                  the impl thread, have the caller provide attribution.
  WebInputEventAttribution attribution;
  widget_input_handler_manager_->input_event_queue()->HandleEvent(
      std::move(event), MainThreadEventQueue::DispatchType::kNonBlocking,
      mojom::blink::InputEventResultState::kNotConsumed, attribution,
      HandledEventCallback());
}

bool WidgetBase::IsForProvisionalFrame() {
  auto* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return false;
  return frame_widget->IsProvisional();
}

bool WidgetBase::ShouldHandleImeEvents() {
  auto* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return false;
  return frame_widget->ShouldHandleImeEvents();
}

void WidgetBase::RequestPresentationAfterScrollAnimationEnd(
    mojom::blink::Widget::ForceRedrawCallback callback) {
  LayerTreeHost()->RequestScrollAnimationEndNotification(
      base::BindOnce(&WidgetBase::ForceRedraw, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void WidgetBase::FlushInputProcessedCallback() {
  widget_input_handler_manager_->InvokeInputProcessedCallback();
}

void WidgetBase::CancelCompositionForPepper() {
  if (mojom::blink::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->ImeCancelComposition();
  }
#if defined(OS_MAC) || defined(USE_AURA)
  UpdateCompositionInfo(false /* not an immediate request */);
#endif
}

void WidgetBase::OnImeEventGuardStart(ImeEventGuard* guard) {
  if (!ime_event_guard_)
    ime_event_guard_ = guard;
}

void WidgetBase::OnImeEventGuardFinish(ImeEventGuard* guard) {
  if (ime_event_guard_ != guard)
    return;
  ime_event_guard_ = nullptr;

  // While handling an ime event, text input state and selection bounds updates
  // are ignored. These must explicitly be updated once finished handling the
  // ime event.
  UpdateSelectionBounds();
#if defined(OS_ANDROID)
  if (guard->show_virtual_keyboard())
    ShowVirtualKeyboard();
  else
    UpdateTextInputState();
#endif
}

void WidgetBase::RequestMouseLock(
    bool has_transient_user_activation,
    bool priviledged,
    bool request_unadjusted_movement,
    base::OnceCallback<void(
        blink::mojom::PointerLockResult,
        CrossVariantMojoRemote<mojom::blink::PointerLockContextInterfaceBase>)>
        callback) {
  if (mojom::blink::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->RequestMouseLock(
        has_transient_user_activation, priviledged, request_unadjusted_movement,
        base::BindOnce(
            [](base::OnceCallback<void(
                   blink::mojom::PointerLockResult,
                   CrossVariantMojoRemote<
                       mojom::blink::PointerLockContextInterfaceBase>)>
                   callback,
               blink::mojom::PointerLockResult result,
               mojo::PendingRemote<mojom::blink::PointerLockContext> context) {
              std::move(callback).Run(result, std::move(context));
            },
            std::move(callback)));
  }
}

void WidgetBase::UpdateSurfaceAndScreenInfo(
    const viz::LocalSurfaceIdAllocation& new_local_surface_id_allocation,
    const gfx::Rect& compositor_viewport_pixel_rect,
    const ScreenInfo& new_screen_info_param) {
  ScreenInfo new_screen_info = new_screen_info_param;

  // If there is a screen orientation override apply it.
  if (auto orientation_override = client_->ScreenOrientationOverride()) {
    new_screen_info.orientation_type = orientation_override.value();
    new_screen_info.orientation_angle =
        OrientationTypeToAngle(new_screen_info.orientation_type);
  }

  // Same logic is used in RenderWidgetHostImpl::SynchronizeVisualProperties to
  // detect if there is a screen orientation change.
  bool orientation_changed =
      screen_info_.orientation_angle != new_screen_info.orientation_angle ||
      screen_info_.orientation_type != new_screen_info.orientation_type;
  ScreenInfo previous_original_screen_info = client_->GetOriginalScreenInfo();

  local_surface_id_allocation_from_parent_ = new_local_surface_id_allocation;
  screen_info_ = new_screen_info;

  // Note carefully that the DSF specified in |new_screen_info| is not the
  // DSF used by the compositor during device emulation!
  LayerTreeHost()->SetViewportRectAndScale(
      compositor_viewport_pixel_rect,
      client_->GetOriginalScreenInfo().device_scale_factor,
      local_surface_id_allocation_from_parent_);
  // The ViewportVisibleRect derives from the LayerTreeView's viewport size,
  // which is set above.
  LayerTreeHost()->SetViewportVisibleRect(client_->ViewportVisibleRect());
  LayerTreeHost()->SetDisplayColorSpaces(screen_info_.display_color_spaces);

  if (orientation_changed)
    client_->OrientationChanged();

  client_->DidUpdateSurfaceAndScreen(previous_original_screen_info);
}

void WidgetBase::UpdateScreenInfo(const ScreenInfo& new_screen_info) {
  UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                             CompositorViewportRect(), new_screen_info);
}

void WidgetBase::UpdateCompositorViewportAndScreenInfo(
    const gfx::Rect& compositor_viewport_pixel_rect,
    const ScreenInfo& new_screen_info) {
  UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                             compositor_viewport_pixel_rect, new_screen_info);
}

void WidgetBase::UpdateCompositorViewportRect(
    const gfx::Rect& compositor_viewport_pixel_rect) {
  UpdateSurfaceAndScreenInfo(local_surface_id_allocation_from_parent_,
                             compositor_viewport_pixel_rect, screen_info_);
}

void WidgetBase::UpdateSurfaceAndCompositorRect(
    const viz::LocalSurfaceIdAllocation& new_local_surface_id_allocation,
    const gfx::Rect& compositor_viewport_pixel_rect) {
  UpdateSurfaceAndScreenInfo(new_local_surface_id_allocation,
                             compositor_viewport_pixel_rect, screen_info_);
}

const ScreenInfo& WidgetBase::GetScreenInfo() {
  return screen_info_;
}

void WidgetBase::SetScreenRects(const gfx::Rect& widget_screen_rect,
                                const gfx::Rect& window_screen_rect) {
  widget_screen_rect_ = widget_screen_rect;
  window_screen_rect_ = window_screen_rect;
}

void WidgetBase::SetPendingWindowRect(const gfx::Rect* rect) {
  if (rect) {
    pending_window_rect_ = *rect;
    // Popups don't get size updates back from the browser so just store the set
    // values.
    if (!client_->FrameWidget()) {
      SetScreenRects(*rect, *rect);
    }
  } else {
    pending_window_rect_.reset();
  }
}

gfx::Rect WidgetBase::WindowRect() {
  gfx::Rect rect;
  if (pending_window_rect_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    rect = pending_window_rect_.value();
  } else {
    rect = window_screen_rect_;
  }

  client_->ScreenRectToEmulated(rect);
  return rect;
}

gfx::Rect WidgetBase::ViewRect() {
  gfx::Rect rect = widget_screen_rect_;
  client_->ScreenRectToEmulated(rect);
  return rect;
}

gfx::Rect WidgetBase::CompositorViewportRect() const {
  return LayerTreeHost()->device_viewport_rect();
}

bool WidgetBase::ComputePreferCompositingToLCDText() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisablePreferCompositingToLCDText))
    return false;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // On Android, we never have subpixel antialiasing. On Chrome OS we prefer to
  // composite all scrollers for better scrolling performance.
  return true;
#else
  // Prefer compositing if the device scale is high enough that losing subpixel
  // antialiasing won't have a noticeable effect on text quality.
  // Note: We should keep kHighDPIDeviceScaleFactorThreshold in
  // cc/metrics/lcd_text_metrics_reporter.cc the same as the value below.
  if (screen_info_.device_scale_factor >= 1.5f)
    return true;
  if (command_line.HasSwitch(switches::kEnablePreferCompositingToLCDText))
    return true;
  if (!Platform::Current()->IsLcdTextEnabled())
    return true;
  if (base::FeatureList::IsEnabled(features::kPreferCompositingToLCDText))
    return true;
  return false;
#endif
}

gfx::PointF WidgetBase::DIPsToBlinkSpace(const gfx::PointF& point) {
  if (!use_zoom_for_dsf_)
    return point;
  // TODO(danakj): Should this be GetScreenInfo() so it changes under emulation?
  return gfx::ScalePoint(point,
                         client_->GetOriginalScreenInfo().device_scale_factor);
}

gfx::PointF WidgetBase::BlinkSpaceToDIPs(const gfx::PointF& point) {
  if (!use_zoom_for_dsf_)
    return point;
  // TODO(danakj): Should this be GetScreenInfo() so it changes under emulation?
  return gfx::ScalePoint(
      point, 1.f / client_->GetOriginalScreenInfo().device_scale_factor);
}

gfx::Size WidgetBase::DIPsToBlinkSpace(const gfx::Size& size) {
  if (!use_zoom_for_dsf_)
    return size;
  return gfx::ScaleToCeiledSize(
      size, client_->GetOriginalScreenInfo().device_scale_factor);
}

gfx::Size WidgetBase::BlinkSpaceToDIPs(const gfx::Size& size) {
  if (!use_zoom_for_dsf_)
    return size;
  float reverse = 1 / client_->GetOriginalScreenInfo().device_scale_factor;
  return gfx::ScaleToCeiledSize(size, reverse);
}

}  // namespace blink
