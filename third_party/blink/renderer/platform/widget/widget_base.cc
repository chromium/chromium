// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/categorized_worker_pool.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/paint_holding_reason.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/direct_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/input/pointer_lock_context.mojom-blink.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom-blink.h"
#include "third_party/blink/public/mojom/widget/visual_properties.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/graphics/raster_dark_mode_filter_impl.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/compositor_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/widget_scheduler.h"
#include "third_party/blink/renderer/platform/widget/compositing/blink_categorized_worker_pool_delegate.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view.h"
#include "third_party/blink/renderer/platform/widget/compositing/render_frame_metadata_observer_impl.h"
#include "third_party/blink/renderer/platform/widget/compositing/widget_compositor.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/input/ime_event_guard.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"
#include "ui/base/ime/mojom/text_input_state.mojom-blink.h"
#include "ui/display/display.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/presentation_feedback.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/platform/widget/compositing/android_webview/synchronous_layer_tree_frame_sink.h"
#endif

namespace blink {

namespace {

#if BUILDFLAG(IS_ANDROID)
// Unique identifier for each output surface created.
uint32_t g_next_layer_tree_frame_sink_id = 1;
#endif

// Used for renderer compositor thread context, WebGL (when high priority is
// not requested by workaround), canvas, etc.
const gpu::SchedulingPriority kGpuStreamPriorityDefault =
    gpu::SchedulingPriority::kNormal;

const uint32_t kGpuStreamIdDefault = 0;

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

unsigned OrientationTypeToAngle(display::mojom::blink::ScreenOrientation type) {
  unsigned angle;
  // FIXME(ostap): This relationship between orientationType and
  // orientationAngle is temporary. The test should be able to specify
  // the angle in addition to the orientation type.
  switch (type) {
    case display::mojom::blink::ScreenOrientation::kLandscapePrimary:
      angle = 90;
      break;
    case display::mojom::blink::ScreenOrientation::kLandscapeSecondary:
      angle = 270;
      break;
    case display::mojom::blink::ScreenOrientation::kPortraitSecondary:
      angle = 180;
      break;
    default:
      angle = 0;
  }
  return angle;
}

std::unique_ptr<viz::SyntheticBeginFrameSource>
CreateSyntheticBeginFrameSource() {
  base::SingleThreadTaskRunner* compositor_impl_side_task_runner =
      Platform::Current()->CompositorThreadTaskRunner()
          ? Platform::Current()->CompositorThreadTaskRunner().get()
          : base::SingleThreadTaskRunner::GetCurrentDefault().get();
  return std::make_unique<viz::BackToBackBeginFrameSource>(
      std::make_unique<viz::DelayBasedTimeSource>(
          compositor_impl_side_task_runner));
}

}  // namespace

WidgetBase::WidgetBase(
    WidgetBaseClient* client,
    CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool hidden,
    bool never_composited,
    bool is_embedded,
    bool is_for_scalable_page)
    : never_composited_(never_composited),
      is_embedded_(is_embedded),
      is_for_scalable_page_(is_for_scalable_page),
      client_(client),
      widget_host_(std::move(widget_host), task_runner),
      receiver_(this, std::move(widget), task_runner),
      next_previous_flags_(kInvalidNextPreviousFlagsValue),
      is_hidden_(hidden),
      task_runner_(task_runner),
      request_animation_after_delay_timer_(
          std::move(task_runner),
          this,
          &WidgetBase::RequestAnimationAfterDelayTimerFired) {}

WidgetBase::~WidgetBase() {
  // Ensure Shutdown was called.
  DCHECK(!layer_tree_view_);
}

void WidgetBase::InitializeCompositing(
    PageScheduler& page_scheduler,
    const display::ScreenInfos& screen_infos,
    const cc::LayerTreeSettings* settings,
    base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
        frame_widget_input_handler,
    WidgetBase* previous_widget) {
  DCHECK(!initialized_);

  widget_scheduler_ = page_scheduler.CreateWidgetScheduler();
  widget_scheduler_->SetHidden(is_hidden_);

  main_thread_compositor_task_runner_ =
      page_scheduler.GetAgentGroupScheduler().CompositorTaskRunner();

  main_thread_id_ = base::PlatformThread::CurrentId();

  auto* compositing_thread_scheduler =
      ThreadScheduler::CompositorThreadScheduler();

  if (previous_widget) {
    CHECK(previous_widget->layer_tree_view_);
    CHECK(!settings);
    AssertAreCompatible(*this, *previous_widget);

    // `screen_infos` is applied to this LayerTreeView below.
    previous_widget->DisconnectLayerTreeView(this);
    CHECK(layer_tree_view_);
  } else {
    layer_tree_view_ = std::make_unique<LayerTreeView>(this, widget_scheduler_);

    std::optional<cc::LayerTreeSettings> default_settings;
    if (!settings) {
      const display::ScreenInfo& screen_info = screen_infos.current();
      default_settings = GenerateLayerTreeSettings(
          compositing_thread_scheduler, is_embedded_, is_for_scalable_page_,
          screen_info.rect.size(), screen_info.device_scale_factor);
      settings = &default_settings.value();
    }
    layer_tree_view_->Initialize(
        *settings, main_thread_compositor_task_runner_,
        compositing_thread_scheduler
            ? compositing_thread_scheduler->DefaultTaskRunner()
            : nullptr,
        cc::CategorizedWorkerPool::GetOrCreate(
            &BlinkCategorizedWorkerPoolDelegate::Get()));
  }

  screen_infos_ = screen_infos;
  max_render_buffer_bounds_sw_ =
      LayerTreeHost()->GetSettings().max_render_buffer_bounds_for_sw;
  FrameWidget* frame_widget = client_->FrameWidget();

  // Even if we have a |compositing_thread_scheduler| we do not process input
  // on the compositor thread for widgets that are not frames. (ie. popups).
  auto* widget_compositing_thread_scheduler =
      frame_widget ? compositing_thread_scheduler : nullptr;

  // We only use an external input handler for frame widgets because only
  // frames use the compositor for input handling. Other kinds of widgets
  // (e.g.  popups, plugins) must forward their input directly through
  // WidgetBaseInputHandler.
  bool uses_input_handler = frame_widget;
  base::PlatformThreadId io_thread_id = Platform::Current()->GetIOThreadId();
  widget_input_handler_manager_ = WidgetInputHandlerManager::Create(
      weak_ptr_factory_.GetWeakPtr(), std::move(frame_widget_input_handler),
      never_composited_, widget_compositing_thread_scheduler, widget_scheduler_,
      uses_input_handler, client_->AllowsScrollResampling(), io_thread_id,
      main_thread_id_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAllowPreCommitInput))
    widget_input_handler_manager_->AllowPreCommitInput();

  UpdateScreenInfo(screen_infos);

  // If the widget is hidden, delay starting the compositor until the user
  // shows it. Otherwise start the compositor immediately. If the widget is
  // for a provisional frame, this importantly starts the compositor before
  // the frame is inserted into the frame tree, which impacts first paint
  // metrics.
  if (!is_hidden_)
    SetCompositorVisible(true);

  if (Platform::Current()->IsThreadedAnimationEnabled()) {
    DCHECK(AnimationHost());
    scroll_animation_timeline_ = cc::AnimationTimeline::Create(
        cc::AnimationIdProvider::NextTimelineId());
    AnimationHost()->AddAnimationTimeline(scroll_animation_timeline_);
  }

  initialized_ = true;
}

void WidgetBase::InitializeNonCompositing() {
  DCHECK(!initialized_);
  // WidgetBase users implicitly expect one default ScreenInfo to exist.
  screen_infos_ = display::ScreenInfos(display::ScreenInfo());
  initialized_ = true;
}

void WidgetBase::DidFirstVisuallyNonEmptyPaint(
    base::TimeTicks& first_paint_time) {
  if (widget_input_handler_manager_) {
    widget_input_handler_manager_->DidFirstVisuallyNonEmptyPaint(
        first_paint_time);
  }
}

void WidgetBase::Shutdown() {
  // The |input_event_queue_| is refcounted and will live while an event is
  // being handled. This drops the connection back to this WidgetBase which
  // is being destroyed.
  if (widget_input_handler_manager_)
    widget_input_handler_manager_->ClearClient();

  DisconnectLayerTreeView(nullptr);

  // The `widget_scheduler_` must be deleted last because the
  // `widget_input_handler_manager_` may request to post a task on the
  // InputTaskQueue. The `widget_input_handler_manager_` must outlive
  // the `layer_tree_view_` because it's `LayerTreeHost` holds a raw ptr to
  // the `InputHandlerProxy` interface on the compositor thread. The
  // `LayerTreeHost` destruction is synchronous and will join with the
  // compositor thread
  if (widget_scheduler_) {
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    cleanup_runner->PostNonNestableTask(
        FROM_HERE, base::BindOnce(
                       [](scoped_refptr<scheduler::WidgetScheduler> scheduler,
                          scoped_refptr<WidgetInputHandlerManager> manager,
                          std::unique_ptr<LayerTreeView> view) {
                         view.reset();
                         manager.reset();
                         scheduler->Shutdown();
                       },
                       std::move(widget_scheduler_),
                       std::move(widget_input_handler_manager_),
                       std::move(layer_tree_view_)));
  }

  if (widget_compositor_) {
    widget_compositor_->Shutdown();
    widget_compositor_ = nullptr;
  }
}

void WidgetBase::DisconnectLayerTreeView(WidgetBase* new_widget) {
  will_be_destroyed_ = true;

  if (!layer_tree_view_) {
    CHECK(!new_widget);
    return;
  }

  // The LayerTreeHost may already be in the call stack, if this WidgetBase
  // is being destroyed during an animation callback for instance. We can not
  // delete it here and unwind the stack back up to it, or it will crash. So
  // we post the deletion to another task, but disconnect the LayerTreeHost
  // (via the LayerTreeView) from the destroying WidgetBase. The
  // LayerTreeView owns the LayerTreeHost, and is its client, so they are kept
  // alive together for a clean call stack.
  if (ScrollAnimationTimeline()) {
    DCHECK(AnimationHost());
    AnimationHost()->RemoveAnimationTimeline(ScrollAnimationTimeline());
  }

  if (new_widget) {
    layer_tree_view_->ReattachTo(new_widget, widget_scheduler_);
    new_widget->layer_tree_view_ = std::move(layer_tree_view_);
    layer_tree_view_ = nullptr;
  } else {
    layer_tree_view_->Disconnect();
  }
}

cc::LayerTreeHost* WidgetBase::LayerTreeHost() const {
  CHECK(layer_tree_view_);
  return layer_tree_view_->layer_tree_host();
}

cc::AnimationHost* WidgetBase::AnimationHost() const {
  return layer_tree_view_ ? layer_tree_view_->animation_host() : nullptr;
}

cc::AnimationTimeline* WidgetBase::ScrollAnimationTimeline() const {
  return scroll_animation_timeline_.get();
}

scheduler::WidgetScheduler* WidgetBase::WidgetScheduler() {
  return widget_scheduler_.get();
}

void WidgetBase::ForceRedraw(
    mojom::blink::Widget::ForceRedrawCallback callback) {
  TRACE_EVENT0("renderer", "WidgetBase::ForceRedraw");
  LayerTreeHost()->RequestPresentationTimeForNextFrame(
      base::BindOnce(&OnDidPresentForceDrawFrame, std::move(callback)));
  LayerTreeHost()->SetNeedsCommitWithForcedRedraw();

  // ScheduleAnimationForWebTests() which is implemented by
  // WebTestWebFrameWidgetImpl, providing the additional control over the
  // lifecycle of compositing required by web tests. This will be a no-op on
  // production.
  client_->ScheduleAnimationForWebTests();
}

void WidgetBase::GetWidgetInputHandler(
    mojo::PendingReceiver<mojom::blink::WidgetInputHandler> request,
    mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host) {
  widget_input_handler_manager_->AddInterface(std::move(request),
                                              std::move(host));
}

void WidgetBase::ShowContextMenu(ui::mojom::blink::MenuSourceType source_type,
                                 const gfx::Point& location) {
  client_->ShowContextMenu(source_type, location);
}

void WidgetBase::BindInputTargetClient(
    mojo::PendingReceiver<viz::mojom::blink::InputTargetClient> host) {
  client_->BindInputTargetClient(std::move(host));
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
  // 3. Computed in the renderer of the main frame WebFrameWidgetImpl (in blink
  //    usually). Passed down through the waterfall dance to child frame
  //    WebFrameWidgetImpl. Here that step is performed by passing the value
  //    along to all RemoteFrame objects that are below this WebFrameWidgetImpl
  //    in the frame tree. The main frame (top level) WebFrameWidgetImpl ignores
  //    this value from its RenderWidgetHost since it is controlled in the
  //    renderer. Child frame WebFrameWidgetImpls consume the value from their
  //    RenderWidgetHost. Example: page_scale_factor.
  // 4. Computed independently in the renderer for each WidgetBase (in blink
  //    usually). Passed down from the parent to the child WidgetBases through
  //    the waterfall dance, but the value only travels one step - the child
  //    frame WebFrameWidgetImpl would compute values for grandchild
  //    WebFrameWidgetImpls independently. Here the value is passed to child
  //    frame RenderWidgets by passing the value along to all RemoteFrame
  //    objects that are below this WebFrameWidgetImpl in the frame tree. Each
  //    WidgetBase consumes this value when it is received from its
  //    RenderWidgetHost. Example: compositor_viewport_pixel_rect.
  // For each of these properties:
  //   If the WebView also knows these properties, each WebFrameWidgetImpl
  //   will pass them along to the WebView as it receives it, even if there
  //   are multiple WebFrameWidgetImpls related to the same WebView.
  //   However when the main frame in the renderer is the source of truth,
  //   then child widgets must not clobber that value! In all cases child frames
  //   do not need to update state in the WebView when a local main frame is
  //   present as it always sets the value first.
  //   TODO(danakj): This does create a race if there are multiple
  //   UpdateVisualProperties updates flowing through the WebFrameWidgetImpl
  //   tree at the same time, and it seems that only one WebFrameWidgetImpl for
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

  VisualProperties visual_properties = visual_properties_from_browser;
  auto& screen_info = visual_properties.screen_infos.mutable_current();

  // Web tests can override the device scale factor in the renderer.
  if (auto scale_factor = client_->GetTestingDeviceScaleFactorOverride()) {
    screen_info.device_scale_factor = scale_factor;
    visual_properties.compositor_viewport_pixel_rect =
        gfx::Rect(gfx::ScaleToCeiledSize(visual_properties.new_size,
                                         screen_info.device_scale_factor));
  }

  // Inform the rendering thread of the color space indicating the presence of
  // HDR capabilities. The HDR bit happens to be globally true/false for all
  // browser windows (on Windows OS) and thus would be the same for all
  // RenderWidgets, so clobbering each other works out since only the HDR bit is
  // used. See https://crbug.com/803451 and
  // https://chromium-review.googlesource.com/c/chromium/src/+/852912/15#message-68bbd3e25c3b421a79cd028b2533629527d21fee
  Platform::Current()->SetRenderingColorSpace(
      screen_info.display_color_spaces.GetScreenInfoColorSpace());

  LayerTreeHost()->SetBrowserControlsParams(
      visual_properties.browser_controls_params);

  LayerTreeHost()->SetVisualDeviceViewportSize(
      gfx::ScaleToCeiledSize(visual_properties.visible_viewport_size,
                             screen_info.device_scale_factor));

  client_->UpdateVisualProperties(visual_properties);
}

void WidgetBase::UpdateScreenRects(const gfx::Rect& widget_screen_rect,
                                   const gfx::Rect& window_screen_rect,
                                   UpdateScreenRectsCallback callback) {
  TRACE_EVENT0("renderer", "WidgetBase::UpdateScreenRects");
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

void WidgetBase::WasShown(bool was_evicted,
                          mojom::blink::RecordContentToVisibleTimeRequestPtr
                              record_tab_switch_time_request) {
  // The frame must be attached to the frame tree (which makes it no longer
  // provisional) before changing visibility.
  DCHECK(!IsForProvisionalFrame());

  TRACE_EVENT_WITH_FLOW0("renderer", "WidgetBase::WasShown", this,
                         TRACE_EVENT_FLAG_FLOW_IN);

  SetHidden(false);

  if (record_tab_switch_time_request) {
    LayerTreeHost()->RequestSuccessfulPresentationTimeForNextFrame(
        tab_switch_time_recorder_.TabWasShown(
            false /* has_saved_frames */,
            record_tab_switch_time_request->event_start_time,
            record_tab_switch_time_request->destination_is_loaded,
            record_tab_switch_time_request->show_reason_tab_switching,
            record_tab_switch_time_request->show_reason_bfcache_restore));
  }

  client_->WasShown(was_evicted);
}

void WidgetBase::RequestSuccessfulPresentationTimeForNextFrame(
    mojom::blink::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  DCHECK(visible_time_request);
  if (is_hidden_) {
    return;
  }
  TRACE_EVENT0("renderer",
               "WidgetBase::RequestSuccessfulPresentationTimeForNextFrame");

  if (visible_time_request->show_reason_unfolding) {
    LayerTreeHost()->RequestSuccessfulPresentationTimeForNextFrame(
        tab_switch_time_recorder_.GetCallbackForNextFrameAfterUnfold(
            visible_time_request->event_start_time));
    return;
  }

  // Tab was shown while widget was already painting, eg. due to being
  // captured.
  LayerTreeHost()->RequestSuccessfulPresentationTimeForNextFrame(
      tab_switch_time_recorder_.TabWasShown(
          false /* has_saved_frames */, visible_time_request->event_start_time,
          visible_time_request->destination_is_loaded,
          visible_time_request->show_reason_tab_switching,
          visible_time_request->show_reason_bfcache_restore));
}

void WidgetBase::CancelSuccessfulPresentationTimeRequest() {
  if (is_hidden_) {
    return;
  }

  TRACE_EVENT0("renderer",
               "WidgetBase::CancelSuccessfulPresentationTimeRequest");
  // Tab was hidden while widget keeps painting, eg. due to being captured.
  tab_switch_time_recorder_.TabWasHidden();
}

void WidgetBase::SetupRenderInputRouterConnections(
    mojo::PendingReceiver<mojom::blink::RenderInputRouterClient>
        browser_request,
    mojo::PendingReceiver<mojom::blink::RenderInputRouterClient> viz_request) {
  TRACE_EVENT("renderer", "WidgetBase::SetupRenderInputRouterConnections");

  // TODO(b/322833330): Investigate binding |browser_input_receiver_| on
  // RendererCompositor to break dependency on CrRendererMain and avoiding
  // contention with javascript during method calls.
  browser_input_receiver_.Bind(std::move(browser_request), task_runner_);
  if (viz_request) {
    viz_input_receiver_.Bind(std::move(viz_request), task_runner_);
  }
}

void WidgetBase::ApplyViewportChanges(
    const cc::ApplyViewportChangesArgs& args) {
  client_->ApplyViewportChanges(args);
}

void WidgetBase::UpdateCompositorScrollState(
    const cc::CompositorCommitData& commit_data) {
  client_->UpdateCompositorScrollState(commit_data);
}

void WidgetBase::OnDeferMainFrameUpdatesChanged(bool defer) {
  // LayerTreeHost::CreateThreaded() will defer main frame updates immediately
  // until it gets a LocalSurfaceId. That's before the
  // |widget_input_handler_manager_| is created, so it can be null here.
  // TODO(rendering-core): To avoid ping-ponging between defer main frame
  // states during initialization, and requiring null checks here, we should
  // probably pass the LocalSurfaceId to the compositor while it is
  // initialized so that it doesn't have to immediately switch into deferred
  // mode without being requested to.
  if (!widget_input_handler_manager_)
    return;

  // The input handler wants to know about the mainframe update status to
  // enable/disable input and for metrics.
  widget_input_handler_manager_->OnDeferMainFrameUpdatesChanged(defer);
}

void WidgetBase::OnDeferCommitsChanged(
    bool defer,
    cc::PaintHoldingReason reason,
    std::optional<cc::PaintHoldingCommitTrigger> trigger) {
  // The input handler wants to know about the commit status for metric purposes
  // and to enable/disable input.
  widget_input_handler_manager_->OnDeferCommitsChanged(defer, reason);
}

void WidgetBase::OnCommitRequested() {
  client_->OnCommitRequested();
}

void WidgetBase::DidBeginMainFrame() {
  if (base::FeatureList::IsEnabled(features::kRunTextInputUpdatePostLifecycle))
    UpdateTextInputState();
  client_->DidBeginMainFrame();
}

void WidgetBase::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::LayerTreeFrameSink.
  DCHECK(!never_composited_);

  // Provide a hook for testing to provide their own layer tree frame sink, if
  // one is returned just run the callback.
  if (std::unique_ptr<cc::LayerTreeFrameSink> sink =
          client_->AllocateNewLayerTreeFrameSink()) {
    std::move(callback).Run(std::move(sink), nullptr);
    return;
  }

  KURL url = client_->GetURLForDebugTrace();
  // The |url| is not always available, fallback to a fixed string.
  if (url.IsEmpty())
    url = KURL("chrome://gpu/WidgetBase::RequestNewLayerTreeFrameSink");

  const bool for_web_tests = WebTestMode();
  // Misconfigured bots (eg. crbug.com/780757) could run web tests on a
  // machine where gpu compositing doesn't work. LOG(FATAL) in that case.
  if (for_web_tests && Platform::Current()->IsGpuCompositingDisabled() &&
      !Platform::Current()->CompositorThreadTaskRunner()) {
    LOG(FATAL) << "Web tests require gpu compositing in single thread mode, "
                  "but it is disabled.";
  }

  // TODO(jonross): Have this generated by the LayerTreeFrameSink itself, which
  // would then handle binding.
  mojo::PendingRemote<cc::mojom::blink::RenderFrameMetadataObserver>
      render_frame_metadata_observer_remote;
  mojo::PendingRemote<cc::mojom::blink::RenderFrameMetadataObserverClient>
      render_frame_metadata_client_remote;
  mojo::PendingReceiver<cc::mojom::blink::RenderFrameMetadataObserverClient>
      render_frame_metadata_observer_client_receiver =
          render_frame_metadata_client_remote.InitWithNewPipeAndPassReceiver();
  auto render_frame_metadata_observer =
      std::make_unique<RenderFrameMetadataObserverImpl>(
          render_frame_metadata_observer_remote
              .InitWithNewPipeAndPassReceiver(),
          std::move(render_frame_metadata_client_remote));

  auto params = std::make_unique<
      cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams>();
  params->io_thread_id = Platform::Current()->GetIOThreadId();
  if (base::FeatureList::IsEnabled(::features::kEnableADPFRendererMain)) {
    params->main_thread_id = main_thread_id_;
  }

  params->compositor_task_runner =
      Platform::Current()->CompositorThreadTaskRunner();
  if (for_web_tests && !params->compositor_task_runner) {
    // The frame sink provider expects a compositor task runner, but we might
    // not have that if we're running web tests in single threaded mode.
    // Set it to be our thread's task runner instead.
    params->compositor_task_runner = main_thread_compositor_task_runner_;
  }

  if (base::FeatureList::IsEnabled(features::kDirectCompositorThreadIpc) &&
      !for_web_tests && params->compositor_task_runner &&
      mojo::IsDirectReceiverSupported()) {
    params->use_direct_client_receiver = true;
  }

  // The renderer runs animations and layout for animate_only BeginFrames.
  params->wants_animate_only_begin_frames = true;

  // In disable frame rate limit mode, also let the renderer tick as fast as it
  // can. The top level begin frame source will also be running as a back to
  // back begin frame source, but using a synthetic begin frame source here
  // reduces latency when in this mode (at least for frames starting--it
  // potentially increases it for input on the other hand.)
  // TODO(b/221220344): Support dynamically setting the BeginFrameSource per VRR
  // state changes.
  const cc::LayerTreeSettings& settings = LayerTreeHost()->GetSettings();
  if (settings.disable_frame_rate_limit ||
      settings.enable_variable_refresh_rate) {
    params->use_begin_frame_presentation_feedback =
        base::FeatureList::IsEnabled(
            features::kUseBeginFramePresentationFeedback);
    params->synthetic_begin_frame_source = CreateSyntheticBeginFrameSource();
  }

  mojo::PendingReceiver<viz::mojom::blink::CompositorFrameSink>
      compositor_frame_sink_receiver = CrossVariantMojoReceiver<
          viz::mojom::blink::CompositorFrameSinkInterfaceBase>(
          params->pipes.compositor_frame_sink_remote
              .InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<viz::mojom::blink::CompositorFrameSinkClient>
      compositor_frame_sink_client;
  params->pipes.client_receiver = CrossVariantMojoReceiver<
      viz::mojom::blink::CompositorFrameSinkClientInterfaceBase>(
      compositor_frame_sink_client.InitWithNewPipeAndPassReceiver());

  Platform::EstablishGpuChannelCallback finish_callback =
      base::BindOnce(&WidgetBase::FinishRequestNewLayerTreeFrameSink,
                     weak_ptr_factory_.GetWeakPtr(), url,
                     std::move(compositor_frame_sink_receiver),
                     std::move(compositor_frame_sink_client),
                     std::move(render_frame_metadata_observer_client_receiver),
                     std::move(render_frame_metadata_observer_remote),
                     std::move(render_frame_metadata_observer),
                     std::move(params), std::move(callback));
  bool needs_sync_composite_for_test =
      layer_tree_view_ && LayerTreeHost()->in_composite_for_test();
  if (base::FeatureList::IsEnabled(features::kEstablishGpuChannelAsync) &&
      !needs_sync_composite_for_test) {
    Platform::Current()->EstablishGpuChannel(std::move(finish_callback));
  } else {
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
        Platform::Current()->EstablishGpuChannelSync();
    std::move(finish_callback).Run(gpu_channel_host);
  }
}

void WidgetBase::FinishRequestNewLayerTreeFrameSink(
    const KURL& url,
    mojo::PendingReceiver<viz::mojom::blink::CompositorFrameSink>
        compositor_frame_sink_receiver,
    mojo::PendingRemote<viz::mojom::blink::CompositorFrameSinkClient>
        compositor_frame_sink_client,
    mojo::PendingReceiver<cc::mojom::blink::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_client_receiver,
    mojo::PendingRemote<cc::mojom::blink::RenderFrameMetadataObserver>
        render_frame_metadata_observer_remote,
    std::unique_ptr<RenderFrameMetadataObserverImpl>
        render_frame_metadata_observer,
    std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams>
        params,
    LayerTreeFrameSinkCallback callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  if (!gpu_channel_host) {
    // Wait and try again. We may hear that the compositing mode has switched
    // to software in the meantime.
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  if (Platform::Current()->IsGpuCompositingDisabled()) {
    widget_host_->CreateFrameSink(std::move(compositor_frame_sink_receiver),
                                  std::move(compositor_frame_sink_client));
    widget_host_->RegisterRenderFrameMetadataObserver(
        std::move(render_frame_metadata_observer_client_receiver),
        std::move(render_frame_metadata_observer_remote));
    std::move(callback).Run(
        std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
            /*context_provider=*/nullptr, /*worker_context_provider=*/nullptr,
            gpu_channel_host->CreateClientSharedImageInterface(), params.get()),
        std::move(render_frame_metadata_observer));
    return;
  }

  if (Platform::Current()->IsGpuCompositingDisabled()) {
    // GPU compositing was disabled after the check in
    // WidgetBase::RequestNewLayerTreeFrameSink(). Fail and let it retry.
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  scoped_refptr<cc::RasterContextProviderWrapper>
      worker_context_provider_wrapper =
          Platform::Current()->SharedCompositorWorkerContextProvider(
              &RasterDarkModeFilterImpl::Instance());
  if (!worker_context_provider_wrapper) {
    // Cause the compositor to wait and try again.
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  {
    viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
        worker_context_provider_wrapper->GetContext().get());
    max_render_buffer_bounds_gpu_ =
        worker_context_provider_wrapper->GetContext()
            ->ContextCapabilities()
            .max_texture_size;
  }

  // The renderer compositor context doesn't do a lot of stuff, so we don't
  // expect it to need a lot of space for commands or transfer. Raster and
  // uploads happen on the worker context instead.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();

  // This is for an offscreen context for the compositor. So the default
  // framebuffer doesn't need alpha, depth, stencil, antialiasing.
  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  // VideoResourceUpdater was the only usage of gles2 interface from this
  // RasterContextProvider and now we use RasterInterface in
  // VideoResourceUpdater.
  attributes.enable_gles2_interface = false;
  attributes.enable_grcontext = false;
  attributes.enable_raster_interface = true;
  attributes.enable_oop_rasterization = false;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();

  auto context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          gpu_channel_host, kGpuStreamIdDefault, kGpuStreamPriorityDefault,
          gpu::kNullSurfaceHandle, GURL(url), automatic_flushes,
          support_locking, limits, attributes,
          viz::command_buffer_metrics::ContextType::RENDER_COMPOSITOR);

#if BUILDFLAG(IS_ANDROID)
  if (Platform::Current()->IsSynchronousCompositingEnabledForAndroidWebView() &&
      !is_embedded_) {
    // TODO(ericrk): Collapse with non-webview registration below.
    if (::features::IsUsingVizFrameSubmissionForWebView()) {
      widget_host_->CreateFrameSink(std::move(compositor_frame_sink_receiver),
                                    std::move(compositor_frame_sink_client));
    }
    widget_host_->RegisterRenderFrameMetadataObserver(
        std::move(render_frame_metadata_observer_client_receiver),
        std::move(render_frame_metadata_observer_remote));

    std::move(callback).Run(
        std::make_unique<SynchronousLayerTreeFrameSink>(
            std::move(context_provider),
            std::move(worker_context_provider_wrapper),
            Platform::Current()->CompositorThreadTaskRunner(),
            gpu_memory_buffer_manager, g_next_layer_tree_frame_sink_id++,
            std::move(params->synthetic_begin_frame_source),
            widget_input_handler_manager_->GetSynchronousCompositorRegistry(),
            CrossVariantMojoRemote<
                viz::mojom::blink::CompositorFrameSinkInterfaceBase>(
                std::move(params->pipes.compositor_frame_sink_remote)),
            CrossVariantMojoReceiver<
                viz::mojom::blink::CompositorFrameSinkClientInterfaceBase>(
                std::move(params->pipes.client_receiver))),
        std::move(render_frame_metadata_observer));
    return;
  }
#endif
  widget_host_->CreateFrameSink(std::move(compositor_frame_sink_receiver),
                                std::move(compositor_frame_sink_client));
  widget_host_->RegisterRenderFrameMetadataObserver(
      std::move(render_frame_metadata_observer_client_receiver),
      std::move(render_frame_metadata_observer_remote));
  params->gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  std::move(callback).Run(
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider),
          std::move(worker_context_provider_wrapper),
          gpu_channel_host->CreateClientSharedImageInterface(), params.get()),
      std::move(render_frame_metadata_observer));
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

void WidgetBase::DidCommitCompositorFrame(base::TimeTicks commit_start_time,
                                          base::TimeTicks commit_finish_time) {
  client_->EndCommitCompositorFrame(commit_start_time, commit_finish_time);
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
  // UpdateTextInputState() will cause a forced style and layout update, which
  // we would like to eliminate.
  if (!base::FeatureList::IsEnabled(features::kRunTextInputUpdatePostLifecycle))
    UpdateTextInputState();
}

void WidgetBase::RunPaintBenchmark(int repeat_count,
                                   cc::PaintBenchmarkResult& result) {
  client_->RunPaintBenchmark(repeat_count, result);
}

void WidgetBase::ScheduleAnimationForWebTests() {
  client_->ScheduleAnimationForWebTests();
}

std::unique_ptr<cc::RenderFrameMetadataObserver>
WidgetBase::CreateRenderFrameObserver() {
  mojo::PendingRemote<cc::mojom::blink::RenderFrameMetadataObserver>
      render_frame_metadata_observer_remote;
  mojo::PendingRemote<cc::mojom::blink::RenderFrameMetadataObserverClient>
      render_frame_metadata_client_remote;
  mojo::PendingReceiver<cc::mojom::blink::RenderFrameMetadataObserverClient>
      render_frame_metadata_observer_client_receiver =
          render_frame_metadata_client_remote.InitWithNewPipeAndPassReceiver();
  auto render_frame_metadata_observer =
      std::make_unique<RenderFrameMetadataObserverImpl>(
          render_frame_metadata_observer_remote
              .InitWithNewPipeAndPassReceiver(),
          std::move(render_frame_metadata_client_remote));
  widget_host_->RegisterRenderFrameMetadataObserver(
      std::move(render_frame_metadata_observer_client_receiver),
      std::move(render_frame_metadata_observer_remote));
  return render_frame_metadata_observer;
}

void WidgetBase::SetCompositorVisible(bool visible) {
  if (never_composited_)
    return;

  layer_tree_view_->SetVisible(visible);
}

void WidgetBase::WarmUpCompositor() {
  if (never_composited_) {
    return;
  }
  layer_tree_view_->SetShouldWarmUp();
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
}

void WidgetBase::BeginMainFrame(base::TimeTicks frame_time) {
  base::TimeTicks raf_aligned_input_start_time;
  if (ShouldRecordBeginMainFrameMetrics()) {
    raf_aligned_input_start_time = base::TimeTicks::Now();
  }

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  widget_input_handler_manager_->input_event_queue()->DispatchRafAlignedInput(
      frame_time);
  // DispatchRafAlignedInput could have detached the frame.
  if (!weak_this)
    return;

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
    base::OnceCallback<void(const viz::FrameTimingDetails&)> callback) {
  layer_tree_view_->AddPresentationCallback(frame_token, std::move(callback));
}

#if BUILDFLAG(IS_APPLE)
void WidgetBase::AddCoreAnimationErrorCodeCallback(
    uint32_t frame_token,
    base::OnceCallback<void(gfx::CALayerResult)> callback) {
  layer_tree_view_->AddCoreAnimationErrorCodeCallback(frame_token,
                                                      std::move(callback));
}
#endif

void WidgetBase::SetCursor(const ui::Cursor& cursor) {
  if (input_handler_.DidChangeCursor(cursor)) {
    widget_host_->SetCursor(cursor);
  }
}

void WidgetBase::UpdateTooltipUnderCursor(const String& tooltip_text,
                                          TextDirection dir) {
  widget_host_->UpdateTooltipUnderCursor(
      tooltip_text.empty() ? "" : tooltip_text, ToBaseTextDirection(dir));
}

void WidgetBase::UpdateTooltipFromKeyboard(const String& tooltip_text,
                                           TextDirection dir,
                                           const gfx::Rect& bounds) {
  widget_host_->UpdateTooltipFromKeyboard(
      tooltip_text.empty() ? "" : tooltip_text, ToBaseTextDirection(dir),
      BlinkSpaceToEnclosedDIPs(bounds));
}

void WidgetBase::ClearKeyboardTriggeredTooltip() {
  widget_host_->ClearKeyboardTriggeredTooltip();
}

void WidgetBase::ShowVirtualKeyboard() {
  UpdateTextInputStateInternal(true, false);
}

void WidgetBase::UpdateTextInputState() {
  UpdateTextInputStateInternal(false, false);
}

// static
void WidgetBase::AssertAreCompatible(const WidgetBase& a, const WidgetBase& b) {
  CHECK_EQ(a.is_embedded_, b.is_embedded_);
  CHECK_EQ(a.is_for_scalable_page_, b.is_for_scalable_page_);
  CHECK_EQ(a.main_thread_compositor_task_runner_,
           b.main_thread_compositor_task_runner_);
}

bool WidgetBase::CanComposeInline() {
  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return true;
  return frame_widget->CanComposeInline();
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
  std::optional<gfx::Rect> control_bounds;
  std::optional<gfx::Rect> selection_bounds;
  if (frame_widget) {
    new_info = frame_widget->TextInputInfo();
    // This will be used to decide whether or not to show VK when VK policy is
    // manual.
    last_vk_visibility_request =
        frame_widget->GetLastVirtualKeyboardVisibilityRequest();

    // Check whether the keyboard should always be hidden for the currently
    // focused element.
    always_hide_ime = frame_widget->ShouldSuppressKeyboardForFocusedElement();
    frame_widget->GetEditContextBoundsInWindow(&control_bounds,
                                               &selection_bounds);
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
        ui::mojom::VirtualKeyboardVisibilityRequest::NONE)) ||
      (control_bounds && frame_control_bounds_ != control_bounds) ||
      (selection_bounds && frame_selection_bounds_ != selection_bounds)) {
    ui::mojom::blink::TextInputStatePtr params =
        ui::mojom::blink::TextInputState::New();
    params->node_id = new_info.node_id;
    params->type = new_type;
    params->mode = new_mode;
    params->action = new_info.action;
    params->flags = new_info.flags;
    params->vk_policy = new_vk_policy;
    params->last_vk_visibility_request = last_vk_visibility_request;
    params->edit_context_control_bounds = control_bounds;
    params->edit_context_selection_bounds = selection_bounds;

    if (!new_info.ime_text_spans.empty() && frame_widget) {
      params->ime_text_spans_info =
          frame_widget->GetImeTextSpansInfo(new_info.ime_text_spans);
    }
#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {
      // It is expected that the selection range is always bounded by
      // the text content, but according to the logs in browser process
      // sometimes it is not.
      // LOG and dump stack traces in renderers temporarily for further
      // investigation.
      // TODO(crbug.com/1457178): Remove the strace when the root cause if
      // identified and fixed.
      gfx::Range text_range(0, params->value.length());
      if (!params->selection.IsBoundedBy(text_range)) {
        LOG(ERROR) << "selection range is not bounded by the text: "
                   << "selection=" << params->selection.ToString()
                   << "text=" << text_range.ToString();
        base::debug::DumpWithoutCrashing();
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
    frame_control_bounds_ = control_bounds.value_or(gfx::Rect());
    // Selection bounds are not populated in non-EditContext scenarios.
    // It is communicated to IMEs via |WidgetBase::UpdateSelectionBounds|.
    frame_selection_bounds_ = selection_bounds.value_or(gfx::Rect());
    // Reset the show/hide state in the InputMethodController.
    if (frame_widget) {
      if (last_vk_visibility_request !=
          ui::mojom::VirtualKeyboardVisibilityRequest::NONE) {
        // Reset the visibility state.
        frame_widget->ResetVirtualKeyboardVisibilityRequest();
      }
    }

#if BUILDFLAG(IS_ANDROID)
    // If we send a new TextInputStateChanged message, we must also deliver a
    // new RenderFrameMetadata, as the IME will need this info to be updated.
    // TODO(ericrk): Consider folding the above IPC into RenderFrameMetadata.
    // https://crbug.com/912309
    // Compositing might not be initialized but input can still be dispatched
    // to non-composited widgets so LayerTreeHost may be null.
    if (layer_tree_view_)
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
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, virtual keyboard is triggered only when users leave the
  // mouse button or the finger and a text input element is focused at that
  // time. Focus event itself shouldn't trigger virtual keyboard.
  UpdateTextInputState();
#else
  ShowVirtualKeyboard();
#endif

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !BUILDFLAG(IS_ANDROID)
  client_->FocusChangeComplete();
#endif
}

void WidgetBase::ProcessTouchAction(cc::TouchAction touch_action) {
  if (!input_handler_.ProcessTouchAction(touch_action))
    return;
  widget_input_handler_manager_->ProcessTouchAction(touch_action);
}

void WidgetBase::SetFocus(mojom::blink::FocusState focus_state) {
  has_focus_ = focus_state == mojom::blink::FocusState::kFocused;
  client_->FocusChanged(focus_state);
}

void WidgetBase::BindWidgetCompositor(
    mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver) {
  if (widget_compositor_)
    widget_compositor_->Shutdown();

  widget_compositor_ = WidgetCompositor::Create(
      weak_ptr_factory_.GetWeakPtr(),
      LayerTreeHost()->GetTaskRunnerProvider()->MainThreadTaskRunner(),
      LayerTreeHost()->GetTaskRunnerProvider()->ImplThreadTaskRunner(),
      std::move(receiver));
}

void WidgetBase::UpdateCompositionInfo(bool immediate_request) {
  if (!monitor_composition_info_ && !immediate_request)
    return;  // Do not calculate composition info if not requested.

  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget) {
    return;
  }

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

  std::optional<Vector<gfx::Rect>> line_bounds;

  // If using the new pipeline for CursorAnchorInfo data, send data from the
  // frame widget.
  if (RuntimeEnabledFeatures::CursorAnchorInfoMojoPipeEnabled()) {
    frame_widget->UpdateCursorAnchorInfo();
    return;
  }
  if (RuntimeEnabledFeatures::ReportVisibleLineBoundsEnabled()) {
    line_bounds = frame_widget->GetVisibleLineBoundsOnScreen();
  }
  if (mojom::blink::WidgetInputHandlerHost* host =
          widget_input_handler_manager_->GetWidgetInputHandlerHost()) {
    host->ImeCompositionRangeChanged(
        composition_range_, composition_character_bounds_, line_bounds);
  }
}

void WidgetBase::ForceTextInputStateUpdate() {
#if BUILDFLAG(IS_ANDROID)
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
  if (!frame_widget)
    return;
  *range = frame_widget->CompositionRange();
}

void WidgetBase::GetCompositionCharacterBounds(Vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();

  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
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
  for (wtf_size_t i = 0; i < bounds.size(); ++i) {
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

  if (widget_scheduler_)
    widget_scheduler_->SetHidden(hidden);

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
            &selection_focus_rect_, &selection_anchor_rect_,
            &selection_bounding_box_, &focus_dir, &anchor_dir,
            &is_anchor_first)) {
      widget_host_->SelectionBoundsChanged(
          selection_anchor_rect_, anchor_dir, selection_focus_rect_, focus_dir,
          selection_bounding_box_, is_anchor_first);
    }
  }
  UpdateCompositionInfo(false /* not an immediate request */);
}

void WidgetBase::MouseCaptureLost() {
  FrameWidget* frame_widget = client_->FrameWidget();
  if (!frame_widget)
    return;
  frame_widget->MouseCaptureLost();
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
  if (frame_widget->ShouldDispatchImeEventsToPlugin()) {
    frame_widget->ImeSetCompositionForPlugin(text, ime_text_spans,
                                             replacement_range, selection_start,
                                             selection_end);
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
  if (frame_widget->ShouldDispatchImeEventsToPlugin()) {
    frame_widget->ImeCommitTextForPlugin(
        text, ime_text_spans, replacement_range, relative_cursor_pos);
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
  if (frame_widget->ShouldDispatchImeEventsToPlugin()) {
    frame_widget->ImeFinishComposingTextForPlugin(keep_selection);
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
  client_->WillQueueSyntheticEvent(*event);

  // Popups, which don't have a threaded input handler, are allowed to queue up
  // main thread gesture scroll events.
  bool uses_input_handler = client_->FrameWidget();

  // TODO(acomminos): If/when we add support for gesture event attribution on
  //                  the impl thread, have the caller provide attribution.
  WebInputEventAttribution attribution;
  widget_input_handler_manager_->input_event_queue()->HandleEvent(
      std::move(event), MainThreadEventQueue::DispatchType::kNonBlocking,
      mojom::blink::InputEventResultState::kNotConsumed, attribution, nullptr,
      HandledEventCallback(), !uses_input_handler);
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
#if BUILDFLAG(IS_MAC) || defined(USE_AURA)
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
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (guard->show_virtual_keyboard())
    ShowVirtualKeyboard();
  else
    UpdateTextInputState();
#endif
}

void WidgetBase::RequestAnimationAfterDelay(const base::TimeDelta& delay) {
  if (delay.is_zero()) {
    client_->ScheduleAnimation();
    return;
  }

  // Consolidate delayed animation frame requests to keep only the longest
  // delay.
  if (request_animation_after_delay_timer_.IsActive() &&
      request_animation_after_delay_timer_.NextFireInterval() > delay) {
    request_animation_after_delay_timer_.Stop();
  }
  if (!request_animation_after_delay_timer_.IsActive()) {
    request_animation_after_delay_timer_.StartOneShot(delay, FROM_HERE);
  }
}

void WidgetBase::RequestAnimationAfterDelayTimerFired(TimerBase*) {
  client_->ScheduleAnimation();
}

float WidgetBase::GetOriginalDeviceScaleFactor() const {
  return client_->GetOriginalScreenInfos().current().device_scale_factor;
}

void WidgetBase::UpdateSurfaceAndScreenInfo(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Rect& compositor_viewport_pixel_rect,
    const display::ScreenInfos& screen_infos) {
  display::ScreenInfos new_screen_infos = screen_infos;
  display::ScreenInfo& new_screen_info = new_screen_infos.mutable_current();

  // If there is a screen orientation override apply it.
  if (auto orientation_override = client_->ScreenOrientationOverride()) {
    new_screen_info.orientation_type = orientation_override.value();
    new_screen_info.orientation_angle =
        OrientationTypeToAngle(new_screen_info.orientation_type);
  }

  // RenderWidgetHostImpl::SynchronizeVisualProperties uses similar logic to
  // detect orientation changes on the display currently showing the widget.
  const display::ScreenInfo& previous_screen_info = screen_infos_.current();
  bool orientation_changed =
      previous_screen_info.orientation_angle !=
          new_screen_info.orientation_angle ||
      previous_screen_info.orientation_type != new_screen_info.orientation_type;
  display::ScreenInfos previous_original_screen_infos =
      client_->GetOriginalScreenInfos();

  local_surface_id_from_parent_ = new_local_surface_id;
  screen_infos_ = new_screen_infos;

  // Note carefully that the DSF specified in |new_screen_info| is not the
  // DSF used by the compositor during device emulation!
  LayerTreeHost()->SetViewportRectAndScale(compositor_viewport_pixel_rect,
                                           GetOriginalDeviceScaleFactor(),
                                           local_surface_id_from_parent_);
  // The VisualDeviceViewportIntersectionRect derives from the LayerTreeView's
  // viewport size, which is set above.
  LayerTreeHost()->SetVisualDeviceViewportIntersectionRect(
      client_->ViewportVisibleRect());
  if (display::Display::HasForceRasterColorProfile()) {
    LayerTreeHost()->SetDisplayColorSpaces(gfx::DisplayColorSpaces(
        display::Display::GetForcedRasterColorProfile()));
  } else {
    LayerTreeHost()->SetDisplayColorSpaces(
        screen_infos_.current().display_color_spaces);
  }

  if (orientation_changed)
    client_->OrientationChanged();

  client_->DidUpdateSurfaceAndScreen(previous_original_screen_infos);
}

void WidgetBase::UpdateScreenInfo(
    const display::ScreenInfos& new_screen_infos) {
  UpdateSurfaceAndScreenInfo(local_surface_id_from_parent_,
                             CompositorViewportRect(), new_screen_infos);
}

void WidgetBase::UpdateCompositorViewportAndScreenInfo(
    const gfx::Rect& compositor_viewport_pixel_rect,
    const display::ScreenInfos& new_screen_infos) {
  UpdateSurfaceAndScreenInfo(local_surface_id_from_parent_,
                             compositor_viewport_pixel_rect, new_screen_infos);
}

void WidgetBase::UpdateCompositorViewportRect(
    const gfx::Rect& compositor_viewport_pixel_rect) {
  UpdateSurfaceAndScreenInfo(local_surface_id_from_parent_,
                             compositor_viewport_pixel_rect, screen_infos_);
}

void WidgetBase::UpdateSurfaceAndCompositorRect(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Rect& compositor_viewport_pixel_rect) {
  UpdateSurfaceAndScreenInfo(new_local_surface_id,
                             compositor_viewport_pixel_rect, screen_infos_);
}

const display::ScreenInfo& WidgetBase::GetScreenInfo() {
  return screen_infos_.current();
}

void WidgetBase::SetScreenRects(const gfx::Rect& widget_screen_rect,
                                const gfx::Rect& window_screen_rect) {
  widget_screen_rect_ = widget_screen_rect;
  window_screen_rect_ = window_screen_rect;
}

void WidgetBase::SetPendingWindowRect(const gfx::Rect& rect) {
  pending_window_rect_count_++;
  pending_window_rect_ = rect;
  // Popups don't get size updates back from the browser so just store the set
  // values.
  if (!client_->FrameWidget()) {
    SetScreenRects(rect, rect);
  }
}

void WidgetBase::AckPendingWindowRect() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
  if (pending_window_rect_count_ == 0)
    pending_window_rect_.reset();
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

LCDTextPreference WidgetBase::ComputeLCDTextPreference() const {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisablePreferCompositingToLCDText)) {
    return LCDTextPreference::kStronglyPreferred;
  }
  if (!Platform::Current()->IsLcdTextEnabled()) {
    return LCDTextPreference::kIgnored;
  }
  // Prefer compositing if the device scale is high enough that losing subpixel
  // antialiasing won't have a noticeable effect on text quality.
  // Note: We should keep kHighDPIDeviceScaleFactorThreshold in
  // cc/metrics/lcd_text_metrics_reporter.cc the same as the value below.
  if (screen_infos_.current().device_scale_factor >= 1.5f) {
    return LCDTextPreference::kIgnored;
  }
  if (command_line.HasSwitch(switches::kEnablePreferCompositingToLCDText) ||
      base::FeatureList::IsEnabled(features::kPreferCompositingToLCDText)) {
    return LCDTextPreference::kWeaklyPreferred;
  }
  return LCDTextPreference::kStronglyPreferred;
}

void WidgetBase::CountDroppedPointerDownForEventTiming(unsigned count) {
  client_->CountDroppedPointerDownForEventTiming(count);
}

gfx::PointF WidgetBase::DIPsToBlinkSpace(const gfx::PointF& point) {
  // TODO(danakj): Should this use non-original scale factor so it changes under
  // emulation?
  return gfx::ScalePoint(point, GetOriginalDeviceScaleFactor());
}

gfx::Point WidgetBase::DIPsToRoundedBlinkSpace(const gfx::Point& point) {
  // TODO(danakj): Should this use non-original scale factor so it changes under
  // emulation?
  return gfx::ScaleToRoundedPoint(point, GetOriginalDeviceScaleFactor());
}

gfx::PointF WidgetBase::BlinkSpaceToDIPs(const gfx::PointF& point) {
  // TODO(danakj): Should this use non-original scale factor so it changes under
  // emulation?
  return gfx::ScalePoint(point, 1.f / GetOriginalDeviceScaleFactor());
}

gfx::Point WidgetBase::BlinkSpaceToFlooredDIPs(const gfx::Point& point) {
  // TODO(danakj): Should this use non-original scale factor so it changes under
  // emulation?
  float reverse = 1 / GetOriginalDeviceScaleFactor();
  return gfx::ScaleToFlooredPoint(point, reverse);
}

gfx::Size WidgetBase::DIPsToCeiledBlinkSpace(const gfx::Size& size) {
  return gfx::ScaleToCeiledSize(size, GetOriginalDeviceScaleFactor());
}

gfx::RectF WidgetBase::DIPsToBlinkSpace(const gfx::RectF& rect) {
  // TODO(danakj): Should this use non-original scale factor so it changes under
  // emulation?
  return gfx::ScaleRect(rect, GetOriginalDeviceScaleFactor());
}

float WidgetBase::DIPsToBlinkSpace(float scalar) {
  // TODO(danakj): Should this use non-original scale factor so it changes under
  // emulation?
  return GetOriginalDeviceScaleFactor() * scalar;
}

gfx::Size WidgetBase::BlinkSpaceToFlooredDIPs(const gfx::Size& size) {
  float reverse = 1 / GetOriginalDeviceScaleFactor();
  return gfx::ScaleToFlooredSize(size, reverse);
}

gfx::Rect WidgetBase::BlinkSpaceToEnclosedDIPs(const gfx::Rect& rect) {
  float reverse = 1 / GetOriginalDeviceScaleFactor();
  return gfx::ScaleToEnclosedRect(rect, reverse);
}

gfx::RectF WidgetBase::BlinkSpaceToDIPs(const gfx::RectF& rect) {
  float reverse = 1 / GetOriginalDeviceScaleFactor();
  return gfx::ScaleRect(rect, reverse);
}

std::optional<int> WidgetBase::GetMaxRenderBufferBounds() const {
  return Platform::Current()->IsGpuCompositingDisabled()
             ? max_render_buffer_bounds_sw_
             : max_render_buffer_bounds_gpu_;
}

}  // namespace blink
