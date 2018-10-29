// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/edit_command.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/common/visual_properties.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/fake_renderer_compositor_frame_sink.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "ui/android/screen_android.h"
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "content/browser/compositor/test/test_image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/events/event.h"
#endif

using base::TimeDelta;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

// MockInputRouter -------------------------------------------------------------

class MockInputRouter : public InputRouter {
 public:
  explicit MockInputRouter(InputRouterClient* client)
      : sent_mouse_event_(false),
        sent_wheel_event_(false),
        sent_keyboard_event_(false),
        sent_gesture_event_(false),
        send_touch_event_not_cancelled_(false),
        has_handlers_(false),
        client_(client) {}
  ~MockInputRouter() override {}

  // InputRouter
  void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event,
                      MouseEventCallback event_result_callback) override {
    sent_mouse_event_ = true;
  }
  void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {
    sent_wheel_event_ = true;
  }
  void SendKeyboardEvent(const NativeWebKeyboardEventWithLatencyInfo& key_event,
                         KeyboardEventCallback event_result_callback) override {
    sent_keyboard_event_ = true;
  }
  void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) override {
    sent_gesture_event_ = true;
  }
  void SendTouchEvent(const TouchEventWithLatencyInfo& touch_event) override {
    send_touch_event_not_cancelled_ =
        client_->FilterInputEvent(touch_event.event, touch_event.latency) ==
        INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized) override {}
  bool HasPendingEvents() const override { return false; }
  void SetDeviceScaleFactor(float device_scale_factor) override {}
  void SetFrameTreeNodeId(int frameTreeNodeId) override {}
  base::Optional<cc::TouchAction> AllowedTouchAction() override {
    return cc::kTouchActionAuto;
  }
  void SetForceEnableZoom(bool enabled) override {}
  void BindHost(mojom::WidgetInputHandlerHostRequest request,
                bool frame_handler) override {}
  void StopFling() override {}
  bool FlingCancellationIsDeferred() override { return false; }
  void OnSetTouchAction(cc::TouchAction touch_action) override {}
  void ForceSetTouchActionAuto() override {}
  void OnHasTouchEventHandlers(bool has_handlers) override {
    has_handlers_ = has_handlers;
  }

  bool sent_mouse_event_;
  bool sent_wheel_event_;
  bool sent_keyboard_event_;
  bool sent_gesture_event_;
  bool send_touch_event_not_cancelled_;
  bool has_handlers_;

 private:
  InputRouterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(MockInputRouter);
};

// TestFrameTokenMessageQueue ----------------------------------------------

class TestFrameTokenMessageQueue : public FrameTokenMessageQueue {
 public:
  explicit TestFrameTokenMessageQueue(FrameTokenMessageQueue::Client* client)
      : FrameTokenMessageQueue(client) {}
  ~TestFrameTokenMessageQueue() override {}

  uint32_t processed_frame_messages_count() {
    return processed_frame_messages_count_;
  }

 protected:
  void ProcessSwapMessages(std::vector<IPC::Message> messages) override {
    processed_frame_messages_count_++;
  }

 private:
  uint32_t processed_frame_messages_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTokenMessageQueue);
};

// MockRenderWidgetHost ----------------------------------------------------

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  // Allow poking at a few private members.
  using RenderWidgetHostImpl::GetVisualProperties;
  using RenderWidgetHostImpl::RendererExited;
  using RenderWidgetHostImpl::SetInitialVisualProperties;
  using RenderWidgetHostImpl::old_visual_properties_;
  using RenderWidgetHostImpl::is_hidden_;
  using RenderWidgetHostImpl::visual_properties_ack_pending_;
  using RenderWidgetHostImpl::input_router_;
  using RenderWidgetHostImpl::frame_token_message_queue_;

  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override {
    // Sniff touch acks.
    acked_touch_event_type_ = event.event.GetType();
    RenderWidgetHostImpl::OnTouchEventAck(event, ack_source, ack_result);
  }

  void reset_new_content_rendering_timeout_fired() {
    new_content_rendering_timeout_fired_ = false;
  }

  bool new_content_rendering_timeout_fired() const {
    return new_content_rendering_timeout_fired_;
  }

  void DisableGestureDebounce() {
    input_router_.reset(new InputRouterImpl(this, this, fling_scheduler_.get(),
                                            InputRouter::Config()));
  }

  void ExpectForceEnableZoom(bool enable) {
    EXPECT_EQ(enable, force_enable_zoom_);

    InputRouterImpl* input_router =
        static_cast<InputRouterImpl*>(input_router_.get());
    EXPECT_EQ(enable, input_router->touch_action_filter_.force_enable_zoom_);
  }

  WebInputEvent::Type acked_touch_event_type() const {
    return acked_touch_event_type_;
  }

  // Mocks out |renderer_compositor_frame_sink_| with a
  // CompositorFrameSinkClientPtr bound to
  // |mock_renderer_compositor_frame_sink|.
  void SetMockRendererCompositorFrameSink(
      viz::MockCompositorFrameSinkClient* mock_renderer_compositor_frame_sink) {
    renderer_compositor_frame_sink_ =
        mock_renderer_compositor_frame_sink->BindInterfacePtr();
  }

  void SetupForInputRouterTest() {
    input_router_.reset(new MockInputRouter(this));
  }

  MockInputRouter* mock_input_router() {
    return static_cast<MockInputRouter*>(input_router_.get());
  }

  InputRouter* input_router() { return input_router_.get(); }

  uint32_t processed_frame_messages_count() {
    CHECK(frame_token_message_queue_);
    return static_cast<TestFrameTokenMessageQueue*>(
               frame_token_message_queue_.get())
        ->processed_frame_messages_count();
  }

  static MockRenderWidgetHost* Create(RenderWidgetHostDelegate* delegate,
                                      RenderProcessHost* process,
                                      int32_t routing_id) {
    mojom::WidgetPtr widget;
    std::unique_ptr<MockWidgetImpl> widget_impl =
        std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));

    return new MockRenderWidgetHost(delegate, process, routing_id,
                                    std::move(widget_impl), std::move(widget));
  }

  mojom::WidgetInputHandler* GetWidgetInputHandler() override {
    return &mock_widget_input_handler_;
  }

  MockWidgetInputHandler mock_widget_input_handler_;

 protected:
  void NotifyNewContentRenderingTimeoutForTesting() override {
    new_content_rendering_timeout_fired_ = true;
  }

  bool new_content_rendering_timeout_fired_;
  WebInputEvent::Type acked_touch_event_type_;

 private:
  MockRenderWidgetHost(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int routing_id,
                       std::unique_ptr<MockWidgetImpl> widget_impl,
                       mojom::WidgetPtr widget)
      : RenderWidgetHostImpl(delegate,
                             process,
                             routing_id,
                             std::move(widget),
                             false),
        new_content_rendering_timeout_fired_(false),
        widget_impl_(std::move(widget_impl)),
        fling_scheduler_(std::make_unique<FlingScheduler>(this)) {
    acked_touch_event_type_ = blink::WebInputEvent::kUndefined;
    frame_token_message_queue_.reset(new TestFrameTokenMessageQueue(this));
  }

  std::unique_ptr<MockWidgetImpl> widget_impl_;

  std::unique_ptr<FlingScheduler> fling_scheduler_;
  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHost);
};

namespace  {

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {
  }
  ~RenderWidgetHostProcess() override {}

  bool IsInitializedAndNotDead() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostProcess);
};

// TestView --------------------------------------------------------------------

// This test view allows us to specify the size, and keep track of acked
// touch-events.
class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh),
        unhandled_wheel_event_count_(0),
        acked_event_count_(0),
        gesture_event_type_(-1),
        use_fake_compositor_viewport_pixel_size_(false),
        ack_result_(INPUT_EVENT_ACK_STATE_UNKNOWN),
        top_controls_height_(0.f),
        bottom_controls_height_(0.f) {}

  // Sets the bounds returned by GetViewBounds.
  void SetBounds(const gfx::Rect& bounds) override {
    if (bounds_ == bounds)
      return;
    bounds_ = bounds;
    local_surface_id_allocator_.GenerateId();
  }

  void SetScreenInfo(const ScreenInfo& screen_info) {
    if (screen_info_ == screen_info)
      return;
    screen_info_ = screen_info;
    local_surface_id_allocator_.GenerateId();
  }

  void InvalidateLocalSurfaceId() { local_surface_id_allocator_.Invalidate(); }

  void GetScreenInfo(ScreenInfo* screen_info) const override {
    *screen_info = screen_info_;
  }

  void set_top_controls_height(float top_controls_height) {
    top_controls_height_ = top_controls_height;
  }

  void set_bottom_controls_height(float bottom_controls_height) {
    bottom_controls_height_ = bottom_controls_height;
  }

  const WebTouchEvent& acked_event() const { return acked_event_; }
  int acked_event_count() const { return acked_event_count_; }
  void ClearAckedEvent() {
    acked_event_.SetType(blink::WebInputEvent::kUndefined);
    acked_event_count_ = 0;
  }

  const WebMouseWheelEvent& unhandled_wheel_event() const {
    return unhandled_wheel_event_;
  }
  int unhandled_wheel_event_count() const {
    return unhandled_wheel_event_count_;
  }
  int gesture_event_type() const { return gesture_event_type_; }
  InputEventAckState ack_result() const { return ack_result_; }

  void SetMockCompositorViewportPixelSize(
      const gfx::Size& mock_compositor_viewport_pixel_size) {
    if (use_fake_compositor_viewport_pixel_size_ &&
        mock_compositor_viewport_pixel_size_ ==
            mock_compositor_viewport_pixel_size) {
      return;
    }
    use_fake_compositor_viewport_pixel_size_ = true;
    mock_compositor_viewport_pixel_size_ = mock_compositor_viewport_pixel_size;
    local_surface_id_allocator_.GenerateId();
  }
  void ClearMockCompositorViewportPixelSize() {
    if (!use_fake_compositor_viewport_pixel_size_)
      return;
    use_fake_compositor_viewport_pixel_size_ = false;
    local_surface_id_allocator_.GenerateId();
  }

  const viz::BeginFrameAck& last_did_not_produce_frame_ack() {
    return last_did_not_produce_frame_ack_;
  }

  // RenderWidgetHostView override.
  gfx::Rect GetViewBounds() const override { return bounds_; }
  float GetTopControlsHeight() const override { return top_controls_height_; }
  float GetBottomControlsHeight() const override {
    return bottom_controls_height_;
  }
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override {
    return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  }

  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override {
    acked_event_ = touch.event;
    ++acked_event_count_;
  }
  void WheelEventAck(const WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      return;
    unhandled_wheel_event_count_++;
    unhandled_wheel_event_ = event;
  }
  void GestureEventAck(const WebGestureEvent& event,
                       InputEventAckState ack_result) override {
    gesture_event_type_ = event.GetType();
    ack_result_ = ack_result;
  }
  gfx::Size GetCompositorViewportPixelSize() const override {
    if (use_fake_compositor_viewport_pixel_size_)
      return mock_compositor_viewport_pixel_size_;
    return TestRenderWidgetHostView::GetCompositorViewportPixelSize();
  }
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack) override {
    last_did_not_produce_frame_ack_ = ack;
  }

 protected:
  WebMouseWheelEvent unhandled_wheel_event_;
  int unhandled_wheel_event_count_;
  WebTouchEvent acked_event_;
  int acked_event_count_;
  int gesture_event_type_;
  gfx::Rect bounds_;
  bool use_fake_compositor_viewport_pixel_size_;
  gfx::Size mock_compositor_viewport_pixel_size_;
  InputEventAckState ack_result_;
  float top_controls_height_;
  float bottom_controls_height_;
  viz::BeginFrameAck last_did_not_produce_frame_ack_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  ScreenInfo screen_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// MockRenderViewHostDelegateView ------------------------------------------
class MockRenderViewHostDelegateView : public RenderViewHostDelegateView {
 public:
  MockRenderViewHostDelegateView() = default;
  ~MockRenderViewHostDelegateView() override = default;

  int start_dragging_count() const { return start_dragging_count_; }

  // RenderViewHostDelegateView:
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override {
    ++start_dragging_count_;
  }

 private:
  int start_dragging_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockRenderViewHostDelegateView);
};

// FakeRenderFrameMetadataObserver -----------------------------------------

// Fake out the renderer side of mojom::RenderFrameMetadataObserver, allowing
// for RenderWidgetHostImpl to be created.
//
// All methods are no-opts, the provided mojo request and info are held, but
// never bound.
class FakeRenderFrameMetadataObserver
    : public mojom::RenderFrameMetadataObserver {
 public:
  FakeRenderFrameMetadataObserver(
      mojom::RenderFrameMetadataObserverRequest request,
      mojom::RenderFrameMetadataObserverClientPtrInfo client_info);
  ~FakeRenderFrameMetadataObserver() override {}

  void ReportAllFrameSubmissionsForTesting(bool enabled) override {}

 private:
  mojom::RenderFrameMetadataObserverRequest request_;
  mojom::RenderFrameMetadataObserverClientPtrInfo client_info_;
  DISALLOW_COPY_AND_ASSIGN(FakeRenderFrameMetadataObserver);
};

FakeRenderFrameMetadataObserver::FakeRenderFrameMetadataObserver(
    mojom::RenderFrameMetadataObserverRequest request,
    mojom::RenderFrameMetadataObserverClientPtrInfo client_info)
    : request_(std::move(request)), client_info_(std::move(client_info)) {}

// MockRenderWidgetHostDelegate --------------------------------------------

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : prehandle_keyboard_event_(false),
        prehandle_keyboard_event_is_shortcut_(false),
        prehandle_keyboard_event_called_(false),
        prehandle_keyboard_event_type_(WebInputEvent::kUndefined),
        unhandled_keyboard_event_called_(false),
        unhandled_keyboard_event_type_(WebInputEvent::kUndefined),
        handle_wheel_event_(false),
        handle_wheel_event_called_(false),
        unresponsive_timer_fired_(false),
        ignore_input_events_(false),
        render_view_host_delegate_view_(new MockRenderViewHostDelegateView()) {}
  ~MockRenderWidgetHostDelegate() override {}

  // Tests that make sure we ignore keyboard event acknowledgments to events we
  // didn't send work by making sure we didn't call UnhandledKeyboardEvent().
  bool unhandled_keyboard_event_called() const {
    return unhandled_keyboard_event_called_;
  }

  WebInputEvent::Type unhandled_keyboard_event_type() const {
    return unhandled_keyboard_event_type_;
  }

  bool prehandle_keyboard_event_called() const {
    return prehandle_keyboard_event_called_;
  }

  WebInputEvent::Type prehandle_keyboard_event_type() const {
    return prehandle_keyboard_event_type_;
  }

  void set_prehandle_keyboard_event(bool handle) {
    prehandle_keyboard_event_ = handle;
  }

  void set_handle_wheel_event(bool handle) {
    handle_wheel_event_ = handle;
  }

  void set_prehandle_keyboard_event_is_shortcut(bool is_shortcut) {
    prehandle_keyboard_event_is_shortcut_ = is_shortcut;
  }

  bool handle_wheel_event_called() const { return handle_wheel_event_called_; }

  bool unresponsive_timer_fired() const { return unresponsive_timer_fired_; }

  MockRenderViewHostDelegateView* mock_delegate_view() {
    return render_view_host_delegate_view_.get();
  }

  void SetZoomLevel(double zoom_level) { zoom_level_ = zoom_level; }

  double GetPendingPageZoomLevel() const override { return zoom_level_; }

  void FocusOwningWebContents(
      RenderWidgetHostImpl* render_widget_host) override {
    focus_owning_web_contents_call_count++;
  }

  int GetFocusOwningWebContentsCallCount() const {
    return focus_owning_web_contents_call_count;
  }

  RenderViewHostDelegateView* GetDelegateView() override {
    return mock_delegate_view();
  }

  void SetIgnoreInputEvents(bool ignore_input_events) {
    ignore_input_events_ = ignore_input_events;
  }

 protected:
  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) override {
    prehandle_keyboard_event_type_ = event.GetType();
    prehandle_keyboard_event_called_ = true;
    if (prehandle_keyboard_event_)
      return KeyboardEventProcessingResult::HANDLED;
    return prehandle_keyboard_event_is_shortcut_
               ? KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT
               : KeyboardEventProcessingResult::NOT_HANDLED;
  }

  bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event) override {
    unhandled_keyboard_event_type_ = event.GetType();
    unhandled_keyboard_event_called_ = true;
    return true;
  }

  bool HandleWheelEvent(const blink::WebMouseWheelEvent& event) override {
    handle_wheel_event_called_ = true;
    return handle_wheel_event_;
  }

  void RendererUnresponsive(
      RenderWidgetHostImpl* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override {
    unresponsive_timer_fired_ = true;
  }

  bool ShouldIgnoreInputEvents() override { return ignore_input_events_; }

  void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) override {}

  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}

 private:
  bool prehandle_keyboard_event_;
  bool prehandle_keyboard_event_is_shortcut_;
  bool prehandle_keyboard_event_called_;
  WebInputEvent::Type prehandle_keyboard_event_type_;

  bool unhandled_keyboard_event_called_;
  WebInputEvent::Type unhandled_keyboard_event_type_;

  bool handle_wheel_event_;
  bool handle_wheel_event_called_;

  bool unresponsive_timer_fired_;

  bool ignore_input_events_;

  std::unique_ptr<MockRenderViewHostDelegateView>
      render_view_host_delegate_view_;

  double zoom_level_ = 0;

  int focus_owning_web_contents_call_count = 0;
};

// RenderWidgetHostTest --------------------------------------------------------

class RenderWidgetHostTest : public testing::Test {
 public:
  RenderWidgetHostTest()
      : process_(nullptr),
        handle_key_press_event_(false),
        handle_mouse_event_(false),
        last_simulated_event_time_(ui::EventTimeForNow()) {
    std::vector<base::StringPiece> features;
    std::vector<base::StringPiece> disabled_features;
    features.push_back(features::kVsyncAlignedInputEvents.name);

    feature_list_.InitFromCommandLine(base::JoinString(features, ","),
                                      base::JoinString(disabled_features, ","));
  }
  ~RenderWidgetHostTest() override {}

  bool KeyPressEventCallback(const NativeWebKeyboardEvent& /* event */) {
    return handle_key_press_event_;
  }
  bool MouseEventCallback(const blink::WebMouseEvent& /* event */) {
    return handle_mouse_event_;
  }

  void RunLoopFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

 protected:
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    browser_context_.reset(new TestBrowserContext());
    delegate_.reset(new MockRenderWidgetHostDelegate());
    process_ = new RenderWidgetHostProcess(browser_context_.get());
    sink_ = &process_->sink();
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif
#if defined(OS_ANDROID)
    ui::SetScreenAndroid();  // calls display::Screen::SetScreenInstance().
#endif
#if defined(USE_AURA)
    screen_.reset(aura::TestScreen::Create(gfx::Size()));
    display::Screen::SetScreenInstance(screen_.get());
#endif
    host_.reset(MockRenderWidgetHost::Create(delegate_.get(), process_,
                                             process_->GetNextRoutingID()));
    view_.reset(new TestView(host_.get()));
    ConfigureView(view_.get());
    host_->SetView(view_.get());
    SetInitialVisualProperties();
    host_->Init();
    host_->DisableGestureDebounce();

    viz::mojom::CompositorFrameSinkPtr sink;
    viz::mojom::CompositorFrameSinkRequest sink_request =
        mojo::MakeRequest(&sink);
    viz::mojom::CompositorFrameSinkClientRequest client_request =
        mojo::MakeRequest(&renderer_compositor_frame_sink_ptr_);
    renderer_compositor_frame_sink_ =
        std::make_unique<FakeRendererCompositorFrameSink>(
            std::move(sink), std::move(client_request));

    mojom::RenderFrameMetadataObserverPtr
        renderer_render_frame_metadata_observer_ptr;
    mojom::RenderFrameMetadataObserverRequest
        render_frame_metadata_observer_request =
            mojo::MakeRequest(&renderer_render_frame_metadata_observer_ptr);
    mojom::RenderFrameMetadataObserverClientPtrInfo
        render_frame_metadata_observer_client_info;
    mojom::RenderFrameMetadataObserverClientRequest
        render_frame_metadata_observer_client_request =
            mojo::MakeRequest(&render_frame_metadata_observer_client_info);
    renderer_render_frame_metadata_observer_ =
        std::make_unique<FakeRenderFrameMetadataObserver>(
            std::move(render_frame_metadata_observer_request),
            std::move(render_frame_metadata_observer_client_info));

    host_->RequestCompositorFrameSink(
        std::move(sink_request),
        std::move(renderer_compositor_frame_sink_ptr_));
    host_->RegisterRenderFrameMetadataObserver(
        std::move(render_frame_metadata_observer_client_request),
        std::move(renderer_render_frame_metadata_observer_ptr));
  }

  void TearDown() override {
    view_.reset();
    host_.reset();
    delegate_.reset();
    process_ = nullptr;
    browser_context_.reset();

#if defined(USE_AURA)
    display::Screen::SetScreenInstance(nullptr);
    screen_.reset();
#endif
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::Terminate();
#endif
#if defined(OS_ANDROID)
    display::Screen::SetScreenInstance(nullptr);
#endif

    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();
  }

  void SetInitialVisualProperties() {
    VisualProperties visual_properties;
    bool needs_ack = false;
    host_->GetVisualProperties(&visual_properties, &needs_ack);
    host_->SetInitialVisualProperties(visual_properties, needs_ack);
  }

  virtual void ConfigureView(TestView* view) {
  }

  base::TimeTicks GetNextSimulatedEventTime() {
    last_simulated_event_time_ += simulated_event_time_delta_;
    return last_simulated_event_time_;
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    SimulateKeyboardEvent(type, 0);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type, int modifiers) {
    NativeWebKeyboardEvent native_event(type, modifiers,
                                        GetNextSimulatedEventTime());
    host_->ForwardKeyboardEvent(native_event);
  }

  void SimulateKeyboardEventWithCommands(WebInputEvent::Type type) {
    NativeWebKeyboardEvent native_event(type, 0, GetNextSimulatedEventTime());
    EditCommands commands;
    commands.emplace_back("name", "value");
    host_->ForwardKeyboardEventWithCommands(native_event, ui::LatencyInfo(),
                                            &commands, nullptr);
  }

  void SimulateMouseEvent(WebInputEvent::Type type) {
    host_->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(type));
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardMouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type),
        ui_latency);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    host_->ForwardWheelEvent(SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise));
  }

  void SimulateWheelEvent(float dX,
                          float dY,
                          int modifiers,
                          bool precise,
                          WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise);
    wheel_event.phase = phase;
    host_->ForwardWheelEvent(wheel_event);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(0, 0, dX, dY, modifiers,
                                                  precise),
        ui_latency);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency,
                                         WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise);
    wheel_event.phase = phase;
    host_->ForwardWheelEventWithLatencyInfo(wheel_event, ui_latency);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    SimulateMouseEvent(WebInputEvent::kMouseMove, x, y, modifiers, false);
  }

  void SimulateMouseEvent(
      WebInputEvent::Type type, int x, int y, int modifiers, bool pressed) {
    WebMouseEvent event =
        SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed)
      event.button = WebMouseEvent::Button::kLeft;
    event.SetTimeStamp(GetNextSimulatedEventTime());
    host_->ForwardMouseEvent(event);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    host_->ForwardGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(WebInputEvent::Type type,
                                           WebGestureDevice sourceDevice,
                                           const ui::LatencyInfo& ui_latency) {
    host_->ForwardGestureEventWithLatencyInfo(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice), ui_latency);
  }

  // Set the timestamp for the touch-event.
  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    host_->ForwardTouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo());

    touch_event_.ResetPoints();
    return touch_event_id;
  }

  int PressTouchPoint(int x, int y) {
    return touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
    base::PickleIterator iter(message);
    const char* data;
    int data_length;
    if (!iter.ReadData(&data, &data_length))
      return nullptr;
    return reinterpret_cast<const WebInputEvent*>(data);
  }

  std::unique_ptr<TestBrowserContext> browser_context_;
  RenderWidgetHostProcess* process_;  // Deleted automatically by the widget.
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  std::unique_ptr<MockRenderWidgetHost> host_;
  std::unique_ptr<TestView> view_;
  std::unique_ptr<display::Screen> screen_;
  bool handle_key_press_event_;
  bool handle_mouse_event_;
  base::TimeTicks last_simulated_event_time_;
  base::TimeDelta simulated_event_time_delta_;
  IPC::TestSink* sink_;
  std::unique_ptr<FakeRendererCompositorFrameSink>
      renderer_compositor_frame_sink_;
  std::unique_ptr<FakeRenderFrameMetadataObserver>
      renderer_render_frame_metadata_observer_;

 private:
  SyntheticWebTouchEvent touch_event_;

  TestBrowserThreadBundle thread_bundle_;
  base::test::ScopedFeatureList feature_list_;
  viz::mojom::CompositorFrameSinkClientPtr renderer_compositor_frame_sink_ptr_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostTest);
};

// RenderWidgetHostWithSourceTest ----------------------------------------------

// This is for tests that are to be run for all source devices.
class RenderWidgetHostWithSourceTest
    : public RenderWidgetHostTest,
      public testing::WithParamInterface<WebGestureDevice> {};

}  // namespace

// -----------------------------------------------------------------------------

TEST_F(RenderWidgetHostTest, SynchronizeVisualProperties) {
  // The initial zoom is 0 so host should not send a sync message
  delegate_->SetZoomLevel(0);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // The zoom has changed so host should send out a sync message
  process_->sink().ClearMessages();
  double new_zoom_level = content::ZoomFactorToZoomLevel(0.25);
  delegate_->SetZoomLevel(new_zoom_level);
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_NEAR(new_zoom_level, host_->old_visual_properties_->zoom_level, 0.01);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // The initial bounds is the empty rect, so setting it to the same thing
  // shouldn't send the resize message.
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // No visual properties ACK if the physical backing gets set, but the view
  // bounds are zero.
  view_->SetMockCompositorViewportPixelSize(gfx::Size(200, 200));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  // Setting the view bounds to nonzero should send out the notification.
  // but should not expect ack for empty physical backing size.
  gfx::Rect original_size(0, 0, 100, 100);
  process_->sink().ClearMessages();
  view_->SetBounds(original_size);
  view_->SetMockCompositorViewportPixelSize(gfx::Size());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // Setting the bounds and physical backing size to nonzero should send out
  // the notification and expect an ack.
  process_->sink().ClearMessages();
  view_->ClearMockCompositorViewportPixelSize();
  host_->SynchronizeVisualProperties();
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = original_size.size();
  metadata.local_surface_id = base::nullopt;
  host_->DidUpdateVisualProperties(metadata);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  process_->sink().ClearMessages();
  gfx::Rect second_size(0, 0, 110, 110);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  view_->SetBounds(second_size);
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  EXPECT_TRUE(host_->visual_properties_ack_pending_);

  // Sending out a new notification should NOT send out a new IPC message since
  // a visual properties ACK is pending.
  gfx::Rect third_size(0, 0, 120, 120);
  process_->sink().ClearMessages();
  view_->SetBounds(third_size);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // Send a update that's a visual properties ACK, but for the original_size we
  // sent. Since this isn't the second_size, the message handler should
  // immediately send a new resize message for the new size to the renderer.
  process_->sink().ClearMessages();
  metadata.viewport_size_in_pixels = original_size.size();
  metadata.local_surface_id = base::nullopt;
  host_->DidUpdateVisualProperties(metadata);
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // Send the visual properties ACK for the latest size.
  process_->sink().ClearMessages();
  metadata.viewport_size_in_pixels = third_size.size();
  metadata.local_surface_id = base::nullopt;
  host_->DidUpdateVisualProperties(metadata);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // Now clearing the bounds should send out a notification but we shouldn't
  // expect a visual properties ACK (since the renderer won't ack empty sizes).
  // The message should contain the new size (0x0) and not the previous one that
  // we skipped.
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // Send a rect that has no area but has either width or height set.
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect(0, 0, 0, 30));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // Set the same size again. It should not be sent again.
  process_->sink().ClearMessages();
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_visual_properties_->new_size);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // A different size should be sent again, however.
  view_->SetBounds(gfx::Rect(0, 0, 0, 31));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 31), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // An invalid LocalSurfaceId should result in no change to the
  // |visual_properties_ack_pending_| bit.
  process_->sink().ClearMessages();
  view_->SetBounds(gfx::Rect(25, 25));
  view_->InvalidateLocalSurfaceId();
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(25, 25), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
}

// Test that a resize event is sent if SynchronizeVisualProperties() is called
// after a ScreenInfo change.
TEST_F(RenderWidgetHostTest, ResizeScreenInfo) {
  ScreenInfo screen_info;
  screen_info.device_scale_factor = 1.f;
  screen_info.rect = blink::WebRect(0, 0, 800, 600);
  screen_info.available_rect = blink::WebRect(0, 0, 800, 600);
  screen_info.orientation_angle = 0;
  screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;

  view_->SetScreenInfo(screen_info);
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
  process_->sink().ClearMessages();

  screen_info.orientation_angle = 180;
  screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;

  view_->SetScreenInfo(screen_info);
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
  process_->sink().ClearMessages();

  screen_info.device_scale_factor = 2.f;

  view_->SetScreenInfo(screen_info);
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
  process_->sink().ClearMessages();

  // No screen change.
  view_->SetScreenInfo(screen_info);
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
}

// Test for crbug.com/25097.  If a renderer crashes between a resize and the
// corresponding update message, we must be sure to clear the visual properties
// ACK logic.
TEST_F(RenderWidgetHostTest, ResizeThenCrash) {
  // Clear the first Resize message that carried screen info.
  process_->sink().ClearMessages();

  // Setting the bounds to a "real" rect should send out the notification.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->SetBounds(original_size);
  host_->SynchronizeVisualProperties();
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));

  // Simulate a renderer crash before the update message.  Ensure all the
  // visual properties ACK logic is cleared.  Must clear the view first so it
  // doesn't get deleted.
  host_->SetView(nullptr);
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(nullptr, host_->old_visual_properties_);

  // Reset the view so we can exit the test cleanly.
  host_->SetView(view_.get());
}

// Unable to include render_widget_host_view_mac.h and compile.
#if !defined(OS_MACOSX)
// Tests setting background transparency.
TEST_F(RenderWidgetHostTest, Background) {
  std::unique_ptr<RenderWidgetHostViewBase> view;
#if defined(USE_AURA)
  view.reset(new RenderWidgetHostViewAura(
      host_.get(), false, false /* is_mus_browser_plugin_guest */));
  // TODO(derat): Call this on all platforms: http://crbug.com/102450.
  view->InitAsChild(nullptr);
#elif defined(OS_ANDROID)
  view.reset(new RenderWidgetHostViewAndroid(host_.get(), NULL));
#endif
  host_->SetView(view.get());

  ASSERT_FALSE(view->GetBackgroundColor());
  view->SetBackgroundColor(SK_ColorTRANSPARENT);
  ASSERT_TRUE(view->GetBackgroundColor());
  EXPECT_EQ(static_cast<unsigned>(SK_ColorTRANSPARENT),
            *view->GetBackgroundColor());

  const IPC::Message* set_background =
      process_->sink().GetUniqueMessageMatching(
          WidgetMsg_SetBackgroundOpaque::ID);
  ASSERT_TRUE(set_background);
  std::tuple<bool> sent_background;
  WidgetMsg_SetBackgroundOpaque::Read(set_background, &sent_background);
  EXPECT_FALSE(std::get<0>(sent_background));

  host_->SetView(nullptr);
  static_cast<RenderWidgetHostViewBase*>(view.release())->Destroy();
}
#endif

// Test that the RenderWidgetHost tells the renderer when it is hidden and
// shown, and can accept a racey update from the renderer after hiding.
TEST_F(RenderWidgetHostTest, HideShowMessages) {
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  host_->WasHidden();
  EXPECT_TRUE(host_->is_hidden_);
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(WidgetMsg_WasHidden::ID));

  // Send it an update as from the renderer.
  process_->sink().ClearMessages();
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = gfx::Size(100, 100);
  metadata.local_surface_id = base::nullopt;
  host_->DidUpdateVisualProperties(metadata);

  // Now unhide.
  process_->sink().ClearMessages();
  host_->WasShown(false /* record_presentation_time */);
  EXPECT_FALSE(host_->is_hidden_);

  // It should have sent out a restored message.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(WidgetMsg_WasShown::ID));
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(delegate_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, SendEditCommandsBeforeKeyEvent) {
  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::kRawKeyDown);

  // Make sure we sent commands and key event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());

  ASSERT_TRUE(dispatched_events[0]->ToEditCommand());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  // Send the simulated response from the renderer back.
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
}

TEST_F(RenderWidgetHostTest, PreHandleRawKeyDownEvent) {
  // Simulate the situation that the browser handled the key down event during
  // pre-handle phrase.
  delegate_->set_prehandle_keyboard_event(true);

  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the commands and key event are not sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::kChar);

  // Make sure the Char event is suppressed.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::kKeyUp);

  // Make sure the KeyUp event is suppressed.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Simulate a new RawKeyDown event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(delegate_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, RawKeyDownShortcutEvent) {
  // Simulate the situation that the browser marks the key down as a keyboard
  // shortcut, but doesn't consume it in the pre-handle phase.
  delegate_->set_prehandle_keyboard_event_is_shortcut(true);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event_is_shortcut(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::kChar);

  // The Char event is not suppressed; the renderer will ignore it
  // if the preceding RawKeyDown shortcut goes unhandled.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kChar,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kChar, delegate_->unhandled_keyboard_event_type());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::kKeyUp);

  // Make sure only KeyUp was sent to the renderer.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kKeyUp,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kKeyUp, delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, UnhandledWheelEvent) {
  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(delegate_->handle_wheel_event_called());
  EXPECT_EQ(1, view_->unhandled_wheel_event_count());
  EXPECT_EQ(-5, view_->unhandled_wheel_event().delta_x);
}

TEST_F(RenderWidgetHostTest, HandleWheelEvent) {
  // Indicate that we're going to handle this wheel event
  delegate_->set_handle_wheel_event(true);

  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // ensure the wheel event handler was invoked
  EXPECT_TRUE(delegate_->handle_wheel_event_called());

  // and that it suppressed the unhandled wheel event handler.
  EXPECT_EQ(0, view_->unhandled_wheel_event_count());
}

TEST_F(RenderWidgetHostTest, EventsCausingFocus) {
  SimulateMouseEvent(WebInputEvent::kMouseDown);
  EXPECT_EQ(1, delegate_->GetFocusOwningWebContentsCallCount());

  PressTouchPoint(0, 1);
  SendTouchEvent();
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  SimulateGestureEvent(WebInputEvent::kGestureTap,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(3, delegate_->GetFocusOwningWebContentsCallCount());
}

TEST_F(RenderWidgetHostTest, UnhandledGestureEvent) {
  SimulateGestureEvent(WebInputEvent::kGestureTwoFingerTap,
                       blink::kWebGestureDeviceTouchscreen);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::kGestureTwoFingerTap,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(WebInputEvent::kGestureTwoFingerTap, view_->gesture_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, view_->ack_result());
}

// Test that the hang monitor timer expires properly if a new timer is started
// while one is in progress (see crbug.com/11007).
TEST_F(RenderWidgetHostTest, DontPostponeInputEventAckTimeout) {
  // Start with a short timeout.
  host_->StartInputEventAckTimeout(TimeDelta::FromMilliseconds(10));

  // Immediately try to add a long 30 second timeout.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartInputEventAckTimeout(TimeDelta::FromSeconds(30));

  // Wait long enough for first timeout and see if it fired.
  RunLoopFor(TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, stopped,
// and then started again.
TEST_F(RenderWidgetHostTest, StopAndStartInputEventAckTimeout) {
  // Start with a short timeout, then stop it.
  host_->StartInputEventAckTimeout(TimeDelta::FromMilliseconds(10));
  host_->StopInputEventAckTimeout();

  // Start it again to ensure it still works.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartInputEventAckTimeout(TimeDelta::FromMilliseconds(10));

  // Wait long enough for first timeout and see if it fired.
  RunLoopFor(TimeDelta::FromMilliseconds(40));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, then
// updated to a shorter duration.
TEST_F(RenderWidgetHostTest, ShorterDelayInputEventAckTimeout) {
  // Start with a timeout.
  host_->StartInputEventAckTimeout(TimeDelta::FromMilliseconds(100));

  // Start it again with shorter delay.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartInputEventAckTimeout(TimeDelta::FromMilliseconds(20));

  // Wait long enough for the second timeout and see if it fired.
  RunLoopFor(TimeDelta::FromMilliseconds(25));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer is effectively disabled when the widget is
// hidden.
TEST_F(RenderWidgetHostTest, InputEventAckTimeoutDisabledForInputWhenHidden) {
  host_->set_hung_renderer_delay(base::TimeDelta::FromMicroseconds(1));
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10, 0, false);

  // Hiding the widget should deactivate the timeout.
  host_->WasHidden();

  // The timeout should not fire.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  RunLoopFor(TimeDelta::FromMicroseconds(2));
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // The timeout should never reactivate while hidden.
  SimulateMouseEvent(WebInputEvent::kMouseMove, 10, 10, 0, false);
  RunLoopFor(TimeDelta::FromMicroseconds(2));
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // Showing the widget should restore the timeout, as the events have
  // not yet been ack'ed.
  host_->WasShown(false /* record_presentation_time */);
  RunLoopFor(TimeDelta::FromMicroseconds(2));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor catches two input events but only one ack.
// This can happen if the second input event causes the renderer to hang.
// This test will catch a regression of crbug.com/111185.
TEST_F(RenderWidgetHostTest, MultipleInputEvents) {
  // Configure the host to wait 10ms before considering
  // the renderer hung.
  host_->set_hung_renderer_delay(base::TimeDelta::FromMicroseconds(10));

  // Send two events but only one ack.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Wait long enough for first timeout and see if it fired.
  RunLoopFor(TimeDelta::FromMicroseconds(20));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the rendering timeout for newly loaded content fires when enough
// time passes without receiving a new compositor frame. This test assumes
// Surface Synchronization is off.
// Disabled due to flakiness on Android.  See https://crbug.com/892700.
#if defined(OS_ANDROID)
#define MAYBE_NewContentRenderingTimeoutWithoutSurfaceSync \
  DISABLED_NewContentRenderingTimeoutWithoutSurfaceSync
#else
#define MAYBE_NewContentRenderingTimeoutWithoutSurfaceSync \
  NewContentRenderingTimeoutWithoutSurfaceSync
#endif
TEST_F(RenderWidgetHostTest,
       NewContentRenderingTimeoutWithoutSurfaceSync_MAYBE) {
  // If Surface Synchronization is on, we have a separate code path for
  // cancelling new content rendering timeout that is tested separately.
  if (features::IsSurfaceSynchronizationEnabled())
    return;

  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());

  // Mocking |renderer_compositor_frame_sink_| to prevent crashes in
  // renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources).
  std::unique_ptr<viz::MockCompositorFrameSinkClient>
      mock_compositor_frame_sink_client =
          std::make_unique<viz::MockCompositorFrameSinkClient>();
  host_->SetMockRendererCompositorFrameSink(
      mock_compositor_frame_sink_client.get());

  host_->set_new_content_rendering_delay_for_testing(
      base::TimeDelta::FromMicroseconds(10));

  // Start the timer and immediately send a CompositorFrame with the
  // content_source_id of the new page. The timeout shouldn't fire.
  host_->DidNavigate(5);
  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetContentSourceId(5)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  RunLoopFor(TimeDelta::FromMicroseconds(20));

  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Start the timer but receive frames only from the old page. The timer
  // should fire.
  host_->DidNavigate(10);
  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetContentSourceId(9)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  RunLoopFor(TimeDelta::FromMicroseconds(20));

  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Send a CompositorFrame with content_source_id of the new page before we
  // attempt to start the timer. The timer shouldn't fire.
  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetContentSourceId(7)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  host_->DidNavigate(7);
  RunLoopFor(TimeDelta::FromMicroseconds(20));

  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Don't send any frames after the timer starts. The timer should fire.
  host_->DidNavigate(20);
  RunLoopFor(TimeDelta::FromMicroseconds(20));
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();
}

// This tests that a compositor frame received with a stale content source ID
// in its metadata is properly discarded.
TEST_F(RenderWidgetHostTest, SwapCompositorFrameWithBadSourceId) {
  // If Surface Synchronization is on, we don't keep track of content_source_id
  // in CompositorFrameMetadata.
  if (features::IsSurfaceSynchronizationEnabled())
    return;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());

  host_->DidNavigate(100);
  host_->set_new_content_rendering_delay_for_testing(
      base::TimeDelta::FromMicroseconds(9999));

  {
    // First swap a frame with an invalid ID.
    auto frame = viz::CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .SetBeginFrameAck(viz::BeginFrameAck(0, 1, true))
                     .SetContentSourceId(99)
                     .Build();

    // Mocking |renderer_compositor_frame_sink_| to prevent crashes in
    // renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources).
    std::unique_ptr<viz::MockCompositorFrameSinkClient>
        mock_compositor_frame_sink_client =
            std::make_unique<viz::MockCompositorFrameSinkClient>();
    host_->SetMockRendererCompositorFrameSink(
        mock_compositor_frame_sink_client.get());

    host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                 base::nullopt, 0);
    EXPECT_FALSE(
        static_cast<TestView*>(host_->GetView())->did_swap_compositor_frame());
    EXPECT_EQ(viz::BeginFrameAck(0, 1, false),
              static_cast<TestView*>(host_->GetView())
                  ->last_did_not_produce_frame_ack());
    static_cast<TestView*>(host_->GetView())->reset_did_swap_compositor_frame();
  }

  {
    // Test with a valid content ID as a control.
    auto frame = viz::CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .SetContentSourceId(100)
                     .Build();
    host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                 base::nullopt, 0);
    EXPECT_TRUE(
        static_cast<TestView*>(host_->GetView())->did_swap_compositor_frame());
    static_cast<TestView*>(host_->GetView())->reset_did_swap_compositor_frame();
  }

  {
    // We also accept frames with higher content IDs, to cover the case where
    // the browser process receives a compositor frame for a new page before
    // the corresponding DidCommitProvisionalLoad (it's a race).
    auto frame = viz::CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .SetContentSourceId(101)
                     .Build();
    host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                 base::nullopt, 0);
    EXPECT_TRUE(
        static_cast<TestView*>(host_->GetView())->did_swap_compositor_frame());
  }
}

TEST_F(RenderWidgetHostTest, IgnoreInputEvent) {
  host_->SetupForInputRouterTest();

  delegate_->SetIgnoreInputEvents(true);

  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  SimulateMouseEvent(WebInputEvent::kMouseMove);
  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  SimulateWheelEvent(0, 100, 0, true);
  EXPECT_FALSE(host_->mock_input_router()->sent_wheel_event_);

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchpad);
  EXPECT_FALSE(host_->mock_input_router()->sent_gesture_event_);

  PressTouchPoint(100, 100);
  SendTouchEvent();
  EXPECT_FALSE(host_->mock_input_router()->send_touch_event_not_cancelled_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerIgnoresEvent) {
  host_->SetupForInputRouterTest();
  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerSuppressFollowingEvents) {
  host_->SetupForInputRouterTest();

  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));

  // The callback handles the first event
  handle_key_press_event_ = true;
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Following Char events should be suppressed
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);
  SimulateKeyboardEvent(WebInputEvent::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Sending RawKeyDown event should stop suppression
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);

  host_->mock_input_router()->sent_keyboard_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::kChar);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, MouseEventCallbackCanHandleEvent) {
  host_->SetupForInputRouterTest();

  host_->AddMouseEventCallback(
      base::Bind(&RenderWidgetHostTest::MouseEventCallback,
                 base::Unretained(this)));

  handle_mouse_event_ = true;
  SimulateMouseEvent(WebInputEvent::kMouseDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  handle_mouse_event_ = false;
  SimulateMouseEvent(WebInputEvent::kMouseDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_mouse_event_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHasTouchEventHandlers) {
  host_->SetupForInputRouterTest();

  ASSERT_FALSE(host_->mock_input_router()->has_handlers_);

  host_->OnMessageReceived(WidgetHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_TRUE(host_->mock_input_router()->has_handlers_);
}

void CheckLatencyInfoComponentInMessage(
    MockWidgetInputHandler::MessageVector& dispatched_events,
    WebInputEvent::Type expected_type) {
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  EXPECT_TRUE(dispatched_events[0]->ToEvent()->Event()->web_event->GetType() ==
              expected_type);
  EXPECT_TRUE(
      dispatched_events[0]->ToEvent()->Event()->latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  dispatched_events[0]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
}

void CheckLatencyInfoComponentInGestureScrollUpdate(
    MockWidgetInputHandler::MessageVector& dispatched_events) {
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  EXPECT_EQ(WebInputEvent::kTouchScrollStarted,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());
  EXPECT_TRUE(
      dispatched_events[1]->ToEvent()->Event()->latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  dispatched_events[1]->ToEvent()->CallCallback(INPUT_EVENT_ACK_STATE_CONSUMED);
}

// Tests that after input event passes through RWHI through ForwardXXXEvent()
// or ForwardXXXEventWithLatencyInfo(), LatencyInfo component
// ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT will always present in the
// event's LatencyInfo.
TEST_F(RenderWidgetHostTest, InputEventRWHLatencyComponent) {
  host_->OnMessageReceived(WidgetHostMsg_HasTouchEventHandlers(0, true));

  // Tests RWHI::ForwardWheelEvent().
  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::kMouseWheel);

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo(),
                                    WebMouseWheelEvent::kPhaseChanged);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::kMouseWheel);

  // Tests RWHI::ForwardMouseEvent().
  SimulateMouseEvent(WebInputEvent::kMouseMove);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::kMouseMove);

  // Tests RWHI::ForwardMouseEventWithLatencyInfo().
  SimulateMouseEventWithLatencyInfo(WebInputEvent::kMouseMove,
                                    ui::LatencyInfo());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::kMouseMove);

  // Tests RWHI::ForwardGestureEvent().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  host_->input_router()->OnSetTouchAction(cc::kTouchActionAuto);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::kTouchStart);

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::kGestureScrollBegin);

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::kGestureScrollUpdate,
                                      blink::kWebGestureDeviceTouchscreen,
                                      ui::LatencyInfo());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInGestureScrollUpdate(dispatched_events);

  ReleaseTouchPoint(0);
  SendTouchEvent();
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();

  // Tests RWHI::ForwardTouchEventWithLatencyInfo().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::kTouchStart);
}

TEST_F(RenderWidgetHostTest, RendererExitedResetsInputRouter) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

// Regression test for http://crbug.com/401859 and http://crbug.com/522795.
TEST_F(RenderWidgetHostTest, RendererExitedResetsIsHidden) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->WasShown(false /* record_presentation_time */);

  ASSERT_FALSE(host_->is_hidden());
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  ASSERT_TRUE(host_->is_hidden());

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

TEST_F(RenderWidgetHostTest, VisualProperties) {
  gfx::Rect bounds(0, 0, 100, 100);
  gfx::Size compositor_viewport_pixel_size(40, 50);
  view_->SetBounds(bounds);
  view_->SetMockCompositorViewportPixelSize(compositor_viewport_pixel_size);

  VisualProperties visual_properties;
  bool needs_ack = false;
  host_->GetVisualProperties(&visual_properties, &needs_ack);
  EXPECT_EQ(bounds.size(), visual_properties.new_size);
  EXPECT_EQ(compositor_viewport_pixel_size,
            visual_properties.compositor_viewport_pixel_size);
}

TEST_F(RenderWidgetHostTest, VisualPropertiesDeviceScale) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kEnableUseZoomForDSF, "true");

  float device_scale = 3.5f;
  ScreenInfo screen_info;
  screen_info.device_scale_factor = device_scale;

  view_->SetScreenInfo(screen_info);
  host_->SynchronizeVisualProperties();

  float top_controls_height = 10.0f;
  float bottom_controls_height = 20.0f;
  view_->set_top_controls_height(top_controls_height);
  view_->set_bottom_controls_height(bottom_controls_height);

  VisualProperties visual_properties;
  bool needs_ack = false;
  host_->GetVisualProperties(&visual_properties, &needs_ack);
  EXPECT_EQ(top_controls_height * device_scale,
            visual_properties.top_controls_height);
  EXPECT_EQ(bottom_controls_height * device_scale,
            visual_properties.bottom_controls_height);
}

// Make sure no dragging occurs after renderer exited. See crbug.com/704832.
TEST_F(RenderWidgetHostTest, RendererExitedNoDrag) {
  host_->SetView(new TestView(host_.get()));

  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 0);

  GURL http_url = GURL("http://www.domain.com/index.html");
  DropData drop_data;
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  blink::WebDragOperationsMask drag_operation = blink::kWebDragOperationEvery;
  DragEventSourceInfo event_info;
  host_->OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                         event_info);
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);

  // Simulate that renderer exited due navigation to the next page.
  host_->RendererExited(base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
  EXPECT_FALSE(host_->GetView());
  host_->OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                         event_info);
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);
}

class RenderWidgetHostInitialSizeTest : public RenderWidgetHostTest {
 public:
  RenderWidgetHostInitialSizeTest()
      : RenderWidgetHostTest(), initial_size_(200, 100) {}

  void ConfigureView(TestView* view) override {
    view->SetBounds(gfx::Rect(initial_size_));
  }

 protected:
  gfx::Size initial_size_;
};

TEST_F(RenderWidgetHostInitialSizeTest, InitialSize) {
  // Having an initial size set means that the size information had been sent
  // with the reqiest to new up the RenderView and so subsequent
  // SynchronizeVisualProperties calls should not result in new IPC (unless the
  // size has actually changed).
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
  EXPECT_EQ(initial_size_, host_->old_visual_properties_->new_size);
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
}

TEST_F(RenderWidgetHostTest, HideUnthrottlesResize) {
  gfx::Size original_size(100, 100);
  view_->SetBounds(gfx::Rect(original_size));
  process_->sink().ClearMessages();
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      WidgetMsg_SynchronizeVisualProperties::ID));
  EXPECT_EQ(original_size, host_->old_visual_properties_->new_size);
  EXPECT_TRUE(host_->visual_properties_ack_pending_);

  // Hiding the widget should unthrottle resize.
  host_->WasHidden();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
}

// Tests that event dispatch after the delegate has been detached doesn't cause
// a crash. See crbug.com/563237.
TEST_F(RenderWidgetHostTest, EventDispatchPostDetach) {
  host_->OnMessageReceived(WidgetHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();

  host_->DetachDelegate();

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::kGestureScrollUpdate,
                                      blink::kWebGestureDeviceTouchscreen,
                                      ui::LatencyInfo());

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo());

  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

// Check that if messages of a frame arrive earlier than the frame itself, we
// queue the messages until the frame arrives and then process them.
TEST_F(RenderWidgetHostTest, FrameToken_MessageThenFrame) {
  const uint32_t frame_token = 99;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages;
  messages.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token, messages));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token)
                   .SetSendFrameTokenToEmbedder(true)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

// Check that if a frame arrives earlier than its messages, we process the
// messages immedtiately.
TEST_F(RenderWidgetHostTest, FrameToken_FrameThenMessage) {
  const uint32_t frame_token = 99;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages;
  messages.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token)
                   .SetSendFrameTokenToEmbedder(true)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token, messages));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

// Check that if messages of multiple frames arrive before the frames, we
// process each message once it frame arrives.
TEST_F(RenderWidgetHostTest, FrameToken_MultipleMessagesThenTokens) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 100;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(2u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token1)
                   .SetSendFrameTokenToEmbedder(true)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());

  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetFrameToken(frame_token2)
              .SetSendFrameTokenToEmbedder(true)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if multiple frames arrive before their messages, each message is
// processed immediately as soon as it arrives.
TEST_F(RenderWidgetHostTest, FrameToken_MultipleTokensThenMessages) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 100;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token1)
                   .SetSendFrameTokenToEmbedder(true)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetFrameToken(frame_token2)
              .SetSendFrameTokenToEmbedder(true)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if one frame is lost but its messages arrive, we process the
// messages on the arrival of the next frame.
TEST_F(RenderWidgetHostTest, FrameToken_DroppedFrame) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 100;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(2u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token2)
                   .SetSendFrameTokenToEmbedder(true)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if the renderer crashes, we drop all queued messages and allow
// smaller frame tokens to be sent by the renderer.
TEST_F(RenderWidgetHostTest, FrameToken_RendererCrash) {
  const uint32_t frame_token1 = 99;
  const uint32_t frame_token2 = 50;
  const uint32_t frame_token3 = 30;
  const viz::LocalSurfaceId local_surface_id(1,
                                             base::UnguessableToken::Create());
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages3;
  messages1.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages3.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  // Mocking |renderer_compositor_frame_sink_| to prevent crashes in
  // renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources).
  std::unique_ptr<viz::MockCompositorFrameSinkClient>
      mock_compositor_frame_sink_client =
          std::make_unique<viz::MockCompositorFrameSinkClient>();
  host_->SetMockRendererCompositorFrameSink(
      mock_compositor_frame_sink_client.get());

  // If we don't do this, then RWHI destroys the view in RendererExited and
  // then a crash occurs when we attempt to destroy it again in TearDown().
  host_->SetView(nullptr);

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());
  host_->Init();

  auto frame = viz::CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetFrameToken(frame_token2)
                   .SetSendFrameTokenToEmbedder(true)
                   .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());
  host_->SetView(view_.get());
  host_->Init();

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token3, messages3));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  frame = viz::CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetFrameToken(frame_token3)
              .SetSendFrameTokenToEmbedder(true)
              .Build();
  host_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                               base::nullopt, 0);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

TEST_F(RenderWidgetHostTest, InflightEventCountResetsAfterRebind) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::kRawKeyDown);

  EXPECT_EQ(1u, host_->in_flight_event_count());
  mojom::WidgetPtr widget;
  std::unique_ptr<MockWidgetImpl> widget_impl =
      std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));
  host_->SetWidget(std::move(widget));
  EXPECT_EQ(0u, host_->in_flight_event_count());
}

TEST_F(RenderWidgetHostTest, ForceEnableZoomShouldUpdateAfterRebind) {
  SCOPED_TRACE("force_enable_zoom is false at start.");
  host_->ExpectForceEnableZoom(false);

  // Set force_enable_zoom true.
  host_->SetForceEnableZoom(true);

  SCOPED_TRACE("force_enable_zoom is true after set.");
  host_->ExpectForceEnableZoom(true);

  // Rebind should also update to the latest force_enable_zoom state.
  mojom::WidgetPtr widget;
  std::unique_ptr<MockWidgetImpl> widget_impl =
      std::make_unique<MockWidgetImpl>(mojo::MakeRequest(&widget));
  host_->SetWidget(std::move(widget));

  SCOPED_TRACE("force_enable_zoom is true after rebind.");
  host_->ExpectForceEnableZoom(true);
}

TEST_F(RenderWidgetHostTest, RenderWidgetSurfaceProperties) {
  RenderWidgetSurfaceProperties prop1;
  prop1.size = gfx::Size(200, 200);
  prop1.device_scale_factor = 1.f;
  RenderWidgetSurfaceProperties prop2;
  prop2.size = gfx::Size(300, 300);
  prop2.device_scale_factor = 2.f;

  EXPECT_EQ(
      "RenderWidgetSurfaceProperties(size(this: 200x200, other: 300x300), "
      "device_scale_factor(this: 1, other: 2))",
      prop1.ToDiffString(prop2));
  EXPECT_EQ(
      "RenderWidgetSurfaceProperties(size(this: 300x300, other: 200x200), "
      "device_scale_factor(this: 2, other: 1))",
      prop2.ToDiffString(prop1));
  EXPECT_EQ("", prop1.ToDiffString(prop1));
  EXPECT_EQ("", prop2.ToDiffString(prop2));
}

// If a navigation happens while the widget is hidden, we shouldn't show
// contents of the previous page when we become visible.
TEST_F(RenderWidgetHostTest, NavigateInBackgroundShowsBlank) {
  // When visible, navigation does not immediately call into
  // ClearDisplayedGraphics.
  host_->WasShown(false /* record_presentation_time */);
  host_->DidNavigate(5);
  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());

  // Hide then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->WasShown(false /* record_presentation_time */);
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Hide, navigate, then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->DidNavigate(6);
  host_->WasShown(false /* record_presentation_time */);
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
}

TEST_F(RenderWidgetHostTest, RendererHangRecordsMetrics) {
  base::SimpleTestTickClock clock;
  host_->set_clock_for_testing(&clock);
  base::HistogramTester tester;

  // RenderWidgetHost makes private the methods it overrides from
  // InputRouterClient. Call them through the base class.
  InputRouterClient* input_router_client = host_.get();

  // Do a 3s hang. This shouldn't affect metrics.
  input_router_client->IncrementInFlightEventCount();
  clock.Advance(base::TimeDelta::FromSeconds(3));
  input_router_client->DecrementInFlightEventCount(
      InputEventAckSource::UNKNOWN);
  tester.ExpectTotalCount("Renderer.Hung.Duration", 0u);

  // Do a 17s hang. This should affect metrics.
  input_router_client->IncrementInFlightEventCount();
  clock.Advance(base::TimeDelta::FromSeconds(17));
  input_router_client->DecrementInFlightEventCount(
      InputEventAckSource::UNKNOWN);
  tester.ExpectTotalCount("Renderer.Hung.Duration", 1u);
  tester.ExpectUniqueSample("Renderer.Hung.Duration", 17000, 1);
}

}  // namespace content
