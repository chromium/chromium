// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_impl.h"

#include <math.h>

#include <algorithm>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/optional_trace_event.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/trees/browser_controls_params.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/display_feature.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/input/fling_scheduler.h"
#include "content/browser/renderer_host/input/input_router_config_helper.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/peak_gpu_memory_tracker.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_process_host_priority_client.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/result_codes.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/filename_util.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_base.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/display/display_util.h"
#include "ui/display/screen.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/snapshot/snapshot.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/input/fling_scheduler_android.h"
#include "ui/android/view_android.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/renderer_host/input/fling_scheduler_mac.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

using blink::DragOperationsMask;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

// How long to wait for newly loaded content to send a compositor frame
// before clearing previously displayed graphics.
constexpr base::TimeDelta kNewContentRenderingDelay = base::Seconds(4);

constexpr gfx::Rect kInvalidScreenRect(std::numeric_limits<int>::max(),
                                       std::numeric_limits<int>::max(),
                                       0,
                                       0);

bool g_check_for_pending_visual_properties_ack = true;

// <process id, routing id>
using RenderWidgetHostID = std::pair<int32_t, int32_t>;
using RoutingIDWidgetMap =
    std::unordered_map<RenderWidgetHostID,
                       RenderWidgetHostImpl*,
                       base::IntPairHash<RenderWidgetHostID>>;
base::LazyInstance<RoutingIDWidgetMap>::DestructorAtExit
    g_routing_id_widget_map = LAZY_INSTANCE_INITIALIZER;

// Implements the RenderWidgetHostIterator interface. It keeps a list of
// RenderWidgetHosts, and makes sure it returns a live RenderWidgetHost at each
// iteration (or NULL if there isn't any left).
class RenderWidgetHostIteratorImpl : public RenderWidgetHostIterator {
 public:
  RenderWidgetHostIteratorImpl() = default;

  RenderWidgetHostIteratorImpl(const RenderWidgetHostIteratorImpl&) = delete;
  RenderWidgetHostIteratorImpl& operator=(const RenderWidgetHostIteratorImpl&) =
      delete;

  ~RenderWidgetHostIteratorImpl() override = default;

  void Add(RenderWidgetHost* host) {
    hosts_.emplace_back(host->GetProcess()->GetID(), host->GetRoutingID());
  }

  // RenderWidgetHostIterator:
  RenderWidgetHost* GetNextHost() override {
    RenderWidgetHost* host = nullptr;
    while (current_index_ < hosts_.size() && !host) {
      RenderWidgetHostID id = hosts_[current_index_];
      host = RenderWidgetHost::FromID(id.first, id.second);
      ++current_index_;
    }
    return host;
  }

 private:
  std::vector<RenderWidgetHostID> hosts_;
  size_t current_index_ = 0;
};

std::vector<DropData::Metadata> DropDataToMetaData(const DropData& drop_data) {
  std::vector<DropData::Metadata> metadata;
  if (drop_data.text) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING, base::ASCIIToUTF16(ui::kMimeTypeText)));
  }

  if (drop_data.url.is_valid()) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING, base::ASCIIToUTF16(ui::kMimeTypeURIList)));
  }

  if (drop_data.html) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING, base::ASCIIToUTF16(ui::kMimeTypeHTML)));
  }

  // On Aura, filenames are available before drop.
  for (const auto& file_info : drop_data.filenames) {
    if (!file_info.path.empty()) {
      metadata.push_back(DropData::Metadata::CreateForFilePath(file_info.path));
    }
  }

  // On Android, only files' mime types are available before drop.
  for (const auto& mime_type : drop_data.file_mime_types) {
    if (!mime_type.empty()) {
      metadata.push_back(DropData::Metadata::CreateForMimeType(
          DropData::Kind::FILENAME, mime_type));
    }
  }

  for (const auto& file_system_file : drop_data.file_system_files) {
    if (!file_system_file.url.is_empty()) {
      metadata.push_back(
          DropData::Metadata::CreateForFileSystemUrl(file_system_file.url));
    }
  }

  if (drop_data.file_contents_source_url.is_valid()) {
    metadata.push_back(DropData::Metadata::CreateForBinary(
        drop_data.file_contents_source_url));
  }

  for (const auto& custom_data_item : drop_data.custom_data) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING, custom_data_item.first));
  }

  return metadata;
}

class UnboundWidgetInputHandler : public blink::mojom::WidgetInputHandler {
 public:
  void SetFocus(blink::mojom::FocusState focus_state) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void MouseCaptureLost() override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void SetEditCommandsForNextKeyEvent(
      std::vector<blink::mojom::EditCommandPtr> commands) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void CursorVisibilityChanged(bool visible) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void ImeSetComposition(const std::u16string& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& range,
                         int32_t start,
                         int32_t end,
                         ImeSetCompositionCallback callback) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void ImeCommitText(const std::u16string& text,
                     const std::vector<ui::ImeTextSpan>& ime_text_spans,
                     const gfx::Range& range,
                     int32_t relative_cursor_position,
                     ImeCommitTextCallback callback) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void ImeFinishComposingText(bool keep_selection) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void RequestTextInputStateUpdate() override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void RequestCompositionUpdates(bool immediate_request,
                                 bool monitor_request) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void DispatchEvent(std::unique_ptr<blink::WebCoalescedInputEvent> event,
                     DispatchEventCallback callback) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void DispatchNonBlockingEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
  void WaitForInputProcessed(WaitForInputProcessedCallback callback) override {
    DLOG(WARNING) << "Input request on unbound interface";
  }
#if BUILDFLAG(IS_ANDROID)
  void AttachSynchronousCompositor(
      mojo::PendingRemote<blink::mojom::SynchronousCompositorControlHost>
          control_host,
      mojo::PendingAssociatedRemote<blink::mojom::SynchronousCompositorHost>
          host,
      mojo::PendingAssociatedReceiver<blink::mojom::SynchronousCompositor>
          compositor_request) override {
    NOTREACHED() << "Input request on unbound interface";
  }
#endif
  void GetFrameWidgetInputHandler(
      mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetInputHandler>
          request) override {
    NOTREACHED() << "Input request on unbound interface";
  }
  void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                  cc::BrowserControlsState current,
                                  bool animate) override {
    NOTREACHED() << "Input request on unbound interface";
  }
};

std::u16string GetWrappedTooltipText(
    const std::u16string& tooltip_text,
    base::i18n::TextDirection text_direction_hint) {
  // First, add directionality marks around tooltip text if necessary.
  // A naive solution would be to simply always wrap the text. However, on
  // windows, Unicode directional embedding characters can't be displayed on
  // systems that lack RTL fonts and are instead displayed as empty squares.
  //
  // To get around this we only wrap the string when we deem it necessary i.e.
  // when the locale direction is different than the tooltip direction hint.
  //
  // Currently, we use element's directionality as the tooltip direction hint.
  // An alternate solution would be to set the overall directionality based on
  // trying to detect the directionality from the tooltip text rather than the
  // element direction.  One could argue that would be a preferable solution
  // but we use the current approach to match Fx & IE's behavior.
  std::u16string wrapped_tooltip_text = tooltip_text;
  if (!tooltip_text.empty()) {
    if (text_direction_hint == base::i18n::LEFT_TO_RIGHT) {
      // Force the tooltip to have LTR directionality.
      wrapped_tooltip_text =
          base::i18n::GetDisplayStringInLTRDirectionality(wrapped_tooltip_text);
    } else if (text_direction_hint == base::i18n::RIGHT_TO_LEFT &&
               !base::i18n::IsRTL()) {
      // Force the tooltip to have RTL directionality.
      base::i18n::WrapStringWithRTLFormatting(&wrapped_tooltip_text);
    }
  }
  return wrapped_tooltip_text;
}

base::LazyInstance<UnboundWidgetInputHandler>::Leaky g_unbound_input_handler =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostImpl

// static
std::unique_ptr<RenderWidgetHostImpl> RenderWidgetHostImpl::Create(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden,
    bool renderer_initiated_creation,
    std::unique_ptr<FrameTokenMessageQueue> frame_token_message_queue) {
  return base::WrapUnique(new RenderWidgetHostImpl(
      frame_tree,
      /*self_owned=*/false, delegate, std::move(site_instance_group),
      routing_id, hidden, renderer_initiated_creation,
      std::move(frame_token_message_queue)));
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::CreateSelfOwned(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden,
    std::unique_ptr<FrameTokenMessageQueue> frame_token_message_queue) {
  return new RenderWidgetHostImpl(frame_tree, /*self_owned=*/true, delegate,
                                  std::move(site_instance_group), routing_id,
                                  hidden,
                                  /*renderer_initiated_creation=*/true,
                                  std::move(frame_token_message_queue));
}

RenderWidgetHostImpl::RenderWidgetHostImpl(
    FrameTree* frame_tree,
    bool self_owned,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden,
    bool renderer_initiated_creation,
    std::unique_ptr<FrameTokenMessageQueue> frame_token_message_queue)
    : frame_tree_(frame_tree),
      self_owned_(self_owned),
      waiting_for_init_(renderer_initiated_creation),
      delegate_(delegate),
      agent_scheduling_group_(site_instance_group->agent_scheduling_group()),
      site_instance_group_(site_instance_group->GetWeakPtrToAllowDangling()),
      routing_id_(routing_id),
      is_hidden_(hidden),
      last_view_screen_rect_(kInvalidScreenRect),
      last_window_screen_rect_(kInvalidScreenRect),
      should_disable_hang_monitor_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableHangMonitor)),
      latency_tracker_(delegate_),
      hung_renderer_delay_(kHungRendererDelay),
      new_content_rendering_delay_(kNewContentRenderingDelay),
      frame_token_message_queue_(std::move(frame_token_message_queue)),
      render_frame_metadata_provider_(
#if BUILDFLAG(IS_MAC)
          ui::WindowResizeHelperMac::Get()->task_runner(),
#else
          content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}),
#endif
          frame_token_message_queue_.get()),
      frame_sink_id_(base::checked_cast<uint32_t>(
                         agent_scheduling_group_->GetProcess()->GetID()),
                     base::checked_cast<uint32_t>(routing_id_)),
      power_mode_input_voter_(
          power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
              "PowerModeVoter.Input")),
      power_mode_loading_voter_(
          power_scheduler::PowerModeArbiter::GetInstance()->NewVoter(
              "PowerModeVoter.Loading")) {
  DCHECK(frame_token_message_queue_);
  frame_token_message_queue_->Init(this);

#if BUILDFLAG(IS_MAC)
  fling_scheduler_ = std::make_unique<FlingSchedulerMac>(this);
#elif BUILDFLAG(IS_ANDROID)
  fling_scheduler_ = std::make_unique<FlingSchedulerAndroid>(this);
#else
  fling_scheduler_ = std::make_unique<FlingScheduler>(this);
#endif
  CHECK(delegate_);
  CHECK_NE(MSG_ROUTING_NONE, routing_id_);
  DCHECK(base::ThreadPoolInstance::Get());

  std::pair<RoutingIDWidgetMap::iterator, bool> result =
      g_routing_id_widget_map.Get().insert(std::make_pair(
          RenderWidgetHostID(agent_scheduling_group_->GetProcess()->GetID(),
                             routing_id_),
          this));
  CHECK(result.second) << "Inserting a duplicate item!";

  // Self-owned RenderWidgetHost lifetime is managed by the renderer process.
  // To avoid leaking any instance. They self-delete when their renderer process
  // is gone.
  if (self_owned_) {
    agent_scheduling_group_->GetProcess()->AddObserver(this);
  }

  render_process_blocked_state_changed_subscription_ =
      agent_scheduling_group_->GetProcess()->RegisterBlockStateChangedCallback(
          base::BindRepeating(
              &RenderWidgetHostImpl::RenderProcessBlockedStateChanged,
              base::Unretained(this)));
  agent_scheduling_group_->GetProcess()->AddPriorityClient(this);

  SetupInputRouter();

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableNewContentRenderingTimeout)) {
    new_content_rendering_timeout_ = std::make_unique<TimeoutMonitor>(
        base::BindRepeating(&RenderWidgetHostImpl::ClearDisplayedGraphics,
                            weak_factory_.GetWeakPtr()));
  }
  input_event_ack_timeout_.SetTaskRunner(
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}));

  delegate_->RenderWidgetCreated(this);
  render_frame_metadata_provider_.AddObserver(this);
}

RenderWidgetHostImpl::~RenderWidgetHostImpl() {
  CHECK(!self_owned_);
  render_frame_metadata_provider_.RemoveObserver(this);
  if (!destroyed_) {
    Destroy(false);
  }
}

// static
RenderWidgetHost* RenderWidgetHost::FromID(int32_t process_id,
                                           int32_t routing_id) {
  return RenderWidgetHostImpl::FromID(process_id, routing_id);
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::FromID(int32_t process_id,
                                                   int32_t routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  auto it = widgets->find(RenderWidgetHostID(process_id, routing_id));
  return it != widgets->end() ? it->second : nullptr;
}

// static
std::unique_ptr<RenderWidgetHostIterator>
RenderWidgetHost::GetRenderWidgetHosts() {
  auto hosts = std::make_unique<RenderWidgetHostIteratorImpl>();
  for (auto& it : g_routing_id_widget_map.Get()) {
    RenderWidgetHostImpl* widget = it.second;
    RenderWidgetHostOwnerDelegate* owner_delegate = widget->owner_delegate();
    // If the widget is not for a main frame, add to |hosts|.
    if (!owner_delegate) {
      hosts->Add(widget);
      continue;
    }

    // If the widget is for a main frame, only add if there is a RenderWidget in
    // the renderer process. When this is false, there is no main RenderFrame
    // and so no RenderWidget for this RenderWidgetHost.
    if (owner_delegate->IsMainFrameActive()) {
      hosts->Add(widget);
    }
  }

  return std::move(hosts);
}

// static
std::unique_ptr<RenderWidgetHostIterator>
RenderWidgetHostImpl::GetAllRenderWidgetHosts() {
  auto hosts = std::make_unique<RenderWidgetHostIteratorImpl>();
  for (auto& it : g_routing_id_widget_map.Get()) {
    hosts->Add(it.second);
  }

  return std::move(hosts);
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::From(RenderWidgetHost* rwh) {
  return static_cast<RenderWidgetHostImpl*>(rwh);
}

void RenderWidgetHostImpl::SetView(RenderWidgetHostViewBase* view) {
  synthetic_gesture_controller_.reset();

  if (view) {
    view_ = view->GetWeakPtr();
    if (!create_frame_sink_callback_.is_null()) {
      std::move(create_frame_sink_callback_).Run(view_->GetFrameSinkId());
    }

    // SendScreenRects() and SynchronizeVisualProperties() delay until a view
    // is set, however we come here with a newly created `view` that is not
    // initialized and ready to be used.
    // The portal codepath comes here because it replaces the view while the
    // renderer-side widget is already created. In that case the renderer will
    // hear about geometry changes from the view being moved/resized as a result
    // of the change.
    // Speculative RenderViews also end up setting a `view` after creating the
    // renderer-side widget, as per https://crbug.com/1161585. That path must
    // be responsible for updating the renderer geometry itself, which it does
    // because it will start hidden, and will send them when shown.
    // TODO(crbug.com/1161585): Once RendererWidgetCreated() is always called
    // with a non-null `view` then this comment can go away. :)
  } else {
    view_.reset();
  }
}

// static
const base::TimeDelta RenderWidgetHostImpl::kActivationNotificationExpireTime =
    base::Milliseconds(300);

RenderProcessHost* RenderWidgetHostImpl::GetProcess() {
  return agent_scheduling_group_->GetProcess();
}

int RenderWidgetHostImpl::GetRoutingID() {
  return routing_id_;
}

RenderWidgetHostViewBase* RenderWidgetHostImpl::GetView() {
  return view_.get();
}

VisibleTimeRequestTrigger&
RenderWidgetHostImpl::GetVisibleTimeRequestTrigger() {
  return delegate()->GetVisibleTimeRequestTrigger();
}

const viz::FrameSinkId& RenderWidgetHostImpl::GetFrameSinkId() {
  return frame_sink_id_;
}

void RenderWidgetHostImpl::SendScreenRects() {
  // Sending screen rects are deferred until we have a connection to a
  // renderer-side Widget to send them to. Further, if we're waiting for the
  // renderer to show (aka Init()) the widget then we defer sending updates
  // until the renderer is ready.
  if (!renderer_widget_created_ || waiting_for_init_) {
    return;
  }
  // TODO(danakj): The `renderer_widget_created_` flag is set to true for
  // widgets owned by inactive RenderViewHosts, even though there is no widget
  // created. In that case the `view_` will not be created.
  if (!view_) {
    return;
  }
  // Throttle to one update at a time.
  if (waiting_for_screen_rects_ack_) {
    return;
  }
  if (is_hidden_) {
    // On GTK, this comes in for backgrounded tabs. Ignore, to match what
    // happens on Win & Mac, and when the view is shown it'll call this again.
    return;
  }

  if (last_view_screen_rect_ == view_->GetViewBounds() &&
      last_window_screen_rect_ == view_->GetBoundsInRootWindow()) {
    return;
  }

  last_view_screen_rect_ = view_->GetViewBounds();
  last_window_screen_rect_ = view_->GetBoundsInRootWindow();
  blink_widget_->UpdateScreenRects(
      last_view_screen_rect_, last_window_screen_rect_,
      base::BindOnce(&RenderWidgetHostImpl::OnUpdateScreenRectsAck,
                     weak_factory_.GetWeakPtr()));
  waiting_for_screen_rects_ack_ = true;
}

void RenderWidgetHostImpl::SetFrameDepth(unsigned int depth) {
  if (frame_depth_ == depth) {
    return;
  }

  frame_depth_ = depth;
  UpdatePriority();
}

void RenderWidgetHostImpl::SetIntersectsViewport(bool intersects) {
  if (intersects_viewport_ == intersects) {
    return;
  }

  intersects_viewport_ = intersects;
  UpdatePriority();
}

void RenderWidgetHostImpl::UpdatePriority() {
  if (!destroyed_) {
    GetProcess()->UpdateClientPriority(this);
  }
}

void RenderWidgetHostImpl::BindWidgetInterfaces(
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> widget) {
  // This API may get called on a RenderWidgetHostImpl from a
  // reused RenderViewHostImpl so we need to ensure old channels are dropped.
  // TODO(dcheng): Rather than resetting here, reset when the process goes away.
  blink_widget_host_receiver_.reset();
  blink_widget_.reset();
  widget_input_handler_.reset();
  blink_widget_host_receiver_.Bind(
      std::move(widget_host),
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}));
  blink_widget_.Bind(std::move(widget), content::GetUIThreadTaskRunner(
                                            {BrowserTaskType::kUserInput}));
}

void RenderWidgetHostImpl::BindPopupWidgetInterface(
    mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
        popup_widget_host) {
  blink_popup_widget_host_receiver_.reset();
  blink_popup_widget_host_receiver_.Bind(
      std::move(popup_widget_host),
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}));
}

void RenderWidgetHostImpl::BindFrameWidgetInterfaces(
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetHost>
        frame_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::FrameWidget> frame_widget) {
  // This API may get called on a RenderWidgetHostImpl from a
  // reused RenderViewHostImpl so we need to ensure old channels are dropped.
  // TODO(dcheng): Rather than resetting here, reset when the process goes away.
  blink_frame_widget_host_receiver_.reset();
  blink_frame_widget_.reset();
  frame_widget_input_handler_.reset();
  input_target_client_.reset();
  widget_compositor_.reset();
  blink_frame_widget_host_receiver_.Bind(
      std::move(frame_widget_host),
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}));
  blink_frame_widget_.Bind(
      std::move(frame_widget),
      content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}));
}

void RenderWidgetHostImpl::RendererWidgetCreated(bool for_frame_widget) {
  DCHECK(GetProcess()->IsInitializedAndNotDead());

  renderer_widget_created_ = true;

  blink_widget_->GetWidgetInputHandler(
      widget_input_handler_.BindNewPipeAndPassReceiver(
          content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput})),
      input_router_->BindNewHost(
          content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput})));
  if (for_frame_widget) {
    widget_input_handler_->GetFrameWidgetInputHandler(
        frame_widget_input_handler_.BindNewEndpointAndPassReceiver(
            content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput})));
    blink_frame_widget_->BindInputTargetClient(
        input_target_client_.BindNewPipeAndPassReceiver(
            content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput})));
  }

  // TODO(crbug.com/1161585): The `view_` can be null. :( Speculative
  // RenderViews along with the main frame and its widget before the
  // RenderWidgetHostView is created. Normally the RenderWidgetHostView should
  // come first. Historically, unit tests also set things up in the wrong order
  // and could get here with a null, but that is no longer the case (hopefully
  // that remains true).
  if (view_) {
    view_->OnRendererWidgetCreated();
  }

  // These two methods avoid running until `renderer_widget_created_` is true,
  // so we run them here after we set it.
  SendScreenRects();
  SynchronizeVisualProperties();
}

void RenderWidgetHostImpl::Init() {
  DCHECK(renderer_widget_created_);
  DCHECK(waiting_for_init_);

  waiting_for_init_ = false;

  // These two methods avoid running while we are `waiting_for_init_`, so we
  // run them here after we clear it.
  SendScreenRects();
  SynchronizeVisualProperties();
  // Show/Hide state is not given to the renderer while we are
  // `waiting_for_init_`, but Init() signals that the renderer is ready to
  // receive them. This call will inform the renderer that the widget is shown.
  if (pending_show_params_) {
    DCHECK(blink_widget_.is_bound());
    blink_widget_->WasShown(
        pending_show_params_->is_evicted,
        std::move(pending_show_params_->visible_time_request));
    pending_show_params_.reset();
  }
}

bool RenderWidgetHostImpl::ShouldShowStaleContentOnEviction() {
  return delegate_ && delegate_->ShouldShowStaleContentOnEviction();
}

void RenderWidgetHostImpl::ShutdownAndDestroyWidget(bool also_delete) {
  CancelKeyboardLock();
  RejectMouseLockOrUnlockIfNecessary(
      blink::mojom::PointerLockResult::kElementDestroyed);
  Destroy(also_delete);
}

void RenderWidgetHostImpl::SetIsLoading(bool is_loading) {
  power_mode_loading_voter_->VoteFor(is_loading
                                         ? power_scheduler::PowerMode::kLoading
                                         : power_scheduler::PowerMode::kIdle);
  if (view_) {
    view_->SetIsLoading(is_loading);
  }
}

void RenderWidgetHostImpl::WasHidden() {
  if (is_hidden_) {
    return;
  }

  RejectMouseLockOrUnlockIfNecessary(
      blink::mojom::PointerLockResult::kWrongDocument);

  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::WasHidden");
  is_hidden_ = true;

  // Unthrottle SynchronizeVisualProperties IPCs so that the first call after
  // show goes through immediately.
  visual_properties_ack_pending_ = false;

  // Don't bother reporting hung state when we aren't active.
  StopInputEventAckTimeout();

  // Show/Hide state is not sent to the renderer when it has requested for us to
  // wait until it requests them via Init().
  if (pending_show_params_) {
    pending_show_params_.reset();
  } else {
    // Widgets start out hidden, so we must have previously been shown to get
    // here, and we'd have a `pending_show_params_` if we are
    // `waiting_for_init_`.
    DCHECK(!waiting_for_init_);
    blink_widget_->WasHidden();
  }

  // Tell the RenderProcessHost we were hidden.
  GetProcess()->UpdateClientPriority(this);

  bool is_visible = false;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this), Details<bool>(&is_visible));
  for (auto& observer : observers_) {
    observer.RenderWidgetHostVisibilityChanged(this, false);
  }
}

void RenderWidgetHostImpl::WasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr
        record_tab_switch_time_request) {
  if (!is_hidden_) {
    return;
  }

  TRACE_EVENT_WITH_FLOW0("renderer_host", "RenderWidgetHostImpl::WasShown",
                         routing_id_, TRACE_EVENT_FLAG_FLOW_OUT);
  is_hidden_ = false;

  // If we navigated in background, clear the displayed graphics of the
  // previous page before going visible.
  // TODO(crbug.com/1396336): Checking if there is a content rendering timeout
  // running isn't ideal for seeing if the tab navigated in the background.
  ForceFirstFrameAfterNavigationTimeout();
  RestartInputEventAckTimeoutIfNecessary();

  // This methods avoids running when the widget is hidden, so we run it here
  // once it is no longer hidden.
  SendScreenRects();
  // SendScreenRects() and SynchronizeVisualProperties() should happen
  // together as one message, but we send them back-to-back for now so that
  // all state gets to the renderer as close together as possible.
  SynchronizeVisualProperties();

  DCHECK(!pending_show_params_);
  if (!waiting_for_init_) {
    blink_widget_->WasShown(view_->is_evicted(),
                            std::move(record_tab_switch_time_request));
  } else {
    // Delay the WasShown message until Init is called.
    pending_show_params_.emplace(view_->is_evicted(),
                                 std::move(record_tab_switch_time_request));
  }
  view_->reset_is_evicted();

  GetProcess()->UpdateClientPriority(this);

  bool is_visible = true;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this), Details<bool>(&is_visible));
  for (auto& observer : observers_) {
    observer.RenderWidgetHostVisibilityChanged(this, true);
  }

  // It's possible for our size to be out of sync with the renderer. The
  // following is one case that leads to this:
  // 1. SynchronizeVisualProperties -> Send
  // WidgetMsg_SynchronizeVisualProperties
  //    to render.
  // 2. SynchronizeVisualProperties -> do nothing as
  //    sync_visual_props_ack_pending_ is true
  // 3. WasHidden
  // By invoking SynchronizeVisualProperties the renderer is updated as
  // necessary. SynchronizeVisualProperties does nothing if the sizes are
  // already in sync.
  //
  // TODO: ideally blink::mojom::Widget's WasShown would take a size. This way,
  // the renderer could handle both the restore and resize at once. This isn't
  // that big a deal as RenderWidget::WasShown delays updating, so that the
  // resize from SynchronizeVisualProperties is usually processed before the
  // renderer is painted.
  SynchronizeVisualProperties();
}

void RenderWidgetHostImpl::RequestSuccessfulPresentationTimeForNextFrame(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  DCHECK(!is_hidden_);
  DCHECK(visible_time_request);
  if (waiting_for_init_) {
    // This method should only be called if the RWHI is already visible, meaning
    // there will be a WasShown call that's queued until init. Update that with
    // the new request.
    DCHECK(pending_show_params_);
    pending_show_params_->visible_time_request =
        std::move(visible_time_request);
    return;
  }
  DCHECK(!pending_show_params_);
  blink_widget_->RequestSuccessfulPresentationTimeForNextFrame(
      std::move(visible_time_request));
}

void RenderWidgetHostImpl::CancelSuccessfulPresentationTimeRequest() {
  DCHECK(!is_hidden_);
  if (waiting_for_init_) {
    // This method should only be called if the RWHI is already visible, meaning
    // there will be a WasShown call that's queued until init. Update that to
    // clear any request that was set.
    DCHECK(pending_show_params_);
    pending_show_params_->visible_time_request = nullptr;
    return;
  }
  DCHECK(!pending_show_params_);
  blink_widget_->CancelSuccessfulPresentationTimeRequest();
}

#if BUILDFLAG(IS_ANDROID)
void RenderWidgetHostImpl::SetImportance(ChildProcessImportance importance) {
  if (importance_ == importance) {
    return;
  }
  importance_ = importance;
  GetProcess()->UpdateClientPriority(this);
}

void RenderWidgetHostImpl::AddImeInputEventObserver(
    RenderWidgetHost::InputEventObserver* observer) {
  if (!ime_input_event_observers_.HasObserver(observer)) {
    ime_input_event_observers_.AddObserver(observer);
  }
}

void RenderWidgetHostImpl::RemoveImeInputEventObserver(
    RenderWidgetHost::InputEventObserver* observer) {
  ime_input_event_observers_.RemoveObserver(observer);
}
#endif

blink::VisualProperties RenderWidgetHostImpl::GetInitialVisualProperties() {
  blink::VisualProperties initial_props = GetVisualProperties();

  // A RenderWidget being created in the renderer means the browser should
  // reset any state that may be set for the previous RenderWidget but which
  // will be incorrect with a fresh RenderWidget.
  ResetStateForCreatedRenderWidget(initial_props);

  return initial_props;
}

blink::VisualProperties RenderWidgetHostImpl::GetVisualProperties() {
  // This is only called while the RenderWidgetHost is attached to a delegate
  // still.
  DCHECK(delegate_);
  // When the renderer process is gone, there's no need for VisualProperties
  // which are to be sent to the renderer process.
  DCHECK(view_);

  // Differentiate between widgets for frames vs widgets for popups/pepper.
  // Historically this was done by finding the RenderViewHost for the widget,
  // but a child local root would not convert to a RenderViewHost but is for a
  // frame.
  const bool is_frame_widget = !self_owned_;

  blink::VisualProperties visual_properties;
  visual_properties.screen_infos = GetScreenInfos();
  auto& current_screen_info = visual_properties.screen_infos.mutable_current();

  // For testing, override the raster color profile.
  // Note: this needs to be done here and not earlier in the pipeline because
  // Mac uses the display color space to update an NSSurface and this setting
  // is only for "raster" color space.
  if (display::Display::HasForceRasterColorProfile()) {
    for (auto& screen_info : visual_properties.screen_infos.screen_infos) {
      screen_info.display_color_spaces = gfx::DisplayColorSpaces(
          display::Display::GetForcedRasterColorProfile());
    }
  }

  visual_properties.is_fullscreen_granted = delegate_->IsFullscreen();

  if (is_frame_widget) {
    visual_properties.display_mode = delegate_->GetDisplayMode();
  } else {
    visual_properties.display_mode = blink::mojom::DisplayMode::kBrowser;
  }
  visual_properties.zoom_level = delegate_->GetPendingPageZoomLevel();

  RenderViewHostDelegateView* rvh_delegate_view = delegate_->GetDelegateView();
  DCHECK(rvh_delegate_view);

  visual_properties.browser_controls_params.browser_controls_shrink_blink_size =
      rvh_delegate_view->DoBrowserControlsShrinkRendererSize();
  visual_properties.browser_controls_params
      .animate_browser_controls_height_changes =
      rvh_delegate_view->ShouldAnimateBrowserControlsHeightChanges();
  visual_properties.browser_controls_params
      .only_expand_top_controls_at_page_top =
      rvh_delegate_view->OnlyExpandTopControlsAtPageTop();

  visual_properties.browser_controls_params.top_controls_height =
      rvh_delegate_view->GetTopControlsHeight();
  visual_properties.browser_controls_params.top_controls_min_height =
      rvh_delegate_view->GetTopControlsMinHeight();
  visual_properties.browser_controls_params.bottom_controls_height =
      rvh_delegate_view->GetBottomControlsHeight();
  visual_properties.browser_controls_params.bottom_controls_min_height =
      rvh_delegate_view->GetBottomControlsMinHeight();

  visual_properties.auto_resize_enabled = auto_resize_enabled_;
  visual_properties.min_size_for_auto_resize = min_size_for_auto_resize_;
  visual_properties.max_size_for_auto_resize = max_size_for_auto_resize_;

  visual_properties.new_size = view_->GetRequestedRendererSize();

  // While in fullscreen, provide the view size as a ScreenInfo size override.
  // This lets `window.screen` provide viewport dimensions while the frame is
  // fullscreen as a speculative site compatibility measure, because web authors
  // may assume that screen dimensions match window.innerWidth/innerHeight while
  // a page is fullscreen, but that is not always true. crbug.com/1367416
  if (visual_properties.is_fullscreen_granted &&
      !base::FeatureList::IsEnabled(
          blink::features::kFullscreenScreenSizeMatchesDisplay)) {
    current_screen_info.size_override = visual_properties.new_size;
  }

  // This widget is for a frame that is the main frame of the outermost frame
  // tree. That makes it the top-most frame. OR this is a non-frame widget.
  const bool is_top_most_widget = !view_->IsRenderWidgetHostViewChildFrame();
  // This widget is for a frame, but not the main frame of its frame tree.
  const bool is_child_frame_widget =
      view_->IsRenderWidgetHostViewChildFrame() && !owner_delegate_;

  // These properties come from the main frame RenderWidget and flow down the
  // tree of RenderWidgets. Some properties are global across all nested
  // WebContents/frame trees. Some properties are global only within their
  // WebContents/frame tree.
  //
  // Each child frame RenderWidgetHost that inherits values gets them from their
  // parent RenderWidget in the renderer process. It then passes them along to
  // its own RenderWidget, and the process repeats down the tree.
  //
  // The plumbing goes:
  // 1. Browser:    parent RenderWidgetHost
  // 2. IPC           -> blink::mojom::Widget::UpdateVisualProperties
  // 3. Renderer A: parent RenderWidget
  //                  (sometimes blink involved)
  // 4. Renderer A: child  blink::RemoteFrame
  // 5. IPC           -> FrameHostMsg_SynchronizeVisualProperties
  // 6. Browser:    child  CrossProcessFrameConnector
  // 7. Browser:    parent RenderWidgetHost (We're here if |is_child_frame|.)
  // 8. IPC           -> blink::mojom::Widget::UpdateVisualProperties
  // 9. Renderer B: child  RenderWidget

  // This property comes from the top-level main frame.
  if (is_top_most_widget) {
    visual_properties.compositor_viewport_pixel_rect =
        gfx::Rect(view_->GetCompositorViewportPixelSize());
    visual_properties.window_controls_overlay_rect =
        delegate_->GetWindowsControlsOverlayRect();
    visual_properties.virtual_keyboard_resize_height_physical_px =
        delegate_->GetVirtualKeyboardResizeHeight();
  } else {
    visual_properties.compositor_viewport_pixel_rect =
        properties_from_parent_local_root_.compositor_viewport;
  }

  // These properties come from the top-level main frame's renderer. The
  // top-level main frame in the browser doesn't specify a value.
  if (!is_top_most_widget) {
    visual_properties.page_scale_factor =
        properties_from_parent_local_root_.page_scale_factor;
    visual_properties.is_pinch_gesture_active =
        properties_from_parent_local_root_.is_pinch_gesture_active;
  }

  visual_properties.compositing_scale_factor =
      properties_from_parent_local_root_.compositing_scale_factor;

  // The |visible_viewport_size| is affected by auto-resize which is magical and
  // tricky.
  //
  // For the top-level main frame, auto resize ends up asynchronously resizing
  // the widget's RenderWidgetHostView and the size will show up there, so
  // nothing needs to be written in here.
  //
  // For nested main frames, auto resize happens in the renderer so we need to
  // store the size on this class and use that. When auto-resize is not enabled
  // we use the size of the nested main frame's RenderWidgetHostView.
  //
  // For child frames, we always use the value provided from the parent.
  //
  // For non-frame widgets, there is no auto-resize and we behave like the top-
  // level main frame.
  gfx::Size viewport;
  if (is_child_frame_widget) {
    viewport = properties_from_parent_local_root_.visible_viewport_size;
  } else {
    viewport = view_->GetVisibleViewportSize();
  }
  visual_properties.visible_viewport_size = viewport;

  // The root widget's window segments are computed here - child frames just
  // use the value provided from the parent.
  if (is_top_most_widget) {
    absl::optional<DisplayFeature> display_feature = view_->GetDisplayFeature();
    if (display_feature) {
      visual_properties.root_widget_window_segments =
          display_feature->ComputeWindowSegments(
              visual_properties.visible_viewport_size);
    } else {
      visual_properties.root_widget_window_segments = {
          gfx::Rect(visual_properties.visible_viewport_size)};
    }
  } else {
    visual_properties.root_widget_window_segments =
        properties_from_parent_local_root_.root_widget_window_segments;
  }

  visual_properties.capture_sequence_number = view_->GetCaptureSequenceNumber();

  // TODO(ccameron): GetLocalSurfaceId is not synchronized with the device
  // scale factor of the surface. Fix this.
  viz::LocalSurfaceId local_surface_id = view_->GetLocalSurfaceId();
  if (local_surface_id.is_valid()) {
    visual_properties.local_surface_id = local_surface_id;
  }

  if (screen_orientation_type_for_testing_) {
    current_screen_info.orientation_type =
        *screen_orientation_type_for_testing_;
  }

  if (screen_orientation_angle_for_testing_) {
    current_screen_info.orientation_angle =
        *screen_orientation_angle_for_testing_;
  }

  return visual_properties;
}

bool RenderWidgetHostImpl::UpdateVisualProperties(bool propagate) {
  return SynchronizeVisualProperties(false, propagate);
}

bool RenderWidgetHostImpl::SynchronizeVisualProperties() {
  return SynchronizeVisualProperties(false, true);
}

bool RenderWidgetHostImpl::SynchronizeVisualPropertiesIgnoringPendingAck() {
  visual_properties_ack_pending_ = false;
  return SynchronizeVisualProperties();
}

bool RenderWidgetHostImpl::SynchronizeVisualProperties(
    bool scroll_focused_node_into_view,
    bool propagate) {
  // If the RenderViewHost is inactive, then there is no RenderWidget that can
  // receive visual properties yet, even though we are setting them on the
  // browser side. Wait until there is a local main frame with a RenderWidget
  // to receive these before sending the visual properties.
  //
  // When the RenderViewHost becomes active, a SynchronizeVisualProperties()
  // call does not explicitly get made. That is because RenderWidgets for frames
  // are created and initialized with a valid VisualProperties already, and once
  // their initial navigation completes (and they are in the foreground) the
  // RenderWidget will be shown, which means a VisualProperties update happens
  // at the time where compositing begins.
  //
  // Note that this drops |scroll_focused_node_into_view| but this value does
  // not make sense for an inactive RenderViewHost's top level RenderWidgetHost,
  // because there is no frames associated with the RenderWidget when it is
  // inactive, so there is no focused node, or anything to scroll and display.
  if (owner_delegate_ && !owner_delegate_->IsMainFrameActive()) {
    return false;
  }
  // Sending VisualProperties are deferred until we have a connection to a
  // renderer-side Widget to send them to. Further, if we're waiting for the
  // renderer to show (aka Init()) the widget then we defer sending updates
  // until the renderer is ready.
  if (!renderer_widget_created_ || waiting_for_init_) {
    return false;
  }
  // TODO(danakj): The `renderer_widget_created_` flag is set to true for
  // widgets owned by inactive RenderViewHosts, even though there is no widget
  // created. In that case the `view_` will not be created.
  if (!view_) {
    return false;
  }
  // Throttle to one update at a time.
  if (visual_properties_ack_pending_) {
    return false;
  }

  // Skip if the |delegate_| has already been detached because it's web contents
  // is being deleted, or if LocalSurfaceId is suppressed, as we are
  // first updating our internal state from a child's request, before
  // subsequently merging ids to send.
  if (!GetProcess()->IsInitializedAndNotDead() || !view_->HasSize() ||
      !delegate_ || surface_id_allocation_suppressed_ ||
      !view_->CanSynchronizeVisualProperties()) {
    return false;
  }

  auto visual_properties = std::make_unique<blink::VisualProperties>();
  *visual_properties = GetVisualProperties();
  if (!StoredVisualPropertiesNeedsUpdate(old_visual_properties_,
                                         *visual_properties)) {
    return false;
  }

  visual_properties->scroll_focused_node_into_view =
      scroll_focused_node_into_view;

  if (propagate) {
    blink_widget_->UpdateVisualProperties(*visual_properties);
  }

  bool width_changed =
      !old_visual_properties_ || old_visual_properties_->new_size.width() !=
                                     visual_properties->new_size.width();

  // WidgetBase::UpdateSurfaceAndScreenInfo uses similar logic to detect
  // orientation changes on the display currently showing the widget.
  // TODO(lanwei): clean the duplicate code.
  if (visual_properties && old_visual_properties_) {
    const auto& old_screen_info =
        old_visual_properties_->screen_infos.current();
    const auto& screen_info = visual_properties->screen_infos.current();
    bool orientation_changed =
        old_screen_info.orientation_angle != screen_info.orientation_angle ||
        old_screen_info.orientation_type != screen_info.orientation_type;
    if (orientation_changed) {
      delegate_->DidChangeScreenOrientation();
    }
  }

  input_router_->SetDeviceScaleFactor(
      visual_properties->screen_infos.current().device_scale_factor);

  // If we do not have a valid viz::LocalSurfaceId then we are a child frame
  // waiting on the id to be propagated from our parent. We cannot create a hash
  // for tracing of an invalid id.
  //
  // TODO(jonross): Untangle startup so that we don't have this invalid partial
  // state. (https://crbug.com/1185286) (https://crbug.com/419087)
  if (visual_properties->local_surface_id.has_value()) {
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "RenderWidgetHostImpl::SynchronizeVisualProperties send message",
        visual_properties->local_surface_id->submission_trace_id(),
        TRACE_EVENT_FLAG_FLOW_OUT, "message",
        "WidgetMsg_SynchronizeVisualProperties", "local_surface_id",
        visual_properties->local_surface_id->ToString());
  }
  visual_properties_ack_pending_ =
      DoesVisualPropertiesNeedAck(old_visual_properties_, *visual_properties);
  old_visual_properties_ = std::move(visual_properties);

  // Warning: |visual_properties| invalid after this point.

  if (delegate_) {
    delegate_->RenderWidgetWasResized(this, width_changed);
  }

  return true;
}

void RenderWidgetHostImpl::GotFocus() {
  Focus();
  if (owner_delegate_) {
    owner_delegate_->RenderWidgetGotFocus();
  }
}

void RenderWidgetHostImpl::LostFocus() {
  Blur();
  if (owner_delegate_) {
    owner_delegate_->RenderWidgetLostFocus();
  }
}

void RenderWidgetHostImpl::Focus() {
  // TODO(crbug.com/689777): This sends it to the main frame RenderWidgetHost
  // should it be going to the local root instead?
  RenderWidgetHostImpl* focused_widget =
      delegate_ ? delegate_->GetRenderWidgetHostWithPageFocus() : nullptr;

  if (!focused_widget) {
    focused_widget = this;
  }
  focused_widget->SetPageFocus(true);
}

void RenderWidgetHostImpl::Blur() {
  // TODO(crbug.com/689777): This sends it to the main frame RenderWidgetHost
  // should it be going to the local root instead?
  RenderWidgetHostImpl* focused_widget =
      delegate_ ? delegate_->GetRenderWidgetHostWithPageFocus() : nullptr;

  if (!focused_widget) {
    focused_widget = this;
  }
  focused_widget->SetPageFocus(false);
}

void RenderWidgetHostImpl::FlushForTesting() {
  if (widget_input_handler_) {
    return widget_input_handler_.FlushForTesting();
  }
}

void RenderWidgetHostImpl::SetPageFocus(bool focused) {
  OPTIONAL_TRACE_EVENT1("content", "RenderWidgetHostImpl::SetPageFocus",
                        "is_focused", focused);
  is_focused_ = focused;

  // If focused state is being set is_active must be true. Android does
  // not call SetActive so if we are trying to focus ensure `is_active`
  // is true.
  if (focused) {
    is_active_ = true;
  }

  // Portals should never get page focus.
  DCHECK(!delegate_ || !delegate_->IsPortal() || !focused);

  if (!focused) {
    // If there is a pending mouse lock request, we don't want to reject it at
    // this point. The user can switch focus back to this view and approve the
    // request later.
    if (IsMouseLocked()) {
      view_->UnlockMouse();
    }

    if (IsKeyboardLocked()) {
      UnlockKeyboard();
    }

    if (auto* touch_emulator = GetExistingTouchEmulator()) {
      touch_emulator->CancelTouch();
    }
  } else if (keyboard_lock_allowed_) {
    LockKeyboard();
  }

  blink::mojom::FocusState focus_state =
      blink::mojom::FocusState::kNotFocusedAndNotActive;
  if (focused) {
    focus_state = blink::mojom::FocusState::kFocused;
  } else if (is_active_) {
    focus_state = blink::mojom::FocusState::kNotFocusedAndActive;
  }

  GetWidgetInputHandler()->SetFocus(focus_state);

  // Also send page-level focus state to other SiteInstances involved in
  // rendering the current FrameTree, if this widget is for a main frame.
  // TODO(crbug.com/689777): We should be telling `frame_tree_` which
  // RenderWidgetHost was focused (if we send it to the focused one instead
  // of the main frame in order to order it correctly with other input events),
  // so that `frame_tree_` can propagate it to all other WebViews based on
  // where this RenderWidgetHost lives.
  if (owner_delegate_ && frame_tree_) {
    frame_tree_->ReplicatePageFocus(focused);
  }
}

void RenderWidgetHostImpl::LostCapture() {
  if (auto* touch_emulator = GetExistingTouchEmulator()) {
    touch_emulator->CancelTouch();
  }

  GetWidgetInputHandler()->MouseCaptureLost();
}

void RenderWidgetHostImpl::SetActive(bool active) {
  is_active_ = active;
  if (blink_frame_widget_) {
    blink_frame_widget_->SetActive(active);
  }
}

void RenderWidgetHostImpl::LostMouseLock() {
  if (delegate_) {
    delegate_->LostMouseLock(this);
  }
}

void RenderWidgetHostImpl::SendMouseLockLost() {
  mouse_lock_context_.reset();
}

void RenderWidgetHostImpl::ViewDestroyed() {
  CancelKeyboardLock();
  RejectMouseLockOrUnlockIfNecessary(
      blink::mojom::PointerLockResult::kElementDestroyed);

  // TODO(evanm): tracking this may no longer be necessary;
  // eliminate this function if so.
  SetView(nullptr);
}

bool RenderWidgetHostImpl::RequestRepaintForTesting() {
  if (!view_) {
    return false;
  }

  return view_->RequestRepaintForTesting();
}

void RenderWidgetHostImpl::RenderProcessBlockedStateChanged(bool blocked) {
  if (blocked) {
    StopInputEventAckTimeout();
  } else {
    RestartInputEventAckTimeoutIfNecessary();
  }
}

void RenderWidgetHostImpl::StartInputEventAckTimeout() {
  if (should_disable_hang_monitor_) {
    return;
  }

  if (!input_event_ack_timeout_.IsRunning()) {
    input_event_ack_timeout_.Start(
        FROM_HERE, hung_renderer_delay_,
        base::BindOnce(&RenderWidgetHostImpl::OnInputEventAckTimeout,
                       weak_factory_.GetWeakPtr()));
  }
}

void RenderWidgetHostImpl::RestartInputEventAckTimeoutIfNecessary() {
  if (!GetProcess()->IsBlocked() && !should_disable_hang_monitor_ &&
      in_flight_event_count_ > 0 && !is_hidden_) {
    input_event_ack_timeout_.Start(
        FROM_HERE, hung_renderer_delay_,
        base::BindOnce(&RenderWidgetHostImpl::OnInputEventAckTimeout,
                       weak_factory_.GetWeakPtr()));
  }
}

bool RenderWidgetHostImpl::IsCurrentlyUnresponsive() {
  return is_unresponsive_;
}

void RenderWidgetHostImpl::StopInputEventAckTimeout() {
  input_event_ack_timeout_.Stop();
  RendererIsResponsive();
}

void RenderWidgetHostImpl::DidNavigate() {
  // Stop the flinging after navigating to a new page.
  StopFling();

  // Resize messages before navigation are not acked, so reset
  // |visual_properties_ack_pending_| and make sure the next resize will be
  // acked if the last resize before navigation was supposed to be acked.
  visual_properties_ack_pending_ = false;
  if (view_) {
    view_->DidNavigate();
  }

  ClearPendingUserActivation();
}

void RenderWidgetHostImpl::StartNewContentRenderingTimeout() {
  if (!new_content_rendering_timeout_) {
    return;
  }
  new_content_rendering_timeout_->Start(new_content_rendering_delay_);
}

void RenderWidgetHostImpl::ForwardMouseEvent(const WebMouseEvent& mouse_event) {
  ForwardMouseEventWithLatencyInfo(mouse_event,
                                   ui::LatencyInfo(ui::SourceEventType::MOUSE));
  if (owner_delegate_) {
    owner_delegate_->RenderWidgetDidForwardMouseEvent(mouse_event);
  }
}

void RenderWidgetHostImpl::ForwardMouseEventWithLatencyInfo(
    const blink::WebMouseEvent& mouse_event,
    const ui::LatencyInfo& latency) {
  TRACE_EVENT2("input", "RenderWidgetHostImpl::ForwardMouseEvent", "x",
               mouse_event.PositionInWidget().x(), "y",
               mouse_event.PositionInWidget().y());

  DCHECK_GE(mouse_event.GetType(), blink::WebInputEvent::Type::kMouseTypeFirst);
  DCHECK_LE(mouse_event.GetType(), blink::WebInputEvent::Type::kMouseTypeLast);

  // This is used to auto-disable accessibility if we detect user input
  // but no accessibility API usage.
  if (mouse_event.GetType() == blink::WebInputEvent::Type::kMouseDown) {
    BrowserAccessibilityStateImpl::GetInstance()->OnUserInputEvent();
  }

  for (auto& mouse_event_callback : mouse_event_callbacks_) {
    if (mouse_event_callback.Run(mouse_event)) {
      return;
    }
  }

  if (IsIgnoringInputEvents()) {
    return;
  }

  auto* touch_emulator = GetExistingTouchEmulator();
  if (touch_emulator &&
      touch_emulator->HandleMouseEvent(mouse_event, GetView())) {
    return;
  }

  MouseEventWithLatencyInfo mouse_with_latency(mouse_event, latency);
  DispatchInputEventWithLatencyInfo(
      mouse_with_latency.event, &mouse_with_latency.latency,
      &mouse_with_latency.event.GetModifiableEventLatencyMetadata());
  input_router_->SendMouseEvent(
      mouse_with_latency, base::BindOnce(&RenderWidgetHostImpl::OnMouseEventAck,
                                         weak_factory_.GetWeakPtr()));
}

void RenderWidgetHostImpl::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  ForwardWheelEventWithLatencyInfo(wheel_event,
                                   ui::LatencyInfo(ui::SourceEventType::WHEEL));
}

void RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo(
    const blink::WebMouseWheelEvent& wheel_event,
    const ui::LatencyInfo& latency) {
  TRACE_EVENT2("input", "RenderWidgetHostImpl::ForwardWheelEvent", "dx",
               wheel_event.delta_x, "dy", wheel_event.delta_y);

  if (IsIgnoringInputEvents()) {
    return;
  }

  auto* touch_emulator = GetExistingTouchEmulator();
  if (touch_emulator && touch_emulator->HandleMouseWheelEvent(wheel_event)) {
    return;
  }

  MouseWheelEventWithLatencyInfo wheel_with_latency(wheel_event, latency);
  DispatchInputEventWithLatencyInfo(
      wheel_with_latency.event, &wheel_with_latency.latency,
      &wheel_with_latency.event.GetModifiableEventLatencyMetadata());
  input_router_->SendWheelEvent(wheel_with_latency);
}

void RenderWidgetHostImpl::WaitForInputProcessed(
    SyntheticGestureParams::GestureType type,
    content::mojom::GestureSourceType source,
    base::OnceClosure callback) {
  // TODO(bokan): Input can be queued and delayed in InputRouterImpl based on
  // the kind of events we're getting. To be truly robust, we should wait until
  // those queues are flushed before issuing this message. This will be done in
  // a follow-up and is the reason for the currently unused type and source
  // params. https://crbug.com/902446.
  WaitForInputProcessed(std::move(callback));
}

void RenderWidgetHostImpl::WaitForInputProcessed(base::OnceClosure callback) {
  // TODO(bokan): The RequestPresentationCallback mechanism doesn't seem to
  // work in OOPIFs. For now, just callback immediately. Remove when fixed.
  // https://crbug.com/924646.
  if (GetView()->IsRenderWidgetHostViewChildFrame()) {
    std::move(callback).Run();
    return;
  }

  input_router_->WaitForInputProcessed(std::move(callback));
}

void RenderWidgetHostImpl::ForwardGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  ForwardGestureEventWithLatencyInfo(
      gesture_event,
      ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(
          gesture_event));
}

void RenderWidgetHostImpl::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency) {
  TRACE_EVENT1("input", "RenderWidgetHostImpl::ForwardGestureEvent", "type",
               WebInputEvent::GetName(gesture_event.GetType()));

  // This is used to auto-disable accessibility if we detect user input
  // but no accessibility API usage.
  if (gesture_event.GetType() == blink::WebInputEvent::Type::kGestureTapDown) {
    BrowserAccessibilityStateImpl::GetInstance()->OnUserInputEvent();
  }

  // Early out if necessary, prior to performing latency logic.
  if (IsIgnoringInputEvents()) {
    return;
  }

  // The gesture events must have a known source.
  DCHECK_NE(gesture_event.SourceDevice(),
            blink::WebGestureDevice::kUninitialized);

  if (gesture_event.GetType() ==
      blink::WebInputEvent::Type::kGestureScrollBegin) {
    DCHECK(
        !is_in_gesture_scroll_[static_cast<int>(gesture_event.SourceDevice())]);
    is_in_gesture_scroll_[static_cast<int>(gesture_event.SourceDevice())] =
        true;
    NotifyUISchedulerOfScrollStateUpdate(
        BrowserUIThreadScheduler::ScrollState::kGestureScrollActive);
    scroll_peak_gpu_mem_tracker_ =
        PeakGpuMemoryTracker::Create(PeakGpuMemoryTracker::Usage::SCROLL);
  } else if (gesture_event.GetType() ==
             blink::WebInputEvent::Type::kGestureScrollEnd) {
    DCHECK(
        is_in_gesture_scroll_[static_cast<int>(gesture_event.SourceDevice())]);
    is_in_gesture_scroll_[static_cast<int>(gesture_event.SourceDevice())] =
        false;
    NotifyUISchedulerOfScrollStateUpdate(
        BrowserUIThreadScheduler::ScrollState::kNone);
    is_in_touchpad_gesture_fling_ = false;
    if (view_) {
      if (scroll_peak_gpu_mem_tracker_ &&
          !view_->is_currently_scrolling_viewport()) {
        // We start tracking peak gpu-memory usage when the initial scroll-begin
        // is dispatched. However, it is possible that the scroll-begin did not
        // trigger any scrolls (e.g. the page is not scrollable). In such cases,
        // we do not want to report the peak-memory usage metric. So it is
        // canceled here.
        scroll_peak_gpu_mem_tracker_->Cancel();
      }

      view_->set_is_currently_scrolling_viewport(false);
    }
    scroll_peak_gpu_mem_tracker_ = nullptr;
  } else if (gesture_event.GetType() ==
             blink::WebInputEvent::Type::kGestureFlingStart) {
    NotifyUISchedulerOfScrollStateUpdate(
        BrowserUIThreadScheduler::ScrollState::kFlingActive);
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad) {
      // a GSB event is generated from the first wheel event in a sequence after
      // the event is acked as not consumed by the renderer. Sometimes when the
      // main thread is busy/slow (e.g ChromeOS debug builds) a GFS arrives
      // before the first wheel is acked. In these cases no GSB will arrive
      // before the GFS. With browser side fling the out of order GFS arrival
      // does not need a DCHECK since the fling controller will process the GFS
      // and start queuing wheel events which will follow the one currently
      // awaiting ACK and the renderer receives the events in order.

      is_in_touchpad_gesture_fling_ = true;
    } else {
      DCHECK(is_in_gesture_scroll_[static_cast<int>(
          gesture_event.SourceDevice())]);

      // The FlingController handles GFS with touchscreen source and sends GSU
      // events with inertial state to the renderer to progress the fling.
      // is_in_gesture_scroll must stay true till the fling progress is
      // finished. Then the FlingController will generate and send a GSE which
      // shows the end of a scroll sequence and resets is_in_gesture_scroll_.
    }
  }

  // Delegate must be non-null, due to |IsIgnoringInputEvents()| test.
  if (delegate_->PreHandleGestureEvent(gesture_event)) {
    return;
  }

  GestureEventWithLatencyInfo gesture_with_latency(gesture_event, latency);
  DispatchInputEventWithLatencyInfo(
      gesture_with_latency.event, &gesture_with_latency.latency,
      &gesture_with_latency.event.GetModifiableEventLatencyMetadata());
  input_router_->SendGestureEvent(gesture_with_latency);
}

void RenderWidgetHostImpl::ForwardTouchEventWithLatencyInfo(
    const blink::WebTouchEvent& touch_event,
    const ui::LatencyInfo& latency) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardTouchEvent");

  // This is used to auto-disable accessibility if we detect user input
  // but no accessibility API usage.
  if (touch_event.GetType() == blink::WebInputEvent::Type::kTouchStart) {
    BrowserAccessibilityStateImpl::GetInstance()->OnUserInputEvent();
  }

  // Always forward TouchEvents for touch stream consistency. They will be
  // ignored if appropriate in FilterInputEvent().

  TouchEventWithLatencyInfo touch_with_latency(touch_event, latency);
  DispatchInputEventWithLatencyInfo(
      touch_with_latency.event, &touch_with_latency.latency,
      &touch_with_latency.event.GetModifiableEventLatencyMetadata());
  input_router_->SendTouchEvent(touch_with_latency);
}

void RenderWidgetHostImpl::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  ui::LatencyInfo latency_info;

  if (key_event.GetType() == WebInputEvent::Type::kRawKeyDown ||
      key_event.GetType() == WebInputEvent::Type::kChar) {
    latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  }
  ForwardKeyboardEventWithLatencyInfo(key_event, latency_info);
}

void RenderWidgetHostImpl::ForwardKeyboardEventWithLatencyInfo(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency) {
  ForwardKeyboardEventWithCommands(
      key_event, latency, std::vector<blink::mojom::EditCommandPtr>(), nullptr);
}

void RenderWidgetHostImpl::ForwardKeyboardEventWithCommands(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency,
    std::vector<blink::mojom::EditCommandPtr> commands,
    bool* update_event) {
  DCHECK(WebInputEvent::IsKeyboardEventType(key_event.GetType()));

  // This is used to auto-disable accessibility if we detect user input
  // but no accessibility API usage.
  if (key_event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      key_event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
    BrowserAccessibilityStateImpl::GetInstance()->OnUserInputEvent();
  }

  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardKeyboardEvent");
  if (owner_delegate_ &&
      !owner_delegate_->MayRenderWidgetForwardKeyboardEvent(key_event)) {
    return;
  }

  if (IsIgnoringInputEvents()) {
    return;
  }

  if (!GetProcess()->IsInitializedAndNotDead()) {
    return;
  }

  // First, let keypress listeners take a shot at handling the event.  If a
  // listener handles the event, it should not be propagated to the renderer.
  if (KeyPressListenersHandleEvent(key_event)) {
    // Some keypresses that are accepted by the listener may be followed by Char
    // and KeyUp events, which should be ignored.
    if (key_event.GetType() == WebKeyboardEvent::Type::kRawKeyDown) {
      suppress_events_until_keydown_ = true;
    }
    return;
  }

  if (suppress_events_until_keydown_) {
    // If the preceding RawKeyDown event was handled by the browser, then we
    // need to suppress all events generated by it until the next RawKeyDown or
    // KeyDown event.
    if (key_event.GetType() == WebKeyboardEvent::Type::kKeyUp ||
        key_event.GetType() == WebKeyboardEvent::Type::kChar) {
      return;
    }
    DCHECK(key_event.GetType() == WebKeyboardEvent::Type::kRawKeyDown ||
           key_event.GetType() == WebKeyboardEvent::Type::kKeyDown);
    suppress_events_until_keydown_ = false;
  }

  bool is_shortcut = false;

  // Only pre-handle the key event if it's not handled by the input method.
  if (delegate_ && !key_event.skip_in_browser) {
    // We need to set |suppress_events_until_keydown_| to true if
    // PreHandleKeyboardEvent() handles the event, but |this| may already be
    // destroyed at that time. So set |suppress_events_until_keydown_| true
    // here, then revert it afterwards when necessary.
    if (key_event.GetType() == WebKeyboardEvent::Type::kRawKeyDown) {
      suppress_events_until_keydown_ = true;
    }

    // Tab switching/closing accelerators aren't sent to the renderer to avoid
    // a hung/malicious renderer from interfering.
    switch (delegate_->PreHandleKeyboardEvent(key_event)) {
      case KeyboardEventProcessingResult::HANDLED:
        return;
#if defined(USE_AURA)
      case KeyboardEventProcessingResult::HANDLED_DONT_UPDATE_EVENT:
        if (update_event) {
          *update_event = false;
        }
        return;
#endif
      case KeyboardEventProcessingResult::NOT_HANDLED:
        break;
      case KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT:
        is_shortcut = true;
        break;
    }

    if (key_event.GetType() == WebKeyboardEvent::Type::kRawKeyDown) {
      suppress_events_until_keydown_ = false;
    }
  }

  auto* touch_emulator = GetExistingTouchEmulator();
  if (touch_emulator && touch_emulator->HandleKeyboardEvent(key_event)) {
    return;
  }
  NativeWebKeyboardEventWithLatencyInfo key_event_with_latency(key_event,
                                                               latency);
  key_event_with_latency.event.is_browser_shortcut = is_shortcut;
  DispatchInputEventWithLatencyInfo(
      key_event_with_latency.event, &key_event_with_latency.latency,
      &key_event_with_latency.event.GetModifiableEventLatencyMetadata());
  // TODO(foolip): |InputRouter::SendKeyboardEvent()| may filter events, in
  // which the commands will be treated as belonging to the next key event.
  // WidgetInputHandler::SetEditCommandsForNextKeyEvent should only be sent if
  // WidgetInputHandler::DispatchEvent is, but has to be sent first.
  // https://crbug.com/684298
  if (!commands.empty()) {
    GetWidgetInputHandler()->SetEditCommandsForNextKeyEvent(
        std::move(commands));
  }

  input_router_->SendKeyboardEvent(
      key_event_with_latency,
      base::BindOnce(&RenderWidgetHostImpl::OnKeyboardEventAck,
                     weak_factory_.GetWeakPtr()));
}

void RenderWidgetHostImpl::CreateSyntheticGestureControllerIfNecessary() {
  if (!synthetic_gesture_controller_ && view_) {
    synthetic_gesture_controller_ =
        std::make_unique<SyntheticGestureController>(
            this, view_->CreateSyntheticGestureTarget());
  }
}

void RenderWidgetHostImpl::QueueSyntheticGesture(
    std::unique_ptr<SyntheticGesture> synthetic_gesture,
    base::OnceCallback<void(SyntheticGesture::Result)> on_complete) {
  CreateSyntheticGestureControllerIfNecessary();
  if (synthetic_gesture_controller_) {
    synthetic_gesture_controller_->QueueSyntheticGesture(
        std::move(synthetic_gesture), std::move(on_complete));
  }
}

void RenderWidgetHostImpl::QueueSyntheticGestureCompleteImmediately(
    std::unique_ptr<SyntheticGesture> synthetic_gesture) {
  CreateSyntheticGestureControllerIfNecessary();
  if (synthetic_gesture_controller_) {
    synthetic_gesture_controller_->QueueSyntheticGestureCompleteImmediately(
        std::move(synthetic_gesture));
  }
}

void RenderWidgetHostImpl::EnsureReadyForSyntheticGestures(
    base::OnceClosure on_ready) {
  CreateSyntheticGestureControllerIfNecessary();
  if (synthetic_gesture_controller_) {
    synthetic_gesture_controller_->EnsureRendererInitialized(
        std::move(on_ready));
  } else {
    // If we couldn't create a SyntheticGestureController then we won't ever be
    // ready.  Invoke the callback to unblock the calling code.
    std::move(on_ready).Run();
  }
}

void RenderWidgetHostImpl::TakeSyntheticGestureController(
    RenderWidgetHostImpl* host) {
  DCHECK(!synthetic_gesture_controller_);
  if (host->synthetic_gesture_controller_) {
    synthetic_gesture_controller_ =
        std::move(host->synthetic_gesture_controller_);
    synthetic_gesture_controller_->UpdateSyntheticGestureTarget(
        view_->CreateSyntheticGestureTarget(), this);
  }
}

void RenderWidgetHostImpl::OnCursorVisibilityStateChanged(bool is_visible) {
  GetWidgetInputHandler()->CursorVisibilityChanged(is_visible);
}

// static
void RenderWidgetHostImpl::DisableResizeAckCheckForTesting() {
  g_check_for_pending_visual_properties_ack = false;
}

void RenderWidgetHostImpl::AddKeyPressEventCallback(
    const KeyPressEventCallback& callback) {
  DCHECK(!base::Contains(key_press_event_callbacks_, callback));
  key_press_event_callbacks_.push_back(callback);
}

void RenderWidgetHostImpl::RemoveKeyPressEventCallback(
    const KeyPressEventCallback& callback) {
  base::Erase(key_press_event_callbacks_, callback);
}

void RenderWidgetHostImpl::AddMouseEventCallback(
    const MouseEventCallback& callback) {
  DCHECK(!base::Contains(mouse_event_callbacks_, callback));
  mouse_event_callbacks_.push_back(callback);
}

void RenderWidgetHostImpl::RemoveMouseEventCallback(
    const MouseEventCallback& callback) {
  base::Erase(mouse_event_callbacks_, callback);
}

void RenderWidgetHostImpl::AddSuppressShowingImeCallback(
    const SuppressShowingImeCallback& callback) {
  DCHECK(!base::Contains(suppress_showing_ime_callbacks_, callback));
  suppress_showing_ime_callbacks_.push_back(callback);
}

void RenderWidgetHostImpl::RemoveSuppressShowingImeCallback(
    const SuppressShowingImeCallback& callback) {
  base::Erase(suppress_showing_ime_callbacks_, callback);
}

void RenderWidgetHostImpl::AddInputEventObserver(
    RenderWidgetHost::InputEventObserver* observer) {
  if (!input_event_observers_.HasObserver(observer)) {
    input_event_observers_.AddObserver(observer);
  }
}

void RenderWidgetHostImpl::RemoveInputEventObserver(
    RenderWidgetHost::InputEventObserver* observer) {
  input_event_observers_.RemoveObserver(observer);
}

void RenderWidgetHostImpl::AddObserver(RenderWidgetHostObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderWidgetHostImpl::RemoveObserver(RenderWidgetHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

display::ScreenInfo RenderWidgetHostImpl::GetScreenInfo() const {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::GetScreenInfo");

  if (view_) {
    return view_->GetScreenInfo();
  }

  // If this widget has not been connected to a view yet (or has been
  // disconnected), the display code may be using a fake primary display.
  display::ScreenInfo screen_info;
  display::DisplayUtil::GetDefaultScreenInfo(&screen_info);
  return screen_info;
}

display::ScreenInfos RenderWidgetHostImpl::GetScreenInfos() const {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::GetScreenInfos");

  return view_ ? view_->GetScreenInfos()
               : display::ScreenInfos(GetScreenInfo());
}

float RenderWidgetHostImpl::GetDeviceScaleFactor() {
  return GetScaleFactorForView(view_.get());
}

absl::optional<cc::TouchAction> RenderWidgetHostImpl::GetAllowedTouchAction() {
  return input_router_->AllowedTouchAction();
}

void RenderWidgetHostImpl::WriteIntoTrace(perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("routing_id", GetRoutingID());
}

void RenderWidgetHostImpl::DragTargetDragEnter(
    const DropData& drop_data,
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    DragOperationsMask operations_allowed,
    int key_modifiers,
    base::OnceCallback<void(::ui::mojom::DragOperation)> callback) {
  DragTargetDragEnterWithMetaData(DropDataToMetaData(drop_data), client_pt,
                                  screen_pt, operations_allowed, key_modifiers,
                                  std::move(callback));
}

void RenderWidgetHostImpl::DragTargetDragEnterWithMetaData(
    const std::vector<DropData::Metadata>& metadata,
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    DragOperationsMask operations_allowed,
    int key_modifiers,
    base::OnceCallback<void(::ui::mojom::DragOperation)> callback) {
  // TODO(https://crbug.com/1102769): Replace with a for_frame() check.
  if (blink_frame_widget_) {
    base::OnceCallback<void(::ui::mojom::DragOperation)> callback_wrapper =
        base::BindOnce(&RenderWidgetHostImpl::OnUpdateDragCursor,
                       base::Unretained(this), std::move(callback));
    blink_frame_widget_->DragTargetDragEnter(
        DropMetaDataToDragData(metadata),
        ConvertWindowPointToViewport(client_pt), screen_pt, operations_allowed,
        key_modifiers, std::move(callback_wrapper));
  }
}

void RenderWidgetHostImpl::DragTargetDragOver(
    const gfx::PointF& client_point,
    const gfx::PointF& screen_point,
    DragOperationsMask operations_allowed,
    int key_modifiers,
    base::OnceCallback<void(ui::mojom::DragOperation)> callback) {
  // TODO(https://crbug.com/1102769): Replace with a for_frame() check.
  if (blink_frame_widget_) {
    blink_frame_widget_->DragTargetDragOver(
        ConvertWindowPointToViewport(client_point), screen_point,
        operations_allowed, key_modifiers,
        base::BindOnce(&RenderWidgetHostImpl::OnUpdateDragCursor,
                       base::Unretained(this), std::move(callback)));
  }
}

void RenderWidgetHostImpl::DragTargetDragLeave(
    const gfx::PointF& client_point,
    const gfx::PointF& screen_point) {
  // TODO(https://crbug.com/1102769): Replace with a for_frame() check.
  if (blink_frame_widget_) {
    blink_frame_widget_->DragTargetDragLeave(
        ConvertWindowPointToViewport(client_point), screen_point);
  }
}

void RenderWidgetHostImpl::DragTargetDrop(const DropData& drop_data,
                                          const gfx::PointF& client_point,
                                          const gfx::PointF& screen_point,
                                          int key_modifiers,
                                          base::OnceClosure callback) {
  // TODO(https://crbug.com/1102769): Replace with a for_frame() check.
  if (blink_frame_widget_) {
    DropData drop_data_with_permissions(drop_data);
    GrantFileAccessFromDropData(&drop_data_with_permissions);
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition());
    blink_frame_widget_->DragTargetDrop(
        DropDataToDragData(drop_data_with_permissions,
                           storage_partition->GetFileSystemAccessManager(),
                           GetProcess()->GetID(),
                           ChromeBlobStorageContext::GetFor(
                               GetProcess()->GetBrowserContext())),
        ConvertWindowPointToViewport(client_point), screen_point, key_modifiers,
        std::move(callback));
  }
}

void RenderWidgetHostImpl::DragSourceEndedAt(const gfx::PointF& client_point,
                                             const gfx::PointF& screen_point,
                                             ui::mojom::DragOperation operation,
                                             base::OnceClosure callback) {
  // TODO(https://crbug.com/1102769): Replace with a for_frame() check.
  if (!blink_frame_widget_) {
    return;
  }
  blink_frame_widget_->DragSourceEndedAt(
      ConvertWindowPointToViewport(client_point), screen_point, operation,
      std::move(callback));
  if (frame_tree_) {
    devtools_instrumentation::DragEnded(*frame_tree_->root());
  }
}

void RenderWidgetHostImpl::DragSourceSystemDragEnded() {
  // TODO(https://crbug.com/1102769): Replace with a for_frame() check.
  if (!blink_frame_widget_) {
    return;
  }
  blink_frame_widget_->DragSourceSystemDragEnded();
  if (frame_tree_) {
    devtools_instrumentation::DragEnded(*frame_tree_->root());
  }
}

void RenderWidgetHostImpl::FilterDropData(DropData* drop_data) {
#if DCHECK_IS_ON()
  drop_data->view_id = GetRoutingID();
#endif  // DCHECK_IS_ON()

  GetProcess()->FilterURL(true, &drop_data->url);
  if (drop_data->did_originate_from_renderer) {
    drop_data->filenames.clear();
  }
}

void RenderWidgetHostImpl::SetCursor(const ui::Cursor& cursor) {
  if (view_) {
    view_->UpdateCursor(cursor);
  }
}

void RenderWidgetHostImpl::ShowContextMenuAtPoint(
    const gfx::Point& point,
    const ui::MenuSourceType source_type) {
  if (blink_frame_widget_) {
    blink_frame_widget_->ShowContextMenu(source_type, point);
  }
}

void RenderWidgetHostImpl::InsertVisualStateCallback(
    VisualStateCallback callback) {
  if (!blink_frame_widget_) {
    std::move(callback).Run(false);
    return;
  }

  if (!widget_compositor_) {
    blink_frame_widget_->BindWidgetCompositor(
        widget_compositor_.BindNewPipeAndPassReceiver(
            content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput})));
  }

  widget_compositor_->VisualStateRequest(base::BindOnce(
      [](VisualStateCallback callback) { std::move(callback).Run(true); },
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false)));
}

RenderProcessHostPriorityClient::Priority RenderWidgetHostImpl::GetPriority() {
  RenderProcessHostPriorityClient::Priority priority = {
    is_hidden_,
    frame_depth_,
    intersects_viewport_,
#if BUILDFLAG(IS_ANDROID)
    importance_,
#endif
  };
  if (owner_delegate_ &&
      !owner_delegate_->ShouldContributePriorityToProcess()) {
    priority.is_hidden = true;
    priority.frame_depth = RenderProcessHostImpl::kMaxFrameDepthForPriority;
#if BUILDFLAG(IS_ANDROID)
    priority.importance = ChildProcessImportance::NORMAL;
#endif
  }
  return priority;
}

void RenderWidgetHostImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  CHECK(self_owned_);
  Destroy(/*also_delete=*/true);  // Delete |this|.
}

blink::mojom::WidgetInputHandler*
RenderWidgetHostImpl::GetWidgetInputHandler() {
  if (widget_input_handler_) {
    return widget_input_handler_.get();
  }
  // TODO(dtapuska): Remove the need for the unbound interface. It is
  // possible that a RVHI may make calls to a WidgetInputHandler when
  // the main frame is remote. This is because of ordering issues during
  // widget shutdown, so we present an UnboundWidgetInputHandler had
  // DLOGS the message calls.
  return g_unbound_input_handler.Pointer();
}

void RenderWidgetHostImpl::NotifyScreenInfoChanged() {
  // The resize message (which may not happen immediately) will carry with it
  // the screen info as well as the new size (if the screen has changed scale
  // factor). Force sending the new visual properties even if there is one in
  // flight to ensure proper IPC ordering for features like the Fullscreen API.
  SynchronizeVisualPropertiesIgnoringPendingAck();

  // The device scale factor will be same for all the views contained by the
  // primary main frame, so just set it once.
  if (delegate_ && !delegate_->IsWidgetForPrimaryMainFrame(this)) {
    return;
  }

  // The delegate may not have an input event router in tests.
  if (auto* touch_emulator = GetExistingTouchEmulator()) {
    touch_emulator->SetDeviceScaleFactor(GetScaleFactorForView(view_.get()));
  }
}

void RenderWidgetHostImpl::GetSnapshotFromBrowser(
    GetSnapshotFromBrowserCallback callback,
    bool from_surface) {
  int snapshot_id = next_browser_snapshot_id_++;
  if (from_surface) {
    pending_surface_browser_snapshots_.insert(
        std::make_pair(snapshot_id, std::move(callback)));
    RequestForceRedraw(snapshot_id);
    return;
  }

#if BUILDFLAG(IS_MAC)
  // MacOS version of underlying GrabViewSnapshot() blocks while
  // display/GPU are in a power-saving mode, so make sure display
  // does not go to sleep for the duration of reading a snapshot.
  if (pending_browser_snapshots_.empty()) {
    GetWakeLock()->RequestWakeLock();
  }
#endif
  // TODO(nzolghadr): Remove the duplication here and the if block just above.
  pending_browser_snapshots_.insert(
      std::make_pair(snapshot_id, std::move(callback)));
  RequestForceRedraw(snapshot_id);
}

void RenderWidgetHostImpl::SelectionChanged(const std::u16string& text,
                                            uint32_t offset,
                                            const gfx::Range& range) {
  if (view_) {
    view_->SelectionChanged(text, static_cast<size_t>(offset), range);
  }
}

void RenderWidgetHostImpl::SelectionBoundsChanged(
    const gfx::Rect& anchor_rect,
    base::i18n::TextDirection anchor_dir,
    const gfx::Rect& focus_rect,
    base::i18n::TextDirection focus_dir,
    const gfx::Rect& bounding_box,
    bool is_anchor_first) {
  if (view_) {
    view_->SelectionBoundsChanged(anchor_rect, anchor_dir, focus_rect,
                                  focus_dir, bounding_box, is_anchor_first);
  }
}

void RenderWidgetHostImpl::OnUpdateDragCursor(
    DragOperationCallback callback,
    ui::mojom::DragOperation current_op) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view) {
    view->UpdateDragCursor(current_op);
  }
  std::move(callback).Run(current_op);
}

void RenderWidgetHostImpl::RendererExited() {
  if (!renderer_widget_created_) {
    return;
  }

  // Clearing this flag causes us to re-create the renderer when recovering
  // from a crashed renderer.
  renderer_widget_created_ = false;
  // This flag is set when creating the renderer widget.
  waiting_for_init_ = false;

  blink_widget_.reset();

  // After the renderer crashes, the view is destroyed and so the
  // RenderWidgetHost cannot track its visibility anymore. We assume such
  // RenderWidgetHost to be invisible for the sake of internal accounting - be
  // careful about changing this - see http://crbug.com/401859 and
  // http://crbug.com/522795.
  //
  // We need to at least make sure that the RenderProcessHost is notified about
  // the |is_hidden_| change, so that the renderer will have correct visibility
  // set when respawned.
  if (!is_hidden_) {
    is_hidden_ = true;
    if (!destroyed_) {
      GetProcess()->UpdateClientPriority(this);
    }
  }

  if (view_) {
    view_->RenderProcessGone();
    SetView(nullptr);  // The View should be deleted by RenderProcessGone.
  }
}

void RenderWidgetHostImpl::ResetStateForCreatedRenderWidget(
    const blink::VisualProperties& initial_props) {
  // When the RenderWidget was destroyed, the ack may never come back. Don't
  // let that prevent us from speaking to the next RenderWidget.
  waiting_for_screen_rects_ack_ = false;
  last_view_screen_rect_ = last_window_screen_rect_ = kInvalidScreenRect;

  visual_properties_ack_pending_ =
      DoesVisualPropertiesNeedAck(nullptr, initial_props);
  old_visual_properties_ =
      std::make_unique<blink::VisualProperties>(initial_props);

  // Reconstruct the input router to ensure that it has fresh state for a new
  // RenderWidget. Otherwise it may be stuck waiting for the old renderer to ack
  // an event. (In particular, the above call to view_->RenderProcessGone() will
  // destroy the aura window, which may dispatch a synthetic mouse move.)
  //
  // This also stops the event ack timeout to ensure the hung renderer mechanism
  // is working properly.
  SetupInputRouter();

  frame_token_message_queue_->Reset();
}

void RenderWidgetHostImpl::UpdateTextDirection(
    base::i18n::TextDirection direction) {
  text_direction_updated_ = true;
  text_direction_ = direction;
}

void RenderWidgetHostImpl::NotifyTextDirection() {
  if (!text_direction_updated_) {
    return;
  }
  blink_frame_widget_->SetTextDirection(text_direction_);
  text_direction_updated_ = false;
}

void RenderWidgetHostImpl::ImeSetComposition(
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  // Passing null callback since it is only needed for Devtools
  GetWidgetInputHandler()->ImeSetComposition(
      text, ime_text_spans, replacement_range, selection_start, selection_end,
      base::OnceClosure());
#if BUILDFLAG(IS_ANDROID)
  for (auto& observer : ime_input_event_observers_) {
    observer.OnImeSetComposingTextEvent(text);
  }
#endif
}

void RenderWidgetHostImpl::ImeCommitText(
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  // Passing null callback since it is only needed for Devtools
  GetWidgetInputHandler()->ImeCommitText(text, ime_text_spans,
                                         replacement_range, relative_cursor_pos,
                                         base::OnceClosure());
#if BUILDFLAG(IS_ANDROID)
  for (auto& observer : ime_input_event_observers_) {
    observer.OnImeTextCommittedEvent(text);
  }
#endif
}

void RenderWidgetHostImpl::ImeFinishComposingText(bool keep_selection) {
  GetWidgetInputHandler()->ImeFinishComposingText(keep_selection);
#if BUILDFLAG(IS_ANDROID)
  for (auto& observer : ime_input_event_observers_) {
    observer.OnImeFinishComposingTextEvent();
  }
#endif
}

void RenderWidgetHostImpl::ImeCancelComposition() {
  // Passing null callback since it is only needed for Devtools
  GetWidgetInputHandler()->ImeSetComposition(
      std::u16string(), std::vector<ui::ImeTextSpan>(),
      gfx::Range::InvalidRange(), 0, 0, base::OnceClosure());
}

void RenderWidgetHostImpl::RejectMouseLockOrUnlockIfNecessary(
    blink::mojom::PointerLockResult reason) {
  DCHECK(!pending_mouse_lock_request_ || !IsMouseLocked());
  DCHECK(reason != blink::mojom::PointerLockResult::kSuccess);
  if (pending_mouse_lock_request_) {
    DCHECK(request_mouse_callback_);
    pending_mouse_lock_request_ = false;
    mouse_lock_raw_movement_ = false;
    std::move(request_mouse_callback_)
        .Run(reason, /*context=*/mojo::NullRemote());

  } else if (IsMouseLocked()) {
    view_->UnlockMouse();
  }
}

bool RenderWidgetHostImpl::IsKeyboardLocked() const {
  return view_ ? view_->IsKeyboardLocked() : false;
}

bool RenderWidgetHostImpl::IsContentRenderingTimeoutRunning() const {
  return new_content_rendering_timeout_ &&
         new_content_rendering_timeout_->IsRunning();
}

void RenderWidgetHostImpl::OnMouseEventAck(
    const MouseEventWithLatencyInfo& mouse_event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  latency_tracker_.OnInputEventAck(mouse_event.event, &mouse_event.latency,
                                   ack_result);
  for (auto& input_event_observer : input_event_observers_) {
    input_event_observer.OnInputEventAck(ack_source, ack_result,
                                         mouse_event.event);
  }

  // Give the delegate the ability to handle a mouse event that wasn't consumed
  // by the renderer. eg. Back/Forward mouse buttons.
  if (delegate_ &&
      ack_result != blink::mojom::InputEventResultState::kConsumed &&
      !is_hidden()) {
    delegate_->HandleMouseEvent(mouse_event.event);
  }
}

bool RenderWidgetHostImpl::IsMouseLocked() const {
  return view_ ? view_->IsMouseLocked() : false;
}

void RenderWidgetHostImpl::SetVisualPropertiesFromParentFrame(
    float page_scale_factor,
    float compositing_scale_factor,
    bool is_pinch_gesture_active,
    const gfx::Size& visible_viewport_size,
    const gfx::Rect& compositor_viewport,
    std::vector<gfx::Rect> root_widget_window_segments) {
  properties_from_parent_local_root_.page_scale_factor = page_scale_factor;
  properties_from_parent_local_root_.compositing_scale_factor =
      compositing_scale_factor;
  properties_from_parent_local_root_.is_pinch_gesture_active =
      is_pinch_gesture_active;
  properties_from_parent_local_root_.visible_viewport_size =
      visible_viewport_size;
  properties_from_parent_local_root_.compositor_viewport = compositor_viewport;
  properties_from_parent_local_root_.root_widget_window_segments =
      std::move(root_widget_window_segments);
}

void RenderWidgetHostImpl::SetAutoResize(bool enable,
                                         const gfx::Size& min_size,
                                         const gfx::Size& max_size) {
  auto_resize_enabled_ = enable;
  min_size_for_auto_resize_ = min_size;
  max_size_for_auto_resize_ = max_size;
}

void RenderWidgetHostImpl::Destroy(bool also_delete) {
  DCHECK(!destroyed_);
  destroyed_ = true;

  for (auto& observer : observers_) {
    observer.RenderWidgetHostDestroyed(this);
  }

  // Tell the view to die.
  // Note that in the process of the view shutting down, it can call a ton
  // of other messages on us.  So if you do any other deinitialization here,
  // do it after this call to view_->Destroy().
  if (view_) {
    view_->Destroy();
    view_.reset();
  }

  // Reset the popup host receiver, this will cause a disconnection notification
  // on the renderer to delete Popup widgets.
  blink_popup_widget_host_receiver_.reset();

  render_process_blocked_state_changed_subscription_ = {};
  GetProcess()->RemovePriorityClient(this);
  GetProcess()->RemoveObserver(this);
  g_routing_id_widget_map.Get().erase(
      RenderWidgetHostID(GetProcess()->GetID(), routing_id_));

  // The |delegate_| may have been destroyed (or is in the process of being
  // destroyed) and detached first.
  if (delegate_) {
    delegate_->RenderWidgetDeleted(this);
  }

  if (also_delete) {
    CHECK(self_owned_);
    // The destructor CHECKs self-owned RenderWidgetHostImpl aren't destroyed
    // externally. This bit needs to be reset to allow internal deletion.
    self_owned_ = false;
    delete this;
  }
}

void RenderWidgetHostImpl::OnInputEventAckTimeout() {
  // Since input has timed out, let the BrowserUiThreadScheduler know we are
  // done with input currently.
  user_input_active_handle_.reset();
  RendererIsUnresponsive(base::BindRepeating(
      &RenderWidgetHostImpl::RestartInputEventAckTimeoutIfNecessary,
      weak_factory_.GetWeakPtr()));
}

void RenderWidgetHostImpl::RendererIsUnresponsive(
    base::RepeatingClosure restart_hang_monitor_timeout) {
  NotificationService::current()->Notify(NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                                         Source<RenderWidgetHost>(this),
                                         NotificationService::NoDetails());
  is_unresponsive_ = true;

  if (delegate_) {
    delegate_->RendererUnresponsive(this,
                                    std::move(restart_hang_monitor_timeout));
  }

  // Do not add code after this since the Delegate may delete this
  // RenderWidgetHostImpl in RendererUnresponsive.
}

void RenderWidgetHostImpl::RendererIsResponsive() {
  if (is_unresponsive_) {
    is_unresponsive_ = false;
    if (delegate_) {
      delegate_->RendererResponsive(this);
    }
  }
}

void RenderWidgetHostImpl::ClearDisplayedGraphics() {
  NotifyNewContentRenderingTimeoutForTesting();
  if (view_) {
    view_->ResetFallbackToFirstNavigationSurface();
  }
}

void RenderWidgetHostImpl::OnKeyboardEventAck(
    const NativeWebKeyboardEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  latency_tracker_.OnInputEventAck(event.event, &event.latency, ack_result);
  for (auto& input_event_observer : input_event_observers_) {
    input_event_observer.OnInputEventAck(ack_source, ack_result, event.event);
  }

  bool processed =
      (blink::mojom::InputEventResultState::kConsumed == ack_result);

  // We only send unprocessed key event upwards if we are not hidden,
  // because the user has moved away from us and no longer expect any effect
  // of this key event.
  if (delegate_ && !processed && !is_hidden() && !event.event.skip_in_browser) {
    delegate_->HandleKeyboardEvent(event.event);
  }
  // WARNING: This RenderWidgetHostImpl can be deallocated at this point
  // (i.e.  in the case of Ctrl+W, where the call to
  // HandleKeyboardEvent destroys this RenderWidgetHostImpl).
}

void RenderWidgetHostImpl::RequestClosePopup() {
  DCHECK(!owner_delegate_);
  ShutdownAndDestroyWidget(true);
}

void RenderWidgetHostImpl::SetPopupBounds(const gfx::Rect& bounds,
                                          SetPopupBoundsCallback callback) {
  // If the browser changes bounds, do not allow renderer changing bounds at the
  // same time until it acked the changes. Otherwise, if they simultaneously
  // change bounds, browser's bounds can be clobbered.
  if (view_ && !waiting_for_screen_rects_ack_) {
    view_->SetBounds(bounds);
  }
  std::move(callback).Run();
}

void RenderWidgetHostImpl::ShowPopup(const gfx::Rect& initial_screen_rect,
                                     const gfx::Rect& anchor_screen_rect,
                                     ShowPopupCallback callback) {
  // `delegate_` may be null since this message may be received from when
  // the delegate shutdown but this widget is not yet destroyed.
  if (delegate_) {
    delegate_->ShowCreatedWidget(GetProcess()->GetID(), GetRoutingID(),
                                 initial_screen_rect, anchor_screen_rect);
  }
  std::move(callback).Run();
}

void RenderWidgetHostImpl::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text,
    base::i18n::TextDirection text_direction_hint) {
  if (!GetView()) {
    return;
  }

  view_->UpdateTooltipUnderCursor(
      GetWrappedTooltipText(tooltip_text, text_direction_hint));
}

void RenderWidgetHostImpl::UpdateTooltipFromKeyboard(
    const std::u16string& tooltip_text,
    base::i18n::TextDirection text_direction_hint,
    const gfx::Rect& bounds) {
  if (!GetView()) {
    return;
  }

  view_->UpdateTooltipFromKeyboard(
      GetWrappedTooltipText(tooltip_text, text_direction_hint), bounds);
}

void RenderWidgetHostImpl::ClearKeyboardTriggeredTooltip() {
  if (!GetView()) {
    return;
  }

  view_->ClearKeyboardTriggeredTooltip();
}

void RenderWidgetHostImpl::OnUpdateScreenRectsAck() {
  waiting_for_screen_rects_ack_ = false;
  if (!view_) {
    return;
  }

  view_->SendInitialPropertiesIfNeeded();

  if (view_->GetViewBounds() == last_view_screen_rect_ &&
      view_->GetBoundsInRootWindow() == last_window_screen_rect_) {
    return;
  }

  SendScreenRects();
}

void RenderWidgetHostImpl::OnRenderFrameSubmission() {}

void RenderWidgetHostImpl::OnLocalSurfaceIdChanged(
    const cc::RenderFrameMetadata& metadata) {
  TRACE_EVENT_WITH_FLOW1(
      "renderer_host," TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "RenderWidgetHostImpl::OnLocalSurfaceIdChanged",
      metadata.local_surface_id && metadata.local_surface_id->is_valid()
          ? metadata.local_surface_id->submission_trace_id() +
                metadata.local_surface_id->embed_trace_id()
          : 0,
      TRACE_EVENT_FLAG_FLOW_IN, "local_surface_id",
      metadata.local_surface_id ? metadata.local_surface_id->ToString()
                                : "null");

  // Update our knowledge of the RenderWidget's size.
  DCHECK(!metadata.viewport_size_in_pixels.IsEmpty());

  visual_properties_ack_pending_ = false;

  for (auto& observer : observers_) {
    observer.RenderWidgetHostDidUpdateVisualProperties(this);
  }

  if (!view_) {
    return;
  }

  viz::ScopedSurfaceIdAllocator scoped_allocator =
      view_->DidUpdateVisualProperties(metadata);
  base::AutoReset<bool> auto_reset(&surface_id_allocation_suppressed_, true);

  if (auto_resize_enabled_ && delegate_) {
    // TODO(fsamuel): The fact that we translate the viewport_size from pixels
    // to DIP is concerning. This could result in invariants violations.
    gfx::Size viewport_size_in_dip = gfx::ScaleToCeiledSize(
        metadata.viewport_size_in_pixels, 1.f / metadata.device_scale_factor);
    delegate_->ResizeDueToAutoResize(this, viewport_size_in_dip);
  }
}

SiteInstanceGroup* RenderWidgetHostImpl::GetSiteInstanceGroup() {
  return site_instance_group_.get();
}

void RenderWidgetHostImpl::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate) {
  GetWidgetInputHandler()->UpdateBrowserControlsState(constraints, current,
                                                      animate);
}

// static
bool RenderWidgetHostImpl::DidVisualPropertiesSizeChange(
    const blink::VisualProperties& old_visual_properties,
    const blink::VisualProperties& new_visual_properties) {
  return old_visual_properties.auto_resize_enabled !=
             new_visual_properties.auto_resize_enabled ||
         (old_visual_properties.auto_resize_enabled &&
          (old_visual_properties.min_size_for_auto_resize !=
               new_visual_properties.min_size_for_auto_resize ||
           old_visual_properties.max_size_for_auto_resize !=
               new_visual_properties.max_size_for_auto_resize)) ||
         (!old_visual_properties.auto_resize_enabled &&
          (old_visual_properties.new_size != new_visual_properties.new_size ||
           (old_visual_properties.compositor_viewport_pixel_rect.IsEmpty() &&
            !new_visual_properties.compositor_viewport_pixel_rect.IsEmpty())));
}

// static
bool RenderWidgetHostImpl::DoesVisualPropertiesNeedAck(
    const std::unique_ptr<blink::VisualProperties>& old_visual_properties,
    const blink::VisualProperties& new_visual_properties) {
  // We should throttle sending updated VisualProperties to the renderer to
  // the rate of commit. This ensures we don't overwhelm the renderer with
  // visual updates faster than it can keep up.  |needs_ack| corresponds to
  // cases where a commit is expected.
  bool is_acking_applicable =
      g_check_for_pending_visual_properties_ack &&
      !new_visual_properties.auto_resize_enabled &&
      !new_visual_properties.new_size.IsEmpty() &&
      !new_visual_properties.compositor_viewport_pixel_rect.IsEmpty() &&
      new_visual_properties.local_surface_id;

  // If acking is applicable, then check if there has been an
  // |old_visual_properties| stored which would indicate an update has been
  // sent. If so, then acking is defined by size changing.
  return is_acking_applicable &&
         (!old_visual_properties ||
          DidVisualPropertiesSizeChange(*old_visual_properties,
                                        new_visual_properties));
}

// static
bool RenderWidgetHostImpl::StoredVisualPropertiesNeedsUpdate(
    const std::unique_ptr<blink::VisualProperties>& old_visual_properties,
    const blink::VisualProperties& new_visual_properties) {
  if (!old_visual_properties) {
    return true;
  }

  const bool size_changed = DidVisualPropertiesSizeChange(
      *old_visual_properties, new_visual_properties);

  // Hold on the the LocalSurfaceId in a local variable otherwise the
  // LocalSurfaceId may become invalid when used later.
  const viz::LocalSurfaceId old_parent_local_surface_id =
      old_visual_properties->local_surface_id.value_or(viz::LocalSurfaceId());
  const viz::LocalSurfaceId new_parent_local_surface_id =
      new_visual_properties.local_surface_id.value_or(viz::LocalSurfaceId());

  const bool parent_local_surface_id_changed =
      old_parent_local_surface_id.parent_sequence_number() !=
          new_parent_local_surface_id.parent_sequence_number() ||
      old_parent_local_surface_id.embed_token() !=
          new_parent_local_surface_id.embed_token();

  const bool zoom_changed =
      old_visual_properties->zoom_level != new_visual_properties.zoom_level;

  return zoom_changed || size_changed || parent_local_surface_id_changed ||
         old_visual_properties->screen_infos !=
             new_visual_properties.screen_infos ||
         old_visual_properties->compositor_viewport_pixel_rect !=
             new_visual_properties.compositor_viewport_pixel_rect ||
         old_visual_properties->is_fullscreen_granted !=
             new_visual_properties.is_fullscreen_granted ||
         old_visual_properties->display_mode !=
             new_visual_properties.display_mode ||
         old_visual_properties->browser_controls_params !=
             new_visual_properties.browser_controls_params ||
         old_visual_properties->visible_viewport_size !=
             new_visual_properties.visible_viewport_size ||
         old_visual_properties->capture_sequence_number !=
             new_visual_properties.capture_sequence_number ||
         old_visual_properties->page_scale_factor !=
             new_visual_properties.page_scale_factor ||
         old_visual_properties->compositing_scale_factor !=
             new_visual_properties.compositing_scale_factor ||
         old_visual_properties->is_pinch_gesture_active !=
             new_visual_properties.is_pinch_gesture_active ||
         old_visual_properties->root_widget_window_segments !=
             new_visual_properties.root_widget_window_segments ||
         old_visual_properties->window_controls_overlay_rect !=
             new_visual_properties.window_controls_overlay_rect;
}

void RenderWidgetHostImpl::AutoscrollStart(const gfx::PointF& position) {
  GetView()->OnAutoscrollStart();
  sent_autoscroll_scroll_begin_ = false;
  autoscroll_in_progress_ = true;
  delegate()->GetInputEventRouter()->SetAutoScrollInProgress(
      autoscroll_in_progress_);
  autoscroll_start_position_ = position;
}

void RenderWidgetHostImpl::AutoscrollFling(const gfx::Vector2dF& velocity) {
  DCHECK(autoscroll_in_progress_);
  if (!sent_autoscroll_scroll_begin_ && velocity != gfx::Vector2dF()) {
    // Send a GSB event with valid delta hints.
    WebGestureEvent scroll_begin =
        blink::SyntheticWebGestureEventBuilder::Build(
            WebInputEvent::Type::kGestureScrollBegin,
            blink::WebGestureDevice::kSyntheticAutoscroll);
    scroll_begin.SetPositionInWidget(autoscroll_start_position_);
    scroll_begin.data.scroll_begin.delta_x_hint = velocity.x();
    scroll_begin.data.scroll_begin.delta_y_hint = velocity.y();

    ForwardGestureEventWithLatencyInfo(
        scroll_begin, ui::LatencyInfo(ui::SourceEventType::OTHER));
    sent_autoscroll_scroll_begin_ = true;
  }

  WebGestureEvent event = blink::SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureFlingStart,
      blink::WebGestureDevice::kSyntheticAutoscroll);
  event.SetPositionInWidget(autoscroll_start_position_);
  event.data.fling_start.velocity_x = velocity.x();
  event.data.fling_start.velocity_y = velocity.y();

  ForwardGestureEventWithLatencyInfo(
      event, ui::LatencyInfo(ui::SourceEventType::OTHER));
}

void RenderWidgetHostImpl::AutoscrollEnd() {
  autoscroll_in_progress_ = false;

  delegate()->GetInputEventRouter()->SetAutoScrollInProgress(
      autoscroll_in_progress_);
  // Don't send a GFC if no GSB is sent.
  if (!sent_autoscroll_scroll_begin_) {
    return;
  }

  sent_autoscroll_scroll_begin_ = false;
  WebGestureEvent cancel_event = blink::SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureFlingCancel,
      blink::WebGestureDevice::kSyntheticAutoscroll);
  cancel_event.data.fling_cancel.prevent_boosting = true;
  cancel_event.SetPositionInWidget(autoscroll_start_position_);

  ForwardGestureEventWithLatencyInfo(
      cancel_event, ui::LatencyInfo(ui::SourceEventType::OTHER));
}

void RenderWidgetHostImpl::StartDragging(
    blink::mojom::DragDataPtr drag_data,
    blink::DragOperationsMask drag_operations_mask,
    const SkBitmap& bitmap,
    const gfx::Vector2d& cursor_offset_in_dip,
    const gfx::Rect& drag_obj_rect_in_dip,
    blink::mojom::DragEventSourceInfoPtr event_info) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (!view || !GetView()) {
    // Need to clear drag and drop state in blink.
    DragSourceSystemDragEnded();
    return;
  }

  DropData drop_data = DragDataToDropData(*drag_data);

  if (frame_tree_) {
    bool intercepted = false;
    devtools_instrumentation::WillStartDragging(
        frame_tree_->root(), std::move(drag_data), drag_operations_mask,
        &intercepted);
    if (intercepted) {
      return;
    }
  }

  DropData filtered_data(drop_data);
  RenderProcessHost* process = GetProcess();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Allow drag of Javascript URLs to enable bookmarklet drag to bookmark bar.
  if (!filtered_data.url.SchemeIs(url::kJavaScriptScheme)) {
    process->FilterURL(true, &filtered_data.url);
  }
  process->FilterURL(false, &filtered_data.html_base_url);
  // Filter out any paths that the renderer didn't have access to. This prevents
  // the following attack on a malicious renderer:
  // 1. StartDragging IPC sent with renderer-specified filesystem paths that it
  //    doesn't have read permissions for.
  // 2. We initiate a native DnD operation.
  // 3. DnD operation immediately ends since mouse is not held down. DnD events
  //    still fire though, which causes read permissions to be granted to the
  //    renderer for any file paths in the drop.
  filtered_data.filenames.clear();
  for (const auto& file_info : drop_data.filenames) {
    if (policy->CanReadFile(GetProcess()->GetID(), file_info.path)) {
      filtered_data.filenames.push_back(file_info);
    }
  }

  storage::FileSystemContext* file_system_context =
      GetProcess()->GetStoragePartition()->GetFileSystemContext();
  filtered_data.file_system_files.clear();

  for (const auto& file_system_file : drop_data.file_system_files) {
    storage::FileSystemURL file_system_url =
        file_system_context->CrackURLInFirstPartyContext(file_system_file.url);

    // Sandboxed filesystem files should never be handled via this path, so
    // skip any that are sent from the renderer. In all other cases, it should
    // be safe to use the FileSystemURL returned from calling
    // CrackURLInFirstPartyContext as long as CanReadFileSystemFile only
    // performs checks on the origin and doesn't use more of the StorageKey.
    if (file_system_url.type() == storage::kFileSystemTypePersistent ||
        file_system_url.type() == storage::kFileSystemTypeTemporary) {
      continue;
    }

    if (policy->CanReadFileSystemFile(GetProcess()->GetID(), file_system_url)) {
      filtered_data.file_system_files.push_back(file_system_file);
    }
  }

  float scale = GetScaleFactorForView(GetView());
  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(bitmap, scale);
  gfx::Vector2d offset = cursor_offset_in_dip;
  gfx::Rect rect = drag_obj_rect_in_dip;
#if BUILDFLAG(IS_WIN)
  // Scale the offset by device scale factor, otherwise the drag
  // image location doesn't line up with the drop location (drag destination).
  // TODO(crbug.com/1354831): this conversion should not be necessary.
  gfx::Vector2dF scaled_offset = static_cast<gfx::Vector2dF>(offset);
  scaled_offset.Scale(scale);
  offset = gfx::ToRoundedVector2d(scaled_offset);
  gfx::RectF scaled_rect = static_cast<gfx::RectF>(rect);
  scaled_rect.Scale(scale);
  rect = gfx::ToRoundedRect(scaled_rect);
#endif
  view->StartDragging(filtered_data, drag_operations_mask, image, offset, rect,
                      *event_info, this);
}

bool RenderWidgetHostImpl::IsAutoscrollInProgress() {
  return autoscroll_in_progress_;
}

TouchEmulator* RenderWidgetHostImpl::GetTouchEmulator() {
  if (!delegate_ || !delegate_->GetInputEventRouter()) {
    return nullptr;
  }

  return delegate_->GetInputEventRouter()->GetTouchEmulator();
}

TouchEmulator* RenderWidgetHostImpl::GetExistingTouchEmulator() {
  if (!delegate_ || !delegate_->GetInputEventRouter() ||
      !delegate_->GetInputEventRouter()->has_touch_emulator()) {
    return nullptr;
  }

  return delegate_->GetInputEventRouter()->GetTouchEmulator();
}

void RenderWidgetHostImpl::TextInputStateChanged(
    ui::mojom::TextInputStatePtr state) {
  if (!view_) {
    return;
  }
  for (auto& callback : suppress_showing_ime_callbacks_) {
    if (callback.Run()) {
      state->always_hide_ime = true;
      break;
    }
  }
  view_->TextInputStateChanged(*state);
}

void RenderWidgetHostImpl::OnImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (view_) {
    view_->ImeCompositionRangeChanged(range, character_bounds);
  }
}

void RenderWidgetHostImpl::OnImeCancelComposition() {
  if (view_) {
    view_->ImeCancelComposition();
  }
}

RenderWidgetHostViewBase* RenderWidgetHostImpl::GetRenderWidgetHostViewBase() {
  return GetView();
}

void RenderWidgetHostImpl::OnStartStylusWriting() {
  if (blink_frame_widget_) {
    auto callback = base::BindOnce(
        &RenderWidgetHostImpl::OnEditElementFocusedForStylusWriting,
        weak_factory_.GetWeakPtr());
    blink_frame_widget_->OnStartStylusWriting(std::move(callback));
  }
}

void RenderWidgetHostImpl::OnEditElementFocusedForStylusWriting(
    const absl::optional<gfx::Rect>& focused_edit_bounds,
    const absl::optional<gfx::Rect>& caret_bounds) {
  if (view_) {
    view_->OnEditElementFocusedForStylusWriting(
        focused_edit_bounds.value_or(gfx::Rect()),
        caret_bounds.value_or(gfx::Rect()));
  }
}

bool RenderWidgetHostImpl::IsWheelScrollInProgress() {
  return is_in_gesture_scroll_[static_cast<int>(
      blink::WebGestureDevice::kTouchpad)];
}

void RenderWidgetHostImpl::SetMouseCapture(bool capture) {
  if (!delegate_ || !delegate_->GetInputEventRouter()) {
    return;
  }

  delegate_->GetInputEventRouter()->SetMouseCaptureTarget(GetView(), capture);
}

void RenderWidgetHostImpl::RequestMouseLock(
    bool from_user_gesture,
    bool unadjusted_movement,
    InputRouterImpl::RequestMouseLockCallback response) {
  if (pending_mouse_lock_request_ || IsMouseLocked()) {
    std::move(response).Run(blink::mojom::PointerLockResult::kAlreadyLocked,
                            /*context=*/mojo::NullRemote());
    return;
  }

  if (!view_ || !view_->CanBeMouseLocked()) {
    std::move(response).Run(blink::mojom::PointerLockResult::kWrongDocument,
                            /*context=*/mojo::NullRemote());
    return;
  }

  request_mouse_callback_ = std::move(response);

  pending_mouse_lock_request_ = true;
  mouse_lock_raw_movement_ = unadjusted_movement;
  if (!delegate_) {
    // No delegate, reject message.
    GotResponseToLockMouseRequest(
        blink::mojom::PointerLockResult::kPermissionDenied);
    return;
  }

  delegate_->RequestToLockMouse(this, from_user_gesture,
                                is_last_unlocked_by_target_, false);
  // We need to reset |is_last_unlocked_by_target_| here as we don't know
  // request source in |LostMouseLock()|.
  is_last_unlocked_by_target_ = false;
}

void RenderWidgetHostImpl::RequestMouseLockChange(
    bool unadjusted_movement,
    PointerLockContext::RequestMouseLockChangeCallback response) {
  if (pending_mouse_lock_request_) {
    std::move(response).Run(blink::mojom::PointerLockResult::kAlreadyLocked);
    return;
  }

  if (!view_ || !view_->HasFocus()) {
    std::move(response).Run(blink::mojom::PointerLockResult::kWrongDocument);
    return;
  }

  std::move(response).Run(view_->ChangeMouseLock(unadjusted_movement));
}

void RenderWidgetHostImpl::UnlockMouse() {
  // Got unlock request from renderer. Will update |is_last_unlocked_by_target_|
  // for silent re-lock.
  const bool was_mouse_locked = !pending_mouse_lock_request_ && IsMouseLocked();
  RejectMouseLockOrUnlockIfNecessary(
      blink::mojom::PointerLockResult::kUserRejected);
  if (was_mouse_locked) {
    is_last_unlocked_by_target_ = true;
  }
}

void RenderWidgetHostImpl::OnInvalidFrameToken(uint32_t frame_token) {
  bad_message::ReceivedBadMessage(GetProcess(),
                                  bad_message::RWH_INVALID_FRAME_TOKEN);
}

bool RenderWidgetHostImpl::RequestKeyboardLock(
    absl::optional<base::flat_set<ui::DomCode>> codes) {
  if (!delegate_) {
    CancelKeyboardLock();
    return false;
  }

  DCHECK(!codes.has_value() || !codes.value().empty());
  keyboard_keys_to_lock_ = std::move(codes);
  keyboard_lock_requested_ = true;

  const bool esc_requested =
      !keyboard_keys_to_lock_.has_value() ||
      base::Contains(keyboard_keys_to_lock_.value(), ui::DomCode::ESCAPE);

  if (!delegate_->RequestKeyboardLock(this, esc_requested)) {
    CancelKeyboardLock();
    return false;
  }

  return true;
}

void RenderWidgetHostImpl::CancelKeyboardLock() {
  if (delegate_) {
    delegate_->CancelKeyboardLock(this);
  }

  UnlockKeyboard();

  keyboard_lock_allowed_ = false;
  keyboard_lock_requested_ = false;
  keyboard_keys_to_lock_.reset();
}

base::flat_map<std::string, std::string>
RenderWidgetHostImpl::GetKeyboardLayoutMap() {
  if (!view_) {
    return {};
  }
  return view_->GetKeyboardLayoutMap();
}

void RenderWidgetHostImpl::RequestForceRedraw(int snapshot_id) {
  if (!blink_widget_) {
    return;
  }

  blink_widget_->ForceRedraw(
      base::BindOnce(&RenderWidgetHostImpl::GotResponseToForceRedraw,
                     base::Unretained(this), snapshot_id));
}

bool RenderWidgetHostImpl::KeyPressListenersHandleEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser ||
      event.GetType() != WebKeyboardEvent::Type::kRawKeyDown) {
    return false;
  }

  for (size_t i = 0; i < key_press_event_callbacks_.size(); i++) {
    size_t original_size = key_press_event_callbacks_.size();
    if (key_press_event_callbacks_[i].Run(event)) {
      return true;
    }

    // Check whether the callback that just ran removed itself, in which case
    // the iterator needs to be decremented to properly account for the removal.
    size_t current_size = key_press_event_callbacks_.size();
    if (current_size != original_size) {
      DCHECK_EQ(original_size - 1, current_size);
      --i;
    }
  }

  return false;
}

blink::mojom::InputEventResultState RenderWidgetHostImpl::FilterInputEvent(
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency_info) {
  // Don't ignore touch cancel events, since they may be sent while input
  // events are being ignored in order to keep the renderer from getting
  // confused about how many touches are active.
  if (IsIgnoringInputEvents() &&
      event.GetType() != WebInputEvent::Type::kTouchCancel) {
    return blink::mojom::InputEventResultState::kNoConsumerExists;
  }

  if (!GetProcess()->IsInitializedAndNotDead()) {
    return blink::mojom::InputEventResultState::kUnknown;
  }

  if (delegate_) {
    if (event.GetType() == WebInputEvent::Type::kMouseDown ||
        event.GetType() == WebInputEvent::Type::kTouchStart ||
        event.GetType() == WebInputEvent::Type::kGestureTap) {
      delegate_->FocusOwningWebContents(this);
    }
    delegate_->DidReceiveInputEvent(this, event);
  }

  return view_ ? view_->FilterInputEvent(event)
               : blink::mojom::InputEventResultState::kNotConsumed;
}

void RenderWidgetHostImpl::IncrementInFlightEventCount() {
  ++in_flight_event_count_;
  if (in_flight_event_count_ == 1) {
    power_mode_input_voter_->VoteFor(power_scheduler::PowerMode::kResponse);
    user_input_active_handle_ = BrowserTaskExecutor::OnUserInputStart();
  }

  if (!is_hidden_) {
    StartInputEventAckTimeout();
  }
}

void RenderWidgetHostImpl::NotifyUISchedulerOfScrollStateUpdate(
    BrowserUIThreadScheduler::ScrollState scroll_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserUIThreadScheduler::Get()->OnScrollStateUpdate(scroll_state);
}
void RenderWidgetHostImpl::DecrementInFlightEventCount(
    blink::mojom::InputEventResultSource ack_source) {
  --in_flight_event_count_;
  if (in_flight_event_count_ <= 0) {
    // Cancel pending hung renderer checks since the renderer is responsive.
    StopInputEventAckTimeout();
    power_mode_input_voter_->ResetVoteAfterTimeout(
        power_scheduler::PowerModeVoter::kResponseTimeout);
    user_input_active_handle_.reset();
  } else {
    // Only restart the hang monitor timer if we got a response from the
    // main thread.
    if (ack_source == blink::mojom::InputEventResultSource::kMainThread) {
      RestartInputEventAckTimeoutIfNecessary();
    }
  }
}

void RenderWidgetHostImpl::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  if (view_) {
    view_->DidOverscroll(params);
  }
}

void RenderWidgetHostImpl::DidStopFlinging() {
  is_in_touchpad_gesture_fling_ = false;
  if (view_) {
    view_->DidStopFlinging();
  }
}

void RenderWidgetHostImpl::DidStartScrollingViewport() {
  if (view_) {
    view_->set_is_currently_scrolling_viewport(true);
  }
}

void RenderWidgetHostImpl::OnInvalidInputEventSource() {
  bad_message::ReceivedBadMessage(
      GetProcess(), bad_message::INPUT_ROUTER_INVALID_EVENT_SOURCE);
}

void RenderWidgetHostImpl::AddPendingUserActivation(
    const WebInputEvent& event) {
  if ((base::FeatureList::IsEnabled(
           features::kBrowserVerifiedUserActivationMouse) &&
       event.GetType() == WebInputEvent::Type::kMouseDown) ||
      (base::FeatureList::IsEnabled(
           features::kBrowserVerifiedUserActivationKeyboard) &&
       (event.GetType() == WebInputEvent::Type::kKeyDown ||
        event.GetType() == WebInputEvent::Type::kRawKeyDown))) {
    pending_user_activation_timer_.Start(
        FROM_HERE, kActivationNotificationExpireTime,
        base::BindOnce(&RenderWidgetHostImpl::ClearPendingUserActivation,
                       base::Unretained(this)));
    pending_user_activation_counter_++;
  }
}

void RenderWidgetHostImpl::ClearPendingUserActivation() {
  pending_user_activation_counter_ = 0;
  pending_user_activation_timer_.Stop();
}

bool RenderWidgetHostImpl::RemovePendingUserActivationIfAvailable() {
  if (pending_user_activation_counter_ > 0) {
    pending_user_activation_counter_--;
    return true;
  }
  return false;
}

const mojo::AssociatedRemote<blink::mojom::FrameWidget>&
RenderWidgetHostImpl::GetAssociatedFrameWidget() {
  return blink_frame_widget_;
}

blink::mojom::FrameWidgetInputHandler*
RenderWidgetHostImpl::GetFrameWidgetInputHandler() {
  if (!frame_widget_input_handler_) {
    return nullptr;
  }
  return frame_widget_input_handler_.get();
}

absl::optional<blink::VisualProperties>
RenderWidgetHostImpl::LastComputedVisualProperties() const {
  if (!old_visual_properties_) {
    return absl::nullopt;
  }
  return *old_visual_properties_;
}

mojom::CreateFrameWidgetParamsPtr
RenderWidgetHostImpl::BindAndGenerateCreateFrameWidgetParams() {
  auto params = mojom::CreateFrameWidgetParams::New();

  params->routing_id = GetRoutingID();

  mojo::PendingAssociatedRemote<blink::mojom::Widget> widget_remote;
  params->widget = widget_remote.InitWithNewEndpointAndPassReceiver();
  BindWidgetInterfaces(params->widget_host.InitWithNewEndpointAndPassReceiver(),
                       std::move(widget_remote));
  mojo::PendingAssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote;
  params->frame_widget =
      frame_widget_remote.InitWithNewEndpointAndPassReceiver();
  BindFrameWidgetInterfaces(
      params->frame_widget_host.InitWithNewEndpointAndPassReceiver(),
      std::move(frame_widget_remote));

  params->visual_properties = GetInitialVisualProperties();

  return params;
}

mojom::CreateFrameWidgetParamsPtr
RenderWidgetHostImpl::BindAndGenerateCreateFrameWidgetParamsForNewWindow() {
  auto params = mojom::CreateFrameWidgetParams::New();
  params->routing_id = GetRoutingID();
  mojo::PendingAssociatedRemote<blink::mojom::Widget> widget_remote;
  params->widget = widget_remote.InitWithNewEndpointAndPassReceiver();
  BindWidgetInterfaces(params->widget_host.InitWithNewEndpointAndPassReceiver(),
                       std::move(widget_remote));
  mojo::PendingAssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote;
  params->frame_widget =
      frame_widget_remote.InitWithNewEndpointAndPassReceiver();
  BindFrameWidgetInterfaces(
      params->frame_widget_host.InitWithNewEndpointAndPassReceiver(),
      std::move(frame_widget_remote));
  // TODO(danakj): For some reason, there is no RenderWidgetHostView here, but
  // it seems like there should be one? In the meantime we send some nonsense
  // with semi-valid but incorrect screen info (it needs a RenderWidgetHostView
  // to be correct). An updated VisualProperties will get to the RenderWidget
  // eventually.
  params->visual_properties.screen_infos = GetScreenInfos();
  return params;
}

void RenderWidgetHostImpl::DispatchInputEventWithLatencyInfo(
    const blink::WebInputEvent& event,
    ui::LatencyInfo* latency,
    ui::EventLatencyMetadata* event_latency_metadata) {
  latency_tracker_.OnInputEvent(event, latency, event_latency_metadata);
  AddPendingUserActivation(event);
  for (auto& observer : input_event_observers_) {
    observer.OnInputEvent(event);
  }
}

void RenderWidgetHostImpl::OnWheelEventAck(
    const MouseWheelEventWithLatencyInfo& wheel_event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  latency_tracker_.OnInputEventAck(wheel_event.event, &wheel_event.latency,
                                   ack_result);
  for (auto& input_event_observer : input_event_observers_) {
    input_event_observer.OnInputEventAck(ack_source, ack_result,
                                         wheel_event.event);
  }

  if (!is_hidden() && view_) {
    if (ack_result != blink::mojom::InputEventResultState::kConsumed &&
        delegate_ && delegate_->HandleWheelEvent(wheel_event.event)) {
      ack_result = blink::mojom::InputEventResultState::kConsumed;
    }
    view_->WheelEventAck(wheel_event.event, ack_result);
  }
}

void RenderWidgetHostImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result,
    blink::mojom::ScrollResultDataPtr scroll_result_data) {
  latency_tracker_.OnInputEventAck(event.event, &event.latency, ack_result);
  for (auto& input_event_observer : input_event_observers_) {
    input_event_observer.OnInputEventAck(ack_source, ack_result, event.event);
  }

  // If the TouchEmulator didn't exist when this GestureEvent was sent, we
  // shouldn't create it here.
  if (auto* touch_emulator = GetExistingTouchEmulator()) {
    touch_emulator->OnGestureEventAck(event.event, GetView());
  }

  if (view_) {
    view_->GestureEventAck(event.event, ack_result,
                           std::move(scroll_result_data));
  }
}

void RenderWidgetHostImpl::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  latency_tracker_.OnInputEventAck(event.event, &event.latency, ack_result);
  for (auto& input_event_observer : input_event_observers_) {
    input_event_observer.OnInputEventAck(ack_source, ack_result, event.event);
  }

  auto* input_event_router =
      delegate() ? delegate()->GetInputEventRouter() : nullptr;

  // At present interstitial pages might not have an input event router, so we
  // just have the view process the ack directly in that case; the view is
  // guaranteed to be a top-level view with an appropriate implementation of
  // ProcessAckedTouchEvent().
  if (input_event_router) {
    input_event_router->ProcessAckedTouchEvent(event, ack_result, view_.get());
  } else if (view_) {
    view_->ProcessAckedTouchEvent(event, ack_result);
  }
}

bool RenderWidgetHostImpl::IsIgnoringInputEvents() const {
  return agent_scheduling_group_->GetProcess()->IsBlocked() || !delegate_ ||
         delegate_->ShouldIgnoreInputEvents();
}

bool RenderWidgetHostImpl::GotResponseToLockMouseRequest(
    blink::mojom::PointerLockResult response) {
  if (response != blink::mojom::PointerLockResult::kSuccess) {
    RejectMouseLockOrUnlockIfNecessary(response);
  }
  if (!pending_mouse_lock_request_) {
    // This is possible, e.g., the plugin sends us an unlock request before
    // the user allows to lock to mouse.
    return false;
  }

  DCHECK(request_mouse_callback_);
  pending_mouse_lock_request_ = false;
  if (!view_ || !view_->HasFocus()) {
    std::move(request_mouse_callback_)
        .Run(blink::mojom::PointerLockResult::kWrongDocument,
             /*context=*/mojo::NullRemote());
    return false;
  }

  blink::mojom::PointerLockResult result =
      view_->LockMouse(mouse_lock_raw_movement_);

  if (result != blink::mojom::PointerLockResult::kSuccess) {
    std::move(request_mouse_callback_)
        .Run(result, /*context=*/mojo::NullRemote());
    return false;
  }

  mojo::PendingRemote<blink::mojom::PointerLockContext> context =
      mouse_lock_context_.BindNewPipeAndPassRemote(
          content::GetUIThreadTaskRunner({BrowserTaskType::kUserInput}));

  std::move(request_mouse_callback_)
      .Run(blink::mojom::PointerLockResult::kSuccess, std::move(context));
  mouse_lock_context_.set_disconnect_handler(base::BindOnce(
      &RenderWidgetHostImpl::UnlockMouse, weak_factory_.GetWeakPtr()));
  return true;
}

void RenderWidgetHostImpl::GotResponseToKeyboardLockRequest(bool allowed) {
  DCHECK(keyboard_lock_requested_);
  keyboard_lock_allowed_ = allowed;

  if (keyboard_lock_allowed_) {
    LockKeyboard();
  } else {
    UnlockKeyboard();
  }
}

void RenderWidgetHostImpl::GotResponseToForceRedraw(int snapshot_id) {
  // Snapshots from surface do not need to wait for the screen update.
  if (!pending_surface_browser_snapshots_.empty()) {
    GetView()->CopyFromSurface(
        gfx::Rect(), gfx::Size(),
        base::BindOnce(&RenderWidgetHostImpl::OnSnapshotFromSurfaceReceived,
                       weak_factory_.GetWeakPtr(), snapshot_id, 0));
  }

  if (pending_browser_snapshots_.empty()) {
    return;
  }
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // On Mac, when using CoreAnimation, or Win32 when using GDI, there is a
  // delay between when content is drawn to the screen, and when the
  // snapshot will actually pick up that content. Insert a manual delay of
  // 1/6th of a second (to simulate 10 frames at 60 fps) before actually
  // taking the snapshot.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderWidgetHostImpl::WindowSnapshotReachedScreen,
                     weak_factory_.GetWeakPtr(), snapshot_id),
      base::Seconds(1. / 6));
#else
  WindowSnapshotReachedScreen(snapshot_id);
#endif
}

void RenderWidgetHostImpl::DetachDelegate() {
  delegate_ = nullptr;
  latency_tracker_.reset_delegate();
}

void RenderWidgetHostImpl::WindowSnapshotReachedScreen(int snapshot_id) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (!pending_browser_snapshots_.empty()) {
#if BUILDFLAG(IS_ANDROID)
    // On Android, call sites should pass in the bounds with correct offset
    // to capture the intended content area.
    gfx::Rect snapshot_bounds(GetView()->GetViewBounds());
    snapshot_bounds.Offset(0, GetView()->GetNativeView()->content_offset());
#else
    gfx::Rect snapshot_bounds(GetView()->GetViewBounds().size());
#endif

    gfx::Image image;
    if (ui::GrabViewSnapshot(GetView()->GetNativeView(), snapshot_bounds,
                             &image)) {
      OnSnapshotReceived(snapshot_id, image);
      return;
    }

    ui::GrabViewSnapshotAsync(
        GetView()->GetNativeView(), snapshot_bounds,
        base::BindOnce(&RenderWidgetHostImpl::OnSnapshotReceived,
                       weak_factory_.GetWeakPtr(), snapshot_id));
  }
}

void RenderWidgetHostImpl::OnSnapshotFromSurfaceReceived(
    int snapshot_id,
    int retry_count,
    const SkBitmap& bitmap) {
  static constexpr int kMaxRetries = 5;
  if (bitmap.drawsNothing() && retry_count < kMaxRetries) {
    GetView()->CopyFromSurface(
        gfx::Rect(), gfx::Size(),
        base::BindOnce(&RenderWidgetHostImpl::OnSnapshotFromSurfaceReceived,
                       weak_factory_.GetWeakPtr(), snapshot_id,
                       retry_count + 1));
    return;
  }
  // If all retries have failed, we return an empty image.
  gfx::Image image;
  if (!bitmap.drawsNothing()) {
    image = gfx::Image::CreateFrom1xBitmap(bitmap);
  }
  // Any pending snapshots with a lower ID than the one received are considered
  // to be implicitly complete, and returned the same snapshot data.
  auto it = pending_surface_browser_snapshots_.begin();
  while (it != pending_surface_browser_snapshots_.end()) {
    if (it->first <= snapshot_id) {
      std::move(it->second).Run(image);
      pending_surface_browser_snapshots_.erase(it++);
    } else {
      ++it;
    }
  }
}

void RenderWidgetHostImpl::OnSnapshotReceived(int snapshot_id,
                                              gfx::Image image) {
  // Any pending snapshots with a lower ID than the one received are considered
  // to be implicitly complete, and returned the same snapshot data.
  auto it = pending_browser_snapshots_.begin();
  while (it != pending_browser_snapshots_.end()) {
    if (it->first <= snapshot_id) {
      std::move(it->second).Run(image);
      pending_browser_snapshots_.erase(it++);
    } else {
      ++it;
    }
  }
#if BUILDFLAG(IS_MAC)
  if (pending_browser_snapshots_.empty()) {
    GetWakeLock()->CancelWakeLock();
  }
#endif
}

BrowserAccessibilityManager*
RenderWidgetHostImpl::GetRootBrowserAccessibilityManager() {
  return delegate_ ? delegate_->GetRootBrowserAccessibilityManager() : nullptr;
}

BrowserAccessibilityManager*
RenderWidgetHostImpl::GetOrCreateRootBrowserAccessibilityManager() {
  return delegate_ ? delegate_->GetOrCreateRootBrowserAccessibilityManager()
                   : nullptr;
}

void RenderWidgetHostImpl::GrantFileAccessFromDropData(DropData* drop_data) {
  DCHECK_EQ(GetRoutingID(), drop_data->view_id);
  RenderProcessHost* process = GetProcess();
  PrepareDropDataForChildProcess(
      drop_data, ChildProcessSecurityPolicyImpl::GetInstance(),
      process->GetID(), process->GetStoragePartition()->GetFileSystemContext());
}

void RenderWidgetHostImpl::RequestCompositionUpdates(bool immediate_request,
                                                     bool monitor_updates) {
  if (!immediate_request && monitor_updates == monitoring_composition_info_) {
    return;
  }
  monitoring_composition_info_ = monitor_updates;
  GetWidgetInputHandler()->RequestCompositionUpdates(immediate_request,
                                                     monitor_updates);
}

void RenderWidgetHostImpl::CreateFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_receiver,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
        compositor_frame_sink_client) {
  // Connects the viz process end of CompositorFrameSink message pipes. The
  // renderer compositor may request a new CompositorFrameSink on context
  // loss, which will destroy the existing CompositorFrameSink.
  auto callback = base::BindOnce(
      [](mojo::PendingReceiver<viz::mojom::CompositorFrameSink> receiver,
         mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client,
         const viz::FrameSinkId& frame_sink_id) {
        GetHostFrameSinkManager()->CreateCompositorFrameSink(
            frame_sink_id, std::move(receiver), std::move(client));
      },
      std::move(compositor_frame_sink_receiver),
      std::move(compositor_frame_sink_client));

  if (view_) {
    std::move(callback).Run(view_->GetFrameSinkId());
  } else {
    create_frame_sink_callback_ = std::move(callback);
  }
}

void RenderWidgetHostImpl::RegisterRenderFrameMetadataObserver(
    mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_client_receiver,
    mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver>
        render_frame_metadata_observer) {
  render_frame_metadata_provider_.Bind(
      std::move(render_frame_metadata_observer_client_receiver),
      std::move(render_frame_metadata_observer));
}

bool RenderWidgetHostImpl::HasGestureStopped() {
  if (delegate_ && delegate_->GetInputEventRouter() &&
      delegate_->GetInputEventRouter()->HasEventsPendingDispatch()) {
    return false;
  }

  if (input_router_->HasPendingEvents()) {
    return false;
  }

  std::unique_ptr<RenderWidgetHostIterator> child_widgets(
      GetEmbeddedRenderWidgetHosts());
  while (RenderWidgetHost* child = child_widgets->GetNextHost()) {
    auto* child_impl = static_cast<RenderWidgetHostImpl*>(child);
    if (!child_impl->HasGestureStopped()) {
      return false;
    }
  }

  return true;
}

void RenderWidgetHostImpl::DidProcessFrame(uint32_t frame_token,
                                           base::TimeTicks activation_time) {
  frame_token_message_queue_->DidProcessFrame(frame_token, activation_time);
}

#if BUILDFLAG(IS_MAC)
device::mojom::WakeLock* RenderWidgetHostImpl::GetWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
    GetDeviceService().BindWakeLockProvider(
        wake_lock_provider.BindNewPipeAndPassReceiver());
    wake_lock_provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, "GetSnapshot",
        wake_lock_.BindNewPipeAndPassReceiver());
  }
  return wake_lock_.get();
}
#endif

void RenderWidgetHostImpl::SetupInputRouter() {
  in_flight_event_count_ = 0;
  // We are resetting in_flight_event_count_ so also inform the
  // BrowserUIThreadScheduler that we are no longer processing input.
  user_input_active_handle_.reset();
  suppress_events_until_keydown_ = false;
  monitoring_composition_info_ = false;
  StopInputEventAckTimeout();

  input_router_ = std::make_unique<InputRouterImpl>(
      this, this, fling_scheduler_.get(), GetInputRouterConfigForPlatform());

  // input_router_ recreated, need to update the force_enable_zoom_ state.
  input_router_->SetForceEnableZoom(force_enable_zoom_);
  input_router_->SetDeviceScaleFactor(GetScaleFactorForView(view_.get()));
}

void RenderWidgetHostImpl::SetForceEnableZoom(bool enabled) {
  force_enable_zoom_ = enabled;
  input_router_->SetForceEnableZoom(enabled);
}

void RenderWidgetHostImpl::SetInputTargetClientForTesting(
    mojo::Remote<viz::mojom::InputTargetClient> input_target_client) {
  input_target_client_ = std::move(input_target_client);
}

void RenderWidgetHostImpl::ProgressFlingIfNeeded(base::TimeTicks current_time) {
  fling_scheduler_->ProgressFlingOnBeginFrameIfneeded(current_time);
}

void RenderWidgetHostImpl::ForceFirstFrameAfterNavigationTimeout() {
  if (!IsContentRenderingTimeoutRunning()) {
    return;
  }
  new_content_rendering_timeout_->Stop();
  ClearDisplayedGraphics();
}

void RenderWidgetHostImpl::StopFling() {
  input_router_->StopFling();
}

void RenderWidgetHostImpl::SetScreenOrientationForTesting(
    uint16_t angle,
    display::mojom::ScreenOrientation type) {
  screen_orientation_angle_for_testing_ = angle;
  screen_orientation_type_for_testing_ = type;
  SynchronizeVisualProperties();
}

bool RenderWidgetHostImpl::LockKeyboard() {
  if (!keyboard_lock_allowed_ || !is_focused_ || !view_) {
    return false;
  }

  // KeyboardLock can be activated and deactivated several times per request,
  // for example when a fullscreen tab loses and gains focus multiple times,
  // so we need to retain a copy of the keys requested.
  absl::optional<base::flat_set<ui::DomCode>> copy = keyboard_keys_to_lock_;
  return view_->LockKeyboard(std::move(copy));
}

void RenderWidgetHostImpl::UnlockKeyboard() {
  if (IsKeyboardLocked()) {
    view_->UnlockKeyboard();
  }
}

void RenderWidgetHostImpl::OnRenderFrameMetadataChangedBeforeActivation(
    const cc::RenderFrameMetadata& metadata) {}

void RenderWidgetHostImpl::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  const auto& metadata =
      render_frame_metadata_provider_.LastRenderFrameMetadata();

  bool is_mobile_optimized = metadata.is_mobile_optimized;
  input_router_->NotifySiteIsMobileOptimized(is_mobile_optimized);
  if (auto* touch_emulator = GetExistingTouchEmulator()) {
    touch_emulator->SetDoubleTapSupportForPageEnabled(!is_mobile_optimized);
  }

  // TODO(danakj): Can this method be called during WebContents destruction?
  if (!delegate()) {
    return;
  }

  // The root BrowserAccessibilityManager only is reachable if there's a
  // delegate() still, ie we're not in shutdown. This can be null in tests.
  BrowserAccessibilityManager* accessibility_manager =
      GetRootBrowserAccessibilityManager();
  if (accessibility_manager) {
    accessibility_manager->SetPageScaleFactor(metadata.page_scale_factor);
  }

  // The value |kNull| is only used to indicate an absence of vertical scroll
  // direction and should therefore be ignored.
  // Also, changes in vertical scroll direction are only propagated for main
  // frames. If there is no |owner_delegate|, this is not a main frame.
  if (metadata.new_vertical_scroll_direction !=
          viz::VerticalScrollDirection::kNull &&
      owner_delegate()) {
    delegate()->OnVerticalScrollDirectionChanged(
        metadata.new_vertical_scroll_direction);
  }
}

std::vector<viz::SurfaceId>
RenderWidgetHostImpl::CollectSurfaceIdsForEviction() {
  RenderViewHostImpl* rvh = RenderViewHostImpl::From(this);
  // A corresponding RenderViewHostImpl may not exist in unit tests.
  if (!rvh) {
    return {};
  }
  return rvh->CollectSurfaceIdsForEviction();
}

std::unique_ptr<RenderWidgetHostIterator>
RenderWidgetHostImpl::GetEmbeddedRenderWidgetHosts() {
  // This iterates over all RenderWidgetHosts and returns those whose Views
  // are children of this host's View.
  auto hosts = std::make_unique<RenderWidgetHostIteratorImpl>();
  auto* parent_view = static_cast<RenderWidgetHostViewBase*>(GetView());
  for (auto& it : g_routing_id_widget_map.Get()) {
    RenderWidgetHost* widget = it.second;

    auto* view = static_cast<RenderWidgetHostViewBase*>(widget->GetView());
    if (view && view->IsRenderWidgetHostViewChildFrame() &&
        static_cast<RenderWidgetHostViewChildFrame*>(view)->GetParentView() ==
            parent_view) {
      hosts->Add(widget);
    }
  }

  return std::move(hosts);
}

namespace {

bool TransformPointAndRectToRootView(RenderWidgetHostViewBase* view,
                                     RenderWidgetHostViewBase* root_view,
                                     gfx::Point* transformed_point,
                                     gfx::Rect* transformed_rect) {
  gfx::Transform transform_to_main_frame;
  if (!view->GetTransformToViewCoordSpace(root_view,
                                          &transform_to_main_frame)) {
    return false;
  }

  if (transformed_point) {
    *transformed_point = transform_to_main_frame.MapPoint(*transformed_point);
  }

  if (transformed_rect) {
    *transformed_rect = transform_to_main_frame.MapRect(*transformed_rect);
  }

  return true;
}

}  // namespace

void RenderWidgetHostImpl::AnimateDoubleTapZoomInMainFrame(
    const gfx::Point& point,
    const gfx::Rect& rect_to_zoom) {
  if (!view_) {
    return;
  }

  auto* root_view = view_->GetRootView();
  gfx::Point transformed_point(point);
  gfx::Rect transformed_rect_to_zoom(rect_to_zoom);
  if (!TransformPointAndRectToRootView(view_.get(), root_view,
                                       &transformed_point,
                                       &transformed_rect_to_zoom)) {
    return;
  }

  auto* root_rvhi = RenderViewHostImpl::From(root_view->GetRenderWidgetHost());
  root_rvhi->AnimateDoubleTapZoom(transformed_point, transformed_rect_to_zoom);
}

void RenderWidgetHostImpl::ZoomToFindInPageRectInMainFrame(
    const gfx::Rect& rect_to_zoom) {
  if (!view_) {
    return;
  }

  auto* root_view = view_->GetRootView();
  gfx::Rect transformed_rect_to_zoom(rect_to_zoom);
  if (!TransformPointAndRectToRootView(view_.get(), root_view, nullptr,
                                       &transformed_rect_to_zoom)) {
    return;
  }

  auto* root_rvhi = RenderViewHostImpl::From(root_view->GetRenderWidgetHost());
  root_rvhi->ZoomToFindInPageRect(transformed_rect_to_zoom);
}

void RenderWidgetHostImpl::SetHasTouchEventConsumers(
    blink::mojom::TouchEventConsumersPtr consumers) {
  input_router_->OnHasTouchEventConsumers(std::move(consumers));
}

void RenderWidgetHostImpl::IntrinsicSizingInfoChanged(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {
  if (view_) {
    view_->UpdateIntrinsicSizingInfo(std::move(sizing_info));
  }
}

gfx::Size RenderWidgetHostImpl::GetRootWidgetViewportSize() {
  if (!view_) {
    return gfx::Size();
  }

  // if |view_| is RWHVCF and |frame_connector_| is destroyed, then call to
  // GetRootView will return null pointer.
  auto* root_view = view_->GetRootView();
  if (!root_view) {
    return gfx::Size();
  }

  return root_view->GetVisibleViewportSize();
}

// This method was copied from RenderWidget::ConvertWindowToViewport() when
// porting drag-and-drop calls to Mojo, so that RenderWidgetHostImpl bypasses
// RenderWidget to talk the the WebFrameWidget and needs to perform the scale
// operation itself.
gfx::PointF RenderWidgetHostImpl::ConvertWindowPointToViewport(
    const gfx::PointF& window_point) {
  gfx::PointF viewport_point = window_point;
  viewport_point.Scale(GetScaleFactorForView(GetView()));
  return viewport_point;
}

RenderWidgetHostImpl::MainFramePropagationProperties::
    MainFramePropagationProperties() = default;

RenderWidgetHostImpl::MainFramePropagationProperties::
    ~MainFramePropagationProperties() = default;

RenderWidgetHostImpl::PendingShowParams::PendingShowParams(
    bool is_evicted,
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request)
    : is_evicted(is_evicted),
      visible_time_request(std::move(visible_time_request)) {}

RenderWidgetHostImpl::PendingShowParams::~PendingShowParams() = default;

}  // namespace content
