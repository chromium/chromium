// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/mojom/render_frame_metadata.mojom.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/input/switches.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/data_transfer_util.h"
#include "content/browser/renderer_host/display_feature.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/input/touch_emulator_impl.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_input_router.h"
#include "content/test/mock_widget.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/stub_render_widget_host_owner_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/display_util.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "ui/android/screen_android.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/renderer_host/test_render_widget_host_view_mac_factory.h"
#include "ui/display/test/test_screen.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "content/browser/renderer_host/test_render_widget_host_view_ios_factory.h"
#endif

#if defined(USE_AURA) || BUILDFLAG(IS_APPLE)
#include "content/public/test/test_image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/common/input/events_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/events/event.h"
#endif

using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

using testing::_;
using testing::Return;

namespace content {
namespace  {

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {
  }

  RenderWidgetHostProcess(const RenderWidgetHostProcess&) = delete;
  RenderWidgetHostProcess& operator=(const RenderWidgetHostProcess&) = delete;

  ~RenderWidgetHostProcess() override {}

  bool IsInitializedAndNotDead() override { return true; }
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
        gesture_event_type_(WebInputEvent::Type::kUndefined),
        use_fake_compositor_viewport_pixel_size_(false),
        ack_result_(blink::mojom::InputEventResultState::kUnknown) {
    local_surface_id_allocator_.GenerateId();
  }

  TestView(const TestView&) = delete;
  TestView& operator=(const TestView&) = delete;

  // Sets the bounds returned by GetViewBounds.
  void SetBounds(const gfx::Rect& bounds) override {
    if (bounds_ == bounds)
      return;
    bounds_ = bounds;
    local_surface_id_allocator_.GenerateId();
  }

  void SetScreenInfo(const display::ScreenInfo& screen_info) {
    if (screen_info_ == screen_info)
      return;
    screen_info_ = screen_info;
    local_surface_id_allocator_.GenerateId();
  }

  void InvalidateLocalSurfaceId() { local_surface_id_allocator_.Invalidate(); }

  display::ScreenInfo GetScreenInfo() const override { return screen_info_; }
  display::ScreenInfos GetScreenInfos() const override {
    return display::ScreenInfos(screen_info_);
  }

  const WebTouchEvent& acked_event() const { return acked_event_; }
  int acked_event_count() const { return acked_event_count_; }
  void ClearAckedEvent() {
    acked_event_.SetType(blink::WebInputEvent::Type::kUndefined);
    acked_event_count_ = 0;
  }

  const WebMouseWheelEvent& unhandled_wheel_event() const {
    return unhandled_wheel_event_;
  }
  int unhandled_wheel_event_count() const {
    return unhandled_wheel_event_count_;
  }
  WebInputEvent::Type gesture_event_type() const { return gesture_event_type_; }
  blink::mojom::InputEventResultState ack_result() const { return ack_result_; }

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

  // RenderWidgetHostView override.
  gfx::Rect GetViewBounds() override { return bounds_; }
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override {
    return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  }

  void SetInsets(const gfx::Insets& insets) override { insets_ = insets; }
  gfx::Size GetVisibleViewportSize() override {
    gfx::Rect requested_rect(GetRequestedRendererSize());
    requested_rect.Inset(insets_);
    return requested_rect.size();
  }

  void ProcessAckedTouchEvent(
      const input::TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override {
    acked_event_ = touch.event;
    ++acked_event_count_;
  }
  void WheelEventAck(const WebMouseWheelEvent& event,
                     blink::mojom::InputEventResultState ack_result) override {
    if (ack_result == blink::mojom::InputEventResultState::kConsumed)
      return;
    unhandled_wheel_event_count_++;
    unhandled_wheel_event_ = event;
  }
  void GestureEventAck(
      const WebGestureEvent& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override {
    gesture_event_type_ = event.GetType();
    ack_result_ = ack_result;
  }
  gfx::Size GetCompositorViewportPixelSize() override {
    if (use_fake_compositor_viewport_pixel_size_)
      return mock_compositor_viewport_pixel_size_;
    return TestRenderWidgetHostView::GetCompositorViewportPixelSize();
  }

 protected:
  WebMouseWheelEvent unhandled_wheel_event_;
  int unhandled_wheel_event_count_;
  WebTouchEvent acked_event_;
  int acked_event_count_;
  WebInputEvent::Type gesture_event_type_;
  gfx::Rect bounds_;
  bool use_fake_compositor_viewport_pixel_size_;
  gfx::Size mock_compositor_viewport_pixel_size_;
  blink::mojom::InputEventResultState ack_result_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  display::ScreenInfo screen_info_;
  gfx::Insets insets_;
};

// MockRenderViewHostDelegateView ------------------------------------------
class MockRenderViewHostDelegateView : public RenderViewHostDelegateView {
 public:
  MockRenderViewHostDelegateView() = default;

  MockRenderViewHostDelegateView(const MockRenderViewHostDelegateView&) =
      delete;
  MockRenderViewHostDelegateView& operator=(
      const MockRenderViewHostDelegateView&) = delete;

  ~MockRenderViewHostDelegateView() override = default;

  int start_dragging_count() const { return start_dragging_count_; }

  // RenderViewHostDelegateView:
  void StartDragging(const DropData& drop_data,
                     const url::Origin& source_origin,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& cursor_offset,
                     const gfx::Rect& drag_obj_rect,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override {
    ++start_dragging_count_;
  }

 private:
  int start_dragging_count_ = 0;
};

// FakeRenderFrameMetadataObserver -----------------------------------------

// Fake out the renderer side of mojom::RenderFrameMetadataObserver, allowing
// for RenderWidgetHostImpl to be created.
//
// All methods are no-opts, the provided mojo receiver and remote are held, but
// never bound.
class FakeRenderFrameMetadataObserver
    : public cc::mojom::RenderFrameMetadataObserver {
 public:
  FakeRenderFrameMetadataObserver(
      mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserver> receiver,
      mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserverClient>
          client_remote);

  FakeRenderFrameMetadataObserver(const FakeRenderFrameMetadataObserver&) =
      delete;
  FakeRenderFrameMetadataObserver& operator=(
      const FakeRenderFrameMetadataObserver&) = delete;

  ~FakeRenderFrameMetadataObserver() override {}

#if BUILDFLAG(IS_ANDROID)
  void UpdateRootScrollOffsetUpdateFrequency(
      cc::mojom::RootScrollOffsetUpdateFrequency frequency) override {}
#endif
  void ReportAllFrameSubmissionsForTesting(bool enabled) override {}

 private:
  mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserver> receiver_;
  mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserverClient>
      client_remote_;
};

FakeRenderFrameMetadataObserver::FakeRenderFrameMetadataObserver(
    mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserver> receiver,
    mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserverClient>
        client_remote)
    : receiver_(std::move(receiver)),
      client_remote_(std::move(client_remote)) {}

// MockInputEventObserver -------------------------------------------------
class MockInputEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  MOCK_METHOD1(OnInputEvent, void(const blink::WebInputEvent&));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD1(OnImeTextCommittedEvent, void(const std::u16string& text_str));
  MOCK_METHOD1(OnImeSetComposingTextEvent,
               void(const std::u16string& text_str));
  MOCK_METHOD0(OnImeFinishComposingTextEvent, void());
#endif
};

// MockRenderWidgetHostDelegate --------------------------------------------

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : prehandle_keyboard_event_(false),
        prehandle_keyboard_event_is_shortcut_(false),
        prehandle_keyboard_event_called_(false),
        prehandle_keyboard_event_type_(WebInputEvent::Type::kUndefined),
        unhandled_keyboard_event_called_(false),
        unhandled_keyboard_event_type_(WebInputEvent::Type::kUndefined),
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

  double GetPendingPageZoomLevel() override { return zoom_level_; }

  void FocusOwningWebContents(
      RenderWidgetHostImpl* render_widget_host) override {
    focus_owning_web_contents_call_count++;
  }

  int GetFocusOwningWebContentsCallCount() const {
    return focus_owning_web_contents_call_count;
  }

  void OnVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection scroll_direction) override {
    ++on_vertical_scroll_direction_changed_call_count_;
    last_vertical_scroll_direction_ = scroll_direction;
  }

  int GetOnVerticalScrollDirectionChangedCallCount() const {
    return on_vertical_scroll_direction_changed_call_count_;
  }

  viz::VerticalScrollDirection GetLastVerticalScrollDirection() const {
    return last_vertical_scroll_direction_;
  }

  RenderViewHostDelegateView* GetDelegateView() override {
    return mock_delegate_view();
  }

  void SetIgnoreInputEvents(bool ignore_input_events) {
    ignore_input_events_ = ignore_input_events;
  }

  bool IsFullscreen() override { return is_fullscreen_; }

  void set_is_fullscreen(bool enabled) { is_fullscreen_ = enabled; }

  MOCK_METHOD(bool,
              IsWaitingForPointerLockPrompt,
              (RenderWidgetHostImpl * host),
              (override));

 protected:
  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override {
    prehandle_keyboard_event_type_ = event.GetType();
    prehandle_keyboard_event_called_ = true;
    if (prehandle_keyboard_event_)
      return KeyboardEventProcessingResult::HANDLED;
    return prehandle_keyboard_event_is_shortcut_
               ? KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT
               : KeyboardEventProcessingResult::NOT_HANDLED;
  }

  bool HandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override {
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
  bool ShouldIgnoreWebInputEvents(const blink::WebInputEvent& event) override {
    return ignore_input_events_;
  }

  void ExecuteEditCommand(const std::string& command,
                          const std::optional<std::u16string>& value) override {
  }

  void Undo() override {}
  void Redo() override {}
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void PasteAndMatchStyle() override {}
  void SelectAll() override {}

  VisibleTimeRequestTrigger& GetVisibleTimeRequestTrigger() override {
    return visible_time_request_trigger_;
  }

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

  int on_vertical_scroll_direction_changed_call_count_ = 0;
  viz::VerticalScrollDirection last_vertical_scroll_direction_ =
      viz::VerticalScrollDirection::kNull;

  bool is_fullscreen_ = false;

  VisibleTimeRequestTrigger visible_time_request_trigger_;
};

class MockRenderWidgetHostOwnerDelegate
    : public StubRenderWidgetHostOwnerDelegate {
 public:
  MOCK_METHOD1(SetBackgroundOpaque, void(bool opaque));
  MOCK_METHOD0(IsMainFrameActive, bool());
};

// RenderWidgetHostTest --------------------------------------------------------

class RenderWidgetHostTest : public testing::Test {
 public:
  RenderWidgetHostTest() : last_simulated_event_time_(ui::EventTimeForNow()) {}

  RenderWidgetHostTest(const RenderWidgetHostTest&) = delete;
  RenderWidgetHostTest& operator=(const RenderWidgetHostTest&) = delete;

  ~RenderWidgetHostTest() override = default;

  bool KeyPressEventCallback(const input::NativeWebKeyboardEvent& /* event */) {
    return handle_key_press_event_;
  }
  bool MouseEventCallback(const blink::WebMouseEvent& /* event */) {
    return handle_mouse_event_;
  }

  void ClearVisualProperties() {
    base::RunLoop().RunUntilIdle();
    widget_.ClearVisualProperties();
  }

  void ClearScreenRects() {
    base::RunLoop().RunUntilIdle();
    widget_.ClearScreenRects();
  }

  bool HasTouchEventHandlers(bool has_handlers) { return has_handlers; }
  bool HasHitTestableScrollbar(bool has_scrollbar) { return has_scrollbar; }

 protected:
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(input::switches::kValidateInputEventStream);
    browser_context_ = std::make_unique<TestBrowserContext>();
    delegate_ = std::make_unique<MockRenderWidgetHostDelegate>();
    process_ =
        std::make_unique<RenderWidgetHostProcess>(browser_context_.get());
    site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            browser_context_.get(), process_.get()));
    sink_ = &process_->sink();
#if defined(USE_AURA) || BUILDFLAG(IS_APPLE)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif
#if BUILDFLAG(IS_ANDROID)
    // calls display::Screen::SetScreenInstance().
    ui::SetScreenAndroid(false /* use_display_wide_color_gamut */);
#endif
#if BUILDFLAG(IS_MAC)
    screen_ = std::make_unique<display::test::TestScreen>();
    display::Screen::SetScreenInstance(screen_.get());
#endif
#if defined(USE_AURA)
    screen_.reset(aura::TestScreen::Create(gfx::Size()));
    display::Screen::SetScreenInstance(screen_.get());
#endif
    host_ = MockRenderWidgetHost::Create(
        /* frame_tree= */ nullptr, delegate_.get(),
        site_instance_group_->GetSafeRef(), process_->GetNextRoutingID(),
        widget_.GetNewRemote());
    // Set up the RenderWidgetHost as being for a main frame.
    host_->set_owner_delegate(&mock_owner_delegate_);
    // Act like there is no RenderWidget present in the renderer yet.
    EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
        .WillRepeatedly(Return(false));

    view_ = std::make_unique<TestView>(host_.get());
    ConfigureView(view_.get());
    host_->SetView(view_.get());
    // Act like we've created a RenderWidget.
    host_->GetInitialVisualProperties();
    EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
        .WillRepeatedly(Return(true));

    mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserver>
        renderer_render_frame_metadata_observer_remote;
    mojo::PendingRemote<cc::mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_remote;
    mojo::PendingReceiver<cc::mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_client_receiver =
            render_frame_metadata_observer_remote
                .InitWithNewPipeAndPassReceiver();
    renderer_render_frame_metadata_observer_ =
        std::make_unique<FakeRenderFrameMetadataObserver>(
            renderer_render_frame_metadata_observer_remote
                .InitWithNewPipeAndPassReceiver(),
            std::move(render_frame_metadata_observer_remote));

    host_->RegisterRenderFrameMetadataObserver(
        std::move(render_frame_metadata_observer_client_receiver),
        std::move(renderer_render_frame_metadata_observer_remote));

    // The blink::mojom::Widget is already set during MockRenderWidgetHost
    // construction.
    host_->BindFrameWidgetInterfaces(
        mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>()
            .InitWithNewEndpointAndPassReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());

    host_->RendererWidgetCreated(/*for_frame_widget=*/true);
  }

  void TearDown() override {
    sink_ = nullptr;
    view_.reset();
    host_.reset();
    delegate_.reset();
    process_->Cleanup();
    site_instance_group_.reset();
    process_.reset();
    browser_context_.reset();
#if defined(USE_AURA) || BUILDFLAG(IS_APPLE)
    ImageTransportFactory::Terminate();
#endif
#if defined(USE_AURA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    display::Screen::SetScreenInstance(nullptr);
    screen_.reset();
#endif

    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();
  }

  virtual void ConfigureView(TestView* view) {}

  void ReinitalizeHost() {
    host_->BindWidgetInterfaces(
        mojo::AssociatedRemote<blink::mojom::WidgetHost>()
            .BindNewEndpointAndPassDedicatedReceiver(),
        widget_.GetNewRemote());
    host_->BindFrameWidgetInterfaces(
        mojo::AssociatedRemote<blink::mojom::FrameWidgetHost>()
            .BindNewEndpointAndPassDedicatedReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());

    host_->RendererWidgetCreated(/*for_frame_widget=*/true);
  }

  base::TimeTicks GetNextSimulatedEventTime() {
    last_simulated_event_time_ += simulated_event_time_delta_;
    return last_simulated_event_time_;
  }

  input::NativeWebKeyboardEvent CreateNativeWebKeyboardEvent(
      WebInputEvent::Type type) {
    return input::NativeWebKeyboardEvent(type, /*modifiers=*/0,
                                         GetNextSimulatedEventTime());
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    host_->ForwardKeyboardEvent(CreateNativeWebKeyboardEvent(type));
  }

  void SimulateKeyboardEventWithCommands(WebInputEvent::Type type) {
    std::vector<blink::mojom::EditCommandPtr> edit_commands;
    edit_commands.push_back(blink::mojom::EditCommand::New("name", "value"));
    host_->ForwardKeyboardEventWithCommands(CreateNativeWebKeyboardEvent(type),
                                            ui::LatencyInfo(),
                                            std::move(edit_commands), nullptr);
  }

  void SimulateMouseEvent(WebInputEvent::Type type) {
    host_->ForwardMouseEvent(blink::SyntheticWebMouseEventBuilder::Build(type));
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardMouseEventWithLatencyInfo(
        blink::SyntheticWebMouseEventBuilder::Build(type), ui_latency);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    host_->ForwardWheelEvent(blink::SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers,
        precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                : ui::ScrollGranularity::kScrollByPixel));
  }

  void SimulateWheelEvent(float dX,
                          float dY,
                          int modifiers,
                          bool precise,
                          WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            0, 0, dX, dY, modifiers,
            precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                    : ui::ScrollGranularity::kScrollByPixel);
    wheel_event.phase = phase;
    host_->ForwardWheelEvent(wheel_event);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardWheelEventWithLatencyInfo(
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            0, 0, dX, dY, modifiers,
            precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                    : ui::ScrollGranularity::kScrollByPixel),
        ui_latency);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency,
                                         WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            0, 0, dX, dY, modifiers,
            precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                    : ui::ScrollGranularity::kScrollByPixel);
    wheel_event.phase = phase;
    host_->ForwardWheelEventWithLatencyInfo(wheel_event, ui_latency);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    SimulateMouseEvent(WebInputEvent::Type::kMouseMove, x, y, modifiers, false);
  }

  void SimulateMouseEvent(
      WebInputEvent::Type type, int x, int y, int modifiers, bool pressed) {
    WebMouseEvent event =
        blink::SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed)
      event.button = WebMouseEvent::Button::kLeft;
    event.SetTimeStamp(GetNextSimulatedEventTime());
    host_->ForwardMouseEvent(event);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    host_->ForwardGestureEvent(
        blink::SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(WebInputEvent::Type type,
                                           WebGestureDevice sourceDevice,
                                           const ui::LatencyInfo& ui_latency) {
    host_->GetRenderInputRouter()->ForwardGestureEventWithLatencyInfo(
        blink::SyntheticWebGestureEventBuilder::Build(type, sourceDevice),
        ui_latency);
  }

  // Set the timestamp for the touch-event.
  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    host_->GetRenderInputRouter()->ForwardTouchEventWithLatencyInfo(
        touch_event_, ui::LatencyInfo());

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
    size_t data_length;
    if (!iter.ReadData(&data, &data_length))
      return nullptr;
    return reinterpret_cast<const WebInputEvent*>(data);
  }

  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<RenderWidgetHostProcess> process_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  testing::NiceMock<MockRenderWidgetHostOwnerDelegate> mock_owner_delegate_;
  std::unique_ptr<MockRenderWidgetHost> host_;
  std::unique_ptr<TestView> view_;
  std::unique_ptr<display::Screen> screen_;
  bool handle_key_press_event_ = false;
  bool handle_mouse_event_ = false;
  base::TimeTicks last_simulated_event_time_;
  base::TimeDelta simulated_event_time_delta_;
  raw_ptr<IPC::TestSink> sink_;
  std::unique_ptr<FakeRenderFrameMetadataObserver>
      renderer_render_frame_metadata_observer_;
  MockWidget widget_;

 private:
  blink::SyntheticWebTouchEvent touch_event_;
};

// RenderWidgetHostWithSourceTest ----------------------------------------------

// This is for tests that are to be run for all source devices.
class RenderWidgetHostWithSourceTest
    : public RenderWidgetHostTest,
      public testing::WithParamInterface<WebGestureDevice> {};

}  // namespace

// -----------------------------------------------------------------------------

// Tests that renderer doesn't change bounds while browser has its
// bounds changed (and until bounds are acked), which might be a result
// of a system's display compositor/server changing bounds of an
// application.
TEST_F(RenderWidgetHostTest, DoNotAcceptPopupBoundsUntilScreenRectsAcked) {
  // The host should wait for the screen rects ack now as SendScreenRects were
  // called during the initialization step.
  EXPECT_TRUE(host_->waiting_for_screen_rects_ack_);

  // Execute pending callbacks and clear screen rects.
  ClearScreenRects();

  // Lets mojo to pass the message from the renderer to the browser (from widget
  // to host).
  base::RunLoop().RunUntilIdle();

  // The host shouldn't wait for ack now as it has received it.
  EXPECT_FALSE(host_->waiting_for_screen_rects_ack_);

  // Change the bounds of the view and send screen rects.
  view_->SetBounds({10, 20, 300, 200});
  // Pass updated bounds from the browser to the renderer.
  host_->SendScreenRects();

  // The host should wait for the screen rects ack now.
  EXPECT_TRUE(host_->waiting_for_screen_rects_ack_);

  // Store the current view's bounds and pretend popup bounds are
  // being changed. However, they mustn't be changed as the host is still
  // waiting for the screen rects ack. This ensures that the renderer
  // doesn't clobber browser's bounds.
  auto old_view_bounds = view_->GetViewBounds();
  auto new_popup_view_bounds = gfx::Rect(5, 5, 20, 20);
  // Act like a renderer sending new bounds to the browser.
  static_cast<blink::mojom::PopupWidgetHost*>(host_.get())
      ->SetPopupBounds(new_popup_view_bounds, base::DoNothing());
  // The view still has the old bounds...
  EXPECT_EQ(old_view_bounds, view_->GetViewBounds());
  // which are not the same as the new bounds that were tried to be
  // set.
  EXPECT_NE(view_->GetViewBounds(), new_popup_view_bounds);

  // Clear the screen rects and send the ack callback back to the host.
  ClearScreenRects();

  // Allows mojo to pass the message from the renderer to the browser
  // (ClearScreenRects executed a callback via mojo that notifies the browser
  // that the renderer completed processing the new rects).
  base::RunLoop().RunUntilIdle();

  // The change must have been acked by now.
  EXPECT_FALSE(host_->waiting_for_screen_rects_ack_);

  // Pretend that the renderer changes the popup bounds again...
  static_cast<blink::mojom::PopupWidgetHost*>(host_.get())
      ->SetPopupBounds(new_popup_view_bounds, base::DoNothing());
  // And the host must accept them now as the screen rects have been
  // acked.
  EXPECT_EQ(new_popup_view_bounds, view_->GetViewBounds());
}

TEST_F(RenderWidgetHostTest, SynchronizeVisualProperties) {
  ClearVisualProperties();

  // The initial zoom is 0 so host should not send a sync message
  delegate_->SetZoomLevel(0);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // The zoom has changed so host should send out a sync message.
  double new_zoom_level = blink::ZoomFactorToZoomLevel(0.25);
  delegate_->SetZoomLevel(new_zoom_level);
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_NEAR(new_zoom_level, host_->old_visual_properties_->zoom_level, 0.01);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // The initial bounds is the empty rect, so setting it to the same thing
  // shouldn't send the resize message.
  view_->SetBounds(gfx::Rect());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // No visual properties ACK if the physical backing gets set, but the view
  // bounds are zero.
  view_->SetMockCompositorViewportPixelSize(gfx::Size(200, 200));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // Setting the view bounds to nonzero should send out the notification.
  // but should not expect ack for empty physical backing size.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->SetBounds(original_size);
  view_->SetMockCompositorViewportPixelSize(gfx::Size());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // Setting the bounds and physical backing size to nonzero should send out
  // the notification and expect an ack.
  view_->ClearMockCompositorViewportPixelSize();
  host_->SynchronizeVisualProperties();
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_visual_properties_->new_size);
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = original_size.size();
  metadata.local_surface_id = std::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  gfx::Rect second_size(0, 0, 110, 110);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  view_->SetBounds(second_size);
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  EXPECT_TRUE(host_->visual_properties_ack_pending_);

  ClearVisualProperties();

  // Sending out a new notification should NOT send out a new IPC message since
  // a visual properties ACK is pending.
  gfx::Rect third_size(0, 0, 120, 120);
  process_->sink().ClearMessages();
  view_->SetBounds(third_size);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // Send a update that's a visual properties ACK, but for the original_size we
  // sent. Since this isn't the second_size, the message handler should
  // immediately send a new resize message for the new size to the renderer.
  metadata.viewport_size_in_pixels = original_size.size();
  metadata.local_surface_id = std::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // Send the visual properties ACK for the latest size.
  metadata.viewport_size_in_pixels = third_size.size();
  metadata.local_surface_id = std::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // Now clearing the bounds should send out a notification but we shouldn't
  // expect a visual properties ACK (since the renderer won't ack empty sizes).
  // The message should contain the new size (0x0) and not the previous one that
  // we skipped.
  view_->SetBounds(gfx::Rect());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // Send a rect that has no area but has either width or height set.
  view_->SetBounds(gfx::Rect(0, 0, 0, 30));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // Set the same size again. It should not be sent again.
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // A different size should be sent again, however.
  view_->SetBounds(gfx::Rect(0, 0, 0, 31));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 31), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());

  ClearVisualProperties();

  // An invalid LocalSurfaceId should result in no change to the
  // |visual_properties_ack_pending_| bit.
  view_->SetBounds(gfx::Rect(25, 25));
  view_->InvalidateLocalSurfaceId();
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(25, 25), host_->old_visual_properties_->new_size);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());
}

// Test that a resize event is sent if SynchronizeVisualProperties() is called
// after a ScreenInfo change.
TEST_F(RenderWidgetHostTest, ResizeScreenInfo) {
  display::ScreenInfo screen_info;
  screen_info.rect = gfx::Rect(0, 0, 800, 600);
  screen_info.available_rect = gfx::Rect(0, 0, 800, 600);
  screen_info.orientation_type =
      display::mojom::ScreenOrientation::kPortraitPrimary;

  ClearVisualProperties();
  view_->SetScreenInfo(screen_info);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // blink::mojom::Widget::UpdateVisualProperties sent to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_.ReceivedVisualProperties().size());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  screen_info.orientation_angle = 180;
  screen_info.orientation_type =
      display::mojom::ScreenOrientation::kLandscapePrimary;

  ClearVisualProperties();
  view_->SetScreenInfo(screen_info);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // blink::mojom::Widget::UpdateVisualProperties sent to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_.ReceivedVisualProperties().size());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  screen_info.device_scale_factor = 2.f;

  ClearVisualProperties();
  view_->SetScreenInfo(screen_info);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // blink::mojom::Widget::UpdateVisualProperties sent to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_.ReceivedVisualProperties().size());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  // No screen change.
  ClearVisualProperties();
  view_->SetScreenInfo(screen_info);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
}

// Ensure VisualProperties continues reporting the size of the current screen,
// not the viewport, when the frame is fullscreen. See crbug.com/1367416.
TEST_F(RenderWidgetHostTest, ScreenSizeInFullscreen) {
  const gfx::Rect kScreenBounds(0, 0, 800, 600);
  const gfx::Rect kViewBounds(55, 66, 600, 500);

  display::ScreenInfo screen_info;
  screen_info.rect = kScreenBounds;
  screen_info.available_rect = kScreenBounds;
  screen_info.orientation_type =
      display::mojom::ScreenOrientation::kPortraitPrimary;
  view_->SetScreenInfo(screen_info);

  ClearVisualProperties();

  // Do initial VisualProperties sync while not fullscreened.
  view_->SetBounds(kViewBounds);
  ASSERT_FALSE(delegate_->IsFullscreen());
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  // blink::mojom::Widget::UpdateVisualProperties sent to the renderer.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());
  blink::VisualProperties props = widget_.ReceivedVisualProperties().at(0);
  EXPECT_EQ(kScreenBounds, props.screen_infos.current().rect);
  EXPECT_EQ(kScreenBounds, props.screen_infos.current().available_rect);
  EXPECT_EQ(kViewBounds.size(), props.new_size);

  // Enter fullscreen and do another VisualProperties sync.
  delegate_->set_is_fullscreen(true);
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  // blink::mojom::Widget::UpdateVisualProperties sent to the renderer.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, widget_.ReceivedVisualProperties().size());
  props = widget_.ReceivedVisualProperties().at(1);
  EXPECT_EQ(kScreenBounds, props.screen_infos.current().rect);
  EXPECT_EQ(kScreenBounds, props.screen_infos.current().available_rect);
  EXPECT_EQ(kViewBounds.size(), props.new_size);

  // Exit fullscreen and do another VisualProperties sync.
  delegate_->set_is_fullscreen(false);
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  // blink::mojom::Widget::UpdateVisualProperties sent to the renderer.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, widget_.ReceivedVisualProperties().size());
  props = widget_.ReceivedVisualProperties().at(2);
  EXPECT_EQ(kScreenBounds, props.screen_infos.current().rect);
  EXPECT_EQ(kScreenBounds, props.screen_infos.current().available_rect);
  EXPECT_EQ(kViewBounds.size(), props.new_size);
}

TEST_F(RenderWidgetHostTest, RootViewportSegments) {
  gfx::Rect screen_rect(0, 0, 800, 600);
  display::ScreenInfo screen_info;
  screen_info.rect = screen_rect;
  screen_info.available_rect = screen_rect;
  screen_info.orientation_type =
      display::mojom::ScreenOrientation::kPortraitPrimary;
  view_->SetScreenInfo(screen_info);

  // Set a vertical display feature which must result in two viewport segments,
  // side-by-side.
  const int kDisplayFeatureLength = 20;
  DisplayFeature emulated_display_feature{
      DisplayFeature::Orientation::kVertical,
      /* offset */ screen_rect.width() / 2 - kDisplayFeatureLength / 2,
      /* mask_length */ kDisplayFeatureLength};
  RenderWidgetHostViewBase* render_widget_host_view = view_.get();
  render_widget_host_view->SetDisplayFeatureForTesting(
      &emulated_display_feature);

  ClearScreenRects();

  view_->SetBounds(screen_rect);
  host_->SendScreenRects();

  ClearVisualProperties();

  // Run SynchronizeVisualProperties and validate the viewport segments sent to
  // the renderer are correct.
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());
  auto viewport_segments =
      widget_.ReceivedVisualProperties().at(0).root_widget_viewport_segments;
  EXPECT_EQ(viewport_segments.size(), 2u);
  gfx::Rect expected_first_rect(0, 0, 390, 600);
  EXPECT_EQ(viewport_segments[0], expected_first_rect);
  gfx::Rect expected_second_rect(410, 0, 390, 600);
  EXPECT_EQ(viewport_segments[1], expected_second_rect);
  ClearVisualProperties();

  // Setting a bottom inset (simulating virtual keyboard displaying on Aura)
  // should result in 'shorter' segments.
  auto insets = gfx::Insets::TLBR(0, 0, 100, 0);
  view_->SetInsets(insets);
  expected_first_rect.Inset(insets);
  expected_second_rect.Inset(insets);
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());
  auto inset_viewport_segments =
      widget_.ReceivedVisualProperties().at(0).root_widget_viewport_segments;
  EXPECT_EQ(inset_viewport_segments.size(), 2u);
  EXPECT_EQ(inset_viewport_segments[0], expected_first_rect);
  EXPECT_EQ(inset_viewport_segments[1], expected_second_rect);
  ClearVisualProperties();

  view_->SetInsets(gfx::Insets(0));

  // Setting back to empty should result in a single rect. The previous call
  // resized the widget and causes a pending ack. This is unrelated to what
  // we're testing here so ignore the pending ack by using
  // |SynchronizeVisualPropertiesIgnoringPendingAck()|.
  render_widget_host_view->SetDisplayFeatureForTesting(nullptr);
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());
  auto single_viewport_segments =
      widget_.ReceivedVisualProperties().at(0).root_widget_viewport_segments;
  EXPECT_EQ(single_viewport_segments.size(), 1u);
  EXPECT_EQ(single_viewport_segments[0], gfx::Rect(0, 0, 800, 600));
  ClearVisualProperties();

  // Set a horizontal display feature which results in two viewport segments
  // stacked on top of each other.
  emulated_display_feature = {
      DisplayFeature::Orientation::kHorizontal,
      /* offset */ screen_rect.height() / 2 - kDisplayFeatureLength / 2,
      /* mask_length */ kDisplayFeatureLength};
  render_widget_host_view->SetDisplayFeatureForTesting(
      &emulated_display_feature);
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());
  auto vertical_viewport_segments =
      widget_.ReceivedVisualProperties().at(0).root_widget_viewport_segments;
  EXPECT_EQ(vertical_viewport_segments.size(), 2u);
  expected_first_rect = gfx::Rect(0, 0, 800, 290);
  EXPECT_EQ(vertical_viewport_segments[0], expected_first_rect);
  expected_second_rect = gfx::Rect(0, 310, 800, 290);
  EXPECT_EQ(vertical_viewport_segments[1], expected_second_rect);
  ClearVisualProperties();

  // If the segments don't change, there should be no IPC message sent.
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedVisualProperties().size());
}

TEST_F(RenderWidgetHostTest, ReceiveFrameTokenFromCrashedRenderer) {
  // The Renderer sends a monotonically increasing frame token.
  host_->DidProcessFrame(2, base::TimeTicks::Now());

  // Simulate a renderer crash.
  host_->SetView(nullptr);
  host_->RendererExited();

  // Receive an in-flight frame token (it needs to monotonically increase)
  // while the RenderWidget is gone.
  host_->DidProcessFrame(3, base::TimeTicks::Now());

  // The renderer is recreated.
  host_->SetView(view_.get());
  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  blink::VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  ReinitalizeHost();

  // The new RenderWidget sends a frame token, which is lower than what the
  // previous RenderWidget sent. This should be okay, as the expected token has
  // been reset.
  host_->DidProcessFrame(1, base::TimeTicks::Now());
}

TEST_F(RenderWidgetHostTest, ReceiveFrameTokenFromDeletedRenderWidget) {
  // The RenderWidget sends a monotonically increasing frame token.
  host_->DidProcessFrame(2, base::TimeTicks::Now());

  // The RenderWidget is destroyed in the renderer process as the main frame
  // is removed from this RenderWidgetHost's RenderWidgetView, but the
  // RenderWidgetView is still kept around for another RenderFrame.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(false));

  // Receive an in-flight frame token (it needs to monotonically increase)
  // while the RenderWidget is gone.
  host_->DidProcessFrame(3, base::TimeTicks::Now());

  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  blink::VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(true));

  // The new RenderWidget sends a frame token, which is lower than what the
  // previous RenderWidget sent. This should be okay, as the expected token has
  // been reset.
  host_->DidProcessFrame(1, base::TimeTicks::Now());
}

// Tests setting background transparency.
TEST_F(RenderWidgetHostTest, Background) {
  RenderWidgetHostViewBase* view;
#if defined(USE_AURA)
  view = new RenderWidgetHostViewAura(host_.get());
#elif BUILDFLAG(IS_ANDROID)
  view = new RenderWidgetHostViewAndroid(host_.get(),
                                         /*parent_native_view=*/nullptr,
                                         /*parent_layer=*/nullptr);
#elif BUILDFLAG(IS_MAC)
  view = CreateRenderWidgetHostViewMacForTesting(host_.get());
#elif BUILDFLAG(IS_IOS)
  view = CreateRenderWidgetHostViewIOSForTesting(host_.get());
#endif

#if !BUILDFLAG(IS_ANDROID)
  // TODO(derat): Call this on all platforms: http://crbug.com/102450.
  view->InitAsChild(gfx::NativeView());
#endif
  host_->SetView(view);

  ASSERT_FALSE(view->GetBackgroundColor());

  {
    // The background is assumed opaque by default, so choosing opaque won't
    // do anything if it's not set to transparent first.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(_)).Times(0);
    view->SetBackgroundColor(SK_ColorRED);
    EXPECT_EQ(unsigned{SK_ColorRED}, *view->GetBackgroundColor());
  }
  {
    // Another opaque color doesn't inform the view of any change.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(_)).Times(0);
    view->SetBackgroundColor(SK_ColorBLUE);
    EXPECT_EQ(unsigned{SK_ColorBLUE}, *view->GetBackgroundColor());
  }
  {
    // The owner delegate will be called to pass it over IPC to the
    // `blink::WebView`.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(false));
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
#if BUILDFLAG(IS_MAC)
    // Mac replaces transparent background colors with white. See the comment in
    // RenderWidgetHostViewMac::GetBackgroundColor. (https://crbug.com/735407)
    EXPECT_EQ(unsigned{SK_ColorWHITE}, *view->GetBackgroundColor());
#else
    // The browser side will represent the background color as transparent
    // immediately.
    EXPECT_EQ(unsigned{SK_ColorTRANSPARENT}, *view->GetBackgroundColor());
#endif
  }
  {
    // Setting back an opaque color informs the view.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(true));
    view->SetBackgroundColor(SK_ColorBLUE);
    EXPECT_EQ(unsigned{SK_ColorBLUE}, *view->GetBackgroundColor());
  }

#if BUILDFLAG(IS_ANDROID)
  // Surface Eviction attempts to crawl the FrameTree. This makes use of
  // RenderViewHostImpl::From which performs a static_cast on the
  // RenderWidgetHostOwnerDelegate. Our MockRenderWidgetHostOwnerDelegate is not
  // a RenderViewHostImpl, so it crashes. Clear this here as it is not needed
  // for TearDown.
  host_->set_owner_delegate(nullptr);
#endif  // BUILDFLAG(IS_ANDROID)
  host_->SetView(nullptr);
  view->Destroy();
}

// Test that the RenderWidgetHost tells the renderer when it is hidden and
// shown, and can accept a racey update from the renderer after hiding.
TEST_F(RenderWidgetHostTest, HideShowMessages) {
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  {
    base::RunLoop run_loop;
    widget_.SetShownHiddenCallback(run_loop.QuitClosure());
    host_->WasHidden();
    run_loop.Run();
  }
  EXPECT_TRUE(host_->is_hidden_);
  ASSERT_TRUE(widget_.IsHidden().has_value());
  EXPECT_TRUE(widget_.IsHidden().value());

  // Send it an update as from the renderer.
  process_->sink().ClearMessages();
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = gfx::Size(100, 100);
  metadata.local_surface_id = std::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);

  // Now unhide.
  widget_.ClearHidden();
  ASSERT_FALSE(widget_.IsHidden().has_value());
  {
    base::RunLoop run_loop;
    widget_.SetShownHiddenCallback(run_loop.QuitClosure());

    host_->WasShown({} /* record_tab_switch_time_request */);
    run_loop.Run();
  }
  EXPECT_FALSE(host_->is_hidden_);

  // It should have sent out a mojo message.
  ASSERT_TRUE(widget_.IsHidden().has_value());
  EXPECT_FALSE(widget_.IsHidden().value());
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_FALSE(delegate_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, SendEditCommandsBeforeKeyEvent) {
  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::Type::kRawKeyDown);

  // Make sure we sent commands and key event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());

  ASSERT_TRUE(dispatched_events[0]->ToEditCommand());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  // Send the simulated response from the renderer back.
  dispatched_events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
}

TEST_F(RenderWidgetHostTest, PreHandleRawKeyDownEvent) {
  // Simulate the situation that the browser handled the key down event during
  // pre-handle phrase.
  delegate_->set_prehandle_keyboard_event(true);

  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::Type::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the commands and key event are not sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);

  // Make sure the Char event is suppressed.
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::Type::kKeyUp);

  // Make sure the KeyUp event is suppressed.
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Forward the Esc RawKeyDown event.
  std::vector<blink::mojom::EditCommandPtr> edit_commands;
  edit_commands.push_back(blink::mojom::EditCommand::New("name", "value"));
  auto event = CreateNativeWebKeyboardEvent(WebInputEvent::Type::kKeyUp);
  event.windows_key_code = ui::VKEY_ESCAPE;
  host_->ForwardKeyboardEventWithCommands(event, ui::LatencyInfo(),
                                          std::move(edit_commands), nullptr);

  // The event should be prehandled by the browser but will never be sent to the
  // renderer, no matter the event is handled or not.
  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Type::kKeyUp,
            delegate_->prehandle_keyboard_event_type());

  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Simulate a new RawKeyDown event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_TRUE(delegate_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, RawKeyDownShortcutEvent) {
  // Simulate the situation that the browser marks the key down as a keyboard
  // shortcut, but doesn't consume it in the pre-handle phase.
  delegate_->set_prehandle_keyboard_event_is_shortcut(true);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event_is_shortcut(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);

  // The Char event is not suppressed; the renderer will ignore it
  // if the preceding RawKeyDown shortcut goes unhandled.
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kChar,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(WebInputEvent::Type::kChar,
            delegate_->unhandled_keyboard_event_type());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::Type::kKeyUp);

  // Make sure only KeyUp was sent to the renderer.
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kKeyUp,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(WebInputEvent::Type::kKeyUp,
            delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, UnhandledWheelEvent) {
  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_TRUE(delegate_->handle_wheel_event_called());
  EXPECT_EQ(1, view_->unhandled_wheel_event_count());
  EXPECT_EQ(-5, view_->unhandled_wheel_event().delta_x);
}

TEST_F(RenderWidgetHostTest, HandleWheelEvent) {
  // Indicate that we're going to handle this wheel event
  delegate_->set_handle_wheel_event(true);

  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // ensure the wheel event handler was invoked
  EXPECT_TRUE(delegate_->handle_wheel_event_called());

  // and that it suppressed the unhandled wheel event handler.
  EXPECT_EQ(0, view_->unhandled_wheel_event_count());
}

TEST_F(RenderWidgetHostTest, EventsCausingFocus) {
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  EXPECT_EQ(1, delegate_->GetFocusOwningWebContentsCallCount());

  PressTouchPoint(0, 1);
  SendTouchEvent();
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  SimulateGestureEvent(WebInputEvent::Type::kGestureTap,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(3, delegate_->GetFocusOwningWebContentsCallCount());
}

TEST_F(RenderWidgetHostTest, UnhandledGestureEvent) {
  SimulateGestureEvent(WebInputEvent::Type::kGestureTwoFingerTap,
                       blink::WebGestureDevice::kTouchscreen);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kGestureTwoFingerTap,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_EQ(WebInputEvent::Type::kGestureTwoFingerTap,
            view_->gesture_event_type());
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            view_->ack_result());
}

// Test that the hang monitor timer expires properly if a new timer is started
// while one is in progress (see crbug.com/11007).
TEST_F(RenderWidgetHostTest, DontPostponeInputEventAckTimeout) {
  base::TimeDelta delay = kHungRendererDelay;

  // Start a timeout.
  host_->StartInputEventAckTimeout();

  task_environment_.FastForwardBy(delay / 2);

  // Add another timeout.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartInputEventAckTimeout();

  // Wait long enough for first timeout and see if it fired.
  task_environment_.FastForwardBy(delay);
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, stopped,
// and then started again.
TEST_F(RenderWidgetHostTest, StopAndStartInputEventAckTimeout) {
  // Start a timeout, then stop it.
  host_->StartInputEventAckTimeout();
  host_->StopInputEventAckTimeout();

  // Start it again to ensure it still works.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartInputEventAckTimeout();

  // Wait long enough for first timeout and see if it fired.
  task_environment_.FastForwardBy(kHungRendererDelay + base::Milliseconds(10));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer is effectively disabled when the widget is
// hidden.
TEST_F(RenderWidgetHostTest, InputEventAckTimeoutDisabledForInputWhenHidden) {
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove, 10, 10, 0, false);

  // Hiding the widget should deactivate the timeout.
  host_->WasHidden();

  // The timeout should not fire.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  task_environment_.FastForwardBy(kHungRendererDelay + base::Milliseconds(10));
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // The timeout should never reactivate while hidden.
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove, 10, 10, 0, false);
  task_environment_.FastForwardBy(kHungRendererDelay + base::Milliseconds(10));
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // Showing the widget should restore the timeout, as the events have
  // not yet been ack'ed.
  host_->WasShown({} /* record_tab_switch_time_request */);
  task_environment_.FastForwardBy(kHungRendererDelay + base::Milliseconds(10));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor catches two input events but only one ack.
// This can happen if the second input event causes the renderer to hang.
// This test will catch a regression of crbug.com/111185.
TEST_F(RenderWidgetHostTest, MultipleInputEvents) {
  // Send two events but only one ack.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  task_environment_.FastForwardBy(kHungRendererDelay / 2);
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Wait long enough for second timeout and see if it fired.
  task_environment_.FastForwardBy(kHungRendererDelay + base::Milliseconds(10));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

TEST_F(RenderWidgetHostTest, IgnoreInputEvent) {
  host_->SetupForInputRouterTest();

  delegate_->SetIgnoreInputEvents(true);

  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  SimulateMouseEvent(WebInputEvent::Type::kMouseMove);
  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  SimulateWheelEvent(0, 100, 0, true);
  EXPECT_FALSE(host_->mock_input_router()->sent_wheel_event_);

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(host_->mock_input_router()->sent_gesture_event_);

  PressTouchPoint(100, 100);
  SendTouchEvent();
  EXPECT_FALSE(host_->mock_input_router()->send_touch_event_not_cancelled_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerIgnoresEvent) {
  host_->SetupForInputRouterTest();
  host_->AddKeyPressEventCallback(base::BindRepeating(
      &RenderWidgetHostTest::KeyPressEventCallback, base::Unretained(this)));
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerSuppressFollowingEvents) {
  host_->SetupForInputRouterTest();

  host_->AddKeyPressEventCallback(base::BindRepeating(
      &RenderWidgetHostTest::KeyPressEventCallback, base::Unretained(this)));

  // The callback handles the first event
  handle_key_press_event_ = true;
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Following Char events should be suppressed
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Sending RawKeyDown event should stop suppression
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);

  host_->mock_input_router()->sent_keyboard_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, MouseEventCallbackCanHandleEvent) {
  host_->SetupForInputRouterTest();

  host_->AddMouseEventCallback(base::BindRepeating(
      &RenderWidgetHostTest::MouseEventCallback, base::Unretained(this)));

  handle_mouse_event_ = true;
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  handle_mouse_event_ = false;
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_mouse_event_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHasTouchEventHandlers) {
  host_->SetupForInputRouterTest();

  ASSERT_FALSE(host_->mock_input_router()->has_handlers_);

  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));
  EXPECT_TRUE(host_->mock_input_router()->has_handlers_);
}

void CheckLatencyInfoComponentInMessage(
    MockWidgetInputHandler::MessageVector& dispatched_events,
    WebInputEvent::Type expected_type) {
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  EXPECT_TRUE(dispatched_events[0]->ToEvent()->Event()->Event().GetType() ==
              expected_type);
  EXPECT_TRUE(
      dispatched_events[0]->ToEvent()->Event()->latency_info().FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
}

void CheckLatencyInfoComponentInGestureScrollUpdate(
    MockWidgetInputHandler::MessageVector& dispatched_events) {
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kTouchScrollStarted,
            dispatched_events[0]->ToEvent()->Event()->Event().GetType());

  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_events[1]->ToEvent()->Event()->Event().GetType());
  EXPECT_TRUE(
      dispatched_events[1]->ToEvent()->Event()->latency_info().FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  dispatched_events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
}

// Tests that after input event passes through RWHI through ForwardXXXEvent()
// or ForwardXXXEventWithLatencyInfo(), LatencyInfo component
// ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT will always present in the
// event's LatencyInfo.
TEST_F(RenderWidgetHostTest, InputEventRWHLatencyComponent) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  // Tests RWHI::ForwardWheelEvent().
  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseWheel);

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo(),
                                    WebMouseWheelEvent::kPhaseChanged);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseWheel);

  // Tests RWHI::ForwardMouseEvent().
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseMove);

  // Tests RWHI::ForwardMouseEventWithLatencyInfo().
  SimulateMouseEventWithLatencyInfo(WebInputEvent::Type::kMouseMove,
                                    ui::LatencyInfo());
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseMove);

  // Tests RWHI::ForwardGestureEvent().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  widget_.SetTouchActionFromMain(cc::TouchAction::kAuto);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kTouchStart);

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kGestureScrollBegin);

  // Tests RIR::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::Type::kGestureScrollUpdate,
                                      blink::WebGestureDevice::kTouchscreen,
                                      ui::LatencyInfo());
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInGestureScrollUpdate(dispatched_events);

  ReleaseTouchPoint(0);
  SendTouchEvent();
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();

  // Tests RWHI::ForwardTouchEventWithLatencyInfo().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kTouchStart);
}

TEST_F(RenderWidgetHostTest, RendererExitedResetsInputRouter) {
  EXPECT_EQ(0u, host_->in_flight_event_count());
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(1u, host_->in_flight_event_count());

  EXPECT_FALSE(host_->input_router()->HasPendingEvents());
  blink::WebMouseWheelEvent event;
  event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  host_->input_router()->SendWheelEvent(
      input::MouseWheelEventWithLatencyInfo(event));
  EXPECT_TRUE(host_->input_router()->HasPendingEvents());

  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->RendererExited();

  // The renderer is recreated.
  host_->SetView(view_.get());
  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  blink::VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  ReinitalizeHost();

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
  // There should be no in flight events. https://crbug.com/615090#152.
  EXPECT_EQ(0u, host_->in_flight_event_count());
}

TEST_F(RenderWidgetHostTest, DestroyingRenderWidgetResetsInputRouter) {
  EXPECT_EQ(0u, host_->in_flight_event_count());
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(1u, host_->in_flight_event_count());

  EXPECT_FALSE(host_->input_router()->HasPendingEvents());
  blink::WebMouseWheelEvent event;
  event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  host_->input_router()->SendWheelEvent(
      input::MouseWheelEventWithLatencyInfo(event));
  EXPECT_TRUE(host_->input_router()->HasPendingEvents());

  // The RenderWidget is destroyed in the renderer process as the main frame
  // is removed from this RenderWidgetHost's RenderWidgetView, but the
  // RenderWidgetView is still kept around for another RenderFrame.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(false));

  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  blink::VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(true));

  // Make sure the input router is in a fresh state.
  EXPECT_FALSE(host_->input_router()->HasPendingEvents());
  // There should be no in flight events. https://crbug.com/615090#152.
  EXPECT_EQ(0u, host_->in_flight_event_count());
}

TEST_F(RenderWidgetHostTest, RendererExitedResetsScreenRectsAck) {
  // Screen rects are sent during initialization, but we are waiting for an ack.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedScreenRects().size());
  // Waiting for the ack prevents further sending.
  host_->SendScreenRects();
  host_->SendScreenRects();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedScreenRects().size());

  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->RendererExited();

  // Still can't send until the RenderWidget is replaced.
  host_->SendScreenRects();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedScreenRects().size());

  // The renderer is recreated.
  host_->SetView(view_.get());
  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  blink::VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  ReinitalizeHost();

  // The RenderWidget is shown when navigation completes. This sends screen
  // rects again. The IPC is sent as it's not waiting for an ack.
  host_->WasShown({});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, widget_.ReceivedScreenRects().size());
}

TEST_F(RenderWidgetHostTest, DestroyingRenderWidgetResetsScreenRectsAck) {
  // Screen rects are sent during initialization, but we are waiting for an ack.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedScreenRects().size());
  // Waiting for the ack prevents further sending.
  host_->SendScreenRects();
  host_->SendScreenRects();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedScreenRects().size());

  // If screen rects haven't changed, don't send them to the widget.
  ClearScreenRects();
  host_->SendScreenRects();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedScreenRects().size());

  // The RenderWidget has been destroyed in the renderer.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(false));

  // Still can't send until the RenderWidget is replaced.
  host_->SendScreenRects();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_.ReceivedScreenRects().size());

  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  blink::VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(true));

  // We are able to send screen rects again. The IPC is sent as it's not waiting
  // for an ack.
  host_->SendScreenRects();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedScreenRects().size());
}

// Regression test for http://crbug.com/401859 and http://crbug.com/522795.
TEST_F(RenderWidgetHostTest, RendererExitedResetsIsHidden) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->WasShown({} /* record_tab_switch_time_request */);

  ASSERT_FALSE(host_->is_hidden());
  host_->RendererExited();
  ASSERT_TRUE(host_->is_hidden());

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

TEST_F(RenderWidgetHostTest, VisualProperties) {
  gfx::Rect bounds(0, 0, 100, 100);
  gfx::Rect compositor_viewport_pixel_rect(40, 50);
  view_->SetBounds(bounds);
  view_->SetMockCompositorViewportPixelSize(
      compositor_viewport_pixel_rect.size());

  blink::VisualProperties visual_properties = host_->GetVisualProperties();
  EXPECT_EQ(bounds.size(), visual_properties.new_size);
  EXPECT_EQ(compositor_viewport_pixel_rect,
            visual_properties.compositor_viewport_pixel_rect);
}

// Make sure no dragging occurs after renderer exited. See crbug.com/704832.
TEST_F(RenderWidgetHostTest, RendererExitedNoDrag) {
  host_->SetView(new TestView(host_.get()));

  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 0);

  GURL http_url = GURL("http://www.domain.com/index.html");
  DropData drop_data;
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  FileSystemAccessManagerImpl* file_system_manager =
      static_cast<StoragePartitionImpl*>(process_->GetStoragePartition())
          ->GetFileSystemAccessManager();
  blink::DragOperationsMask drag_operation = blink::kDragOperationEvery;
  host_->StartDragging(
      DropDataToDragData(
          drop_data, file_system_manager, process_->GetID(),
          ChromeBlobStorageContext::GetFor(process_->GetBrowserContext())),
      url::Origin(), drag_operation, SkBitmap(), gfx::Vector2d(), gfx::Rect(),
      blink::mojom::DragEventSourceInfo::New());
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);

  // Simulate that renderer exited due navigation to the next page.
  host_->RendererExited();
  EXPECT_FALSE(host_->GetView());
  host_->StartDragging(
      DropDataToDragData(
          drop_data, file_system_manager, process_->GetID(),
          ChromeBlobStorageContext::GetFor(process_->GetBrowserContext())),
      url::Origin(), drag_operation, SkBitmap(), gfx::Vector2d(), gfx::Rect(),
      blink::mojom::DragEventSourceInfo::New());
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);
}

// Hiding the RenderWidgetHostImpl instance via a call to WasHidden should
// not reject a pending pointer lock, if the operation is waiting for the
// user to make a selection on the permission prompt.
TEST_F(RenderWidgetHostTest,
       WasHiddenDoesNotRejectPointerLockIfWaitingForPrompt) {
  // Set up the mock delegate to return true for
  // IsWaitingForPointerLockPrompt().
  EXPECT_CALL(*delegate_, IsWaitingForPointerLockPrompt(
                              static_cast<RenderWidgetHostImpl*>(host_.get())))
      .WillOnce(Return(true));

  // Hide the RenderWidgetHostImpl instance.
  host_->WasHidden();

  EXPECT_FALSE(host_->pointer_lock_rejected());
}

// Hiding the RenderWidgetHostImpl instance via a call to WasHidden should
// reject a pending pointer lock, if the operation is not waiting for the
// user to make a selection on the permission prompt.
TEST_F(RenderWidgetHostTest, WasHiddenRejectsPointerLockIfNotWaitingForPrompt) {
  // Set up the mock delegate to return false for
  // IsWaitingForPointerLockPrompt().
  EXPECT_CALL(*delegate_, IsWaitingForPointerLockPrompt(
                              static_cast<RenderWidgetHostImpl*>(host_.get())))
      .WillOnce(Return(false));

  // Hide the RenderWidgetHostImpl instance.
  host_->WasHidden();

  EXPECT_TRUE(host_->pointer_lock_rejected());
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
  // with the request to new up the `blink::WebView` and so subsequent
  // SynchronizeVisualProperties calls should not result in new IPC (unless the
  // size has actually changed).
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_EQ(initial_size_, host_->old_visual_properties_->new_size);
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
}

TEST_F(RenderWidgetHostTest, HideUnthrottlesResize) {
  ClearVisualProperties();
  view_->SetBounds(gfx::Rect(100, 100));
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // blink::mojom::Widget::UpdateVisualProperties sent to the renderer.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_.ReceivedVisualProperties().size());
  {
    // Size sent to the renderer.
    EXPECT_EQ(gfx::Size(100, 100),
              widget_.ReceivedVisualProperties().at(0).new_size);
  }
  // An ack is pending, throttling further updates.
  EXPECT_TRUE(host_->visual_properties_ack_pending_);

  // Hiding the widget should unthrottle resize.
  host_->WasHidden();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
}

// Tests that event dispatch after the delegate has been detached doesn't cause
// a crash. See crbug.com/563237.
TEST_F(RenderWidgetHostTest, EventDispatchPostDetach) {
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));
  process_->sink().ClearMessages();

  host_->DetachDelegate();

  // Tests RIR::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::Type::kGestureScrollUpdate,
                                      blink::WebGestureDevice::kTouchscreen,
                                      ui::LatencyInfo());

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo());

  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

// If a navigation happens while the widget is hidden, we shouldn't show
// contents of the previous page when we become visible.
TEST_F(RenderWidgetHostTest, NavigateInBackgroundShowsBlank) {
  // When visible, navigation does not immediately call into
  // ClearDisplayedGraphics.
  host_->WasShown({} /* record_tab_switch_time_request */);
  host_->DidNavigate();
  host_->StartNewContentRenderingTimeout();
  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());

  // Hide then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->WasShown({} /* record_tab_switch_time_request */);
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Hide, navigate, then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->DidNavigate();
  host_->StartNewContentRenderingTimeout();
  host_->WasShown({} /* record_tab_switch_time_request */);
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
}

TEST_F(RenderWidgetHostTest, PendingUserActivationTimeout) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kBrowserVerifiedUserActivationMouse,
       features::kBrowserVerifiedUserActivationKeyboard},
      {});

  // One event allows one activation notification.
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  EXPECT_TRUE(host_->RemovePendingUserActivationIfAvailable());
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());

  // Mouse move and up does not increase pending user activation counter.
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove);
  SimulateMouseEvent(WebInputEvent::Type::kMouseUp);
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());

  // 2 events allow 2 activation notifications.
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  SimulateKeyboardEvent(WebInputEvent::Type::kKeyDown);
  EXPECT_TRUE(host_->RemovePendingUserActivationIfAvailable());
  EXPECT_TRUE(host_->RemovePendingUserActivationIfAvailable());
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());

  // Timer reset the pending activation.
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  task_environment_.FastForwardBy(
      RenderWidgetHostImpl::kActivationNotificationExpireTime);
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());
}

// Tests that fling events are not dispatched when the wheel event is consumed.
TEST_F(RenderWidgetHostTest, NoFlingEventsWhenLastScrollEventConsumed) {
  // Simulate a consumed wheel event.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // A GestureFlingStart event following a consumed scroll event should not be
  // dispatched.
  SimulateGestureEvent(blink::WebInputEvent::Type::kGestureFlingStart,
                       blink::WebGestureDevice::kTouchpad);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());
}

// Tests that fling events are dispatched when some, but not all, scroll events
// were consumed.
TEST_F(RenderWidgetHostTest, FlingEventsWhenSomeScrollEventsConsumed) {
  // Simulate a consumed wheel event.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Followed by a not consumed wheel event.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // A GestureFlingStart event following the scroll events should be dispatched.
  SimulateGestureEvent(blink::WebInputEvent::Type::kGestureFlingStart,
                       blink::WebGestureDevice::kTouchpad);
  dispatched_events =
      host_->mock_render_input_router()->GetAndResetDispatchedMessages();
  EXPECT_NE(0u, dispatched_events.size());
}

TEST_F(RenderWidgetHostTest, AddAndRemoveInputEventObserver) {
  MockInputEventObserver observer;

  // Add InputEventObserver.
  host_->AddInputEventObserver(&observer);

  // Confirm OnInputEvent is triggered.
  input::NativeWebKeyboardEvent native_event =
      CreateNativeWebKeyboardEvent(WebInputEvent::Type::kChar);
  ui::LatencyInfo latency_info = ui::LatencyInfo();
  ui::EventLatencyMetadata event_latency_metadata;
  EXPECT_CALL(observer, OnInputEvent(_)).Times(1);
  host_->GetRenderInputRouter()->DispatchInputEventWithLatencyInfo(
      native_event, &latency_info, &event_latency_metadata);

  // Remove InputEventObserver.
  host_->RemoveInputEventObserver(&observer);

  // Confirm InputEventObserver is removed.
  EXPECT_CALL(observer, OnInputEvent(_)).Times(0);
  latency_info = ui::LatencyInfo();
  event_latency_metadata = ui::EventLatencyMetadata();
  host_->GetRenderInputRouter()->DispatchInputEventWithLatencyInfo(
      native_event, &latency_info, &event_latency_metadata);
}

TEST_F(RenderWidgetHostTest, ScopedObservationWithInputEventObserver) {
  // Verify that the specialization of `ScopedObserverationTraits` correctly
  // adds and removes InputEventObservers.
  MockInputEventObserver observer;
  base::ScopedObservation<RenderWidgetHost,
                          RenderWidgetHost::InputEventObserver>
      scoped_observation(&observer);

  // Add InputEventObserver.
  scoped_observation.Observe(host_.get());

  // Confirm OnInputEvent is triggered.
  input::NativeWebKeyboardEvent native_event =
      CreateNativeWebKeyboardEvent(WebInputEvent::Type::kChar);
  ui::LatencyInfo latency_info = ui::LatencyInfo();
  ui::EventLatencyMetadata event_latency_metadata;
  EXPECT_CALL(observer, OnInputEvent(_)).Times(1);
  host_->GetRenderInputRouter()->DispatchInputEventWithLatencyInfo(
      native_event, &latency_info, &event_latency_metadata);

  // Remove InputEventObserver.
  scoped_observation.Reset();

  // Confirm InputEventObserver is removed.
  EXPECT_CALL(observer, OnInputEvent(_)).Times(0);
  latency_info = ui::LatencyInfo();
  event_latency_metadata = ui::EventLatencyMetadata();
  host_->GetRenderInputRouter()->DispatchInputEventWithLatencyInfo(
      native_event, &latency_info, &event_latency_metadata);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(RenderWidgetHostTest, AddAndRemoveImeInputEventObserver) {
  MockInputEventObserver observer;

  // Add ImeInputEventObserver.
  host_->AddImeInputEventObserver(&observer);

  // Confirm ImeFinishComposingTextEvent is triggered.
  EXPECT_CALL(observer, OnImeFinishComposingTextEvent()).Times(1);
  host_->ImeFinishComposingText(true);

  // Remove ImeInputEventObserver.
  host_->RemoveImeInputEventObserver(&observer);

  // Confirm ImeInputEventObserver is removed.
  EXPECT_CALL(observer, OnImeFinishComposingTextEvent()).Times(0);
  host_->ImeFinishComposingText(true);
}
#endif

// Tests that vertical scroll direction changes are propagated to the delegate.
TEST_F(RenderWidgetHostTest, OnVerticalScrollDirectionChanged) {
  const auto NotifyVerticalScrollDirectionChanged =
      [this](viz::VerticalScrollDirection scroll_direction) {
        static uint32_t frame_token = 1u;
        host_->frame_token_message_queue_->DidProcessFrame(
            frame_token, base::TimeTicks::Now());

        cc::RenderFrameMetadata metadata;
        metadata.new_vertical_scroll_direction = scroll_direction;
        static_cast<cc::mojom::RenderFrameMetadataObserverClient*>(
            host_->render_frame_metadata_provider())
            ->OnRenderFrameMetadataChanged(frame_token++, metadata);
      };

  // Verify initial state.
  EXPECT_EQ(0, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            delegate_->GetLastVerticalScrollDirection());

  // Verify that we will *not* propagate a vertical scroll of |kNull| which is
  // only used to indicate the absence of a change in vertical scroll direction.
  NotifyVerticalScrollDirectionChanged(viz::VerticalScrollDirection::kNull);
  EXPECT_EQ(0, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            delegate_->GetLastVerticalScrollDirection());

  // Verify that we will propagate a vertical scroll |kUp|.
  NotifyVerticalScrollDirectionChanged(viz::VerticalScrollDirection::kUp);
  EXPECT_EQ(1, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kUp,
            delegate_->GetLastVerticalScrollDirection());

  // Verify that we will propagate a vertical scroll |kDown|.
  NotifyVerticalScrollDirectionChanged(viz::VerticalScrollDirection::kDown);
  EXPECT_EQ(2, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            delegate_->GetLastVerticalScrollDirection());
}

TEST_F(RenderWidgetHostTest, SetCursorWithBitmap) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorGREEN);

  const ui::Cursor cursor =
      ui::Cursor::NewCustom(std::move(bitmap), gfx::Point());
  host_->SetCursor(cursor);
  EXPECT_EQ(cursor, view_->last_cursor());
}

}  // namespace content
