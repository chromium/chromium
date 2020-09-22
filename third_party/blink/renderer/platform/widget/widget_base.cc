// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/ukm_manager.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_view.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"
#include "ui/base/ime/mojom/text_input_state.mojom-blink.h"
#include "ui/gfx/presentation_feedback.h"

namespace blink {

namespace {

static const int kInvalidNextPreviousFlagsValue = -1;

scoped_refptr<base::SingleThreadTaskRunner> GetCleanupTaskRunner() {
  if (auto* main_thread_scheduler =
          scheduler::WebThreadScheduler::MainThreadScheduler()) {
    return main_thread_scheduler->CleanupTaskRunner();
  } else {
    return base::ThreadTaskRunnerHandle::Get();
  }
}

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

}  // namespace

WidgetBase::WidgetBase(
    WidgetBaseClient* client,
    CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget)
    : client_(client),
      widget_host_(std::move(widget_host)),
      receiver_(this, std::move(widget)) {
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
    cc::TaskGraphRunner* task_graph_runner,
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  auto* main_thread_scheduler =
      scheduler::WebThreadScheduler::MainThreadScheduler();
  auto* compositing_thread_scheduler =
      scheduler::WebThreadScheduler::CompositorThreadScheduler();
  layer_tree_view_ = std::make_unique<LayerTreeView>(
      this,
      main_thread_scheduler ? main_thread_scheduler->CompositorTaskRunner()
                            : base::ThreadTaskRunnerHandle::Get(),
      compositing_thread_scheduler
          ? compositing_thread_scheduler->DefaultTaskRunner()
          : nullptr,
      task_graph_runner, main_thread_scheduler);
  layer_tree_view_->Initialize(settings, std::move(ukm_recorder_factory));
}

void WidgetBase::Shutdown(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner,
    base::OnceCallback<void()> cleanup_task) {
  if (!cleanup_runner)
    cleanup_runner = GetCleanupTaskRunner();

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
  // This needs to be a NonNestableTask as it needs to occur after DeleteSoon.
  if (cleanup_task)
    cleanup_runner->PostNonNestableTask(FROM_HERE, std::move(cleanup_task));
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
  client_->GetWidgetInputHandler(std::move(request), std::move(host));
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
  client_->OnDeferMainFrameUpdatesChanged(defer);
}

void WidgetBase::OnDeferCommitsChanged(bool defer) {
  client_->OnDeferCommitsChanged(defer);
}

void WidgetBase::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void WidgetBase::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  client_->RequestNewLayerTreeFrameSink(std::move(callback));
}

void WidgetBase::DidCommitAndDrawCompositorFrame() {
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

void WidgetBase::SubmitThroughputData(ukm::SourceId source_id,
                                      int aggregated_percent,
                                      int impl_percent,
                                      base::Optional<int> main_percent) {
  client_->SubmitThroughputData(source_id, aggregated_percent, impl_percent,
                                main_percent);
}

void WidgetBase::SetCompositorVisible(bool visible) {
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
  client_->DispatchRafAlignedInput(frame_time);
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
  if (client_->HasCurrentImeGuard(show_virtual_keyboard) ||
      input_handler_.ProtectedByIMEGuard(show_virtual_keyboard)) {
    DCHECK(!reply_to_request);
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
      text_input_info_ != new_info ||
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

bool WidgetBase::ProcessTouchAction(cc::TouchAction touch_action) {
  return input_handler_.ProcessTouchAction(touch_action);
}

void WidgetBase::SetFocus(bool enable) {
  has_focus_ = enable;
  client_->FocusChanged(enable);
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

  client_->SendCompositionRangeChanged(
      composition_range_,
      std::vector<gfx::Rect>(composition_character_bounds_.begin(),
                             composition_character_bounds_.end()));
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

ui::TextInputType WidgetBase::GetTextInputType() {
  return ConvertWebTextInputType(client_->GetTextInputType());
}

void WidgetBase::UpdateSelectionBounds() {
  TRACE_EVENT0("renderer", "WidgetBase::UpdateSelectionBounds");
  if (client_->HasCurrentImeGuard(false) ||
      input_handler_.ProtectedByIMEGuard(false)) {
    return;
  }
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

}  // namespace blink
