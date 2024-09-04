// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/input/input_router.h"
#include "components/input/mouse_wheel_event_queue.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/fake_surface_observer.h"
#include "components/viz/test/test_latest_local_surface_id_lookup_delegate.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/delegated_frame_host_client_aura.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_aura.h"
#include "content/common/features.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/test/fake_frame_widget.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_image_transport_factory.h"
#include "content/test/mock_render_input_router.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/test_overscroll_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/scoped_keyboard_hook.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/motion_event_aura.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/selection_bound.h"
#include "ui/wm/core/window_util.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/window_event_target.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

using testing::_;

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::WebInputEventTraits;
using viz::FrameEvictionManager;

#define EXPECT_EVICTED(view)                   \
  {                                            \
    EXPECT_FALSE((view)->HasPrimarySurface()); \
    EXPECT_FALSE((view)->HasSavedFrame());     \
  }

#define EXPECT_HAS_FRAME(view)                \
  {                                           \
    EXPECT_TRUE((view)->HasPrimarySurface()); \
    EXPECT_TRUE((view)->HasSavedFrame());     \
  }

namespace content {

void ParentHostView(RenderWidgetHostView* host_view,
                    RenderWidgetHostView* parent_host_view,
                    const gfx::Rect& bounds = gfx::Rect()) {
  aura::client::ParentWindowWithContext(
      host_view->GetNativeView(),
      parent_host_view->GetNativeView()->GetRootWindow(), bounds,
      display::kInvalidDisplayId);
}

void InstallDelegatedFrameHostClient(
    RenderWidgetHostViewAura* render_widget_host_view);

const viz::LocalSurfaceId kArbitraryLocalSurfaceId(
    1,
    base::UnguessableToken::CreateForTesting(2, 3));

std::string GetMessageNames(
    const MockWidgetInputHandler::MessageVector& events) {
  std::vector<std::string> result;
  for (auto& event : events)
    result.push_back(event->name());
  return base::JoinString(result, " ");
}

// Simple observer that keeps track of changes to a window for tests.
class TestWindowObserver : public aura::WindowObserver {
 public:
  explicit TestWindowObserver(aura::Window* window_to_observe)
      : window_(window_to_observe) {
    window_->AddObserver(this);
  }

  TestWindowObserver(const TestWindowObserver&) = delete;
  TestWindowObserver& operator=(const TestWindowObserver&) = delete;

  ~TestWindowObserver() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  bool destroyed() const { return destroyed_; }

  // aura::WindowObserver overrides:
  void OnWindowDestroyed(aura::Window* window) override {
    CHECK_EQ(window, window_);
    destroyed_ = true;
    window_ = nullptr;
  }

 private:
  // Window that we're observing, or nullptr if it's been destroyed.
  raw_ptr<aura::Window> window_;

  // Was |window_| destroyed?
  bool destroyed_;
};

class FakeWindowEventDispatcher : public aura::WindowEventDispatcher {
 public:
  FakeWindowEventDispatcher(aura::WindowTreeHost* host)
      : WindowEventDispatcher(host), processed_touch_event_count_(0) {}

  void ProcessedTouchEvent(uint32_t unique_event_id,
                           aura::Window* window,
                           ui::EventResult result,
                           bool is_source_touch_event_set_blocking) override {
    WindowEventDispatcher::ProcessedTouchEvent(
        unique_event_id, window, result, is_source_touch_event_set_blocking);
    processed_touch_event_count_++;
  }

  size_t GetAndResetProcessedTouchEventCount() {
    size_t count = processed_touch_event_count_;
    processed_touch_event_count_ = 0;
    return count;
  }

 private:
  size_t processed_touch_event_count_;
};

class FakeRenderWidgetHostViewAura : public RenderWidgetHostViewAura {
 public:
  FakeRenderWidgetHostViewAura(RenderWidgetHost* widget)
      : RenderWidgetHostViewAura(widget) {
    InstallDelegatedFrameHostClient(this);
  }

  FakeRenderWidgetHostViewAura(const FakeRenderWidgetHostViewAura&) = delete;
  FakeRenderWidgetHostViewAura& operator=(const FakeRenderWidgetHostViewAura&) =
      delete;

  ~FakeRenderWidgetHostViewAura() override = default;

  void UseFakeDispatcher() {
    aura::WindowTreeHost* host = window()->GetHost();
    DCHECK(host);
    auto dispatcher = std::make_unique<FakeWindowEventDispatcher>(host);
    dispatcher_ = dispatcher.get();
    aura::test::SetHostDispatcher(host, std::move(dispatcher));
  }

  void RunOnCompositingDidCommit() {
    GetDelegatedFrameHost()->OnCompositingDidCommitForTesting(
        window()->GetHost()->compositor());
  }

  viz::SurfaceId surface_id() const {
    return GetDelegatedFrameHost()->GetCurrentSurfaceId();
  }

  bool HasPrimarySurface() const {
    return GetDelegatedFrameHost()->HasPrimarySurface();
  }

  bool HasFallbackSurface() const override {
    return GetDelegatedFrameHost()->HasFallbackSurface();
  }

  bool HasSavedFrame() const {
    return GetDelegatedFrameHost()->HasSavedFrame();
  }

  const ui::MotionEventAura& pointer_state() {
    return event_handler()->pointer_state();
  }

  void SetRenderFrameMetadata(cc::RenderFrameMetadata metadata) {
    host()->render_frame_metadata_provider()->SetLastRenderFrameMetadataForTest(
        metadata);
  }

  bool ShouldSkipCursorUpdate() const override { return false; }

  gfx::Size last_frame_size_;
  raw_ptr<FakeWindowEventDispatcher> dispatcher_;
};

// A layout manager that always resizes a child to the root window size.
class FullscreenLayoutManager : public aura::LayoutManager {
 public:
  explicit FullscreenLayoutManager(aura::Window* owner) : owner_(owner) {}

  FullscreenLayoutManager(const FullscreenLayoutManager&) = delete;
  FullscreenLayoutManager& operator=(const FullscreenLayoutManager&) = delete;

  ~FullscreenLayoutManager() override {}

  // Overridden from aura::LayoutManager:
  void OnWindowResized() override {
    aura::Window::Windows::const_iterator i;
    for (i = owner_->children().begin(); i != owner_->children().end(); ++i) {
      (*i)->SetBounds(gfx::Rect());
    }
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(gfx::Rect());
  }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, gfx::Rect(owner_->bounds().size()));
  }

 private:
  raw_ptr<aura::Window> owner_;
};

class MockRenderWidgetHostImpl : public RenderWidgetHostImpl {
 public:
  using RenderWidgetHostImpl::render_input_router_;

  ~MockRenderWidgetHostImpl() override = default;

  // Extracts |latency_info| for wheel event, and stores it in
  // |last_wheel_or_touch_event_latency_info_|.
  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& ui_latency) override {
    RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo(wheel_event,
                                                           ui_latency);
    GetMockRenderInputRouter()->SetLastWheelOrTouchEventLatencyInfo(
        ui::LatencyInfo(ui_latency));
  }

  void ClearVisualProperties() {
    base::RunLoop().RunUntilIdle();
    widget_.ClearVisualProperties();
  }

  void ClearScreenRects() {
    base::RunLoop().RunUntilIdle();
    widget_.ClearScreenRects();
  }

  const std::vector<blink::VisualProperties>& visual_properties() {
    return widget_.ReceivedVisualProperties();
  }

  const std::vector<std::pair<gfx::Rect, gfx::Rect>>& screen_rects() {
    return widget_.ReceivedScreenRects();
  }

  // Instance self-delete when its |site_instance_group|'s process will
  // exit.
  static MockRenderWidgetHostImpl* Create(
      FrameTree& frame_tree,
      RenderWidgetHostDelegate* delegate,
      base::SafeRef<SiteInstanceGroup> site_instance_group,
      int32_t routing_id,
      bool hidden) {
    return new MockRenderWidgetHostImpl(&frame_tree, delegate,
                                        std::move(site_instance_group),
                                        routing_id, hidden);
  }

  MockWidgetInputHandler* input_handler() {
    return GetMockRenderInputRouter()->mock_widget_input_handler_.get();
  }

  MockRenderInputRouter* GetMockRenderInputRouter() {
    return static_cast<MockRenderInputRouter*>(render_input_router_.get());
  }

  input::RenderInputRouter* GetRenderInputRouter() override {
    return render_input_router_.get();
  }

  void reset_new_content_rendering_timeout_fired() {
    new_content_rendering_timeout_fired_ = false;
  }

  bool new_content_rendering_timeout_fired() const {
    return new_content_rendering_timeout_fired_;
  }

  void SetTouchActionFromMain(cc::TouchAction touch_action) {
    widget_.SetTouchActionFromMain(touch_action);
  }

 private:
  MockRenderWidgetHostImpl(FrameTree* frame_tree,
                           RenderWidgetHostDelegate* delegate,
                           base::SafeRef<SiteInstanceGroup> site_instance_group,
                           int32_t routing_id,
                           bool hidden)
      : RenderWidgetHostImpl(
            frame_tree,
            /*self_owned=*/true,
            DefaultFrameSinkId(*site_instance_group, routing_id),
            delegate,
            site_instance_group,
            routing_id,
            hidden,
            /*renderer_initiated_creation=*/false,
            std::make_unique<FrameTokenMessageQueue>()) {
    SetupMockRenderInputRouter();
    BindWidgetInterfaces(mojo::AssociatedRemote<blink::mojom::WidgetHost>()
                             .BindNewEndpointAndPassDedicatedReceiver(),
                         widget_.GetNewRemote());
  }

  void NotifyNewContentRenderingTimeoutForTesting() override {
    new_content_rendering_timeout_fired_ = true;
  }

  void SetupMockRenderInputRouter() {
    render_input_router_.reset();
    render_input_router_ = std::make_unique<MockRenderInputRouter>(
        this, MakeFlingScheduler(), this,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    SetupInputRouter();
  }

  bool new_content_rendering_timeout_fired_ = false;
  MockWidget widget_;
};

class TestScopedKeyboardHook : public aura::ScopedKeyboardHook {
 public:
  TestScopedKeyboardHook();

  TestScopedKeyboardHook(const TestScopedKeyboardHook&) = delete;
  TestScopedKeyboardHook& operator=(const TestScopedKeyboardHook&) = delete;

  ~TestScopedKeyboardHook() override;

  // aura::ScopedKeyboardHook override.
  bool IsKeyLocked(ui::DomCode dom_code) override;

  // Set up the keys being locked for testing.  One of these methods must be
  // called before using an instance.
  void LockAllKeys();
  void LockSpecificKey(ui::DomCode dom_code);

 private:
  bool keyboard_lock_active_ = false;
  std::optional<ui::DomCode> locked_key_;
};

TestScopedKeyboardHook::TestScopedKeyboardHook() = default;

TestScopedKeyboardHook::~TestScopedKeyboardHook() = default;

bool TestScopedKeyboardHook::IsKeyLocked(ui::DomCode dom_code) {
  DCHECK(keyboard_lock_active_) << "Did you forget to reserve keys to lock?";
  return !locked_key_ || (locked_key_.value() == dom_code);
}

void TestScopedKeyboardHook::LockAllKeys() {
  keyboard_lock_active_ = true;
  locked_key_.reset();
}

void TestScopedKeyboardHook::LockSpecificKey(ui::DomCode dom_code) {
  keyboard_lock_active_ = true;
  locked_key_ = dom_code;
}

class RenderWidgetHostViewAuraTest : public testing::Test {
 public:
  RenderWidgetHostViewAuraTest() {
    ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
        0);
  }

  RenderWidgetHostViewAuraTest(const RenderWidgetHostViewAuraTest&) = delete;
  RenderWidgetHostViewAuraTest& operator=(const RenderWidgetHostViewAuraTest&) =
      delete;

  static void InstallDelegatedFrameHostClient(RenderWidgetHostViewAura* view) {
    // Follow RWHVAura code that does not create DelegateFrameHost when there is
    // no valid frame sink id.
    if (!view->frame_sink_id_.is_valid())
      return;
    view->delegated_frame_host_ = nullptr;
    view->delegated_frame_host_ = std::make_unique<DelegatedFrameHost>(
        view->frame_sink_id_, view->delegated_frame_host_client_.get(),
        false /* should_register_frame_sink_id */);
  }

  FakeRenderWidgetHostViewAura* CreateView(bool hidden = false) {
    int32_t routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(std::make_unique<MockRenderWidgetHostDelegate>());
    auto* widget_host = MockRenderWidgetHostImpl::Create(
        GetFrameTree(), delegates_.back().get(),
        site_instance_group_->GetSafeRef(), routing_id, hidden);
    delegates_.back()->set_widget_host(widget_host);

    return new FakeRenderWidgetHostViewAura(widget_host);
  }

  FrameTree& GetFrameTree() {
    DCHECK(web_contents_);
    return static_cast<WebContentsImpl*>(web_contents_.get())
        ->GetPrimaryFrameTree();
  }

  void DestroyView(FakeRenderWidgetHostViewAura* view) {
    // For guest-views, |view_| is not the view used by |widget_host_|.
    RenderWidgetHostImpl* host = view->host();
    EXPECT_EQ(view, host->GetView());
    view->Destroy();
    EXPECT_EQ(nullptr, host->GetView());
  }

  void SetUpEnvironment() {
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>(
        ImageTransportFactory::GetInstance()->GetContextFactory());
    aura_test_helper_->SetUp();

    browser_context_ = std::make_unique<TestBrowserContext>();
    process_host_ =
        std::make_unique<MockRenderProcessHost>(browser_context_.get());
    process_host_->Init();
    auto site_instance = SiteInstance::Create(browser_context_.get());
    static_cast<SiteInstanceImpl*>(site_instance.get())
        ->SetProcessForTesting(process_host_.get());
    site_instance_group_ =
        static_cast<SiteInstanceImpl*>(site_instance.get())->group();

    sink_ = &process_host_->sink();

    web_contents_ = WebContents::Create(
        WebContents::CreateParams(browser_context_.get(), site_instance));

    int32_t routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(std::make_unique<MockRenderWidgetHostDelegate>());
    parent_host_ = MockRenderWidgetHostImpl::Create(
        GetFrameTree(), delegates_.back().get(),
        site_instance_group_->GetSafeRef(), routing_id, /*hidden = */ false);
    delegates_.back()->set_widget_host(parent_host_);

    // This is an owning pointer. Released manually in TearDownEnvironment().
    parent_view_ = new RenderWidgetHostViewAura(parent_host_);
    parent_view_->InitAsChild(nullptr);
    aura::client::ParentWindowWithContext(
        parent_view_->GetNativeView(), aura_test_helper_->GetContext(),
        gfx::Rect(), display::kInvalidDisplayId);

    parent_host_->BindFrameWidgetInterfaces(
        mojo::AssociatedRemote<blink::mojom::FrameWidgetHost>()
            .BindNewEndpointAndPassDedicatedReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());
    parent_host_->RendererWidgetCreated(/*for_frame_widget=*/true);
    // The RenderWidgetHostImpl sets up additional connections over mojo to the
    // renderer widget, which we need to complete before the test runs.
    base::RunLoop().RunUntilIdle();

    view_ = CreateView();
    widget_host_ = static_cast<MockRenderWidgetHostImpl*>(view_->host());
    // This `view_` is left uninitialzed, and the `widget_host_` does not have
    // its renderer connections set up. These are done by the InitViewForPopup()
    // or InitViewForFrame() helpers.

    // Set the mouse_wheel_phase_handler_ timer timeout to 100ms.
    view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
        base::Milliseconds(100));
  }

  void InitViewForPopup(RenderWidgetHostViewAura* parent_view,
                        const gfx::Rect& bounds_in_screen) {
    view_->SetWidgetType(WidgetType::kPopup);
    // Let anchor have same origin as bounds, but its width and height should be
    // 1,1 as RenderWidgetHostViewAura sets OwnedWindowAnchorPosition as
    // kBottomLeft.
    gfx::Rect anchor = bounds_in_screen;
    anchor.set_size({1, 1});
    view_->InitAsPopup(parent_view, bounds_in_screen, anchor);

    widget_host_->RendererWidgetCreated(/*for_frame_widget=*/false);
    // The RenderWidgetHostImpl sets up additional connections over mojo to the
    // renderer widget, which we need to complete before the test runs.
    base::RunLoop().RunUntilIdle();
  }

  void InitViewForFrame(gfx::NativeView parent_native_view) {
    view_->InitAsChild(parent_native_view);

    // The blink::mojom::Widget interfaces are bound during
    // MockRenderWidgetHostImpl construction.
    widget_host_->BindFrameWidgetInterfaces(
        mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>()
            .InitWithNewEndpointAndPassReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());

    widget_host_->RendererWidgetCreated(/*for_frame_widget=*/true);
    // The RenderWidgetHostImpl sets up additional connections over mojo to the
    // renderer widget, which we need to complete before the test runs.
    base::RunLoop().RunUntilIdle();
  }

  void TearDownEnvironment() {
    parent_host_ = nullptr;  // Owned indirectly by `view_`, destroyed below.

    sink_ = nullptr;
    widget_host_ = nullptr;  // Owned by `view_` destroyed below:
    if (view_) {
      DestroyView(view_.ExtractAsDangling());
    }
    parent_view_.ExtractAsDangling()->Destroy();

    process_host_->Cleanup();
    site_instance_group_.reset();
    web_contents_ = nullptr;
    process_host_ = nullptr;
    browser_context_ = nullptr;
    aura_test_helper_->TearDown();

    base::RunLoop().RunUntilIdle();
    ImageTransportFactory::Terminate();
  }

  void SetUp() override {
    SetUpEnvironment();
  }

  void TearDown() override { TearDownEnvironment(); }

  void SimulateMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    // Here should be base::MemoryPressureListener::NotifyMemoryPressure, but
    // since the FrameEvictionManager is installing a MemoryPressureListener
    // which uses base::ObserverListThreadSafe, which furthermore remembers the
    // message loop for the thread it was created in. Between tests, the
    // FrameEvictionManager singleton survives and and the MessageLoop gets
    // destroyed. The correct fix would be to have base::ObserverListThreadSafe
    // look
    // up the proper message loop every time (see crbug.com/443824.)
    FrameEvictionManager::GetInstance()->OnMemoryPressure(level);
  }

  MockWidgetInputHandler::MessageVector GetAndResetDispatchedMessages() {
    return widget_host_->input_handler()->GetAndResetDispatchedMessages();
  }

  void SendNotConsumedAcks(MockWidgetInputHandler::MessageVector& events) {
    events.clear();
  }

  const ui::MotionEventAura& pointer_state() { return view_->pointer_state(); }

  bool HasTouchEventHandlers(bool has_handlers) { return has_handlers; }
  bool HasHitTestableScrollbar(bool has_scrollbar) { return has_scrollbar; }

  ui::InputMethod* GetInputMethod() const {
    return parent_view_->GetInputMethod();
  }

 protected:
  BrowserContext* browser_context() { return browser_context_.get(); }

  MockRenderWidgetHostDelegate* render_widget_host_delegate() const {
    return delegates_.back().get();
  }

  MouseWheelPhaseHandler* GetMouseWheelPhaseHandler() const {
    return view_->GetMouseWheelPhaseHandler();
  }

  TextInputManager* GetTextInputManager(RenderWidgetHostViewBase* view) const {
    return static_cast<RenderWidgetHostImpl*>(view->GetRenderWidgetHost())
        ->delegate()
        ->GetTextInputManager();
  }

#if BUILDFLAG(IS_WIN)
  LegacyRenderWidgetHostHWND* legacy_render_widget_host_HWND() const {
    return view_->legacy_render_widget_host_HWND_;
  }
#endif  // BUILDFLAG(IS_WIN)

  // Sets the |view| active in TextInputManager with the given |type|. |type|
  // cannot be ui::TEXT_INPUT_TYPE_NONE.
  // Must not be called in the destruction path of |view|.
  void ActivateViewForTextInputManager(RenderWidgetHostViewBase* view,
                                       ui::TextInputType type) {
    DCHECK_NE(ui::TEXT_INPUT_TYPE_NONE, type);
    // First mock-focus the widget if not already.
    if (render_widget_host_delegate()->GetFocusedRenderWidgetHost(
            widget_host_) != view->GetRenderWidgetHost()) {
      render_widget_host_delegate()->set_focused_widget(view->host());
    }

    TextInputManager* manager = GetTextInputManager(view);
    if (manager->GetActiveWidget()) {
      manager->active_view_for_testing()->TextInputStateChanged(
          ui::mojom::TextInputState());
    }

    if (!view)
      return;

    ui::mojom::TextInputState state_with_type_text;
    state_with_type_text.type = type;
    state_with_type_text.show_ime_if_needed = true;
    view->TextInputStateChanged(state_with_type_text);
  }

  void RunTimerBasedWheelEventPhaseInfoTest(
      bool percent_based_scrolling_enabled);

  BrowserTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<WebContents> web_contents_;
  std::vector<std::unique_ptr<MockRenderWidgetHostDelegate>> delegates_;
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;

  // Tests should set these to nullptr if they've already triggered their
  // destruction.
  raw_ptr<RenderWidgetHostImpl> parent_host_ = nullptr;
  // The `parent_view_` owns the object.
  // Note: It would be great turning this into a std::unique_ptr<>.
  raw_ptr<RenderWidgetHostViewAura> parent_view_ = nullptr;

  // Tests should set `view` to nullptr if they've already triggered their
  // destruction.
  raw_ptr<FakeRenderWidgetHostViewAura> view_;
  raw_ptr<MockRenderWidgetHostImpl> widget_host_ = nullptr;  // Owned by `view_`

  raw_ptr<IPC::TestSink> sink_ = nullptr;
  base::test::ScopedFeatureList mojo_feature_list_;
  base::test::ScopedFeatureList feature_list_;

  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
};

void InstallDelegatedFrameHostClient(
    RenderWidgetHostViewAura* render_widget_host_view) {
  RenderWidgetHostViewAuraTest::InstallDelegatedFrameHostClient(
      render_widget_host_view);
}

// TODO(mohsen): Consider moving these tests to OverscrollControllerTest if
// appropriate.
class RenderWidgetHostViewAuraOverscrollTest
    : public RenderWidgetHostViewAuraTest {
 public:
  RenderWidgetHostViewAuraOverscrollTest() : RenderWidgetHostViewAuraTest() {}

  RenderWidgetHostViewAuraOverscrollTest(
      const RenderWidgetHostViewAuraOverscrollTest&) = delete;
  RenderWidgetHostViewAuraOverscrollTest& operator=(
      const RenderWidgetHostViewAuraOverscrollTest&) = delete;

  // We explicitly invoke SetUp to allow gesture debounce customization.
  void SetUp() override {}

  void SendScrollUpdateAck(MockWidgetInputHandler::MessageVector& messages,
                           blink::mojom::InputEventResultState ack_result) {
    for (size_t i = 0; i < messages.size(); ++i) {
      MockWidgetInputHandler::DispatchedEventMessage* event =
          messages[i]->ToEvent();
      if (event &&
          event->Event()->Event().GetType() ==
              WebInputEvent::Type::kGestureScrollUpdate &&
          event->HasCallback()) {
        event->CallCallback(ack_result);
        return;
      }
    }
    EXPECT_TRUE(false);
  }

  void SendScrollBeginAckIfNeeded(
      MockWidgetInputHandler::MessageVector& messages,
      blink::mojom::InputEventResultState ack_result) {
    for (size_t i = 0; i < messages.size(); ++i) {
      MockWidgetInputHandler::DispatchedEventMessage* event =
          messages[i]->ToEvent();
      // GSB events are blocking, send the ack.
      if (event && event->Event()->Event().GetType() ==
                       WebInputEvent::Type::kGestureScrollBegin) {
        event->CallCallback(ack_result);
        return;
      }
    }
  }

  void SendScrollBeginAckIfNeeded(
      blink::mojom::InputEventResultState ack_result) {
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
    SendScrollBeginAckIfNeeded(events, ack_result);
  }

 protected:
  void SetUpOverscrollEnvironmentWithDebounce(int debounce_interval_in_ms) {
    SetUpOverscrollEnvironmentImpl(debounce_interval_in_ms);
  }

  void SetUpOverscrollEnvironment() { SetUpOverscrollEnvironmentImpl(0); }

  void SetUpOverscrollEnvironmentImpl(int debounce_interval_in_ms) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kTouchpadOverscrollHistoryNavigation);

    ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
        debounce_interval_in_ms);

    RenderWidgetHostViewAuraTest::SetUpEnvironment();

    view_->SetOverscrollControllerEnabled(true);
    gfx::Size display_size = display::Screen::GetScreen()
                                 ->GetDisplayNearestView(view_->GetNativeView())
                                 .size();
    overscroll_delegate_ =
        std::make_unique<TestOverscrollDelegate>(display_size);
    view_->overscroll_controller()->set_delegate(
        overscroll_delegate_->GetWeakPtr());

    InitViewForFrame(nullptr);
    view_->SetBounds(gfx::Rect(0, 0, 400, 200));
    view_->Show();

    sink_->ClearMessages();
  }

  // TODO(jdduke): Simulate ui::Events, injecting through the view.
  void SimulateMouseEvent(WebInputEvent::Type type) {
    widget_host_->ForwardMouseEvent(
        blink::SyntheticWebMouseEventBuilder::Build(type));
    base::RunLoop().RunUntilIdle();
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    widget_host_->ForwardMouseEventWithLatencyInfo(
        blink::SyntheticWebMouseEventBuilder::Build(type), ui_latency);
    base::RunLoop().RunUntilIdle();
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
    widget_host_->ForwardWheelEvent(wheel_event);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    SimulateMouseEvent(WebInputEvent::Type::kMouseMove, x, y, modifiers, false);
  }

  void SimulateMouseEvent(WebInputEvent::Type type,
                          int x,
                          int y,
                          int modifiers,
                          bool pressed) {
    WebMouseEvent event =
        blink::SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed)
      event.button = WebMouseEvent::Button::kLeft;
    widget_host_->ForwardMouseEvent(event);
    base::RunLoop().RunUntilIdle();
  }

  // Inject provided synthetic WebGestureEvent instance.
  void SimulateGestureEventCore(const WebGestureEvent& gesture_event) {
    widget_host_->ForwardGestureEvent(gesture_event);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateGestureEventCoreWithLatencyInfo(
      const WebGestureEvent& gesture_event,
      const ui::LatencyInfo& ui_latency) {
    widget_host_->GetRenderInputRouter()->ForwardGestureEventWithLatencyInfo(
        gesture_event, ui_latency);
    base::RunLoop().RunUntilIdle();
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            blink::WebGestureDevice sourceDevice) {
    SimulateGestureEventCore(
        blink::SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(WebInputEvent::Type type,
                                           blink::WebGestureDevice sourceDevice,
                                           const ui::LatencyInfo& ui_latency) {
    SimulateGestureEventCoreWithLatencyInfo(
        blink::SyntheticWebGestureEventBuilder::Build(type, sourceDevice),
        ui_latency);
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEventCore(
        blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
            dX, dY, modifiers, blink::WebGestureDevice::kTouchscreen));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    SimulateGestureEventCore(
        blink::SyntheticWebGestureEventBuilder::BuildPinchUpdate(
            scale, anchorX, anchorY, modifiers,
            blink::WebGestureDevice::kTouchscreen));
  }

  // Inject synthetic GestureFlingStart events.
  void SimulateGestureFlingStartEvent(float velocityX,
                                      float velocityY,
                                      blink::WebGestureDevice sourceDevice) {
    SimulateGestureEventCore(blink::SyntheticWebGestureEventBuilder::BuildFling(
        velocityX, velocityY, sourceDevice));
  }

  bool ScrollStateIsContentConsuming() const {
    return scroll_state() ==
           OverscrollController::ScrollState::CONTENT_CONSUMING;
  }

  bool ScrollStateIsOverscrolling() const {
    return scroll_state() == OverscrollController::ScrollState::OVERSCROLLING;
  }

  bool ScrollStateIsUnknown() const {
    return scroll_state() == OverscrollController::ScrollState::NONE;
  }

  OverscrollController::ScrollState scroll_state() const {
    return view_->overscroll_controller()->scroll_state_;
  }

  OverscrollMode overscroll_mode() const {
    return view_->overscroll_controller()->overscroll_mode_;
  }

  OverscrollSource overscroll_source() const {
    return view_->overscroll_controller()->overscroll_source_;
  }

  float overscroll_delta_x() const {
    return view_->overscroll_controller()->overscroll_delta_x_;
  }

  float overscroll_delta_y() const {
    return view_->overscroll_controller()->overscroll_delta_y_;
  }

  TestOverscrollDelegate* overscroll_delegate() {
    return overscroll_delegate_.get();
  }

  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    widget_host_->GetMockRenderInputRouter()->ForwardTouchEventWithLatencyInfo(
        touch_event_, ui::LatencyInfo());
    touch_event_.ResetPoints();
    base::RunLoop().RunUntilIdle();
    return touch_event_id;
  }

  void PressTouchPoint(int x, int y) {
    touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  void PressAndSetTouchActionAuto() {
    PressTouchPoint(0, 1);
    SendTouchEvent();
    widget_host_->SetTouchActionFromMain(cc::TouchAction::kAuto);
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
    EXPECT_EQ("TouchStart", GetMessageNames(events));
  }

  void ReleaseAndResetDispatchedMessages() {
    ReleaseTouchPoint(0);
    SendTouchEvent();
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
  }

  MockWidgetInputHandler::MessageVector ExpectGestureScrollEndForWheelScrolling(
      bool is_last) {
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
    if (is_last) {
      // Scroll latching will have one GestureScrollEnd at the end.
      EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
      return events;
    }
    // No GestureScrollEnd during the scroll.
    EXPECT_EQ(0U, events.size());
    return events;
  }

  MockWidgetInputHandler::MessageVector
  ExpectGestureScrollEventsAfterMouseWheelACK(
      bool is_first_ack,
      size_t enqueued_wheel_event_count) {
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
    std::string expected_events;
      // If the ack for the first sent event is not consumed,
      // MouseWheelEventQueue(MWEQ) sends the rest of the wheel events in the
      // current scrolling sequence as non-blocking events. Since MWEQ
      // receives the ack for non-blocking events asynchronously, it sends the
      // next queued wheel event immediately and this continues till the queue
      // is empty.
      // Expecting a GSB+GSU for ACKing the first MouseWheel, plus an additional
      // MouseWheel+GSU per enqueued wheel event. Note that GestureEventQueue
      // allows multiple in-flight events.
      if (is_first_ack)
        expected_events += "GestureScrollBegin GestureScrollUpdate ";
      for (size_t i = 0; i < enqueued_wheel_event_count; ++i)
        expected_events += "MouseWheel GestureScrollUpdate ";

    EXPECT_EQ(base::TrimWhitespaceASCII(expected_events, base::TRIM_TRAILING),
              GetMessageNames(events));
    return events;
  }

  MockWidgetInputHandler::MessageVector
  ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(
      bool wheel_was_queued) {
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
    size_t gesture_scroll_update_index;
    if (wheel_was_queued) {
      // The queued wheel event is already sent.
      gesture_scroll_update_index = 0;
    } else {
      // The first sent must be the wheel event and the second one must be
      // GestureScrollUpdate since the ack for the wheel event is non-blocking.
      EXPECT_TRUE(events[0]->ToEvent());
      EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
                events[0]->ToEvent()->Event()->Event().GetType());
      gesture_scroll_update_index = 1;
    }
    EXPECT_EQ(gesture_scroll_update_index + 1, events.size());
    EXPECT_TRUE(events[gesture_scroll_update_index]->ToEvent());
    EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
              events[gesture_scroll_update_index]
                  ->ToEvent()
                  ->Event()
                  ->Event()
                  .GetType());
    return events;
  }

  blink::SyntheticWebTouchEvent touch_event_;

  std::unique_ptr<TestOverscrollDelegate> overscroll_delegate_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class RenderWidgetHostViewAuraShutdownTest
    : public RenderWidgetHostViewAuraTest {
 public:
  RenderWidgetHostViewAuraShutdownTest() {}

  RenderWidgetHostViewAuraShutdownTest(
      const RenderWidgetHostViewAuraShutdownTest&) = delete;
  RenderWidgetHostViewAuraShutdownTest& operator=(
      const RenderWidgetHostViewAuraShutdownTest&) = delete;

  void TearDown() override {
    // No TearDownEnvironment here, we do this explicitly during the test.
  }
};

// Checks that a popup is positioned correctly relative to its parent using
// screen coordinates.
TEST_F(RenderWidgetHostViewAuraTest, PositionChildPopup) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 800, 600));
  gfx::Rect bounds_in_screen = parent_view_->GetViewBounds();
  int horiz = bounds_in_screen.width() / 4;
  int vert = bounds_in_screen.height() / 4;
  bounds_in_screen.Inset(gfx::Insets::VH(vert, horiz));

  // Verify that when the popup is initialized for the first time, it correctly
  // treats the input bounds as screen coordinates.
  InitViewForPopup(parent_view_, bounds_in_screen);

  gfx::Rect final_bounds_in_screen = view_->GetViewBounds();
  EXPECT_EQ(final_bounds_in_screen.ToString(), bounds_in_screen.ToString());

  // Verify that directly setting the bounds via SetBounds() treats the input
  // as screen coordinates.
  bounds_in_screen = gfx::Rect(60, 60, 100, 100);
  view_->SetBounds(bounds_in_screen);
  final_bounds_in_screen = view_->GetViewBounds();
  EXPECT_EQ(final_bounds_in_screen.ToString(), bounds_in_screen.ToString());

  // Verify that setting the size does not alter the origin.
  aura::Window* window = parent_view_->GetNativeView();
  gfx::Point original_origin = window->bounds().origin();
  view_->SetSize(gfx::Size(120, 120));
  gfx::Point new_origin = window->bounds().origin();
  EXPECT_EQ(original_origin.ToString(), new_origin.ToString());
}

// Checks that moving parent sends new screen bounds.
TEST_F(RenderWidgetHostViewAuraTest, ParentMovementUpdatesScreenRect) {
  InitViewForFrame(nullptr);

  aura::Window* root = parent_view_->GetNativeView()->GetRootWindow();

  aura::test::TestWindowDelegate delegate1, delegate2;
  std::unique_ptr<aura::Window> parent1(new aura::Window(&delegate1));
  parent1->Init(ui::LAYER_TEXTURED);
  parent1->Show();
  std::unique_ptr<aura::Window> parent2(new aura::Window(&delegate2));
  parent2->Init(ui::LAYER_TEXTURED);
  parent2->Show();

  root->AddChild(parent1.get());
  parent1->AddChild(parent2.get());
  parent2->AddChild(view_->GetNativeView());

  root->SetBounds(gfx::Rect(0, 0, 800, 600));
  // NOTE: Window::SetBounds() takes parent coordinates but
  // RenderWidgetHostView::SetBounds() takes screen coordinates.  So |view_| is
  // positioned at |parent2|'s origin.
  parent1->SetBounds(gfx::Rect(1, 1, 300, 300));
  parent2->SetBounds(gfx::Rect(2, 2, 200, 200));
  view_->SetBounds(gfx::Rect(3, 3, 100, 100));
  // view_ will be destroyed when parent is destroyed.
  view_ = nullptr;

  // Flush the state after initial setup is done.
  widget_host_->ClearScreenRects();

  // Move parents.
  parent2->SetBounds(gfx::Rect(20, 20, 200, 200));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, widget_host_->screen_rects().size());
  EXPECT_EQ(gfx::Rect(21, 21, 100, 100),
            widget_host_->screen_rects().at(0).first);
  EXPECT_EQ(gfx::Rect(1, 1, 300, 300),
            widget_host_->screen_rects().at(0).second);
  widget_host_->ClearScreenRects();

  parent1->SetBounds(gfx::Rect(10, 10, 300, 300));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1U, widget_host_->screen_rects().size());
  EXPECT_EQ(gfx::Rect(30, 30, 100, 100),
            widget_host_->screen_rects().at(0).first);
  EXPECT_EQ(gfx::Rect(10, 10, 300, 300),
            widget_host_->screen_rects().at(0).second);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Checks that a popup view is destroyed when a user clicks outside of the popup
// view and focus does not change. This is the case when the user clicks on the
// desktop background on Chrome OS.
TEST_F(RenderWidgetHostViewAuraTest, DestroyPopupClickOutsidePopup) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  InitViewForPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != nullptr);

  gfx::Point click_point(0, 0);
  EXPECT_FALSE(window->GetBoundsInRootWindow().Contains(click_point));
  aura::Window* parent_window = parent_view_->GetNativeView();
  EXPECT_FALSE(parent_window->GetBoundsInRootWindow().Contains(click_point));

  TestWindowObserver observer(window);
  ui::test::EventGenerator generator(window->GetRootWindow(), click_point);
  widget_host_ = nullptr;  // Owned by `view_`.
  view_ = nullptr;         // Self destroying during `ClickLeftButton`.
  generator.ClickLeftButton();
  ASSERT_TRUE(parent_view_->HasFocus());
  ASSERT_TRUE(observer.destroyed());
}

// Checks that a popup view is destroyed when a user taps outside of the popup
// view and focus does not change. This is the case when the user taps the
// desktop background on Chrome OS.
TEST_F(RenderWidgetHostViewAuraTest, DestroyPopupTapOutsidePopup) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  InitViewForPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  aura::Window* window = view_->GetNativeView();
  ASSERT_TRUE(window != nullptr);

  gfx::Point tap_point(0, 0);
  EXPECT_FALSE(window->GetBoundsInRootWindow().Contains(tap_point));
  aura::Window* parent_window = parent_view_->GetNativeView();
  EXPECT_FALSE(parent_window->GetBoundsInRootWindow().Contains(tap_point));

  TestWindowObserver observer(window);
  ui::test::EventGenerator generator(window->GetRootWindow(), tap_point);
  widget_host_ = nullptr;  // Owned by `view_`.
  view_ = nullptr;         // Self destroying during `GestureTapAt`.
  generator.GestureTapAt(tap_point);
  ASSERT_TRUE(parent_view_->HasFocus());
  ASSERT_TRUE(observer.destroyed());
}
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// On Desktop Linux, select boxes need mouse capture in order to work. Test that
// when a select box is opened via a mouse press that it retains mouse capture
// after the mouse is released.
TEST_F(RenderWidgetHostViewAuraTest, PopupRetainsCaptureAfterMouseRelease) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  ui::test::EventGenerator generator(
      parent_view_->GetNativeView()->GetRootWindow(), gfx::Point(300, 300));
  generator.PressLeftButton();

  InitViewForPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  ASSERT_TRUE(view_->NeedsMouseCapture());
  aura::Window* window = view_->GetNativeView();
  EXPECT_TRUE(window->HasCapture());

  generator.ReleaseLeftButton();
  EXPECT_TRUE(window->HasCapture());
}
#endif

// Test that select boxes close when their parent window loses focus (e.g. due
// to an alert or system modal dialog).
TEST_F(RenderWidgetHostViewAuraTest, PopupClosesWhenParentLosesFocus) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  EXPECT_TRUE(parent_view_->HasFocus());

  InitViewForPopup(parent_view_, gfx::Rect(10, 10, 100, 100));

  aura::Window* popup_window = view_->GetNativeView();
  TestWindowObserver observer(popup_window);

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> dialog_window(new aura::Window(&delegate));
  dialog_window->Init(ui::LAYER_TEXTURED);
  aura::client::ParentWindowWithContext(dialog_window.get(), popup_window,
                                        gfx::Rect(),
                                        display::kInvalidDisplayId);
  dialog_window->Show();
  wm::ActivateWindow(dialog_window.get());
  widget_host_ = nullptr;  // Owned by `view_`.
  view_ = nullptr;         // Self destroying during `Focus` below:
  dialog_window->Focus();

  ASSERT_TRUE(wm::IsActiveWindow(dialog_window.get()));
  EXPECT_TRUE(observer.destroyed());
}

// Checks that IME-composition-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraTest, SetCompositionText) {
  InitViewForFrame(nullptr);
  view_->Show();
  ActivateViewForTextInputManager(view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::CompositionText composition_text;
  composition_text.text = u"|a|b";

  // Focused segment
  composition_text.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 3,
                      ui::ImeTextSpan::Thickness::kThick,
                      ui::ImeTextSpan::UnderlineStyle::kSolid, 0x78563412));

  // Non-focused segment, with different background color.
  composition_text.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 3, 4,
                      ui::ImeTextSpan::Thickness::kThin,
                      ui::ImeTextSpan::UnderlineStyle::kSolid, 0xefcdab90));

  const ui::ImeTextSpans& ime_text_spans = composition_text.ime_text_spans;

  // Caret is at the end. (This emulates Japanese MSIME 2007 and later)
  composition_text.selection = gfx::Range(4);

  view_->SetCompositionText(composition_text);
  EXPECT_TRUE(view_->has_composition_text_);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("SetComposition", GetMessageNames(events));

  MockWidgetInputHandler::DispatchedIMEMessage* ime_message =
      events[0]->ToIME();
  EXPECT_TRUE(ime_message);
  EXPECT_TRUE(ime_message->Matches(composition_text.text, ime_text_spans,
                                   gfx::Range::InvalidRange(), 4, 4));

  view_->ImeCancelComposition();
  EXPECT_FALSE(view_->has_composition_text_);
}

// Checks that we reset has_composition_text_ to false upon when the focused
// node is changed.
TEST_F(RenderWidgetHostViewAuraTest, FocusedNodeChanged) {
  InitViewForFrame(nullptr);
  view_->Show();
  ActivateViewForTextInputManager(view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::CompositionText composition_text;
  composition_text.text = u"hello";
  view_->SetCompositionText(composition_text);
  EXPECT_TRUE(view_->has_composition_text_);

  view_->FocusedNodeChanged(true, gfx::Rect());
  EXPECT_FALSE(view_->has_composition_text_);
}

// Checks that sequence of IME-composition-event and mouse-event when mouse
// clicking to cancel the composition.
TEST_F(RenderWidgetHostViewAuraTest, FinishCompositionByMouse) {
  InitViewForFrame(nullptr);
  view_->Show();
  ActivateViewForTextInputManager(view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::CompositionText composition_text;
  composition_text.text = u"|a|b";

  // Focused segment
  composition_text.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 3,
                      ui::ImeTextSpan::Thickness::kThick,
                      ui::ImeTextSpan::UnderlineStyle::kSolid, 0x78563412));

  // Non-focused segment, with different background color.
  composition_text.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 3, 4,
                      ui::ImeTextSpan::Thickness::kThin,
                      ui::ImeTextSpan::UnderlineStyle::kSolid, 0xefcdab90));

  // Caret is at the end. (This emulates Japanese MSIME 2007 and later)
  composition_text.selection = gfx::Range(4);

  view_->SetCompositionText(composition_text);
  EXPECT_TRUE(view_->has_composition_text_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("SetComposition", GetMessageNames(GetAndResetDispatchedMessages()));

  // Simulates the mouse press.
  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  view_->OnMouseEvent(&mouse_event);
  base::RunLoop().RunUntilIdle();

  auto events = GetAndResetDispatchedMessages();
  EXPECT_FALSE(view_->has_composition_text_);
  EXPECT_EQ("FinishComposingText MouseDown", GetMessageNames(events));

  MockWidgetInputHandler::DispatchedFinishComposingMessage* ime_message =
      events[0]->ToFinishComposing();
  EXPECT_TRUE(ime_message);
  EXPECT_TRUE(ime_message->keep_selection());
}

// Checks that WasOcculded/WasUnOccluded notifies RenderWidgetHostImpl.
TEST_F(RenderWidgetHostViewAuraTest, WasOccluded) {
  InitViewForFrame(nullptr);
  view_->Show();
  EXPECT_FALSE(widget_host_->is_hidden());

  // Verifies WasOccluded sets RenderWidgetHostImpl as hidden and WasUnOccluded
  // resets the state.
  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());
  view_->WasUnOccluded();
  EXPECT_FALSE(widget_host_->is_hidden());

  // Verifies WasOccluded sets RenderWidgetHostImpl as hidden and Show resets
  // the state.
  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());
  view_->Show();
  EXPECT_FALSE(widget_host_->is_hidden());

  // WasOccluded and WasUnOccluded are not in pairs. The last one dictates
  // the final state.
  for (int i = 0; i < 2; ++i) {
    view_->WasOccluded();
    EXPECT_TRUE(widget_host_->is_hidden());
  }
  view_->WasUnOccluded();
  EXPECT_FALSE(widget_host_->is_hidden());

  for (int i = 0; i < 4; ++i) {
    view_->WasUnOccluded();
    EXPECT_FALSE(widget_host_->is_hidden());
  }
  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());
}

// Checks that touch-event state is maintained correctly.
TEST_F(RenderWidgetHostViewAuraTest, TouchEventState) {
  InitViewForFrame(nullptr);
  view_->Show();

  // Start with no touch-event handler in the renderer.
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(false), HasHitTestableScrollbar(false));
  widget_host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  ui::TouchEvent press(ui::EventType::kTouchPressed, gfx::Point(30, 30),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::TouchEvent move(ui::EventType::kTouchMoved, gfx::Point(20, 20),
                      ui::EventTimeForNow(),
                      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::TouchEvent release(ui::EventType::kTouchReleased, gfx::Point(20, 20),
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  // The touch events should get forwarded from the view but only the discrete
  // events should make it all the way to the renderer.
  view_->OnTouchEvent(&press);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, events.size());
  EXPECT_EQ("TouchStart", GetMessageNames(events));
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::DOWN, pointer_state().GetAction());

  view_->OnTouchEvent(&move);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&release);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, events.size());
  EXPECT_EQ("TouchEnd", GetMessageNames(events));
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());

  // Now install some touch-event handlers and do the same steps. The touch
  // events should now be consumed. However, the touch-event state should be
  // updated as before.
  touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  widget_host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  view_->OnTouchEvent(&press);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, events.size());
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  widget_host_->SetTouchActionFromMain(cc::TouchAction::kAuto);

  view_->OnTouchEvent(&move);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(move.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());

  // Now start a touch event, and remove the event-handlers before the release.
  view_->OnTouchEvent(&press);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(3U, events.size());

  touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(false), HasHitTestableScrollbar(false));
  widget_host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  // All outstanding events should have already been sent but no new events
  // should get sent.
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());

  ui::TouchEvent move2(ui::EventType::kTouchMoved, gfx::Point(20, 20),
                       base::TimeTicks::Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  view_->OnTouchEvent(&move2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  ui::TouchEvent release2(ui::EventType::kTouchReleased, gfx::Point(20, 20),
                          base::TimeTicks::Now(),
                          ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  view_->OnTouchEvent(&release2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, events.size());
  EXPECT_EQ("TouchEnd", GetMessageNames(events));
}

TEST_F(RenderWidgetHostViewAuraTest,
       KeyEventRoutingWithKeyboardLockActiveForOneKey) {
  InitViewForFrame(nullptr);
  view_->Show();

  auto test_hook = std::make_unique<TestScopedKeyboardHook>();
  test_hook->LockSpecificKey(ui::DomCode::US_A);
  view_->event_handler()->scoped_keyboard_hook_ = std::move(test_hook);

  // This locked key will skip the prehandler and be sent to the input handler.
  ui::KeyEvent key_event1(ui::EventType::kKeyPressed,
                          ui::DomCodeToUsLayoutKeyboardCode(ui::DomCode::US_A),
                          ui::DomCode::US_A, ui::EF_NONE);
  view_->OnKeyEvent(&key_event1);
  const input::NativeWebKeyboardEvent* event1 =
      render_widget_host_delegate()->last_event();
  ASSERT_FALSE(event1);
  // Run the runloop to ensure input messages are dispatched.  Otherwise the
  // result of GetAndResetDispatchedMessages() will not be valid.
  base::RunLoop().RunUntilIdle();
  auto events = GetAndResetDispatchedMessages();
  ASSERT_FALSE(events.empty());
  const blink::WebKeyboardEvent* blink_key_event1 =
      static_cast<const blink::WebKeyboardEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  ASSERT_TRUE(blink_key_event1);
  ASSERT_EQ(key_event1.key_code(), blink_key_event1->windows_key_code);
  ASSERT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event1.code()),
            blink_key_event1->native_key_code);

  // These keys will pass through the prehandler since they aren't locked.
  std::vector<ui::DomCode> dom_codes = {
      ui::DomCode::US_B,     ui::DomCode::US_Z,  ui::DomCode::TAB,
      ui::DomCode::ALT_LEFT, ui::DomCode::ENTER, ui::DomCode::ESCAPE};
  for (ui::DomCode dom_code : dom_codes) {
    ui::KeyEvent key_event(ui::EventType::kKeyPressed,
                           ui::DomCodeToUsLayoutKeyboardCode(dom_code),
                           dom_code, ui::EF_NONE);
    view_->OnKeyEvent(&key_event);
    const input::NativeWebKeyboardEvent* event =
        render_widget_host_delegate()->last_event();
    ASSERT_TRUE(event) << "Failed for DomCode: "
                       << ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    ASSERT_EQ(key_event.key_code(), event->windows_key_code);
    ASSERT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event.code()),
              event->native_key_code);
  }
}

TEST_F(RenderWidgetHostViewAuraTest,
       KeyEventRoutingWithKeyboardLockActiveForEscKey) {
  InitViewForFrame(nullptr);
  view_->Show();

  auto test_hook = std::make_unique<TestScopedKeyboardHook>();
  test_hook->LockSpecificKey(ui::DomCode::ESCAPE);
  view_->event_handler()->scoped_keyboard_hook_ = std::move(test_hook);

  // Although this key was locked, it will still pass through the prehandler as
  // we do not want to prevent ESC from being used to exit fullscreen.
  ui::KeyEvent key_event1(
      ui::EventType::kKeyPressed,
      ui::DomCodeToUsLayoutKeyboardCode(ui::DomCode::ESCAPE),
      ui::DomCode::ESCAPE, ui::EF_NONE);
  view_->OnKeyEvent(&key_event1);
  const input::NativeWebKeyboardEvent* event1 =
      render_widget_host_delegate()->last_event();
  ASSERT_TRUE(event1);
  ASSERT_EQ(key_event1.key_code(), event1->windows_key_code);
  ASSERT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event1.code()),
            event1->native_key_code);

  // This event will pass through the prehandler since it isn't locked.
  ui::KeyEvent key_event2(ui::EventType::kKeyPressed,
                          ui::DomCodeToUsLayoutKeyboardCode(ui::DomCode::US_B),
                          ui::DomCode::US_B, ui::EF_NONE);
  view_->OnKeyEvent(&key_event2);
  const input::NativeWebKeyboardEvent* event2 =
      render_widget_host_delegate()->last_event();
  ASSERT_TRUE(event2);
  ASSERT_EQ(key_event2.key_code(), event2->windows_key_code);
  ASSERT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event2.code()),
            event2->native_key_code);
}

TEST_F(RenderWidgetHostViewAuraTest,
       KeyEventRoutingWithKeyboardLockActiveForAllKeys) {
  InitViewForFrame(nullptr);
  view_->Show();

  auto test_hook = std::make_unique<TestScopedKeyboardHook>();
  test_hook->LockAllKeys();
  view_->event_handler()->scoped_keyboard_hook_ = std::move(test_hook);

  // These keys will skip the prehandler and be sent to the input handler.
  std::vector<ui::DomCode> dom_codes = {ui::DomCode::US_A, ui::DomCode::US_B,
                                        ui::DomCode::TAB, ui::DomCode::ALT_LEFT,
                                        ui::DomCode::ENTER};
  for (ui::DomCode dom_code : dom_codes) {
    ui::KeyEvent key_event(ui::EventType::kKeyPressed,
                           ui::DomCodeToUsLayoutKeyboardCode(dom_code),
                           dom_code, ui::EF_NONE);
    view_->OnKeyEvent(&key_event);
    const input::NativeWebKeyboardEvent* event =
        render_widget_host_delegate()->last_event();
    ASSERT_FALSE(event) << "Failed for DomCode: "
                        << ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    // Run the runloop to ensure input messages are dispatched.  Otherwise the
    // result of GetAndResetDispatchedMessages() will not be valid.
    base::RunLoop().RunUntilIdle();
    auto events = GetAndResetDispatchedMessages();
    ASSERT_FALSE(events.empty())
        << "Failed for DomCode: "
        << ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    const blink::WebKeyboardEvent* blink_key_event =
        static_cast<const blink::WebKeyboardEvent*>(
            &events[0]->ToEvent()->Event()->Event());
    ASSERT_TRUE(blink_key_event)
        << "Failed for DomCode: "
        << ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    ASSERT_EQ(key_event.key_code(), blink_key_event->windows_key_code);
    ASSERT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event.code()),
              blink_key_event->native_key_code);
  }

  // Although this key was locked, it will still pass through the prehandler as
  // we do not want to prevent ESC from being used to exit fullscreen.
  ui::KeyEvent esc_key_event(
      ui::EventType::kKeyPressed,
      ui::DomCodeToUsLayoutKeyboardCode(ui::DomCode::ESCAPE),
      ui::DomCode::ESCAPE, ui::EF_NONE);
  view_->OnKeyEvent(&esc_key_event);
  const input::NativeWebKeyboardEvent* esc_event =
      render_widget_host_delegate()->last_event();
  ASSERT_TRUE(esc_event);
  ASSERT_EQ(esc_key_event.key_code(), esc_event->windows_key_code);
  ASSERT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(esc_key_event.code()),
            esc_event->native_key_code);
}

TEST_F(RenderWidgetHostViewAuraTest,
       KeyEventRoutingKeyboardLockAndChildPopupWithInputGrab) {
  parent_view_->SetBounds(gfx::Rect(10, 10, 400, 400));
  parent_view_->Focus();
  ASSERT_TRUE(parent_view_->HasFocus());

  InitViewForPopup(parent_view_, gfx::Rect(10, 10, 100, 100));
  ASSERT_NE(nullptr, view_->GetNativeView());
  view_->Show();

  MockRenderWidgetHostImpl* parent_host =
      static_cast<MockRenderWidgetHostImpl*>(parent_host_);
  // Run the runloop to ensure input messages are dispatched.  Otherwise the
  // result of GetAndResetDispatchedMessages() will not be valid.
  base::RunLoop().RunUntilIdle();
  // A MouseCapture lost message is posted when the child gains focus, clear
  // that message out so we can reliably test the number of messages
  // dispatched later on in the test.
  parent_host->input_handler()->GetAndResetDispatchedMessages();

  // The parent view owns the KeyboardLock for this test.
  auto test_hook = std::make_unique<TestScopedKeyboardHook>();
  test_hook->LockAllKeys();
  parent_view_->event_handler()->scoped_keyboard_hook_ = std::move(test_hook);

  // These keys will not be processed by the parent view but will be handled in
  // the child (popup) view.
  std::vector<ui::DomCode> dom_codes = {
      ui::DomCode::US_A,     ui::DomCode::ENTER, ui::DomCode::TAB,
      ui::DomCode::ALT_LEFT, ui::DomCode::US_Z,  ui::DomCode::ESCAPE};
  for (ui::DomCode dom_code : dom_codes) {
    ui::KeyEvent key_event(ui::EventType::kKeyPressed,
                           ui::DomCodeToUsLayoutKeyboardCode(dom_code),
                           dom_code, ui::EF_NONE);
    parent_view_->OnKeyEvent(&key_event);
    const input::NativeWebKeyboardEvent* parent_event =
        delegates_[0]->last_event();
    ASSERT_FALSE(parent_event)
        << "Failed for DomCode: "
        << ui::KeycodeConverter::DomCodeToCodeString(dom_code);

    const input::NativeWebKeyboardEvent* child_event =
        render_widget_host_delegate()->last_event();
    ASSERT_TRUE(child_event)
        << "Failed for DomCode: "
        << ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    ASSERT_EQ(key_event.key_code(), child_event->windows_key_code);
    ASSERT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event.code()),
              child_event->native_key_code);
    // Run the runloop to ensure input messages are dispatched.  Otherwise the
    // result of GetAndResetDispatchedMessages() will not be valid.
    base::RunLoop().RunUntilIdle();
    auto parent_events =
        parent_host->input_handler()->GetAndResetDispatchedMessages();
    ASSERT_TRUE(parent_events.empty())
        << "Failed for DomCode: "
        << ui::KeycodeConverter::DomCodeToCodeString(dom_code);
    auto child_events = GetAndResetDispatchedMessages();
    ASSERT_FALSE(child_events.empty())
        << "Failed for DomCode: "
        << ui::KeycodeConverter::DomCodeToCodeString(dom_code);
  }
}

TEST_F(RenderWidgetHostViewAuraTest, TimerBasedWheelEventPhaseInfo) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kWindowsScrollingPersonality);
  RunTimerBasedWheelEventPhaseInfoTest(false);
}

TEST_F(RenderWidgetHostViewAuraTest,
       TimerBasedWheelEventPhaseInfoWithPercentBasedScrolling) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWindowsScrollingPersonality);
  RunTimerBasedWheelEventPhaseInfoTest(true);
}

void RenderWidgetHostViewAuraTest::RunTimerBasedWheelEventPhaseInfoTest(
    bool percent_based_scrolling_enabled) {
  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  ui::MouseWheelEvent event(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();

  EXPECT_TRUE(events[0]->ToEvent());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  events = GetAndResetDispatchedMessages();
  const WebGestureEvent* gesture_event = static_cast<const WebGestureEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin, gesture_event->GetType());
  EXPECT_TRUE(gesture_event->data.scroll_begin.synthetic);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(percent_based_scrolling_enabled
                ? 5 * ui::kScrollPercentPerLineOrChar /
                      ui::MouseWheelEvent::kWheelDelta
                : 5U,
            gesture_event->data.scroll_update.delta_y);
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Send a ui::ScrollEvent instead of ui::MouseWheel event, the timer based
  // phase info doesn't differentiate between the two types of events.
  ui::ScrollEvent scroll1(ui::EventType::kScroll, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 2, 0, 2, 2);
  view_->OnScrollEvent(&scroll1);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  base::TimeTicks wheel_event_timestamp = wheel_event->TimeStamp();
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, wheel_event->phase);

  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(2U, gesture_event->data.scroll_update.delta_y);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Let the MouseWheelPhaseHandler::mouse_wheel_end_dispatch_timer_ fire. A
  // synthetic wheel event with zero deltas and kPhaseEnded will be sent.
  task_environment_.FastForwardBy(base::Milliseconds(100));

  events = GetAndResetDispatchedMessages();
  const WebMouseWheelEvent* wheel_end_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_end_event->phase);
  EXPECT_EQ(0U, wheel_end_event->delta_x);
  EXPECT_EQ(0U, wheel_end_event->delta_y);
  EXPECT_EQ(0U, wheel_end_event->wheel_ticks_x);
  EXPECT_EQ(0U, wheel_end_event->wheel_ticks_y);
  EXPECT_GT(wheel_end_event->TimeStamp(), wheel_event_timestamp);

  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd, gesture_event->GetType());
  EXPECT_TRUE(gesture_event->data.scroll_end.synthetic);
}

// Tests that latching breaks when the difference between location of the first
// wheel event in the sequence and the location of the current wheel event is
// larger than some maximum threshold.
TEST_F(RenderWidgetHostViewAuraTest, TimerBasedLatchingBreaksWithMouseMove) {
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when the wheel event with different
  // location is sent.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  ui::MouseWheelEvent event(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();

  EXPECT_TRUE(events[0]->ToEvent());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  events = GetAndResetDispatchedMessages();

  // Send the second wheel event with a location within the slop region. The
  // second wheel event will still be part of the current scrolling sequence
  // since the location difference is less than the allowed threshold.
  ui::MouseWheelEvent event2(gfx::Vector2d(0, 5),
                             gfx::Point(2 + kWheelLatchingSlopRegion / 2, 2),
                             gfx::Point(2 + kWheelLatchingSlopRegion / 2, 2),
                             ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event2);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollUpdate", GetMessageNames(events));

  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, wheel_event->phase);
  events = GetAndResetDispatchedMessages();

  // Send the third wheel event with a location outside of the slop region. The
  // third wheel event will break the latching since the location difference is
  // larger than the allowed threshold.
  ui::MouseWheelEvent event3(
      gfx::Vector2d(0, 5), gfx::Point(2 + kWheelLatchingSlopRegion, 2),
      gfx::Point(2 + kWheelLatchingSlopRegion, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event3);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);

  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[2]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
}

// Tests that latching breaks when the current wheel event has different
// modifiers.
TEST_F(RenderWidgetHostViewAuraTest,
       TimerBasedLatchingBreaksWithModifiersChange) {
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when the wheel event with different
  // modifiers is sent.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  ui::MouseWheelEvent event(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();

  EXPECT_TRUE(events[0]->ToEvent());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  events = GetAndResetDispatchedMessages();

  // Send the second wheel event with the same modifiers. The second wheel event
  // will still be part of the current scrolling sequence.
  ui::MouseWheelEvent event2(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                             gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event2);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollUpdate", GetMessageNames(events));

  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, wheel_event->phase);
  events = GetAndResetDispatchedMessages();

  // Send the third wheel event with a ctrl key down. The third wheel event will
  // break the latching since the event modifiers have changed.
  ui::MouseWheelEvent event3(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                             gfx::Point(2, 2), ui::EventTimeForNow(),
                             ui::EF_CONTROL_DOWN, 0);
  view_->OnMouseEvent(&event3);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);

  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[2]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
}

// Tests that latching breaks when the new wheel event goes a different
// direction from previous wheel events and the previous GSU events are not
// consumed.
TEST_F(RenderWidgetHostViewAuraTest,
       TimerBasedLatchingBreaksWithDirectionChange) {
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when the wheel event with different
  // modifiers is sent.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  ui::MouseWheelEvent event(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();

  EXPECT_TRUE(events[0]->ToEvent());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // ACK the GSU as NOT_CONSUMED.
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
  EXPECT_TRUE(events[0]->ToEvent());
  EXPECT_TRUE(events[1]->ToEvent());
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // Send the second wheel event with different directions. This wheel event
  // will break the latching since the last GSU was NOT_CONSUMED and the
  // scrolling direction has changed.
  ui::MouseWheelEvent event2(gfx::Vector2d(-5, 0), gfx::Point(2, 2),
                             gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event2);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);

  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[2]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
}

TEST_F(RenderWidgetHostViewAuraTest,
       TimerBasedLatchingBreaksWithAutoscrollStart) {
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when the Autoscroll starts.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  ui::MouseWheelEvent event(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  EXPECT_TRUE(events[0]->ToEvent());
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_TRUE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());
  events = GetAndResetDispatchedMessages();

  // Autoscroll start breaks wheel scroll latching sequence by sending the
  // pending wheel end event, the non-blocking wheel end event will be acked
  // immediately and a GSE will be sent. The next wheel event will start a new
  // scrolling sequence.
  view_->OnAutoscrollStart();
  EXPECT_FALSE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());
  ui::MouseWheelEvent event2(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                             gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&event2);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  EXPECT_TRUE(events[0]->ToEvent());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);
  EXPECT_TRUE(events[2]->ToEvent());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[2]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
}

// Tests that a gesture fling start with touchpad source resets wheel phase
// state.
TEST_F(RenderWidgetHostViewAuraTest, TouchpadFlingStartResetsWheelPhaseState) {
  // Calling InitAsChild so it will create aura::Window. This will be queried by
  // fling controller to get the root viewport size when it receives GFS.
  InitViewForFrame(nullptr);
  view_->SetSize(gfx::Size(100, 100));
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when the touchpad fling start is sent.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  // When the user puts their fingers down a GFC is receieved.
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel,
                               gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0, 0,
                               0, 0, 2);
  view_->OnScrollEvent(&fling_cancel);

  // Scrolling starts.
  ui::ScrollEvent scroll0(ui::EventType::kScroll, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 5, 0, 5, 2);
  view_->OnScrollEvent(&scroll0);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();

  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
  const WebGestureEvent* gesture_event = static_cast<const WebGestureEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin, gesture_event->GetType());
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(5U, gesture_event->data.scroll_update.delta_y);
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Wait for some time and resume scrolling. The second scroll will latch since
  // the user hasn't lifted their fingers, yet.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();
  ui::ScrollEvent scroll1(ui::EventType::kScroll, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 15, 0, 15, 2);
  view_->OnScrollEvent(&scroll1);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(2U, events.size());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseChanged, wheel_event->phase);
  EXPECT_EQ("MouseWheel GestureScrollUpdate", GetMessageNames(events));
  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            gesture_event->GetType());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(15U, gesture_event->data.scroll_update.delta_y);

  // A GFS is received showing that the user has lifted their fingers. This will
  // reset the scroll state of the wheel phase handler. The velocity should be
  // big enough to make sure that fling is still active while sending the scroll
  // event.
  ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart,
                              gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0,
                              1000, 0, 1000, 2);
  view_->OnScrollEvent(&fling_start);
  base::RunLoop().RunUntilIdle();

  events = GetAndResetDispatchedMessages();
  // A GFS with touchpad source won't get dispatched to the renderer. However,
  // since progressFling is called right away after processing the GFS, it is
  // possible that a progress event is sent if the time delta between GFS
  // timestamp and the time that it gets processed is large enough.
  bool progress_event_sent = events.size();
  if (progress_event_sent)
    EXPECT_EQ("MouseWheel GestureScrollUpdate", GetMessageNames(events));

  // Handling the next ui::EventType::kScroll event will generate a GFC which
  // resets the phase state. The fling controller processes GFC and generates a
  // wheel event with momentum_phase == kPhaseEnded. The mouse wheel created
  // from scroll2 will have phase == kPhaseBegan.
  ui::ScrollEvent scroll2(ui::EventType::kScroll, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 15, 0, 15, 2);
  view_->OnScrollEvent(&scroll2);
  base::RunLoop().RunUntilIdle();

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->momentum_phase);
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[2]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
}

// Tests that the touchpad scroll state in mouse wheel phase handler gets reset
// when a mouse wheel event from an external mouse arrives.
TEST_F(RenderWidgetHostViewAuraTest, MouseWheelScrollingAfterGFCWithoutGFS) {
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when we are checking for the pending
  // wheel end event after sending ui::MouseWheelEvent.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  // When the user puts their fingers down a GFC is received. This will change
  // the touchpad scroll state in mouse wheel phase handler to may_begin.
  EXPECT_EQ(
      content::TOUCHPAD_SCROLL_STATE_UNKNOWN,
      GetMouseWheelPhaseHandler()->touchpad_scroll_phase_state_for_test());
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel,
                               gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0, 0,
                               0, 0, 2);
  view_->OnScrollEvent(&fling_cancel);
  GetAndResetDispatchedMessages();
  EXPECT_EQ(
      content::TOUCHPAD_SCROLL_MAY_BEGIN,
      GetMouseWheelPhaseHandler()->touchpad_scroll_phase_state_for_test());
  EXPECT_FALSE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());

  // The user lifts their fingers without doing any touchpad scroll
  // (ui::ScrollEevent), the touchpad scroll state must still be may_begin since
  // without touchpad scrolling no GFS is recieved to reset the state.
  EXPECT_EQ(
      content::TOUCHPAD_SCROLL_MAY_BEGIN,
      GetMouseWheelPhaseHandler()->touchpad_scroll_phase_state_for_test());

  // The user starts scrolling by external mouse device.
  ui::MouseWheelEvent wheel(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&wheel);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);

  // After arrival of the mouse wheel event, the touchpad scroll state must get
  // reset and the timer based wheel scroll latching must be active.
  EXPECT_EQ(
      content::TOUCHPAD_SCROLL_STATE_UNKNOWN,
      GetMouseWheelPhaseHandler()->touchpad_scroll_phase_state_for_test());
  EXPECT_TRUE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());
}

TEST_F(RenderWidgetHostViewAuraTest,
       ScrollingWithExternalMouseBreaksTouchpadScrollLatching) {
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when we are checking for the pending
  // wheel end event after sending ui::MouseWheelEvent.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  // When the user puts their fingers down a GFC is receieved.
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel,
                               gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0, 0,
                               0, 0, 2);
  view_->OnScrollEvent(&fling_cancel);

  // Start touchpad scrolling by sending a ui::EventType::kScroll event.
  ui::ScrollEvent scroll0(ui::EventType::kScroll, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 5, 0, 5, 2);
  view_->OnScrollEvent(&scroll0);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();

  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // The mouse_wheel_phase_handler's timer won't be running during touchpad
  // scroll.
  EXPECT_FALSE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());

  // ACK the GSB and GSU events generated from the first touchpad wheel event.
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
  const WebGestureEvent* gesture_event = static_cast<const WebGestureEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin, gesture_event->GetType());
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            gesture_event->GetType());
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Start mouse wheel scrolling by sending a ui::EventType::kMousewheel event.
  // This should end the touchpad scrolling sequence and start a new timer-based
  // wheel scrolling sequence.
  ui::MouseWheelEvent wheel(gfx::Vector2d(0, 5), gfx::Point(2, 2),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&wheel);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd MouseWheel", GetMessageNames(events));
  EXPECT_TRUE(events[0]->ToEvent());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);
  EXPECT_TRUE(events[2]->ToEvent());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[2]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);

  // The mouse_wheel_phase_handler's timer will be running during mouse wheel
  // scroll.
  EXPECT_TRUE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());
}

TEST_F(RenderWidgetHostViewAuraTest,
       GSBWithTouchSourceStopsWheelScrollSequence) {
  // Set the mouse_wheel_phase_handler_ timer timeout to a large value to make
  // sure that the timer is still running when the GSB event with touch source
  // is sent.
  view_->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
      TestTimeouts::action_max_timeout());

  ui::ScrollEvent scroll0(ui::EventType::kScroll, gfx::Point(2, 2),
                          ui::EventTimeForNow(), 0, 0, 5, 0, 5, 2);
  view_->OnScrollEvent(&scroll0);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseBegan, wheel_event->phase);
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin GestureScrollUpdate", GetMessageNames(events));
  const WebGestureEvent* gesture_event = static_cast<const WebGestureEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_EQ(0U, gesture_event->data.scroll_update.delta_x);
  EXPECT_EQ(5U, gesture_event->data.scroll_update.delta_y);
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  ui::GestureEventDetails gesture_tap_down_details(
      ui::EventType::kGestureTapDown);
  gesture_tap_down_details.set_is_source_touch_event_set_blocking(true);
  gesture_tap_down_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_tap_down(2, 2, 0, ui::EventTimeForNow(),
                                    gesture_tap_down_details);
  view_->OnGestureEvent(&gesture_tap_down);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();

  ui::GestureEventDetails event_details(ui::EventType::kGestureScrollBegin);
  event_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent scroll_begin(2, 2, 0, ui::EventTimeForNow(), event_details);
  view_->OnGestureEvent(&scroll_begin);
  base::RunLoop().RunUntilIdle();

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd GestureScrollBegin",
            GetMessageNames(events));
  EXPECT_EQ(3U, events.size());

  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);
  EXPECT_EQ(0U, wheel_event->delta_x);
  EXPECT_EQ(0U, wheel_event->delta_y);

  gesture_event = static_cast<const WebGestureEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd, gesture_event->GetType());
  EXPECT_EQ(blink::WebGestureDevice::kTouchpad, gesture_event->SourceDevice());

  gesture_event = static_cast<const WebGestureEvent*>(
      &events[2]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin, gesture_event->GetType());
  EXPECT_EQ(blink::WebGestureDevice::kTouchscreen,
            gesture_event->SourceDevice());
}

TEST_F(RenderWidgetHostViewAuraTest,
       SyntheticFlingCancelAtTouchpadScrollBegin) {
  ui::ScrollEvent scroll_event(ui::EventType::kScroll, gfx::Point(2, 2),
                               ui::EventTimeForNow(), 0, 0, 5, 0, 5, 2);

  // Send the beginning scroll event. This should generate a synthetic fling
  // cancel to cancel any ongoing flings before the start of this scroll.
  view_->OnScrollEvent(&scroll_event);
  base::RunLoop().RunUntilIdle();
  std::optional<WebGestureEvent> last_gesture =
      widget_host_->GetMockRenderInputRouter()
          ->GetAndResetLastForwardedGestureEvent();
  ASSERT_TRUE(last_gesture);
  EXPECT_EQ(WebInputEvent::Type::kGestureFlingCancel, last_gesture->GetType());

  // Consume the wheel to prevent gesture scrolls from interfering with the
  // rest of the test.
  MockWidgetInputHandler::MessageVector dispatched_events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(dispatched_events));
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  dispatched_events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, dispatched_events.size());

  // Send a scroll update. A synthetic fling cancel has already been sent for
  // this sequence, so we should not generate another.
  view_->OnScrollEvent(&scroll_event);
  base::RunLoop().RunUntilIdle();
  last_gesture = widget_host_->GetMockRenderInputRouter()
                     ->GetAndResetLastForwardedGestureEvent();
  EXPECT_FALSE(last_gesture);

  dispatched_events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(dispatched_events));
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  dispatched_events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, dispatched_events.size());
}

// Checks that touch-event state is maintained correctly for multiple touch
// points.
TEST_F(RenderWidgetHostViewAuraTest, MultiTouchPointsStates) {
  InitViewForFrame(parent_view_->GetNativeView());
  view_->Focus();
  view_->Show();
  view_->UseFakeDispatcher();

  ui::TouchEvent press0(ui::EventType::kTouchPressed, gfx::Point(30, 30),
                        ui::EventTimeForNow(),
                        ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  view_->OnTouchEvent(&press0);
  static_cast<input::InputRouterImpl*>(
      view_->GetFocusedWidget()->input_router())
      ->SetTouchActionFromMain(cc::TouchAction::kAuto);
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("SetFocus TouchStart", GetMessageNames(events));
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(ui::MotionEvent::Action::DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  ui::TouchEvent move0(ui::EventType::kTouchMoved, gfx::Point(20, 20),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  view_->OnTouchEvent(&move0);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchMove", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  // For the second touchstart, only the state of the second touch point is
  // StatePressed, the state of the first touch point is StateStationary.
  ui::TouchEvent press1(ui::EventType::kTouchPressed, gfx::Point(10, 10),
                        ui::EventTimeForNow(),
                        ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  view_->OnTouchEvent(&press1);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchStart", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(ui::MotionEvent::Action::POINTER_DOWN, pointer_state().GetAction());
  EXPECT_EQ(1, pointer_state().GetActionIndex());
  EXPECT_EQ(2U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  // For the touchmove of second point, the state of the second touch point is
  // StateMoved, the state of the first touch point is StateStationary.
  ui::TouchEvent move1(ui::EventType::kTouchMoved, gfx::Point(30, 30),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  view_->OnTouchEvent(&move1);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchMove", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(2U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  // For the touchmove of first point, the state of the first touch point is
  // StateMoved, the state of the second touch point is StateStationary.
  ui::TouchEvent move2(ui::EventType::kTouchMoved, gfx::Point(10, 10),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  view_->OnTouchEvent(&move2);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchMove", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(2U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  ui::TouchEvent cancel0(ui::EventType::kTouchCancelled, gfx::Point(10, 10),
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  // For the touchcancel, only the state of the current touch point is
  // StateCancelled, the state of the other touch point is StateStationary.
  view_->OnTouchEvent(&cancel0);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchCancel", GetMessageNames(events));
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());

  ui::TouchEvent cancel1(ui::EventType::kTouchCancelled, gfx::Point(30, 30),
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  view_->OnTouchEvent(&cancel1);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchCancel", GetMessageNames(events));
  EXPECT_EQ(1U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());
}

// Checks that touch-events are queued properly when there is a touch-event
// handler on the page.
TEST_F(RenderWidgetHostViewAuraTest, TouchEventSyncAsync) {
  InitViewForFrame(nullptr);
  view_->Show();

  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  widget_host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  ui::TouchEvent press(ui::EventType::kTouchPressed, gfx::Point(30, 30),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::TouchEvent move(ui::EventType::kTouchMoved, gfx::Point(20, 20),
                      ui::EventTimeForNow(),
                      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::TouchEvent release(ui::EventType::kTouchReleased, gfx::Point(20, 20),
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  view_->OnTouchEvent(&press);
  EXPECT_TRUE(press.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  // Send the same move event. Since the point hasn't moved, it won't affect the
  // queue. However, the view should consume the event.
  view_->OnTouchEvent(&move);
  EXPECT_TRUE(move.synchronous_handling_disabled());
  EXPECT_EQ(ui::MotionEvent::Action::MOVE, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());

  view_->OnTouchEvent(&release);
  EXPECT_TRUE(release.synchronous_handling_disabled());
  EXPECT_EQ(0U, pointer_state().GetPointerCount());
}

TEST_F(RenderWidgetHostViewAuraTest, CompositorViewportPixelSizeWithScale) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);

  widget_host_->ClearVisualProperties();

  view_->SetSize(gfx::Size(100, 100));

  // Physical pixel size.
  EXPECT_EQ(gfx::Size(100, 100), view_->GetCompositorViewportPixelSize());
  // Update to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // DIP size.
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.new_size);
    // Physical pixel size.
    EXPECT_EQ(gfx::Size(100, 100),
              visual_properties.compositor_viewport_pixel_rect.size());
  }

  // Get back the UpdateVisualProperties ack.
  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(100, 100);
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }
  sink_->ClearMessages();
  widget_host_->ClearVisualProperties();

  // Device scale factor changes to 2, so the physical pixel sizes should
  // change, while the DIP sizes do not.

  aura_test_helper_->GetTestScreen()->SetDeviceScaleFactor(2.0f);
  // Physical pixel size.
  EXPECT_EQ(gfx::Size(200, 200), view_->GetCompositorViewportPixelSize());
  // Update to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // DIP size.
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.new_size);
    // Physical pixel size.
    EXPECT_EQ(gfx::Size(200, 200),
              visual_properties.compositor_viewport_pixel_rect.size());
  }

  // Get back the UpdateVisualProperties ack.
  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(200, 200);
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }
  widget_host_->ClearVisualProperties();

  aura_test_helper_->GetTestScreen()->SetDeviceScaleFactor(1.0f);

  // Physical pixel size.
  EXPECT_EQ(gfx::Size(100, 100), view_->GetCompositorViewportPixelSize());
  // Update to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // DIP size.
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.new_size);
    // Physical pixel size.
    EXPECT_EQ(gfx::Size(100, 100),
              visual_properties.compositor_viewport_pixel_rect.size());
  }
}

// This test verifies that in AutoResize mode a new
// blink::mojom::Widget::UpdateVisualProperties message is sent when ScreenInfo
// changes and that message contains the latest ScreenInfo.
TEST_F(RenderWidgetHostViewAuraTest, AutoResizeWithScale) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);

  viz::LocalSurfaceId host_local_surface_id = view_->GetLocalSurfaceId();
  EXPECT_TRUE(host_local_surface_id.is_valid());

  widget_host_->ClearVisualProperties();

  view_->EnableAutoResize(gfx::Size(50, 50), gfx::Size(100, 100));

  // Update to the renderer. It includes the current LocalSurfaceId.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // Auto resize parameters that we set above.
    EXPECT_EQ(gfx::Size(50, 50), visual_properties.min_size_for_auto_resize);
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.max_size_for_auto_resize);
    // Default DSF is 1.
    EXPECT_EQ(1, visual_properties.screen_infos.current().device_scale_factor);
    // Passed the original LocalSurfaceId.
    EXPECT_TRUE(visual_properties.local_surface_id.has_value());
    EXPECT_EQ(host_local_surface_id,
              visual_properties.local_surface_id.value());
  }

  // Receive a changed LocalSurfaceId from the renderer with a size.
  viz::LocalSurfaceId renderer_local_surface_id(
      host_local_surface_id.parent_sequence_number(),
      host_local_surface_id.child_sequence_number() + 1,
      host_local_surface_id.embed_token());
  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(75, 75);
    metadata.local_surface_id = renderer_local_surface_id;
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }

  // Changing the device scale factor updates the renderer.
  widget_host_->ClearVisualProperties();
  aura_test_helper_->GetTestScreen()->SetDeviceScaleFactor(2.0f);

  // Update to the renderer.
  // TODO(samans): There should be only one message in the sink, but some
  // testers are seeing two (crrev.com/c/839580). Investigate why.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // Auto resize parameters did not change as they DIP values.
    EXPECT_EQ(gfx::Size(50, 50), visual_properties.min_size_for_auto_resize);
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.max_size_for_auto_resize);
    // Updated DSF for the renderer.
    EXPECT_EQ(2, visual_properties.screen_infos.current().device_scale_factor);
    // The LocalSurfaceId has changed to the one from the renderer.
    EXPECT_TRUE(visual_properties.local_surface_id.has_value());
    EXPECT_NE(host_local_surface_id,
              visual_properties.local_surface_id.value());
    EXPECT_NE(renderer_local_surface_id,
              visual_properties.local_surface_id.value());
  }
}

// Verifies that when synchronizing the visual properties after disabling the
// auto resize mode, both the view size and the auto resize state get updated at
// the same time (https://crbug.com/1200601).
TEST_F(RenderWidgetHostViewAuraTest,
       VerifyVisualPropertiesWhenDisablingAutoResize) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);

  // Enable the auto resize.
  widget_host_->ClearVisualProperties();
  view_->EnableAutoResize(gfx::Size(50, 50), gfx::Size(100, 100));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());

  widget_host_->ClearVisualProperties();

  // Disable the auto resize and verify the received visual properties.
  const gfx::Size size_after_disabling(200, 200);
  view_->DisableAutoResize(size_after_disabling);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, widget_host_->visual_properties().size());
  const auto& received_property = widget_host_->visual_properties()[0];
  EXPECT_EQ(false, received_property.auto_resize_enabled);
  EXPECT_EQ(size_after_disabling, received_property.new_size);
}

// This test verifies that in AutoResize mode a new
// blink::mojom::Widget::UpdateVisualProperties message is sent when size
// changes.
TEST_F(RenderWidgetHostViewAuraTest, AutoResizeWithBrowserInitiatedResize) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  viz::LocalSurfaceId host_local_surface_id(view_->GetLocalSurfaceId());
  EXPECT_TRUE(host_local_surface_id.is_valid());

  widget_host_->ClearVisualProperties();
  view_->EnableAutoResize(gfx::Size(50, 50), gfx::Size(100, 100));

  // blink::mojom::Widget::UpdateVisualProperties is sent to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // Auto-resizve limits sent to the renderer.
    EXPECT_EQ(gfx::Size(50, 50), visual_properties.min_size_for_auto_resize);
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.max_size_for_auto_resize);
    // The original LocalSurfaceId is sent.
    EXPECT_TRUE(visual_properties.local_surface_id.has_value());
    EXPECT_EQ(host_local_surface_id,
              visual_properties.local_surface_id.value());
  }

  // A size arrives from the renderer with a changed LocalSurfaceId.
  viz::LocalSurfaceId renderer_local_surface_id(
      host_local_surface_id.parent_sequence_number(),
      host_local_surface_id.child_sequence_number() + 1,
      host_local_surface_id.embed_token());
  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(75, 75);
    metadata.local_surface_id = renderer_local_surface_id;
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }

  // Do a resize in the browser. It does not apply, but VisualProperties are
  // sent. (Why?)
  widget_host_->ClearVisualProperties();
  view_->SetSize(gfx::Size(120, 120));

  // blink::mojom::Widget::UpdateVisualProperties is sent to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // Auto-resizve limits sent to the renderer.
    EXPECT_EQ(gfx::Size(50, 50), visual_properties.min_size_for_auto_resize);
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.max_size_for_auto_resize);
    EXPECT_EQ(gfx::Size(120, 120), visual_properties.new_size);
    EXPECT_EQ(1, visual_properties.screen_infos.current().device_scale_factor);
    // A newly generated LocalSurfaceId is sent.
    EXPECT_TRUE(visual_properties.local_surface_id.has_value());
    EXPECT_NE(host_local_surface_id,
              visual_properties.local_surface_id.value());
    EXPECT_NE(renderer_local_surface_id,
              visual_properties.local_surface_id.value());
  }
}

// This test verifies that in AutoResize mode a child-allocated
// viz::LocalSurfaceId will be properly routed and stored in the parent.
TEST_F(RenderWidgetHostViewAuraTest, ChildAllocationAcceptedInParent) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  sink_->ClearMessages();
  viz::LocalSurfaceId local_surface_id1(view_->GetLocalSurfaceId());
  EXPECT_TRUE(local_surface_id1.is_valid());

  widget_host_->SetAutoResize(true, gfx::Size(50, 50), gfx::Size(100, 100));
  viz::ChildLocalSurfaceIdAllocator child_allocator;
  child_allocator.UpdateFromParent(local_surface_id1);
  child_allocator.GenerateId();
  viz::LocalSurfaceId local_surface_id2 =
      child_allocator.GetCurrentLocalSurfaceId();

  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(75, 75);
    metadata.local_surface_id = local_surface_id2;
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }

  viz::LocalSurfaceId local_surface_id3(view_->GetLocalSurfaceId());
  EXPECT_NE(local_surface_id1, local_surface_id3);
  EXPECT_EQ(local_surface_id2, local_surface_id3);
}

// This test verifies that if the parent is hidden when the child sends a
// child-allocated viz::LocalSurfaceId, the parent will store it and it will
// not send a blink::mojom::Widget::UpdateVisualProperties back to the child.
TEST_F(RenderWidgetHostViewAuraTest,
       ChildAllocationAcceptedInParentWhileHidden) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  widget_host_->ClearVisualProperties();
  viz::LocalSurfaceId local_surface_id1(view_->GetLocalSurfaceId());
  EXPECT_TRUE(local_surface_id1.is_valid());

  widget_host_->SetAutoResize(true, gfx::Size(50, 50), gfx::Size(100, 100));
  viz::ChildLocalSurfaceIdAllocator child_allocator;
  child_allocator.UpdateFromParent(local_surface_id1);
  child_allocator.GenerateId();
  viz::LocalSurfaceId local_surface_id2 =
      child_allocator.GetCurrentLocalSurfaceId();

  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());

  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(75, 75);
    metadata.local_surface_id = local_surface_id2;
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }

  viz::LocalSurfaceId local_surface_id3(view_->GetLocalSurfaceId());
  EXPECT_NE(local_surface_id1, local_surface_id3);
  EXPECT_EQ(local_surface_id2, local_surface_id3);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, widget_host_->visual_properties().size());
}

// This test verifies that when the child and parent both allocate their own
// viz::LocalSurfaceId the resulting conflict is resolved.
TEST_F(RenderWidgetHostViewAuraTest, ConflictingAllocationsResolve) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  sink_->ClearMessages();
  viz::LocalSurfaceId local_surface_id1(view_->GetLocalSurfaceId());
  EXPECT_TRUE(local_surface_id1.is_valid());

  widget_host_->SetAutoResize(true, gfx::Size(50, 50), gfx::Size(100, 100));
  viz::ChildLocalSurfaceIdAllocator child_allocator;
  child_allocator.UpdateFromParent(local_surface_id1);
  child_allocator.GenerateId();
  viz::LocalSurfaceId local_surface_id2 =
      child_allocator.GetCurrentLocalSurfaceId();

  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(75, 75);
    metadata.local_surface_id = local_surface_id2;
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }

  // Cause a conflicting viz::LocalSurfaceId allocation
  aura_test_helper_->GetTestScreen()->SetDeviceScaleFactor(2.0f);
  viz::LocalSurfaceId merged_local_surface_id(view_->GetLocalSurfaceId());
  EXPECT_NE(local_surface_id1, merged_local_surface_id);
  EXPECT_NE(local_surface_id2, merged_local_surface_id);
  EXPECT_GT(merged_local_surface_id.parent_sequence_number(),
            local_surface_id2.parent_sequence_number());
  EXPECT_EQ(merged_local_surface_id.child_sequence_number(),
            local_surface_id2.child_sequence_number());
}

// Checks that WidgetInputHandler::CursorVisibilityChange IPC messages are
// dispatched to the renderer at the correct times.
TEST_F(RenderWidgetHostViewAuraTest, CursorVisibilityChange) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->SetSize(gfx::Size(100, 100));

  aura::test::TestCursorClient cursor_client(
      parent_view_->GetNativeView()->GetRootWindow());

  cursor_client.AddObserver(view_);

  // Expect a message the first time the cursor is shown.
  view_->Show();
  base::RunLoop().RunUntilIdle();
  GetAndResetDispatchedMessages();
  cursor_client.ShowCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("CursorVisibilityChanged",
            GetMessageNames(GetAndResetDispatchedMessages()));

  // No message expected if the renderer already knows the cursor is visible.
  cursor_client.ShowCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetAndResetDispatchedMessages().size());

  // Hiding the cursor should send a message.
  cursor_client.HideCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("CursorVisibilityChanged",
            GetMessageNames(GetAndResetDispatchedMessages()));

  // No message expected if the renderer already knows the cursor is invisible.
  cursor_client.HideCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetAndResetDispatchedMessages().size());

  // No messages should be sent while the view is invisible.
  view_->Hide();
  base::RunLoop().RunUntilIdle();
  GetAndResetDispatchedMessages();
  cursor_client.ShowCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetAndResetDispatchedMessages().size());
  cursor_client.HideCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetAndResetDispatchedMessages().size());

  // Show the view. Since the cursor was invisible when the view was hidden,
  // no message should be sent.
  view_->Show();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetAndResetDispatchedMessages().size());

  // No message expected if the renderer already knows the cursor is invisible.
  cursor_client.HideCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetAndResetDispatchedMessages().size());

  // Showing the cursor should send a message.
  cursor_client.ShowCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("CursorVisibilityChanged",
            GetMessageNames(GetAndResetDispatchedMessages()));

  // No messages should be sent while the view is invisible.
  view_->Hide();
  cursor_client.HideCursor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetAndResetDispatchedMessages().size());

  // Show the view. Since the cursor was visible when the view was hidden,
  // a message is expected to be sent.
  view_->Show();
  base::RunLoop().RunUntilIdle();
  auto events = GetAndResetDispatchedMessages();
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40163541): Investigate occasional extra mousemoves in CrOS.
  EXPECT_GE(1u, events.size());
#else
  EXPECT_EQ(1u, events.size());
#endif
  EXPECT_EQ("CursorVisibilityChanged", events[0]->name());

  cursor_client.RemoveObserver(view_);
}

TEST_F(RenderWidgetHostViewAuraTest, UpdateCursorIfOverSelf) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  // Note that all coordinates in this test are screen coordinates.
  view_->SetBounds(gfx::Rect(60, 60, 100, 100));
  view_->Show();

  aura::test::TestCursorClient cursor_client(
      parent_view_->GetNativeView()->GetRootWindow());

  // Cursor is in the middle of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(110, 110));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is near the top of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(80, 65));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is near the bottom of the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(159, 159));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(1, cursor_client.calls_to_set_cursor());

  // Cursor is above the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(67, 59));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(0, cursor_client.calls_to_set_cursor());

  // Cursor is below the window.
  cursor_client.reset_calls_to_set_cursor();
  aura::Env::GetInstance()->SetLastMouseLocation(gfx::Point(161, 161));
  view_->UpdateCursorIfOverSelf();
  EXPECT_EQ(0, cursor_client.calls_to_set_cursor());
}

TEST_F(RenderWidgetHostViewAuraTest, ZeroSizeStillGetsLocalSurfaceId) {
  parent_local_surface_id_allocator_.GenerateId();

  InitViewForFrame(nullptr);
  ASSERT_EQ(1u, widget_host_->visual_properties().size());

  // Set an empty size.
  ParentHostView(view_, parent_view_);
  // It's set on the layer.
  ui::Layer* parent_layer = view_->GetNativeView()->layer();
  EXPECT_EQ(gfx::Rect(), parent_layer->bounds());

  // Update to the renderer.
  base::RunLoop().RunUntilIdle();

  // The renderer received the update with a new surface LocalSurfaceId.
  ASSERT_EQ(2u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    // Empty size is sent.
    EXPECT_EQ(gfx::Size(), visual_properties.new_size);
    // A LocalSurfaceId is sent too.
    ASSERT_TRUE(visual_properties.local_surface_id.has_value());
    EXPECT_TRUE(visual_properties.local_surface_id->is_valid());
  }
}

TEST_F(RenderWidgetHostViewAuraTest, BackgroundColorMatchesCompositorFrame) {
  gfx::Size frame_size(100, 100);
  parent_local_surface_id_allocator_.GenerateId();

  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->SetSize(frame_size);
  view_->Show();
  cc::RenderFrameMetadata metadata;
  metadata.root_background_color = SkColors::kRed;
  view_->SetRenderFrameMetadata(metadata);
  view_->OnRenderFrameMetadataChangedAfterActivation(base::TimeTicks::Now());
  ui::Layer* parent_layer = view_->GetNativeView()->layer();

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), parent_layer->bounds());
  EXPECT_EQ(SK_ColorRED, parent_layer->background_color());
}

// Tests background setting priority.
TEST_F(RenderWidgetHostViewAuraTest, BackgroundColorOrder) {
  // If the default background color is not available, then use the theme
  // background color.
  InitViewForFrame(nullptr);
  view_->SetBackgroundColor(SK_ColorBLUE);
  ASSERT_TRUE(view_->GetBackgroundColor());
  EXPECT_EQ(static_cast<unsigned>(SK_ColorBLUE), *view_->GetBackgroundColor());

  // If the content background color is available, ignore the default background
  // color setting.
  cc::RenderFrameMetadata metadata;
  metadata.root_background_color = SkColors::kWhite;
  view_->SetRenderFrameMetadata(metadata);
  view_->OnRenderFrameMetadataChangedAfterActivation(base::TimeTicks::Now());
  ASSERT_TRUE(view_->GetBackgroundColor());
  EXPECT_EQ(static_cast<unsigned>(SK_ColorWHITE), *view_->GetBackgroundColor());

  view_->SetBackgroundColor(SK_ColorRED);
  ASSERT_TRUE(view_->GetBackgroundColor());
  EXPECT_EQ(static_cast<unsigned>(SK_ColorWHITE), *view_->GetBackgroundColor());
}

TEST_F(RenderWidgetHostViewAuraTest, Resize) {
  constexpr gfx::Size size1(100, 100);
  constexpr gfx::Size size2(200, 200);

  aura::Window* root_window = parent_view_->GetNativeView()->GetRootWindow();
  InitViewForFrame(nullptr);
  aura::client::ParentWindowWithContext(view_->GetNativeView(), root_window,
                                        gfx::Rect(size1),
                                        display::kInvalidDisplayId);
  view_->Show();
  view_->SetSize(size1);
  EXPECT_EQ(size1.ToString(), view_->GetRequestedRendererSize().ToString());
  EXPECT_TRUE(widget_host_->visual_properties_ack_pending_for_testing());

  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = size1;
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
    EXPECT_FALSE(widget_host_->visual_properties_ack_pending_for_testing());
  }
  widget_host_->ClearVisualProperties();

  // Resize the renderer. This should produce an UpdateVisualProperties IPC.
  view_->SetSize(size2);
  EXPECT_EQ(size2.ToString(), view_->GetRequestedRendererSize().ToString());
  EXPECT_TRUE(widget_host_->visual_properties_ack_pending_for_testing());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  EXPECT_EQ(size2, widget_host_->visual_properties().at(0).new_size);
  // Render should send back RenderFrameMetadata with new size.
  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = size2;
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
    EXPECT_FALSE(widget_host_->visual_properties_ack_pending_for_testing());
  }
  widget_host_->ClearVisualProperties();

  // Calling SetSize() with the current size should be a no-op.
  view_->SetSize(size2);
  EXPECT_FALSE(widget_host_->visual_properties_ack_pending_for_testing());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, widget_host_->visual_properties().size());
}

// This test verifies that the primary SurfaceId is populated on resize.
TEST_F(RenderWidgetHostViewAuraTest, SurfaceChanges) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  ASSERT_TRUE(view_->delegated_frame_host_);

  view_->SetSize(gfx::Size(300, 300));
  ASSERT_TRUE(view_->HasPrimarySurface());
  EXPECT_EQ(gfx::Size(300, 300), view_->window_->layer()->size());
  EXPECT_EQ(gfx::Size(300, 300),
            view_->delegated_frame_host_->CurrentFrameSizeInDipForTesting());
}

// This test verifies that the primary SurfaceId is updated on device scale
// factor changes.
TEST_F(RenderWidgetHostViewAuraTest, DeviceScaleFactorChanges) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->SetSize(gfx::Size(300, 300));
  ASSERT_TRUE(view_->HasPrimarySurface());
  EXPECT_EQ(gfx::Size(300, 300), view_->window_->layer()->size());
  viz::SurfaceId initial_surface_id = *view_->window_->layer()->GetSurfaceId();
  EXPECT_EQ(nullptr, view_->window_->layer()->GetOldestAcceptableFallback());

  // Resizing should update the primary SurfaceId.
  aura_test_helper_->GetTestScreen()->SetDeviceScaleFactor(2.0f);
  viz::SurfaceId new_surface_id = *view_->window_->layer()->GetSurfaceId();
  EXPECT_NE(new_surface_id, initial_surface_id);
  EXPECT_EQ(gfx::Size(300, 300), view_->window_->layer()->bounds().size());
}

// This test verifies that frame eviction plays well with surface
// synchronization.
TEST_F(RenderWidgetHostViewAuraTest, DiscardDelegatedFrames) {
  // Make sure |parent_view_| is evicted to avoid interfering with the code
  // below.
  parent_view_->Hide();
  auto* dfh = parent_view_->delegated_frame_host_.get();
  static_cast<viz::FrameEvictorClient*>(dfh)->EvictDelegatedFrame(
      dfh->GetFrameEvictorForTesting()->CollectSurfaceIdsForEviction());

  size_t max_renderer_frames =
      FrameEvictionManager::GetInstance()->GetMaxNumberOfSavedFrames();
  ASSERT_LE(2u, max_renderer_frames);
  size_t renderer_count = max_renderer_frames + 1;
  gfx::Rect view_rect(100, 100);

  std::unique_ptr<RenderWidgetHostImpl* []> hosts(
      new RenderWidgetHostImpl*[renderer_count]);
  std::unique_ptr<FakeRenderWidgetHostViewAura* []> views(
      new FakeRenderWidgetHostViewAura*[renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    int32_t routing_id = process_host_->GetNextRoutingID();
    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    hosts[i] = MockRenderWidgetHostImpl::Create(
        GetFrameTree(), delegates_.back().get(),
        site_instance_group_->GetSafeRef(), routing_id, /*hidden = */ false);
    delegates_.back()->set_widget_host(hosts[i]);

    views[i] = new FakeRenderWidgetHostViewAura(hosts[i]);
    // Prevent frames from being skipped due to resize, this test does not
    // run a UI compositor so the DelegatedFrameHost doesn't get the chance
    // to release its resize lock once it receives a frame of the expected
    // size.
    views[i]->InitAsChild(nullptr);
    ParentHostView(views[i], parent_view_);

    // The blink::mojom::Widget interfaces are bound during
    // MockRenderWidgetHostImpl construction.
    hosts[i]->BindFrameWidgetInterfaces(
        mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>()
            .InitWithNewEndpointAndPassReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());
    hosts[i]->RendererWidgetCreated(/*for_frame_widget=*/true);

    views[i]->SetSize(view_rect.size());
    EXPECT_HAS_FRAME(views[i]);
  }

  // Make each renderer visible, and swap a frame on it, then make it invisible.
  for (size_t i = 0; i < renderer_count; ++i) {
    views[i]->Show();
    EXPECT_HAS_FRAME(views[i]);
    views[i]->Hide();
  }

  // There should be max_renderer_frames with a frame in it, and one without it.
  // Since the logic is LRU eviction, the first one should be without.
  EXPECT_EVICTED(views[0]);
  for (size_t i = 1; i < renderer_count; ++i)
    EXPECT_HAS_FRAME(views[i]);

  // LRU renderer is [0], make it visible, it should evict the next LRU [1].
  views[0]->Show();
  EXPECT_HAS_FRAME(views[0]);
  EXPECT_EVICTED(views[1]);
  views[0]->Hide();

  // LRU renderer is [1], which is still hidden. Showing it and submitting a
  // CompositorFrame to it should evict the next LRU [2].
  views[1]->Show();
  EXPECT_HAS_FRAME(views[0]);
  EXPECT_HAS_FRAME(views[1]);
  EXPECT_EVICTED(views[2]);
  for (size_t i = 3; i < renderer_count; ++i)
    EXPECT_HAS_FRAME(views[i]);

  // Make all renderers but [0] visible and swap a frame on them, keep [0]
  // hidden, it becomes the LRU.
  for (size_t i = 1; i < renderer_count; ++i) {
    views[i]->Show();
    EXPECT_HAS_FRAME(views[i]);
  }
  EXPECT_EVICTED(views[0]);

  // Make [0] visible, and swap a frame on it. Nothing should be evicted
  // although we're above the limit.
  views[0]->Show();
  for (size_t i = 0; i < renderer_count; ++i)
    EXPECT_HAS_FRAME(views[i]);

  // Make [0] hidden, it should evict its frame.
  views[0]->Hide();
  EXPECT_EVICTED(views[0]);

  // Make [0] visible, don't give it a frame, it should be waiting.
  views[0]->Show();
  // Make [0] hidden, it should stop waiting.
  views[0]->Hide();

  // Make [1] hidden, resize it. It should advance its fallback.
  views[1]->Hide();
  gfx::Size size2(200, 200);
  views[1]->SetSize(size2);
  // Show it, it should block until we give it a frame.
  views[1]->Show();
  ASSERT_TRUE(views[1]->window_->layer()->GetOldestAcceptableFallback());
  EXPECT_EQ(*views[1]->window_->layer()->GetOldestAcceptableFallback(),
            *views[1]->window_->layer()->GetSurfaceId());

  for (size_t i = 0; i < renderer_count; ++i)
    views[i]->Destroy();
}

// Test that changing the memory pressure should delete saved frames. This test
// only applies to ChromeOS.
TEST_F(RenderWidgetHostViewAuraTest, DiscardDelegatedFramesWithMemoryPressure) {
  // Make sure |parent_view_| is evicted to avoid interfering with the code
  // below.
  parent_view_->Hide();
  auto* dfh = parent_view_->delegated_frame_host_.get();
  static_cast<viz::FrameEvictorClient*>(dfh)->EvictDelegatedFrame(
      dfh->GetFrameEvictorForTesting()->CollectSurfaceIdsForEviction());

  // The test logic below relies on having max_renderer_frames > 2.  By default,
  // this value is calculated from total physical memory and causes the test to
  // fail when run on hardware with < 256MB of RAM.
  const size_t kMaxRendererFrames = 5;
  FrameEvictionManager::GetInstance()->set_max_number_of_saved_frames(
      kMaxRendererFrames);

  size_t renderer_count = kMaxRendererFrames;
  gfx::Rect view_rect(100, 100);

  std::unique_ptr<RenderWidgetHostImpl* []> hosts(
      new RenderWidgetHostImpl*[renderer_count]);
  std::unique_ptr<FakeRenderWidgetHostViewAura* []> views(
      new FakeRenderWidgetHostViewAura*[renderer_count]);

  // Create a bunch of renderers.
  for (size_t i = 0; i < renderer_count; ++i) {
    int32_t routing_id = process_host_->GetNextRoutingID();

    delegates_.push_back(base::WrapUnique(new MockRenderWidgetHostDelegate));
    hosts[i] = MockRenderWidgetHostImpl::Create(
        GetFrameTree(), delegates_.back().get(),
        site_instance_group_->GetSafeRef(), routing_id, /*hidden = */ false);
    delegates_.back()->set_widget_host(hosts[i]);

    hosts[i]->BindWidgetInterfaces(
        mojo::PendingAssociatedRemote<blink::mojom::WidgetHost>()
            .InitWithNewEndpointAndPassReceiver(),
        TestRenderWidgetHost::CreateStubWidgetRemote());
    hosts[i]->BindFrameWidgetInterfaces(
        mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>()
            .InitWithNewEndpointAndPassReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());
    hosts[i]->RendererWidgetCreated(/*for_frame_widget=*/true);

    views[i] = new FakeRenderWidgetHostViewAura(hosts[i]);
    views[i]->InitAsChild(nullptr);
    ParentHostView(views[i], parent_view_);
    views[i]->SetSize(view_rect.size());
    views[i]->Show();
    EXPECT_HAS_FRAME(views[i]);
  }

  // If we hide one, it should not get evicted.
  views[0]->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_HAS_FRAME(views[0]);
  // Using a lesser memory pressure event however, should evict.
  SimulateMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EVICTED(views[0]);

  // Check the same for a higher pressure event.
  views[1]->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_HAS_FRAME(views[1]);
  SimulateMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  EXPECT_EVICTED(views[1]);

  for (size_t i = 0; i < renderer_count; ++i)
    views[i]->Destroy();
}

TEST_F(RenderWidgetHostViewAuraTest, VisibleViewportTest) {
  gfx::Rect view_rect(100, 100);

  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  widget_host_->ClearVisualProperties();
  view_->SetSize(view_rect.size());
  view_->Show();

  // Defaults to full height of the view.
  EXPECT_EQ(100, view_->GetVisibleViewportSize().height());

  // Update to the renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.new_size);
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.visible_viewport_size);
  }

  // Get back the UpdateVisualProperties ack.
  {
    cc::RenderFrameMetadata metadata;
    metadata.viewport_size_in_pixels = gfx::Size(100, 100);
    static_cast<RenderFrameMetadataProvider::Observer*>(widget_host_)
        ->OnLocalSurfaceIdChanged(metadata);
  }
  widget_host_->ClearVisualProperties();

  view_->SetInsets(gfx::Insets::TLBR(0, 0, 40, 0));
  EXPECT_EQ(60, view_->GetVisibleViewportSize().height());

  // Update to the renderer has the inset size.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, widget_host_->visual_properties().size());
  {
    blink::VisualProperties visual_properties =
        widget_host_->visual_properties().at(0);
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.new_size);
    EXPECT_EQ(gfx::Size(100, 60), visual_properties.visible_viewport_size);
  }
}

#if BUILDFLAG(IS_OZONE)
// Regression test for crbug.com/360147125.
// Tests that when per-window scaling is used, the window scale is not compared
// to display scale when display metrics changes.
TEST_F(RenderWidgetHostViewAuraTest,
       SkipDisplayScaleCheckOnDisplayMetricsChangeWithPerWindowScaleEnabled) {
  if (ui::OzonePlatform::GetPlatformNameForTest() != "wayland") {
    GTEST_SKIP() << "test only applicable on wayland";
  }
  using SupportsForTest =
      ui::OzonePlatform::PlatformRuntimeProperties::SupportsForTest;
  base::AutoReset<SupportsForTest> auto_reset(
      &ui::OzonePlatform::PlatformRuntimeProperties::
          override_supports_per_window_scaling_for_test,
      SupportsForTest::kYes);
  aura::Window* root_window = parent_view_->GetNativeView()->GetRootWindow();
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  EXPECT_TRUE(view_->CanSynchronizeVisualProperties());

  // Set different values for window and display scales.
  aura_test_helper_->GetTestScreen()->SetPreferredScaleFactorForWindow(
      root_window, 1.75f);
  aura_test_helper_->GetTestScreen()->SetDeviceScaleFactor(1.74623f);

  view_->OnDisplayMetricsChanged(
      display::Screen::GetScreen()->GetDisplayNearestView(
          view_->GetNativeView()),
      0);

  // Synchronization of visual properties should be allowed in spite of the
  // mismatch in preferred window scale and display scale.
  EXPECT_TRUE(view_->CanSynchronizeVisualProperties());
}
#endif

// Ensures that touch event positions are never truncated to integers.
TEST_F(RenderWidgetHostViewAuraTest, TouchEventPositionsArentRounded) {
  const float kX = 30.58f;
  const float kY = 50.23f;

  InitViewForFrame(nullptr);
  view_->Show();

  ui::TouchEvent press(ui::EventType::kTouchPressed, gfx::Point(),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  press.set_location_f(gfx::PointF(kX, kY));
  press.set_root_location_f(gfx::PointF(kX, kY));

  view_->OnTouchEvent(&press);
  EXPECT_EQ(ui::MotionEvent::Action::DOWN, pointer_state().GetAction());
  EXPECT_EQ(1U, pointer_state().GetPointerCount());
  EXPECT_EQ(kX, pointer_state().GetX(0));
  EXPECT_EQ(kY, pointer_state().GetY(0));
}

// Tests that non-precise mouse-wheel events do not initiate overscroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, WheelNotPreciseScrollEvent) {
  SetUpOverscrollEnvironment();

  // Simulate wheel event. Does not cross start threshold.
  SimulateWheelEvent(-5, 0, 0, false,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  // Simulate wheel event. Crosses start threshold.
  SimulateWheelEvent(-70, 1, 0, false,
                     WebMouseWheelEvent::kPhaseChanged);  // enqueued
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  // Receive ACK the first wheel event as not processed.
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  SimulateWheelEvent(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd", GetMessageNames(events));

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
}
// Tests that precise mouse-wheel events initiate overscroll and a mouse move
// will cancel it.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, WheelScrollEventOverscrolls) {
  SetUpOverscrollEnvironment();

  // Simulate wheel events. Do not cross start threshold.
  SimulateWheelEvent(-5, 0, 0, true,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEvent(-10, 1, 0, true,
                     WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEvent(
      -10, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      -15, -1, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  // Simulate wheel events. Cross start threshold.
  SimulateWheelEvent(
      -30, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      -20, 6, 1, true,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  // Receive ACK the first wheel event as not processed.
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 2);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  ExpectGestureScrollEndForWheelScrolling(false);
  SendNotConsumedAcks(events);

  EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());
  EXPECT_EQ(-90.f, overscroll_delta_x());
  EXPECT_EQ(-30.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());

  // Send a mouse-move event. This should cancel the overscroll navigation.
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseMove", GetMessageNames(events));
}

// Tests that if some scroll events are consumed towards the start, then
// subsequent scrolls do not overscroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       WheelScrollConsumedDoNotOverscroll) {
  SetUpOverscrollEnvironment();

  // Simulate wheel events. Do not cross start threshold.
  SimulateWheelEvent(-5, 0, 0, true,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEvent(-10, -1, 0, true,
                     WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEvent(
      -10, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      -15, -1, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  // Simulate wheel events. Cross start threshold.
  SimulateWheelEvent(
      -30, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      -20, 6, 1, true,
      WebMouseWheelEvent::kPhaseChanged);  // enqueued, different modifiers

  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  // Receive ACK the first wheel event as processed.
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 2);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

    // The GSU events are coalesced. This is the ack for the coalesced event.
    // Since it is the first GSU, the ack should be consumed.
  SendScrollUpdateAck(events, blink::mojom::InputEventResultState::kConsumed);

  SimulateWheelEvent(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
}

// Tests that wheel-scrolling correctly turns overscroll on and off.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, WheelScrollOverscrollToggle) {
  SetUpOverscrollEnvironment();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  SendNotConsumedAcks(events);

  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  SendNotConsumedAcks(events);

  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more so as to not overscroll.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  events = ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);
  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more to initiate an overscroll.
  SimulateWheelEvent(50, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);

  events = ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);

  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(70.f, overscroll_delta_x());
  EXPECT_EQ(10.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Scroll in the reverse direction enough to abort the overscroll.
  SimulateWheelEvent(-20, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Continue to scroll in the reverse direction.
  SimulateWheelEvent(-20, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Continue to scroll in the reverse direction enough to initiate overscroll
  // in that direction. However, overscroll should not be initiated as the
  // overscroll mode is locked to east mode.
  SimulateWheelEvent(-65, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  SimulateWheelEvent(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollEnd", GetMessageNames(events));

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(-105.f, overscroll_delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
}

// Tests that a small fling after overscroll is initiated aborts the overscroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       ScrollEventsOverscrollWithFling) {
  SetUpOverscrollEnvironment();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  SendNotConsumedAcks(events);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more so as to not overscroll.
  SimulateWheelEvent(20, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);

  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more to initiate an overscroll.
  SimulateWheelEvent(40, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);

  SendNotConsumedAcks(events);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  EXPECT_EQ(70.f, overscroll_delta_x());
  EXPECT_EQ(10.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());

  // Send a fling start, but with a small velocity, the fling controller handles
  // GFS with touchpad source and the event doesn't get queued in gesture event
  // queue. The overscroll state doesn't get reset till the fling progress sends
  // the fling end event.
  SimulateGestureFlingStartEvent(0.f, 0.1f, blink::WebGestureDevice::kTouchpad);
  events = GetAndResetDispatchedMessages();
  bool fling_end_event_sent_ = events.size();
  if (fling_end_event_sent_) {
    EXPECT_EQ("MouseWheel GestureScrollEnd", GetMessageNames(events));
    EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
    EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  } else {
    EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
    EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  }

  base::TimeTicks progress_time =
      base::TimeTicks::Now() + base::Milliseconds(17);
  // Overscroll mode will get reset at the end of the fling progress.
  while (overscroll_mode() != OVERSCROLL_NONE) {
    widget_host_->ProgressFlingIfNeeded(progress_time);
    progress_time += base::Milliseconds(17);
  }
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
}

// Same as ScrollEventsOverscrollWithFling, but with zero velocity. Checks that
// the zero-velocity fling does not reach the renderer.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       ScrollEventsOverscrollWithZeroFling) {
  SetUpOverscrollEnvironment();

  // Send a wheel event. ACK the event as not processed. This should not
  // initiate an overscroll gesture since it doesn't cross the threshold yet.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  SendNotConsumedAcks(events);

  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more so as to not overscroll.
  SimulateWheelEvent(20, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);

  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll some more to initiate an overscroll.
  SimulateWheelEvent(40, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  ExpectGestureScrollUpdateAfterNonBlockingMouseWheelACK(false);

  SendNotConsumedAcks(events);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  EXPECT_EQ(70.f, overscroll_delta_x());
  EXPECT_EQ(10.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());

  // Send a fling start, but with a zero velocity, the fling should not proceed
  // to the renderer.
  SimulateGestureFlingStartEvent(0.f, 0.f, blink::WebGestureDevice::kTouchpad);
  events = GetAndResetDispatchedMessages();
  for (const auto& event : events) {
    EXPECT_NE(WebInputEvent::Type::kGestureFlingStart,
              event->ToEvent()->Event()->Event().GetType());
  }

  // Fling controller handles the GFS with touchpad source and zero velocity and
  // sends a nonblocking wheel end event. The GSE generated from wheel end event
  // resets scroll state.
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            events[events.size() - 1]->ToEvent()->Event()->Event().GetType());

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
}

// Tests that a fling in the opposite direction of the overscroll cancels the
// overscroll instead of completing it.
// Flaky on Fuchsia:  http://crbug.com/810690.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_ReverseFlingCancelsOverscroll \
  DISABLED_ReverseFlingCancelsOverscroll
#else
#define MAYBE_ReverseFlingCancelsOverscroll ReverseFlingCancelsOverscroll
#endif
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       MAYBE_ReverseFlingCancelsOverscroll) {
  SetUpOverscrollEnvironment();

  {
    PressAndSetTouchActionAuto();
    // Start and end a gesture in the same direction without processing the
    // gesture events in the renderer. This should initiate and complete an
    // overscroll.
    SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                         blink::WebGestureDevice::kTouchscreen);
    SimulateGestureScrollUpdateEvent(300, -5, 0);
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
    EXPECT_EQ("GestureScrollBegin TouchScrollStarted GestureScrollUpdate",
              GetMessageNames(events));
    SendScrollBeginAckIfNeeded(events,
                               blink::mojom::InputEventResultState::kConsumed);
    SendNotConsumedAcks(events);
    EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
    EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
    EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

    SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                         blink::WebGestureDevice::kTouchscreen);
    events = GetAndResetDispatchedMessages();
    EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
    EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
    EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
    ReleaseAndResetDispatchedMessages();
  }

  {
    PressAndSetTouchActionAuto();
    // Start over, except instead of ending the gesture with ScrollEnd, end it
    // with a FlingStart, with velocity in the reverse direction. This should
    // initiate an overscroll, the overscroll mode should get reset after the
    // first GSU event generated by the fling controller.
    overscroll_delegate()->Reset();
    SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                         blink::WebGestureDevice::kTouchscreen);
    SimulateGestureScrollUpdateEvent(-300, -5, 0);
    MockWidgetInputHandler::MessageVector events =
        GetAndResetDispatchedMessages();
    EXPECT_EQ("GestureScrollBegin TouchScrollStarted GestureScrollUpdate",
              GetMessageNames(events));
    SendScrollBeginAckIfNeeded(events,
                               blink::mojom::InputEventResultState::kConsumed);
    SendNotConsumedAcks(events);
    EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
    EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
    EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());

    SimulateGestureFlingStartEvent(100, 0,
                                   blink::WebGestureDevice::kTouchscreen);
    events = GetAndResetDispatchedMessages();
    // The fling start event is not sent to the renderer.
    EXPECT_EQ(0U, events.size());
    EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
    EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
    EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());

    // The overscrolling mode will reset after the first GSU from fling
    // progress.
    base::TimeTicks progress_time =
        base::TimeTicks::Now() + base::Milliseconds(17);
    widget_host_->ProgressFlingIfNeeded(progress_time);
    EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
    ReleaseAndResetDispatchedMessages();
  }
}

// Tests that touch-scroll events are handled correctly by the overscroll
// controller. This also tests that the overscroll controller and the
// gesture-event filter play nice with each other.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, GestureScrollOverscrolls) {
  SetUpOverscrollEnvironment();

  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Send another gesture event and ACK as not being processed. This should
  // initiate the overscroll.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Send another gesture update event. This event should be consumed by the
  // controller, and not be forwarded to the renderer. The gesture-event filter
  // should not also receive this event.
  SimulateGestureScrollUpdateEvent(10, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(-10.f, overscroll_delta_y());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Now send a scroll end. This should cancel the overscroll gesture, and send
  // the event to the renderer. The gesture-event filter should receive this
  // event.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  ReleaseAndResetDispatchedMessages();
}

// Tests that when a cap is set for overscroll delta, extra overscroll delta is
// ignored.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollDeltaCap) {
  SetUpOverscrollEnvironment();

  PressAndSetTouchActionAuto();
  // Set overscroll cap and start scrolling.
  overscroll_delegate()->set_delta_cap(50);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Scroll enough to initiate the overscrolling.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));
  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Scroll beyond overscroll cap. Overscroll delta should not surpass the cap.
  SimulateGestureScrollUpdateEvent(75, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(100.f, overscroll_delta_x());
  EXPECT_EQ(-10.f, overscroll_delta_y());
  EXPECT_EQ(50.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Scroll back a bit. Since the extra scroll after cap in previous step is
  // ignored, scrolling back should immediately reduce overscroll delta.
  SimulateGestureScrollUpdateEvent(-10, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(90.f, overscroll_delta_x());
  EXPECT_EQ(-15.f, overscroll_delta_y());
  EXPECT_EQ(40.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // End overscrolling.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  ReleaseAndResetDispatchedMessages();
}

// Tests that if the page is scrolled because of a scroll-gesture, then that
// particular scroll sequence never generates overscroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, GestureScrollConsumed) {
  SetUpOverscrollEnvironment();

  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SimulateGestureScrollUpdateEvent(10, 0, 0);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin TouchScrollStarted GestureScrollUpdate",
            GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  // Start scrolling on content. ACK both events as being processed.
  SendScrollUpdateAck(events, blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Send another gesture event and ACK as not being processed. This should
  // not initiate overscroll because the beginning of the scroll event did
  // scroll some content on the page. Since there was no overscroll, the event
  // should reach the renderer.
  SimulateGestureScrollUpdateEvent(55, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollUpdate", GetMessageNames(events));
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  ReleaseAndResetDispatchedMessages();
}

// Tests that the overscroll controller plays nice with touch-scrolls and the
// gesture event filter with debounce filtering turned on.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       GestureScrollDebounceOverscrolls) {
  SetUpOverscrollEnvironmentWithDebounce(100);

  PressAndSetTouchActionAuto();
  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));

  // Send update events.
  SimulateGestureScrollUpdateEvent(25, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));

  // Quickly end and restart the scroll gesture. These two events should get
  // discarded.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector second_scroll_update_events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, second_scroll_update_events.size());
  ReleaseAndResetDispatchedMessages();

  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  second_scroll_update_events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, second_scroll_update_events.size());

  // Send another update event. This should be sent right away.
  SimulateGestureScrollUpdateEvent(30, 0, 0);
  second_scroll_update_events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate",
            GetMessageNames(second_scroll_update_events));

  // Receive an ACK for the first scroll-update event as not being processed.
  // This will contribute to the overscroll gesture, but not enough for the
  // overscroll controller to start consuming gesture events.
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  // The second GSU was already sent.
  MockWidgetInputHandler::MessageVector third_scroll_update_events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, third_scroll_update_events.size());

  // Send another update event. This should be forwarded immediately since
  // GestureEventQueue allows multiple in-flight events.
  SimulateGestureScrollUpdateEvent(10, 0, 0);
  third_scroll_update_events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollUpdate", GetMessageNames(third_scroll_update_events));

  // Receive an ACK for the second scroll-update event as not being processed.
  // This will now initiate an overscroll.
  SendNotConsumedAcks(second_scroll_update_events);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Receive an ACK for the last scroll-update event as not being processed.
  // This will be consumed by the overscroll controller.
  SendNotConsumedAcks(third_scroll_update_events);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  ReleaseAndResetDispatchedMessages();
}

// Tests that the gesture debounce timer plays nice with the overscroll
// controller.
// TODO(crbug.com/40545668): Disabled due to flakiness on Linux tsan.
#if BUILDFLAG(USING_SANITIZER)
#define MAYBE_GestureScrollDebounceTimerOverscroll \
  DISABLED_GestureScrollDebounceTimerOverscroll
#else
#define MAYBE_GestureScrollDebounceTimerOverscroll \
  GestureScrollDebounceTimerOverscroll
#endif
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       MAYBE_GestureScrollDebounceTimerOverscroll) {
  SetUpOverscrollEnvironmentWithDebounce(10);

  PressAndSetTouchActionAuto();
  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  // Send update events.
  SimulateGestureScrollUpdateEvent(55, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));

  // Send an end event. This should get in the debounce queue.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetDispatchedMessages().size());
  ReleaseAndResetDispatchedMessages();

  // Receive ACK for the scroll-update event.
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  EXPECT_EQ(0U, sink_->message_count());

  // Let the timer for the debounce queue fire. That should release the queued
  // scroll-end event. Since overscroll has started, but there hasn't been
  // enough overscroll to complete the gesture, the overscroll controller
  // will reset the state. The scroll-end should therefore be dispatched to the
  // renderer, and the gesture-event-filter should await an ACK for it.
  task_environment_.FastForwardBy(base::Milliseconds(10));
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
}

// Tests that when touch-events are dispatched to the renderer, the overscroll
// gesture deals with them correctly.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollWithTouchEvents) {
  SetUpOverscrollEnvironmentWithDebounce(10);
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  widget_host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  // The test sends an intermingled sequence of touch and gesture events.
  PressTouchPoint(0, 1);
  SendTouchEvent();
  widget_host_->SetTouchActionFromMain(cc::TouchAction::kAuto);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchStart", GetMessageNames(events));
  SendNotConsumedAcks(events);

  MoveTouchPoint(0, 20, 5);
  SendTouchEvent();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchMove", GetMessageNames(events));
  SendNotConsumedAcks(events);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));
  SimulateGestureScrollUpdateEvent(20, 0, 0);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Another touch move event should reach the renderer since overscroll hasn't
  // started yet.  Note that touch events sent during the scroll period may
  // not require an ack (having been marked uncancelable).
  MoveTouchPoint(0, 65, 10);
  SendTouchEvent();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchMove", GetMessageNames(events));
  SendNotConsumedAcks(events);

  SimulateGestureScrollUpdateEvent(45, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollUpdate", GetMessageNames(events));
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Send another touch event. The page should get the touch-move event, even
  // though overscroll has started.
  MoveTouchPoint(0, 55, 5);
  SendTouchEvent();
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(65.f, overscroll_delta_x());
  EXPECT_EQ(15.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchMove", GetMessageNames(events));
  SendNotConsumedAcks(events);

  SimulateGestureScrollUpdateEvent(-10, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  PressTouchPoint(255, 5);
  SendTouchEvent();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchStart", GetMessageNames(events));
  SendNotConsumedAcks(events);

  SimulateGestureScrollUpdateEvent(200, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(255.f, overscroll_delta_x());
  EXPECT_EQ(205.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // The touch-end/cancel event should always reach the renderer if the page has
  // touch handlers.
  ReleaseTouchPoint(1);
  SendTouchEvent();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchEnd", GetMessageNames(events));
  SendNotConsumedAcks(events);
  ReleaseTouchPoint(0);
  SendTouchEvent();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchEnd", GetMessageNames(events));
  SendNotConsumedAcks(events);

  SimulateGestureEvent(blink::WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
}

// Tests that touch-gesture end is dispatched to the renderer at the end of a
// touch-gesture initiated overscroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       TouchGestureEndDispatchedAfterOverscrollComplete) {
  SetUpOverscrollEnvironmentWithDebounce(10);
  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  widget_host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  PressAndSetTouchActionAuto();
  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  // The scroll begin event will have received a synthetic ack from the input
  // router.
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Send update events.
  SimulateGestureScrollUpdateEvent(55, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SendNotConsumedAcks(events);
  EXPECT_EQ(0U, sink_->message_count());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(55.f, overscroll_delta_x());
  EXPECT_EQ(5.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Send end event.
  SimulateGestureEvent(blink::WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
  ReleaseAndResetDispatchedMessages();

  PressAndSetTouchActionAuto();
  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  // Send update events.
  SimulateGestureScrollUpdateEvent(235, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(235.f, overscroll_delta_x());
  EXPECT_EQ(185.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  // Send end event.
  SimulateGestureEvent(blink::WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
  ReleaseAndResetDispatchedMessages();
}

// Tests that after touchscreen overscroll is initiated, scrolling in the
// opposite direction ends the overscroll in the original direction without
// initiating overscroll in the opposite direction. The scroll-update events
// should still be consumed to prevent content scroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollDirectionChange) {
  SetUpOverscrollEnvironmentWithDebounce(100);

  PressAndSetTouchActionAuto();
  // Start scrolling. Receive ACK as it being processed.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  // Send update events and receive ack as not consumed.
  SimulateGestureScrollUpdateEvent(125, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));
  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());

  // Send another update event, but in the reverse direction. Although the
  // overscroll controller is not triggering overscroll, it will consume the
  // ScrollUpdate event to prevent content scroll.
  SimulateGestureScrollUpdateEvent(-260, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  // Although the overscroll mode has been reset, the next scroll update events
  // should be consumed by the overscroll controller to prevent content scroll.
  SimulateGestureScrollUpdateEvent(-20, 0, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  ReleaseAndResetDispatchedMessages();
}

TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       CompleteOverscrollOnGestureScrollEndAck) {
  SetUpOverscrollEnvironment();

  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  EXPECT_EQ("GestureScrollBegin", GetMessageNames(events));

  // Send GSU to trigger overscroll.
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  // Send GSE immediately before ACKing GSU.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);

  // Now ACK the GSU. Should see a completed overscroll.
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate GestureScrollEnd",
            GetMessageNames(events));
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
  ReleaseAndResetDispatchedMessages();
}

TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       InterleavedScrollUpdateAckAndScrollEnd) {
  SetUpOverscrollEnvironment();

  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SimulateGestureScrollUpdateEvent(30, -5, 0);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin TouchScrollStarted GestureScrollUpdate",
            GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  // Send the first GSU which shouldn't trigger overscroll.
  SendNotConsumedAcks(events);

  EXPECT_EQ(0U, overscroll_delegate()->historical_modes().size());

  // Send the second GSU which should be able to trigger overscroll if combined.
  SimulateGestureScrollUpdateEvent(30, -5, 0);

  // Send GSE immediately before ACKing GSU.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollUpdate GestureScrollEnd", GetMessageNames(events));

  // Now ACK the second GSU, should see overscroll being triggered and cleared.
  SendNotConsumedAcks(events);

  EXPECT_EQ(2U, overscroll_delegate()->historical_modes().size());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->historical_modes().at(0));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->historical_modes().at(1));
  ReleaseAndResetDispatchedMessages();
}

// Tests that after touchpad overscroll is initiated, scrolling in the opposite
// direction ends the overscroll in the original direction without initiating
// overscroll in the opposite direction. The scroll-update events should still
// be consumed to prevent content scroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       OverscrollDirectionChangeMouseWheel) {
  SetUpOverscrollEnvironment();

  // Send wheel event and receive ack as not consumed.
  SimulateWheelEvent(125, -5, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  // Receive ACK the first wheel event as not processed.
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  if (events.size() > 1)
    events[1]->ToEvent()->CallCallback(
        blink::mojom::InputEventResultState::kConsumed);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 0);
  SendNotConsumedAcks(events);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  // Send another wheel event, but in the reverse direction. Although the
  // overscroll controller is not triggering overscroll, it will consume the
  // ScrollUpdate event to prevent content scroll.
  SimulateWheelEvent(-260, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  // Although the overscroll controller consumes ScrollUpdate, it will not
  // initiate west overscroll as it is now locked in east mode.
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SimulateWheelEvent(-20, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
}

// Tests that mouse-move completes overscoll if it has passed activation
// threshold and aborts it otherwise.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollMouseMoveCompletion) {
  SetUpOverscrollEnvironment();

  SimulateWheelEvent(-5, 0, 0, true,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEvent(-10, 0, 0, true,
                     WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEvent(
      -10, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      -15, -1, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      -30, -3, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event

  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());

  // Receive ACK the first wheel event as not processed.
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SendNotConsumedAcks(events);
  ExpectGestureScrollEndForWheelScrolling(false);

  EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_EQ(OVERSCROLL_WEST, overscroll_delegate()->current_mode());

  // Send a mouse-move event. This should cancel the overscroll gesture (since
  // the amount overscrolled is not above the threshold), and so the mouse-move
  // should reach the renderer.
  SimulateMouseMove(5, 10, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseMove", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());

  SendNotConsumedAcks(events);

  // Moving the mouse more should continue to send the events to the renderer.
  SimulateMouseMove(5, 10, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseMove", GetMessageNames(events));
  SendNotConsumedAcks(events);

  // Now try with gestures.
  PressAndSetTouchActionAuto();
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SendScrollBeginAckIfNeeded(blink::mojom::InputEventResultState::kConsumed);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("TouchScrollStarted GestureScrollUpdate", GetMessageNames(events));
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  // Overscroll gesture is in progress. Send a mouse-move now. This should
  // complete the gesture (because the amount overscrolled is above the
  // threshold).
  SimulateMouseMove(5, 10, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseMove", GetMessageNames(events));
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  SendNotConsumedAcks(events);

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
  ReleaseAndResetDispatchedMessages();

  // Move mouse some more. The mouse-move events should reach the renderer.
  SimulateMouseMove(5, 10, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseMove", GetMessageNames(events));
}

// Tests that if a page scrolled, then the overscroll controller's states are
// reset after the end of the scroll.
TEST_F(RenderWidgetHostViewAuraOverscrollTest,
       OverscrollStateResetsAfterScroll) {
  SetUpOverscrollEnvironment();

  SimulateWheelEvent(0, 5, 0, true,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEvent(0, 30, 0, true,
                     WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEvent(
      0, 40, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  SimulateWheelEvent(
      0, 10, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  // The first wheel event is not consumed. Dispatches the queued wheel event.
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  SendScrollUpdateAck(events, blink::mojom::InputEventResultState::kConsumed);
  EXPECT_TRUE(ScrollStateIsContentConsuming());

  SendScrollUpdateAck(events, blink::mojom::InputEventResultState::kConsumed);
  EXPECT_TRUE(ScrollStateIsContentConsuming());

  // Touchpad scroll can end with a zero-velocity fling which is not dispatched.
  SimulateGestureFlingStartEvent(0.f, 0.f, blink::WebGestureDevice::kTouchpad);
  events = GetAndResetDispatchedMessages();
  for (const auto& event : events) {
    EXPECT_NE(WebInputEvent::Type::kGestureFlingStart,
              event->ToEvent()->Event()->Event().GetType());
  }

  // Fling controller handles a GFS with touchpad source and zero velocity and
  // sends a nonblocking wheel end event. The GSE generated from wheel end event
  // resets scroll state.
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            events[events.size() - 1]->ToEvent()->Event()->Event().GetType());
  EXPECT_TRUE(ScrollStateIsUnknown());

  // Dropped flings should neither propagate *nor* indicate that they were
  // consumed and have triggered a fling animation (as tracked by the router).
  EXPECT_FALSE(parent_host_->input_router()->HasPendingEvents());

  SimulateWheelEvent(-5, 0, 0, true,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  SimulateWheelEvent(-60, 0, 0, true,
                     WebMouseWheelEvent::kPhaseChanged);  // enqueued
  SimulateWheelEvent(
      -100, 0, 0, true,
      WebMouseWheelEvent::kPhaseChanged);  // coalesced into previous event

  EXPECT_TRUE(ScrollStateIsUnknown());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  // The first wheel scroll did not scroll content. Overscroll should not start
  // yet, since enough hasn't been scrolled.
  SendNotConsumedAcks(events);
  events = ExpectGestureScrollEventsAfterMouseWheelACK(true, 1);
  SendScrollBeginAckIfNeeded(blink::mojom::InputEventResultState::kConsumed);

  EXPECT_TRUE(ScrollStateIsUnknown());

  SendNotConsumedAcks(events);

  EXPECT_EQ(OVERSCROLL_WEST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, overscroll_source());
  EXPECT_TRUE(ScrollStateIsOverscrolling());

  // Touchpad scroll can end with a zero-velocity fling which is not dispatched.
  SimulateGestureFlingStartEvent(0.f, 0.f, blink::WebGestureDevice::kTouchpad);
  events = GetAndResetDispatchedMessages();

  for (const auto& event : events) {
    EXPECT_NE(WebInputEvent::Type::kGestureFlingStart,
              event->ToEvent()->Event()->Event().GetType());
  }

  // Fling controller handles a GFS with touchpad source and zero velocity and
  // sends a nonblocking wheel end event. The GSE generated from wheel end event
  // resets scroll state.
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            events[events.size() - 1]->ToEvent()->Event()->Event().GetType());

  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_TRUE(ScrollStateIsUnknown());
  EXPECT_FALSE(parent_host_->input_router()->HasPendingEvents());
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, events.size());
}

// Tests that overscroll is reset when window loses focus. It should not affect
// subsequent overscrolls.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, OverscrollResetsOnBlur) {
  SetUpOverscrollEnvironment();

  PressAndSetTouchActionAuto();
  // Start an overscroll with gesture scroll. In the middle of the scroll, blur
  // the host.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin TouchScrollStarted GestureScrollUpdate",
            GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());

  view_->OnWindowFocused(nullptr, view_->GetNativeView());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_x());
  EXPECT_EQ(0.f, overscroll_delegate()->delta_y());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("SetFocus GestureScrollEnd", GetMessageNames(events));
  ReleaseAndResetDispatchedMessages();

  PressAndSetTouchActionAuto();
  // Start a scroll gesture again. This should correctly start the overscroll
  // after the threshold.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SimulateGestureScrollUpdateEvent(300, -5, 0);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("GestureScrollBegin TouchScrollStarted GestureScrollUpdate",
            GetMessageNames(events));
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);

  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, overscroll_source());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->completed_mode());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, overscroll_delegate()->completed_mode());
  EXPECT_EQ("GestureScrollEnd", GetMessageNames(events));
  ReleaseAndResetDispatchedMessages();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Check that when accessibility virtual keyboard is enabled, windows are
// shifted up when focused and restored when focus is lost.
TEST_F(RenderWidgetHostViewAuraTest, VirtualKeyboardFocusEnsureCaretInRect) {
  // TODO (oshima): Test that overscroll occurs.

  InitViewForFrame(nullptr);
  aura::Window* root_window = parent_view_->GetNativeView()->GetRootWindow();
  aura::client::ParentWindowWithContext(view_->GetNativeView(), root_window,
                                        gfx::Rect(),
                                        display::kInvalidDisplayId);

  const gfx::Rect orig_view_bounds = gfx::Rect(0, 300, 400, 200);
  const gfx::Rect shifted_view_bounds = gfx::Rect(0, 200, 400, 200);
  const gfx::Rect root_bounds = root_window->bounds();
  const int keyboard_height = 200;
  const gfx::Rect keyboard_view_bounds =
      gfx::Rect(0, root_bounds.height() - keyboard_height, root_bounds.width(),
                keyboard_height);

  ui::InputMethod* input_method = root_window->GetHost()->GetInputMethod();

  // Focus the window.
  view_->SetBounds(orig_view_bounds);
  input_method->SetFocusedTextInputClient(view_);
  EXPECT_EQ(view_->GetNativeView()->bounds(), orig_view_bounds);

  // Simulate virtual keyboard.
  input_method->SetVirtualKeyboardBounds(keyboard_view_bounds);

  // Window should be shifted.
  EXPECT_EQ(view_->GetNativeView()->bounds(), shifted_view_bounds);

  // Detach the RenderWidgetHostViewAura from the IME.
  view_->DetachFromInputMethod(false);

  // Window should be restored.
  EXPECT_EQ(view_->GetNativeView()->bounds(), orig_view_bounds);
}

// Check that the window insets is updated with the bounds changed when the
// virtual keyboard is shown.
TEST_F(RenderWidgetHostViewAuraTest, UpdateInsetsWithVirtualKeyboardEnabled) {
  InitViewForFrame(nullptr);
  aura::Window* root_window = parent_view_->GetNativeView()->GetRootWindow();
  aura::client::ParentWindowWithContext(view_->GetNativeView(), root_window,
                                        gfx::Rect(),
                                        display::kInvalidDisplayId);

  const gfx::Rect orig_view_bounds = gfx::Rect(0, 300, 400, 200);
  const gfx::Rect shifted_view_bounds = gfx::Rect(0, 200, 400, 200);
  const gfx::Rect moved_view_bounds = gfx::Rect(100, 250, 400, 200);
  const gfx::Rect resized_view_bounds = gfx::Rect(100, 250, 300, 175);

  const auto origin_view_insets = gfx::Insets::TLBR(0, 0, 100, 0);
  const auto shifted_view_insets = gfx::Insets();
  const auto moved_view_insets = gfx::Insets::TLBR(0, 0, 50, 0);
  const auto resized_view_insets = gfx::Insets::TLBR(0, 0, 25, 0);

  const gfx::Rect root_bounds = root_window->bounds();
  const int keyboard_height = 200;
  const gfx::Rect keyboard_view_bounds =
      gfx::Rect(0, root_bounds.height() - keyboard_height, root_bounds.width(),
                keyboard_height);

  ui::InputMethod* input_method = root_window->GetHost()->GetInputMethod();

  // Focus the window.
  view_->SetBounds(orig_view_bounds);
  input_method->SetFocusedTextInputClient(view_);
  EXPECT_EQ(view_->GetNativeView()->bounds(), orig_view_bounds);

  // Simulate virtual keyboard. For chrome browser window, the window insets
  // will be changed.
  view_->SetInsets(gfx::Insets::TLBR(
      0, 0,
      gfx::IntersectRects(orig_view_bounds, keyboard_view_bounds).height(), 0));
  EXPECT_EQ(view_->insets_, origin_view_insets);
  input_method->SetVirtualKeyboardBounds(keyboard_view_bounds);

  // Window should be shifted. The insets will be updated.
  EXPECT_EQ(view_->GetNativeView()->bounds(), shifted_view_bounds);
  EXPECT_EQ(view_->insets_, shifted_view_insets);

  // Move the view and the insets will be updated.
  view_->SetBounds(moved_view_bounds);
  EXPECT_EQ(view_->insets_, moved_view_insets);

  // Resize the view and the insets will be updated.
  view_->SetBounds(resized_view_bounds);
  EXPECT_EQ(view_->insets_, resized_view_insets);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that invalid touch events are consumed and handled
// synchronously.
TEST_F(RenderWidgetHostViewAuraTest,
       InvalidEventsHaveSyncHandlingDisabled) {
  InitViewForFrame(nullptr);
  view_->Show();

  auto touch_event_consumers = blink::mojom::TouchEventConsumers::New(
      HasTouchEventHandlers(true), HasHitTestableScrollbar(false));
  widget_host_->SetHasTouchEventConsumers(std::move(touch_event_consumers));

  ui::TouchEvent press(ui::EventType::kTouchPressed, gfx::Point(30, 30),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  // Construct a move with a touch id which doesn't exist.
  ui::TouchEvent invalid_move(
      ui::EventType::kTouchMoved, gfx::Point(30, 30), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  // Valid press is handled asynchronously.
  view_->OnTouchEvent(&press);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(press.synchronous_handling_disabled());
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ(1U, events.size());
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Invalid move is handled synchronously, but is consumed. It should not
  // be forwarded to the renderer.
  view_->OnTouchEvent(&invalid_move);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());
  EXPECT_FALSE(invalid_move.synchronous_handling_disabled());
  EXPECT_TRUE(invalid_move.stopped_propagation());
}

// Checks key event codes.
TEST_F(RenderWidgetHostViewAuraTest, KeyEvent) {
  InitViewForFrame(nullptr);
  view_->Show();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                         ui::DomCode::US_A, ui::EF_NONE);
  view_->OnKeyEvent(&key_event);

  const input::NativeWebKeyboardEvent* event = delegates_.back()->last_event();
  ASSERT_TRUE(event);
  EXPECT_EQ(key_event.key_code(), event->windows_key_code);
  EXPECT_EQ(ui::KeycodeConverter::DomCodeToNativeKeycode(key_event.code()),
            event->native_key_code);
}

TEST_F(RenderWidgetHostViewAuraTest, KeyEventsHandled) {
  InitViewForFrame(nullptr);
  view_->Show();

  ui::KeyEvent key_event1(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  view_->OnKeyEvent(&key_event1);
  // Normally event should be handled.
  EXPECT_TRUE(key_event1.handled());

  ASSERT_FALSE(delegates_.empty());
  // Make the delegate mark the event as not-handled.
  delegates_.back()->set_pre_handle_keyboard_event_result(
      KeyboardEventProcessingResult::HANDLED_DONT_UPDATE_EVENT);
  ui::KeyEvent key_event2(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  view_->OnKeyEvent(&key_event2);
  EXPECT_FALSE(key_event2.handled());
}

TEST_F(RenderWidgetHostViewAuraTest, SetCanScrollForWebMouseWheelEvent) {
  InitViewForFrame(nullptr);
  view_->Show();

  sink_->ClearMessages();

  // Simulates the mouse wheel event with ctrl modifier applied.
  ui::MouseWheelEvent event(gfx::Vector2d(1, 1), gfx::Point(), gfx::Point(),
                            ui::EventTimeForNow(), ui::EF_CONTROL_DOWN, 0);
  view_->OnMouseEvent(&event);
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[0]->ToEvent()->Event()->Event());
  // Check if scroll is caused when ctrl-scroll is generated from
  // mouse wheel event.
  EXPECT_EQ(blink::WebMouseWheelEvent::EventAction::kPageZoom,
            wheel_event->event_action);

  // Ack'ing the outstanding event should flush the pending event queue.
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Simulates the mouse wheel event with no modifier applied.
  event = ui::MouseWheelEvent(gfx::Vector2d(1, 1), gfx::Point(), gfx::Point(),
                              ui::EventTimeForNow(), ui::EF_NONE, 0);

  view_->OnMouseEvent(&event);
  base::RunLoop().RunUntilIdle();

  events = GetAndResetDispatchedMessages();
  // Since the modifiers has changed a wheel end event will be sent before
  // dispatching the wheel event.
  EXPECT_EQ(2u, events.size());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);

  // Check if scroll is caused when no modifier is applied to the
  // mouse wheel event.
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_NE(blink::WebMouseWheelEvent::EventAction::kPageZoom,
            wheel_event->event_action);

  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Simulates the scroll event with ctrl modifier applied.
  ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(2, 2),
                         ui::EventTimeForNow(), ui::EF_CONTROL_DOWN, 0, 5, 0, 5,
                         2);
  view_->OnScrollEvent(&scroll);
  base::RunLoop().RunUntilIdle();

  events = GetAndResetDispatchedMessages();
  // Since the modifiers has changed a wheel end event will be sent before
  // dispatching the wheel event.
  EXPECT_EQ(2u, events.size());
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[0]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebMouseWheelEvent::kPhaseEnded, wheel_event->phase);
  // Check if scroll is caused when ctrl-touchpad-scroll is generated
  // from scroll event.
  wheel_event = static_cast<const WebMouseWheelEvent*>(
      &events[1]->ToEvent()->Event()->Event());
  EXPECT_NE(blink::WebMouseWheelEvent::EventAction::kPageZoom,
            wheel_event->event_action);
}

// Ensures that the mapping from ui::TouchEvent to blink::WebTouchEvent doesn't
// lose track of the number of acks required.
TEST_F(RenderWidgetHostViewAuraTest, CorrectNumberOfAcksAreDispatched) {
  InitViewForFrame(parent_view_->GetNativeView());
  view_->Focus();
  view_->Show();
  view_->UseFakeDispatcher();

  ui::TouchEvent press1(ui::EventType::kTouchPressed, gfx::Point(30, 30),
                        ui::EventTimeForNow(),
                        ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  view_->OnTouchEvent(&press1);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("SetFocus TouchStart", GetMessageNames(events));
  events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  ui::TouchEvent press2(ui::EventType::kTouchPressed, gfx::Point(20, 20),
                        ui::EventTimeForNow(),
                        ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  view_->OnTouchEvent(&press2);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(1u, events.size());
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  EXPECT_EQ(2U, view_->dispatcher_->GetAndResetProcessedTouchEventCount());
}

// Tests that the scroll deltas stored within the overscroll controller get
// reset at the end of the overscroll gesture even if the overscroll threshold
// isn't surpassed and the overscroll mode stays OVERSCROLL_NONE.
TEST_F(RenderWidgetHostViewAuraOverscrollTest, ScrollDeltasResetOnEnd) {
  SetUpOverscrollEnvironment();

  PressAndSetTouchActionAuto();
  // Wheel event scroll ending with mouse move.
  SimulateWheelEvent(-30, -10, 0, true,
                     WebMouseWheelEvent::kPhaseBegan);  // sent directly
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  SendNotConsumedAcks(events);
  events = GetAndResetDispatchedMessages();
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(-30.f, overscroll_delta_x());
  EXPECT_EQ(-10.f, overscroll_delta_y());
  SimulateMouseMove(5, 10, 0);
  EXPECT_EQ(0.f, overscroll_delta_x());
  EXPECT_EQ(0.f, overscroll_delta_y());

  // A wheel event with phase ended is sent before a GSB with touchscreen
  // device.
  SimulateWheelEvent(0, 0, 0, true, WebMouseWheelEvent::kPhaseEnded);
  events = GetAndResetDispatchedMessages();
  SendNotConsumedAcks(events);

  // Scroll gesture.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SimulateGestureScrollUpdateEvent(-30, -5, 0);
  events = GetAndResetDispatchedMessages();
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(-30.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0.f, overscroll_delta_x());
  EXPECT_EQ(0.f, overscroll_delta_y());
  events = GetAndResetDispatchedMessages();
  SendNotConsumedAcks(events);
  ReleaseAndResetDispatchedMessages();

  // Wheel event scroll ending with a fling. This is the first wheel event after
  // touchscreen scrolling ends so it will have phase = kPhaseBegan.
  SimulateWheelEvent(5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  // ACK the MouseWheel event
  events = GetAndResetDispatchedMessages();
  SendNotConsumedAcks(events);

  events = GetAndResetDispatchedMessages();
  SendScrollBeginAckIfNeeded(events,
                             blink::mojom::InputEventResultState::kConsumed);
  SendScrollUpdateAck(events,
                      blink::mojom::InputEventResultState::kNotConsumed);

  SimulateWheelEvent(10, -5, 0, true, WebMouseWheelEvent::kPhaseChanged);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel GestureScrollUpdate", GetMessageNames(events));

  SendNotConsumedAcks(events);
  EXPECT_EQ(OVERSCROLL_NONE, overscroll_mode());
  EXPECT_EQ(OverscrollSource::NONE, overscroll_source());
  EXPECT_EQ(15.f, overscroll_delta_x());
  EXPECT_EQ(-5.f, overscroll_delta_y());
  SimulateGestureFlingStartEvent(0.f, 0.1f, blink::WebGestureDevice::kTouchpad);
  // Fling controller handles GFS with touchpad source and the event doesn't get
  // queued in gesture event queue.
  EXPECT_EQ(0U, events.size());

  base::TimeTicks progress_time =
      base::TimeTicks::Now() + base::Milliseconds(17);
  // Overscroll delta will get reset at the end of the fling progress.
  while (overscroll_delta_y() != 0.f) {
    widget_host_->ProgressFlingIfNeeded(progress_time);
    progress_time += base::Milliseconds(17);
  }
}

TEST_F(RenderWidgetHostViewAuraTest, ForwardMouseEvent) {
  aura::Window* root = parent_view_->GetNativeView()->GetRootWindow();

  // Set up test delegate and window hierarchy.
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> parent(new aura::Window(&delegate));
  parent->Init(ui::LAYER_TEXTURED);
  root->AddChild(parent.get());
  InitViewForFrame(parent.get());

  // Simulate mouse events, ensure they are forwarded to delegate.
  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("1 0", delegate.GetMouseButtonCountsAndReset());

  // Simulate mouse events, ensure they are forwarded to delegate.
  mouse_event = ui::MouseEvent(ui::EventType::kMouseMoved, gfx::Point(1, 1),
                               gfx::Point(), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("0 1 0", delegate.GetMouseMotionCountsAndReset());

  // Lock the mouse, simulate, and ensure they are forwarded.
  view_->LockPointer(false /* request_unadjusted_movement */);

  mouse_event =
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("1 0", delegate.GetMouseButtonCountsAndReset());

  mouse_event = ui::MouseEvent(ui::EventType::kMouseMoved, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(), 0, 0);
  view_->OnMouseEvent(&mouse_event);
  EXPECT_EQ("0 1 0", delegate.GetMouseMotionCountsAndReset());

  view_->UnlockPointer();

  // view_ will be destroyed when parent is destroyed.
  view_ = nullptr;
}

#if BUILDFLAG(IS_WIN)
class MockWindowEventTarget : public ui::WindowEventTarget {
 public:
  MockWindowEventTarget() = default;

  MockWindowEventTarget(const MockWindowEventTarget&) = delete;
  MockWindowEventTarget& operator=(const MockWindowEventTarget&) = delete;

  LRESULT HandleMouseMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override {
    return S_OK;
  }

  LRESULT HandlePointerMessage(unsigned int message,
                               WPARAM w_param,
                               LPARAM l_param,
                               bool* handled) override {
    return S_OK;
  }

  LRESULT HandleKeyboardMessage(unsigned int message,
                                WPARAM w_param,
                                LPARAM l_param,
                                bool* handled) override {
    return S_OK;
  }

  LRESULT HandleTouchMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override {
    return S_OK;
  }

  LRESULT HandleInputMessage(unsigned int message,
                             WPARAM w_param,
                             LPARAM l_param,
                             bool* handled) override {
    return S_OK;
  }

  LRESULT HandleScrollMessage(unsigned int message,
                              WPARAM w_param,
                              LPARAM l_param,
                              bool* handled) override {
    return S_OK;
  }

  LRESULT HandleNcHitTestMessage(unsigned int message,
                                 WPARAM w_param,
                                 LPARAM l_param,
                                 bool* handled) override {
    return S_OK;
  }

  void HandleParentChanged() override {}
  void ApplyPinchZoomScale(float scale) override {}
  void ApplyPinchZoomBegin() override {}
  void ApplyPinchZoomEnd() override {}
  void ApplyPanGestureScroll(int scroll_x, int scroll_y) override {}
  void ApplyPanGestureFling(int scroll_x, int scroll_y) override {}
  void ApplyPanGestureScrollBegin(int scroll_x, int scroll_y) override {}
  void ApplyPanGestureFlingBegin() override {}
  void ApplyPanGestureFlingEnd() override {}
  void ApplyPanGestureScrollEnd(bool tranisitioning_to_pinch) override {}
};

// On Windows, a native HWND (Chrome_RenderWidgetHostHWND) forwards mouse events
// to the browser window so that they can reach the tooltip controller. Since we
// reparent this HWND when the view is occluded, some mouse exits might not be
// forwarded, resulting in stuck tooltips. Test that tooltips are cleared.
TEST_F(RenderWidgetHostViewAuraTest, OcclusionHidesTooltip) {
  // Give the host window an event target, which allows the view to create the
  // LegacyRenderWidgetHostHWND Chrome_RenderWidgetHostHWND window.
  MockWindowEventTarget event_target;
  auto prop_window_target = std::make_unique<ui::ViewProp>(
      parent_view_->GetHostWindowHWND(),
      ui::WindowEventTarget::kWin32InputEventTarget,
      static_cast<ui::WindowEventTarget*>(&event_target));

  // Initialize the view.
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->Show();
  EXPECT_TRUE(legacy_render_widget_host_HWND());

  // Simulate a tooltip.
  std::u16string tooltip_text(u"The tooltip!");
  view_->UpdateTooltipUnderCursor(tooltip_text);
  EXPECT_FALSE(widget_host_->is_hidden());
  EXPECT_EQ(tooltip_text, view_->tooltip_);

  // Simulate occlusion, which should clear the tooltip.
  view_->WasOccluded();
  EXPECT_TRUE(widget_host_->is_hidden());
  EXPECT_EQ(std::u16string(), view_->tooltip_);
}

TEST_F(RenderWidgetHostViewAuraTest, LegacyRenderWidgetHostHWNDAuraLookup) {
  // Give the host window an event target, which allows the view to create the
  // LegacyRenderWidgetHostHWND Chrome_RenderWidgetHostHWND window.
  MockWindowEventTarget event_target;
  auto prop_window_target = std::make_unique<ui::ViewProp>(
      parent_view_->GetHostWindowHWND(),
      ui::WindowEventTarget::kWin32InputEventTarget,
      static_cast<ui::WindowEventTarget*>(&event_target));

  // Initialize the view.
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->Show();

  ASSERT_TRUE(legacy_render_widget_host_HWND());
  HWND hwnd = legacy_render_widget_host_HWND()->hwnd();
  EXPECT_TRUE(hwnd);
  auto* window_tree_host = aura::WindowTreeHost::GetForAcceleratedWidget(hwnd);
  EXPECT_TRUE(window_tree_host);
  EXPECT_EQ(view_->GetNativeView()->GetHost(), window_tree_host);
}
#endif

// Test that we elide touchpad pinch gesture steams consisting of only begin
// and end events.
TEST_F(RenderWidgetHostViewAuraTest, ElideEmptyTouchpadPinchSequence) {
  ui::GestureEventDetails begin_details(ui::EventType::kGesturePinchBegin);
  begin_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent begin_event(0, 0, 0, ui::EventTimeForNow(), begin_details);

  ui::GestureEventDetails update_details(ui::EventType::kGesturePinchUpdate);
  update_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  update_details.set_scale(1.23);
  ui::GestureEvent update_event(0, 0, 0, ui::EventTimeForNow(), update_details);

  ui::GestureEventDetails end_details(ui::EventType::kGesturePinchEnd);
  end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent end_event(0, 0, 0, ui::EventTimeForNow(), end_details);

  view_->OnGestureEvent(&begin_event);
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  // Since we don't know if we'll have GesturePinchUpdates at this point, the
  // GesturePinchBegin should not be sent yet.
  EXPECT_EQ(0U, events.size());

  view_->OnGestureEvent(&update_event);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));

  // If the page consumes the update, then no GesturePinchUpdate is sent and
  // we continue to postpone sending the GesturePinchBegin.
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(0U, events.size());

  view_->OnGestureEvent(&end_event);
  base::RunLoop().RunUntilIdle();
  events = GetAndResetDispatchedMessages();
  // Since we have not sent any GesturePinchUpdates by the time we get to the
  // end of the pinch, the GesturePinchBegin and GesturePinchEnd events should
  // be elided.
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
}

TEST_F(RenderWidgetHostViewAuraTest,
       TouchpadScrollThenPinchFiresImmediateScrollEnd) {
  // Set the max_time_between_phase_ended_and_momentum_phase_began timer
  // timeout to a large value to make sure that the timer is still running
  // when the wheel event with phase == end is sent.
  view_->event_handler()
      ->set_max_time_between_phase_ended_and_momentum_phase_began(
          TestTimeouts::action_max_timeout());

  InitViewForFrame(nullptr);
  view_->Show();
  sink_->ClearMessages();

  ui::ScrollEvent begin_scroll(
      ui::EventType::kScroll, gfx::Point(2, 2), ui::EventTimeForNow(), 0, 2, 2,
      2, 2, 2, ui::EventMomentumPhase::NONE, ui::ScrollEventPhase::kBegan);
  view_->OnScrollEvent(&begin_scroll);
  base::RunLoop().RunUntilIdle();

  // If a pinch is coming next, then a ScrollEvent is created with
  // momentum_phase == BLOCKED so that the end phase event can be dispatched
  // immediately, rather than scheduling for later dispatch.
  ui::ScrollEvent end_scroll_with_pinch_next(
      ui::EventType::kScroll, gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0, 0,
      0, 0, 2, ui::EventMomentumPhase::BLOCKED, ui::ScrollEventPhase::kEnd);
  view_->OnScrollEvent(&end_scroll_with_pinch_next);
  base::RunLoop().RunUntilIdle();

  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(5U, events.size());
  EXPECT_EQ(
      "GestureScrollBegin GestureScrollUpdate MouseWheel GestureScrollEnd "
      "MouseWheel",
      GetMessageNames(events));
  EXPECT_FALSE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());

  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(
          &events[4]->ToEvent()->Event()->Event());
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseBlocked,
            wheel_event->momentum_phase);

  // Now, try the same thing as above, but without knowing if pinch is next.
  ui::ScrollEvent begin_scroll2(
      ui::EventType::kScroll, gfx::Point(2, 2), ui::EventTimeForNow(), 0, 2, 2,
      2, 2, 2, ui::EventMomentumPhase::NONE, ui::ScrollEventPhase::kBegan);
  view_->OnScrollEvent(&begin_scroll2);
  base::RunLoop().RunUntilIdle();

  // If its unknown what is coming next, set the event momentum_phase to NONE.
  // This results in the phase end event being scheduled for dispatch, but not
  // ultimately dispatched in this test.
  ui::ScrollEvent end_scroll_with_momentum_next_maybe(
      ui::EventType::kScroll, gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0, 0,
      0, 0, 2, ui::EventMomentumPhase::NONE, ui::ScrollEventPhase::kEnd);
  view_->OnScrollEvent(&end_scroll_with_momentum_next_maybe);
  base::RunLoop().RunUntilIdle();

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ("MouseWheel", GetMessageNames(events));
  events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(4U, events.size());
  EXPECT_EQ(
      "GestureScrollBegin GestureScrollUpdate MouseWheel GestureScrollEnd",
      GetMessageNames(events));
  EXPECT_TRUE(GetMouseWheelPhaseHandler()->HasPendingWheelEndEvent());
}

TEST_F(RenderWidgetHostViewAuraTest, GestureTapFromStylusHasPointerType) {
  // TODO(flackr): This test fails as the gesture events are not generated
  // unless the `view_` is parented directly to the root window.
  InitViewForFrame(parent_view_->GetNativeView()->GetRootWindow());
  view_->Focus();
  view_->Show();

  aura::Window* root = view_->GetNativeView()->GetRootWindow();
  root->SetTargetHandler(view_);

  ui::test::EventGenerator generator(root, root->bounds().CenterPoint());

  // Simulate touch press and release to generate a GestureTap.
  generator.EnterPenPointerMode();
  generator.PressTouch();
  widget_host_->SetTouchActionFromMain(cc::TouchAction::kAuto);
  generator.ReleaseTouch();
  base::RunLoop().RunUntilIdle();
  MockWidgetInputHandler::MessageVector events =
      GetAndResetDispatchedMessages();
  EXPECT_EQ("SetFocus TouchStart TouchEnd", GetMessageNames(events));
  SendNotConsumedAcks(events);

  // GestureTap event should have correct pointer type.
  events = GetAndResetDispatchedMessages();
  EXPECT_EQ(
      "GestureBegin GestureTapDown GestureShowPress GestureTap GestureEnd",
      GetMessageNames(events));
  const WebGestureEvent* gesture_event = static_cast<const WebGestureEvent*>(
      &events[3]->ToEvent()->Event()->Event());
  EXPECT_EQ(WebInputEvent::Type::kGestureTap, gesture_event->GetType());
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kPen,
            gesture_event->primary_pointer_type);
}

// Test that the rendering timeout for newly loaded content fires when enough
// time passes without receiving a new compositor frame.
// TODO(crbug.com/40775652): This test is flaky on "Linux ASan LSan Tests
// (1)"
#if BUILDFLAG(IS_LINUX)
#define MAYBE_NewContentRenderingTimeout DISABLED_NewContentRenderingTimeout
#else
#define MAYBE_NewContentRenderingTimeout NewContentRenderingTimeout
#endif
TEST_F(RenderWidgetHostViewAuraTest, MAYBE_NewContentRenderingTimeout) {
  constexpr base::TimeDelta kTimeout = base::Microseconds(10);

  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  widget_host_->set_new_content_rendering_delay_for_testing(kTimeout);

  viz::LocalSurfaceId id0 = view_->GetLocalSurfaceId();
  EXPECT_TRUE(id0.is_valid());

  // No LocalSurfaceId will be allocated if the view is hidden during
  // naviagtion.
  view_->Show();
  // No new LocalSurfaceId should be allocated for the first navigation and the
  // timer should not fire.
  widget_host_->DidNavigate();
  widget_host_->StartNewContentRenderingTimeout();
  viz::LocalSurfaceId id1 = view_->GetLocalSurfaceId();
  EXPECT_EQ(id0, id1);
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), 2 * kTimeout);
    run_loop.Run();
  }

  EXPECT_TRUE(widget_host_->new_content_rendering_timeout_fired());
  widget_host_->reset_new_content_rendering_timeout_fired();

  // Start the timer. Verify that a new LocalSurfaceId is allocated.
  widget_host_->DidNavigate();
  widget_host_->StartNewContentRenderingTimeout();
  viz::LocalSurfaceId id2 = view_->GetLocalSurfaceId();
  EXPECT_TRUE(id2.is_valid());
  EXPECT_LT(id1.parent_sequence_number(), id2.parent_sequence_number());

  // The renderer submits a frame to the old LocalSurfaceId. The timer should
  // still fire.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), 2 * kTimeout);
    run_loop.Run();
  }
  EXPECT_TRUE(widget_host_->new_content_rendering_timeout_fired());
  widget_host_->reset_new_content_rendering_timeout_fired();
}

// If a tab is evicted, allocate a new LocalSurfaceId next time it's shown.
TEST_F(RenderWidgetHostViewAuraTest, AllocateLocalSurfaceIdOnEviction) {
  InitViewForFrame(nullptr);
  // View has to not be empty in order for frame eviction to be invoked.
  view_->SetSize(gfx::Size(54, 32));
  ParentHostView(view_, parent_view_);
  view_->Show();
  viz::LocalSurfaceId id1 = view_->GetLocalSurfaceId();
  view_->Hide();
  auto* dfh = view_->delegated_frame_host_.get();
  static_cast<viz::FrameEvictorClient*>(dfh)->EvictDelegatedFrame(
      dfh->GetFrameEvictorForTesting()->CollectSurfaceIdsForEviction());
  view_->Show();
  viz::LocalSurfaceId id2 = view_->GetLocalSurfaceId();
  EXPECT_NE(id1, id2);
}

// If a tab was resized while it's hidden, drop the fallback so next time it's
// visible we show blank.
TEST_F(RenderWidgetHostViewAuraTest, DropFallbackIfResizedWhileHidden) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->SetSize(gfx::Size(50, 30));
  view_->Show();
  view_->Hide();
  view_->SetSize(gfx::Size(54, 32));
  view_->Show();
  ASSERT_TRUE(view_->window_->layer()->GetOldestAcceptableFallback());
  EXPECT_EQ(*view_->window_->layer()->GetOldestAcceptableFallback(),
            *view_->window_->layer()->GetSurfaceId());
}

// If a tab is hidden and shown without being resized in the meantime, the
// fallback SurfaceId has to be preserved.
TEST_F(RenderWidgetHostViewAuraTest, DontDropFallbackIfNotResizedWhileHidden) {
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->Show();
  // Force fallback being set.
  view_->DidNavigate();
  view_->ResetFallbackToFirstNavigationSurface();
  ASSERT_TRUE(view_->window_->layer()->GetOldestAcceptableFallback());
  viz::SurfaceId fallback =
      *view_->window_->layer()->GetOldestAcceptableFallback();
  view_->Hide();
  view_->Show();
  ASSERT_TRUE(view_->window_->layer()->GetOldestAcceptableFallback());
  EXPECT_EQ(fallback, *view_->window_->layer()->GetSurfaceId());
}

// Check that TakeFallbackContentFrom() copies the fallback SurfaceId and
// background color from the previous view to the new view.
TEST_F(RenderWidgetHostViewAuraTest, TakeFallbackContent) {
  // Initialize the first view.
  InitViewForFrame(nullptr);
  ParentHostView(view_, parent_view_);
  view_->Show();

  // Create and initialize the second view.
  FakeRenderWidgetHostViewAura* view2 = CreateView();
  view2->InitAsChild(nullptr);
  ParentHostView(view2, parent_view_);

  // Call TakeFallbackContentFrom(). The second view should obtain a fallback
  // from the first view.
  view2->TakeFallbackContentFrom(view_);
  EXPECT_EQ(view_->window_->layer()->GetSurfaceId()->ToSmallestId(),
            *view2->window_->layer()->GetOldestAcceptableFallback());

  DestroyView(view2);
}

// Check that TakeFallbackContentFrom() copies the fallback SurfaceId and
// background color from the previous view to the new view if the new view is
// for a pre-rendered page which is loaded as hidden.
TEST_F(RenderWidgetHostViewAuraTest, TakeFallbackContentForPrerender) {
  FakeRenderWidgetHostViewAura* old_view = CreateView(/*hidden = */ false);
  old_view->InitAsChild(nullptr);
  ParentHostView(old_view, parent_view_);
  old_view->Show();
  ASSERT_TRUE(old_view->IsShowing());
  ASSERT_TRUE(
      old_view->delegated_frame_host_client_->DelegatedFrameHostIsVisible());
  old_view->SetSize(gfx::Size(50, 50));

  // Initialize the view as hidden.
  FakeRenderWidgetHostViewAura* prerender_view = CreateView(/*hidden = */ true);
  prerender_view->InitAsChild(nullptr);
  ParentHostView(prerender_view, parent_view_);

  ASSERT_FALSE(prerender_view->IsShowing());
  ASSERT_FALSE(prerender_view->delegated_frame_host_client_
                   ->DelegatedFrameHostIsVisible());
  prerender_view->SetSize(gfx::Size(50, 50));
  ASSERT_FALSE(prerender_view->window_->layer()->GetOldestAcceptableFallback());

  prerender_view->TakeFallbackContentFrom(old_view);
  ASSERT_TRUE(prerender_view->window_->layer()->GetOldestAcceptableFallback());
  EXPECT_EQ(old_view->window_->layer()->GetSurfaceId()->ToSmallestId(),
            *(prerender_view->window_->layer()->GetOldestAcceptableFallback()));

  DestroyView(prerender_view);
  DestroyView(old_view);
}

// This class provides functionality to test a RenderWidgetHostViewAura
// instance which has been hooked up to a test RenderViewHost instance and
// a WebContents instance.
class RenderWidgetHostViewAuraWithViewHarnessTest
    : public RenderViewHostImplTestHarness {
 public:
   RenderWidgetHostViewAuraWithViewHarnessTest()
      : view_(nullptr) {}

   RenderWidgetHostViewAuraWithViewHarnessTest(
       const RenderWidgetHostViewAuraWithViewHarnessTest&) = delete;
   RenderWidgetHostViewAuraWithViewHarnessTest& operator=(
       const RenderWidgetHostViewAuraWithViewHarnessTest&) = delete;

   ~RenderWidgetHostViewAuraWithViewHarnessTest() override {}

 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // Delete the current RenderWidgetHostView instance before setting
    // the RWHVA as the view.
    contents()->GetRenderViewHost()->GetWidget()->GetView()->Destroy();
    // This instance is destroyed in the TearDown method below.
    view_ = new RenderWidgetHostViewAura(
        contents()->GetRenderViewHost()->GetWidget());
  }

  void TearDown() override {
    view_.ExtractAsDangling()->Destroy();
    RenderViewHostImplTestHarness::TearDown();
  }

  RenderWidgetHostViewAura* view() { return view_; }

 private:
  // The `view_` pointer owns the object.
  // Note: It would be great turning this into a std::unique_ptr<>.
  raw_ptr<RenderWidgetHostViewAura> view_;
};

// Provides a mock implementation of the WebContentsViewDelegate class.
// Currently provides functionality to validate the ShowContextMenu
// callback.
class MockWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  MockWebContentsViewDelegate()
      : context_menu_request_received_(false) {}

  MockWebContentsViewDelegate(const MockWebContentsViewDelegate&) = delete;
  MockWebContentsViewDelegate& operator=(const MockWebContentsViewDelegate&) =
      delete;

  ~MockWebContentsViewDelegate() override {}

  bool context_menu_request_received() const {
    return context_menu_request_received_;
  }

  ui::MenuSourceType context_menu_source_type() const {
    return context_menu_params_.source_type;
  }

  // WebContentsViewDelegate overrides.
  void ShowContextMenu(RenderFrameHost& render_frame_host,
                       const ContextMenuParams& params) override {
    context_menu_request_received_ = true;
    context_menu_params_ = params;
  }

  void ClearState() {
    context_menu_request_received_ = false;
    context_menu_params_.source_type = ui::MENU_SOURCE_NONE;
  }

 private:
  bool context_menu_request_received_;
  ContextMenuParams context_menu_params_;
};

// On Windows we don't want the context menu to be displayed in the context of
// a long press gesture. It should be displayed when the touch is released.
// On other platforms we should display the context menu in the long press
// gesture.
// This test validates this behavior.
TEST_F(RenderWidgetHostViewAuraWithViewHarnessTest,
       ContextMenuTest) {
  auto delegate = std::make_unique<MockWebContentsViewDelegate>();
  MockWebContentsViewDelegate* delegate_ptr = delegate.get();
  static_cast<WebContentsViewAura*>(contents()->GetView())
      ->SetDelegateForTesting(std::move(delegate));

  RenderViewHostFactory::set_is_real_render_view_host(true);

  // A context menu request with the MENU_SOURCE_MOUSE source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called. This means that the request worked correctly.
  ContextMenuParams context_menu_params;
  context_menu_params.source_type = ui::MENU_SOURCE_MOUSE;
  contents()->ShowContextMenu(
      *contents()->GetRenderViewHost()->GetMainRenderFrameHost(),
      mojo::NullAssociatedRemote(), context_menu_params);
  EXPECT_TRUE(delegate_ptr->context_menu_request_received());
  EXPECT_EQ(delegate_ptr->context_menu_source_type(), ui::MENU_SOURCE_MOUSE);

  // A context menu request with the MENU_SOURCE_TOUCH source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called on all platforms. This means that the request worked
  // correctly.
  delegate_ptr->ClearState();
  context_menu_params.source_type = ui::MENU_SOURCE_TOUCH;
  contents()->ShowContextMenu(
      *contents()->GetRenderViewHost()->GetMainRenderFrameHost(),
      mojo::NullAssociatedRemote(), context_menu_params);
  EXPECT_TRUE(delegate_ptr->context_menu_request_received());

  // A context menu request with the MENU_SOURCE_LONG_TAP source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called on all platforms. This means that the request worked
  // correctly.
  delegate_ptr->ClearState();
  context_menu_params.source_type = ui::MENU_SOURCE_LONG_TAP;
  contents()->ShowContextMenu(
      *contents()->GetRenderViewHost()->GetMainRenderFrameHost(),
      mojo::NullAssociatedRemote(), context_menu_params);
  EXPECT_TRUE(delegate_ptr->context_menu_request_received());

  // A context menu request with the MENU_SOURCE_LONG_PRESS source type should
  // result in the MockWebContentsViewDelegate::ShowContextMenu method
  // getting called on non Windows platforms. This means that the request
  //  worked correctly.
  delegate_ptr->ClearState();
  context_menu_params.source_type = ui::MENU_SOURCE_LONG_PRESS;
  contents()->ShowContextMenu(
      *contents()->GetRenderViewHost()->GetMainRenderFrameHost(),
      mojo::NullAssociatedRemote(), context_menu_params);
  EXPECT_TRUE(delegate_ptr->context_menu_request_received());

  RenderViewHostFactory::set_is_real_render_view_host(false);
}

// ----------------------------------------------------------------------------
// TextInputManager and IME-Related Tests

// The test class for OOPIF IME related unit tests in RenderWidgetHostViewAura.
// In each test, 3 views are created where one is in process with main frame and
// the other two are in distinct processes (this makes a total of 4 RWHVs).
class InputMethodAuraTestBase : public RenderWidgetHostViewAuraTest {
 public:
  InputMethodAuraTestBase() = default;

  InputMethodAuraTestBase(const InputMethodAuraTestBase&) = delete;
  InputMethodAuraTestBase& operator=(const InputMethodAuraTestBase&) = delete;

  ~InputMethodAuraTestBase() override = default;

  void SetUp() override {
    RenderWidgetHostViewAuraTest::SetUp();
    InitializeAura();

    MockRenderWidgetHostImpl* widget_host_for_process_1 =
        CreateRenderWidgetHostForSiteInstanceGroup(tab_site_instance_group());
    widget_hosts_to_cleanup_.push_back(widget_host_for_process_1);

    view_for_first_process_ = CreateViewForProcess(widget_host_for_process_1);

    second_process_host_ = CreateNewProcessHost();
    second_site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            tab_site_instance_group(), second_process_host_.get()));
    MockRenderWidgetHostImpl* widget_host_for_process_2 =
        CreateRenderWidgetHostForSiteInstanceGroup(
            second_site_instance_group_.get());
    widget_hosts_to_cleanup_.push_back(widget_host_for_process_2);
    view_for_second_process_ = CreateViewForProcess(widget_host_for_process_2);

    third_process_host_ = CreateNewProcessHost();
    third_site_instance_group_ =
        base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
            tab_site_instance_group(), third_process_host_.get()));
    MockRenderWidgetHostImpl* widget_host_for_process_3 =
        CreateRenderWidgetHostForSiteInstanceGroup(
            third_site_instance_group_.get());
    widget_hosts_to_cleanup_.push_back(widget_host_for_process_3);
    view_for_third_process_ = CreateViewForProcess(widget_host_for_process_3);

    views_.insert(views_.begin(), {
                                      tab_view(),
                                      view_for_first_process_.get(),
                                      view_for_second_process_.get(),
                                      view_for_third_process_.get(),
                                  });
    widget_hosts_.insert(widget_hosts_.begin(), {
                                                    tab_widget_host(),
                                                    widget_host_for_process_1,
                                                    widget_host_for_process_2,
                                                    widget_host_for_process_3,
                                                });
    active_view_sequence_.insert(active_view_sequence_.begin(),
                                 {0, 1, 2, 1, 1, 3, 0, 3, 1});
  }

  void TearDown() override {
    view_for_first_process_.ExtractAsDangling()->Destroy();
    view_for_second_process_.ExtractAsDangling()->Destroy();
    view_for_third_process_.ExtractAsDangling()->Destroy();

    for (content::MockRenderWidgetHostImpl* host : widget_hosts_to_cleanup_) {
      host->ShutdownAndDestroyWidget(true);
    }

    second_process_host_->Cleanup();
    third_process_host_->Cleanup();

    second_site_instance_group_.reset();
    third_site_instance_group_.reset();

    second_process_host_.reset();
    third_process_host_.reset();

    RenderWidgetHostViewAuraTest::TearDown();
  }

 protected:
  ui::TextInputClient* text_input_client() const { return view_; }

  bool has_composition_text() const {
    return tab_view()->has_composition_text_;
  }

  std::unique_ptr<MockRenderProcessHost> CreateNewProcessHost() {
    return std::make_unique<MockRenderProcessHost>(browser_context());
  }

  MockRenderWidgetHostImpl* CreateRenderWidgetHostForSiteInstanceGroup(
      SiteInstanceGroup* site_instance_group) {
    return MockRenderWidgetHostImpl::Create(
        GetFrameTree(), render_widget_host_delegate(),
        site_instance_group->GetSafeRef(),
        site_instance_group->process()->GetNextRoutingID(),
        /*hidden = */ false);
  }

  TestRenderWidgetHostView* CreateViewForProcess(
      MockRenderWidgetHostImpl* host) {
    TestRenderWidgetHostView* view = new TestRenderWidgetHostView(host);
    host->SetView(view);
    return view;
  }

  void SetHasCompositionTextToTrue() {
    ui::CompositionText composition_text;
    composition_text.text = u"text";
    tab_view()->SetCompositionText(composition_text);
    EXPECT_TRUE(has_composition_text());
  }

  MockRenderProcessHost* tab_process() const { return process_host_.get(); }

  SiteInstanceGroup* tab_site_instance_group() const {
    return site_instance_group_.get();
  }

  RenderWidgetHostViewAura* tab_view() const { return view_; }

  MockRenderWidgetHostImpl* tab_widget_host() const { return widget_host_; }

  std::vector<raw_ptr<RenderWidgetHostViewBase, VectorExperimental>> views_;
  std::vector<raw_ptr<MockRenderWidgetHostImpl, VectorExperimental>>
      widget_hosts_;
  std::vector<raw_ptr<MockRenderWidgetHostImpl, VectorExperimental>>
      widget_hosts_to_cleanup_;
  // A sequence of indices in [0, 3] which determines the index of a RWHV in
  // |views_|. This sequence is used in the tests to sequentially make a RWHV
  // active for a subsequent IME result method call.
  std::vector<size_t> active_view_sequence_;

 private:
  // This will initialize |window_| in RenderWidgetHostViewAura. It is needed
  // for RenderWidgetHostViewAura::GetInputMethod() to work.
  void InitializeAura() {
    InitViewForFrame(nullptr);
    view_->Show();
  }
  std::unique_ptr<MockRenderProcessHost> second_process_host_;
  std::unique_ptr<MockRenderProcessHost> third_process_host_;

  raw_ptr<TestRenderWidgetHostView> view_for_first_process_;
  raw_ptr<TestRenderWidgetHostView> view_for_second_process_;
  raw_ptr<TestRenderWidgetHostView> view_for_third_process_;

  scoped_refptr<SiteInstanceGroup> second_site_instance_group_;
  scoped_refptr<SiteInstanceGroup> third_site_instance_group_;
};

// A group of tests which verify that the IME method results are routed to the
// right RenderWidget when there are multiple RenderWidgetHostViews on tab. Each
// test will verify the correctness of routing for one of the IME result
// methods. The method is called on ui::TextInputClient (i.e., RWHV for the tab
// in aura) and then the test verifies that the IPC is routed to the
// RenderWidget corresponding to the active view (i.e., the RenderWidget
// with focused <input>).
class InputMethodResultAuraTest : public InputMethodAuraTestBase {
 public:
  InputMethodResultAuraTest() {}

  InputMethodResultAuraTest(const InputMethodResultAuraTest&) = delete;
  InputMethodResultAuraTest& operator=(const InputMethodResultAuraTest&) =
      delete;

  ~InputMethodResultAuraTest() override {}

 protected:
  const IPC::Message* RunAndReturnIPCSent(base::OnceClosure closure,
                                          MockRenderProcessHost* process,
                                          int32_t message_id) {
    process->sink().ClearMessages();
    std::move(closure).Run();
    return process->sink().GetFirstMessageMatching(message_id);
  }
};

// This test verifies ui::TextInputClient::SetCompositionText.
TEST_F(InputMethodResultAuraTest, SetCompositionText) {
  base::RepeatingClosure ime_call = base::BindRepeating(
      &ui::TextInputClient::SetCompositionText,
      base::Unretained(text_input_client()), ui::CompositionText());
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    ime_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("SetComposition",
              GetMessageNames(widget_hosts_[index]
                                  ->input_handler()
                                  ->GetAndResetDispatchedMessages()));
  }
}

// This test is for ui::TextInputClient::ConfirmCompositionText.
TEST_F(InputMethodResultAuraTest, ConfirmCompositionText) {
  base::RepeatingCallback<size_t()> ime_call = base::BindRepeating(
      &ui::TextInputClient::ConfirmCompositionText,
      base::Unretained(text_input_client()), /** keep_selection */ true);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    // Due to a webkit bug. See: https://bugs.webkit.org/show_bug.cgi?id=37788
    // RenderWidgetHostViewAura::SetCompositionText() will ignore the
    // selection range passed into it. Hence, RWHVA::SetCompositionText()
    // cannot be used to set the selection range.

    // RenderWidgetHostViewAura::GetFocusedFrame() does not return a focused
    // frame due to (crbug.com/689777). Hence,
    // RWHVA::SetEditableSelectionRange(gfx::Range(0, 2)) also cannot be used
    // to set the selection range.

    // Hence, there exists no easy way to set the selection range to a specific
    //  value and test the behaviour of keep_selection.
    ime_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("SetComposition FinishComposingText",
              GetMessageNames(widget_hosts_[index]
                                  ->input_handler()
                                  ->GetAndResetDispatchedMessages()));
    // TODO(keithlee) - If either of the previous bugs get fixed, amend
    // this unittest to check if the TIC::SelectionRange is updated to the
    // gfx::Range(0,2) value after the IME call.
  }
}

// This test is for ui::TextInputClient::ClearCompositionText.
TEST_F(InputMethodResultAuraTest, ClearCompositionText) {
  base::RepeatingClosure ime_call =
      base::BindRepeating(&ui::TextInputClient::ClearCompositionText,
                          base::Unretained(text_input_client()));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    ime_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("SetComposition SetComposition",
              GetMessageNames(widget_hosts_[index]
                                  ->input_handler()
                                  ->GetAndResetDispatchedMessages()));
  }
}

// This test is for ui::TextInputClient::InsertText with empty text.
TEST_F(InputMethodResultAuraTest, InsertEmptyText) {
  base::RepeatingClosure ime_call = base::BindRepeating(
      &ui::TextInputClient::InsertText, base::Unretained(text_input_client()),
      std::u16string(),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    ime_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("SetComposition CommitText",
              GetMessageNames(widget_hosts_[index]
                                  ->input_handler()
                                  ->GetAndResetDispatchedMessages()));
  }
}

// This test is for ui::TextInputClient::InsertText with non-empty text.
TEST_F(InputMethodResultAuraTest, CommitText) {
  base::RepeatingClosure ime_call = base::BindRepeating(
      &ui::TextInputClient::InsertText, base::Unretained(text_input_client()),
      u"hello",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    ime_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("CommitText",
              GetMessageNames(widget_hosts_[index]
                                  ->input_handler()
                                  ->GetAndResetDispatchedMessages()));
  }
}

TEST_F(InputMethodResultAuraTest, CommitTextWithEmptyText) {
  base::RepeatingClosure ime_call = base::BindRepeating(
      &ui::TextInputClient::InsertText, base::Unretained(text_input_client()),
      u"", ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    ime_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("CommitText",
              GetMessageNames(widget_hosts_[index]
                                  ->input_handler()
                                  ->GetAndResetDispatchedMessages()));
  }
}

TEST_F(InputMethodResultAuraTest, CommitTextBeforeCursor) {
  base::RepeatingClosure ime_call = base::BindRepeating(
      &ui::TextInputClient::InsertText, base::Unretained(text_input_client()),
      u"hello",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorBeforeText);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    ime_call.Run();
    base::RunLoop().RunUntilIdle();

    MockWidgetInputHandler::MessageVector events =
        widget_hosts_[index]->input_handler()->GetAndResetDispatchedMessages();
    EXPECT_EQ("CommitText", GetMessageNames(events));

    MockWidgetInputHandler::DispatchedIMEMessage* ime_message =
        events[0]->ToIME();
    EXPECT_TRUE(ime_message);
    EXPECT_TRUE(
        ime_message->Matches(u"hello", {}, gfx::Range::InvalidRange(), -5, -5));
  }
}

// This test is for RenderWidgetHostViewAura::FinishImeCompositionSession which
// is in response to a mouse click during an ongoing composition.
TEST_F(InputMethodResultAuraTest, FinishImeCompositionSession) {
  base::RepeatingClosure ime_finish_session_call = base::BindRepeating(
      &RenderWidgetHostViewEventHandler::FinishImeCompositionSession,
      base::Unretained(tab_view()->event_handler()));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    SetHasCompositionTextToTrue();
    ime_finish_session_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("SetComposition FinishComposingText",
              GetMessageNames(widget_hosts_[index]
                                  ->input_handler()
                                  ->GetAndResetDispatchedMessages()));
  }
}

// This test is for ui::TextInputClient::ChangeTextDirectionAndLayoutAlignment.
TEST_F(InputMethodResultAuraTest, ChangeTextDirectionAndLayoutAlignment) {
  base::RepeatingClosure ime_finish_session_call = base::BindRepeating(
      base::IgnoreResult(
          &RenderWidgetHostViewAura::ChangeTextDirectionAndLayoutAlignment),
      base::Unretained(tab_view()), base::i18n::LEFT_TO_RIGHT);
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);

    mojo::AssociatedRemote<blink::mojom::FrameWidget> frame_widget_remote;
    mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
        frame_widget_receiver =
            frame_widget_remote.BindNewEndpointAndPassDedicatedReceiver();
    static_cast<RenderWidgetHostImpl*>(views_[index]->GetRenderWidgetHost())
        ->BindFrameWidgetInterfaces(
            mojo::AssociatedRemote<blink::mojom::FrameWidgetHost>()
                .BindNewEndpointAndPassDedicatedReceiver(),
            frame_widget_remote.Unbind());

    FakeFrameWidget fake_frame_widget(std::move(frame_widget_receiver));

    ime_finish_session_call.Run();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(fake_frame_widget.GetTextDirection(), base::i18n::LEFT_TO_RIGHT);
  }
}

// A class of tests which verify the correctness of some tracked IME related
// state at the browser side. Each test verifies the correctness tracking for
// one specific state. To do so, the views are activated in a predetermined
// sequence and each time, the IPC call for the corresponding state is simulated
// through calling the method on the view. Then the test verifies that the value
// returned by the view or ui::TextInputClient is the expected value from IPC.
class InputMethodStateAuraTest : public InputMethodAuraTestBase {
 public:
  InputMethodStateAuraTest() {}

  InputMethodStateAuraTest(const InputMethodStateAuraTest&) = delete;
  InputMethodStateAuraTest& operator=(const InputMethodStateAuraTest&) = delete;

  ~InputMethodStateAuraTest() override {}

 protected:
  gfx::SelectionBound GetSelectionBoundFromRect(const gfx::Rect& rect) {
    gfx::SelectionBound bound;
    bound.SetEdge(gfx::PointF(rect.origin()), gfx::PointF(rect.bottom_left()));
    return bound;
  }

  gfx::Rect TransformRectToViewsRootCoordSpace(const gfx::Rect rect,
                                               RenderWidgetHostView* view) {
    return gfx::Rect(view->TransformPointToRootCoordSpace(rect.origin()),
                     rect.size());
  }
};

// This test is for caret bounds which are calculated based on the tracked value
// for selection bounds.
TEST_F(InputMethodStateAuraTest, GetCaretBounds) {
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    gfx::Rect anchor_rect = gfx::Rect(0, 0, 10, 10);
    gfx::Rect focus_rect = gfx::Rect(10 + index, 10 + index, 10, 10);
    views_[index]->SelectionBoundsChanged(
        anchor_rect, base::i18n::LEFT_TO_RIGHT, focus_rect,
        base::i18n::LEFT_TO_RIGHT, /*bounding_box=*/gfx::Rect(), true);

    // Calculate the bounds.
    gfx::SelectionBound anchor_bound = GetSelectionBoundFromRect(
        TransformRectToViewsRootCoordSpace(anchor_rect, views_[index]));
    gfx::SelectionBound focus_bound = GetSelectionBoundFromRect(
        TransformRectToViewsRootCoordSpace(focus_rect, views_[index]));
    anchor_bound.set_type(gfx::SelectionBound::LEFT);
    focus_bound.set_type(gfx::SelectionBound::RIGHT);
    gfx::Rect measured_rect =
        gfx::RectBetweenSelectionBounds(anchor_bound, focus_bound);

    EXPECT_EQ(measured_rect, text_input_client()->GetCaretBounds());
  }
}

// This test is for composition character bounds.
TEST_F(InputMethodStateAuraTest, GetCompositionCharacterBounds) {
  gfx::Rect bound;
  // Initially, there should be no bounds.
  EXPECT_FALSE(text_input_client()->GetCompositionCharacterBounds(0, &bound));
  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    // Simulate an IPC to set character bounds for the view.
    views_[index]->ImeCompositionRangeChanged(
        gfx::Range(), {{gfx::Rect(1, 2, 3, 4 + index)}}, std::nullopt);

    // No bounds at index 1.
    EXPECT_FALSE(text_input_client()->GetCompositionCharacterBounds(1, &bound));

    // Valid bound at index 0.
    EXPECT_TRUE(text_input_client()->GetCompositionCharacterBounds(0, &bound));
    EXPECT_EQ(4 + (int)index, bound.height());
  }
}

// This test is for selected text.
TEST_F(InputMethodStateAuraTest, GetSelectedText) {
  std::u16string text = u"some text of length 22";
  size_t offset = 0U;
  gfx::Range selection_range(20, 21);

  for (auto index : active_view_sequence_) {
    render_widget_host_delegate()->set_focused_widget(
        RenderWidgetHostImpl::From(views_[index]->GetRenderWidgetHost()));
    views_[index]->SelectionChanged(text, offset, selection_range);
    std::u16string expected_text = text.substr(
        selection_range.GetMin() - offset, selection_range.length());

    EXPECT_EQ(expected_text, views_[index]->GetSelectedText());

    // Changing offset to make sure that the next view has a different text
    // selection.
    offset++;
  }
}

// This test is for text range.
TEST_F(InputMethodStateAuraTest, GetTextRange) {
  const std::u16string text = u"some text of length 22";

  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    ui::mojom::TextInputState state;
    state.type = ui::TEXT_INPUT_TYPE_TEXT;
    state.value = text;
    gfx::Range expected_range(0, 22);
    views_[index]->TextInputStateChanged(state);
    gfx::Range range_from_client;

    // For aura this always returns true.
    EXPECT_TRUE(text_input_client()->GetTextRange(&range_from_client));
    EXPECT_EQ(expected_range, range_from_client);
  }
}

TEST_F(InputMethodStateAuraTest, GetCompositionTextRange) {
  // Initially, there should be no range.
  gfx::Range range_from_client;
  EXPECT_FALSE(
      text_input_client()->GetCompositionTextRange(&range_from_client));

  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    gfx::Range expected_range(1, 2 + index);
    ui::mojom::TextInputState state;
    state.type = ui::TEXT_INPUT_TYPE_TEXT;
    state.composition = expected_range;
    views_[index]->TextInputStateChanged(state);

    EXPECT_TRUE(
        text_input_client()->GetCompositionTextRange(&range_from_client));
    EXPECT_EQ(expected_range, range_from_client);
  }
}

// This test is for selection range.
TEST_F(InputMethodStateAuraTest, GetEditableSelectionRange) {
  gfx::Range expected_range(0U, 1U);

  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    ui::mojom::TextInputState state_with_selection;
    state_with_selection.type = ui::TEXT_INPUT_TYPE_TEXT;
    state_with_selection.selection = expected_range;
    views_[index]->TextInputStateChanged(state_with_selection);
    gfx::Range range_from_client;

    // This method always returns true.
    EXPECT_TRUE(
        text_input_client()->GetEditableSelectionRange(&range_from_client));
    EXPECT_EQ(expected_range, range_from_client);

    // Changing range to make sure that the next view has a different text
    // selection.
    expected_range.set_end(expected_range.end() + 1U);
  }
}

TEST_F(InputMethodStateAuraTest, GetTextFromRange) {
  const std::u16string text = u"some text of length 22";

  for (auto index : active_view_sequence_) {
    ActivateViewForTextInputManager(views_[index], ui::TEXT_INPUT_TYPE_TEXT);
    ui::mojom::TextInputState state;
    state.type = ui::TEXT_INPUT_TYPE_TEXT;
    state.value = text;
    views_[index]->TextInputStateChanged(state);

    gfx::Range request_range(std::min(index, text.length() - 1),
                             std::min(index + 3, text.length() - 1));
    std::u16string result;
    EXPECT_TRUE(text_input_client()->GetTextFromRange(request_range, &result));
    EXPECT_EQ(text.substr(request_range.start(), request_range.length()),
              result);
  }
}

// This test will verify that after selection, the selected text is written to
// the clipboard from the focused widget.
TEST_F(InputMethodStateAuraTest, SelectedTextCopiedToClipboard) {
  // Skip test for platforms that do not support selection clipboard.
  if (!ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    return;
  }

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  EXPECT_TRUE(!!clipboard);
  std::vector<std::string> texts = {"text0", "text1", "text2", "text3"};
  for (auto index : active_view_sequence_) {
    clipboard->Clear(ui::ClipboardBuffer::kSelection);

    // Focus the corresponding widget.
    render_widget_host_delegate()->set_focused_widget(
        RenderWidgetHostImpl::From(views_[index]->GetRenderWidgetHost()));

    // Change the selection of the currently focused widget. It suffices to just
    // call the method on the view.
    std::u16string expected_text = base::ASCIIToUTF16(texts[index]);
    views_[index]->SelectionChanged(expected_text, 0U, gfx::Range(0, 5));

    // Retrieve the selected text from clipboard and verify it is as expected.
    std::u16string result_text;
    clipboard->ReadText(ui::ClipboardBuffer::kSelection,
                        /* data_dst = */ nullptr, &result_text);
    EXPECT_EQ(expected_text, result_text);
  }
}

// This test verifies that when any view on the page cancels an ongoing
// composition, the RenderWidgetHostViewAura will receive the notification and
// the current composition is canceled.
TEST_F(InputMethodStateAuraTest, ImeCancelCompositionForAllViews) {
  for (content::RenderWidgetHostViewBase* view : views_) {
    ActivateViewForTextInputManager(view, ui::TEXT_INPUT_TYPE_TEXT);
    // There is no composition in the beginning.
    EXPECT_FALSE(has_composition_text());
    SetHasCompositionTextToTrue();
    view->ImeCancelComposition();
    // The composition must have been canceled.
    EXPECT_FALSE(has_composition_text());
  }
}

// This test verifies that when the focused node is changed,
// RenderWidgetHostViewAura will tell InputMethodAuraLinux to cancel the current
// composition.
TEST_F(InputMethodStateAuraTest, ImeFocusedNodeChanged) {
  ActivateViewForTextInputManager(tab_view(), ui::TEXT_INPUT_TYPE_TEXT);
  // There is no composition in the beginning.
  EXPECT_FALSE(has_composition_text());
  SetHasCompositionTextToTrue();
  tab_view()->FocusedNodeChanged(true, gfx::Rect());
  // The composition must have been canceled.
  EXPECT_FALSE(has_composition_text());
}

TEST_F(RenderWidgetHostViewAuraTest, FocusReasonNotFocused) {
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            parent_view_->GetFocusReason());
}

TEST_F(RenderWidgetHostViewAuraTest, FocusReasonMouse) {
  parent_view_->Focus();
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  parent_view_->OnMouseEvent(&mouse_event);
  parent_view_->FocusedNodeChanged(true, gfx::Rect());

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_MOUSE,
            parent_view_->GetFocusReason());
}

TEST_F(RenderWidgetHostViewAuraTest, FocusReasonTouch) {
  parent_view_->Focus();
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::GestureEventDetails tap_details(ui::EventType::kGestureTapDown);
  tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  tap_details.set_primary_pointer_type(ui::EventPointerType::kTouch);
  ui::GestureEvent touch_event(0, 0, 0, base::TimeTicks(), tap_details);

  parent_view_->OnGestureEvent(&touch_event);
  parent_view_->FocusedNodeChanged(true, gfx::Rect());

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_TOUCH,
            parent_view_->GetFocusReason());
}

TEST_F(RenderWidgetHostViewAuraTest, FocusReasonPen) {
  parent_view_->Focus();
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::GestureEventDetails tap_details(ui::EventType::kGestureTapDown);
  tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  tap_details.set_primary_pointer_type(ui::EventPointerType::kPen);
  ui::GestureEvent pen_event(0, 0, 0, base::TimeTicks(), tap_details);

  parent_view_->OnGestureEvent(&pen_event);
  parent_view_->FocusedNodeChanged(true, gfx::Rect());

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_PEN,
            parent_view_->GetFocusReason());
}

TEST_F(RenderWidgetHostViewAuraTest, FocusReasonMultipleEventsOnSameNode) {
  parent_view_->Focus();
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);

  // Touch then pen.
  {
    ui::GestureEventDetails tap_details(ui::EventType::kGestureTapDown);
    tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    tap_details.set_primary_pointer_type(ui::EventPointerType::kTouch);
    ui::GestureEvent touch_event(0, 0, 0, base::TimeTicks(), tap_details);

    parent_view_->OnGestureEvent(&touch_event);
    parent_view_->FocusedNodeChanged(true, gfx::Rect());
  }

  {
    ui::GestureEventDetails tap_details(ui::EventType::kGestureTapDown);
    tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    tap_details.set_primary_pointer_type(ui::EventPointerType::kPen);
    ui::GestureEvent pen_event(0, 0, 0, base::TimeTicks(), tap_details);

    parent_view_->OnGestureEvent(&pen_event);
  }

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_TOUCH,
            parent_view_->GetFocusReason());
}

class RenderWidgetHostViewAuraInputMethodTest
    : public RenderWidgetHostViewAuraTest,
      public ui::InputMethodObserver {
 public:
  RenderWidgetHostViewAuraInputMethodTest() = default;

  RenderWidgetHostViewAuraInputMethodTest(
      const RenderWidgetHostViewAuraInputMethodTest&) = delete;
  RenderWidgetHostViewAuraInputMethodTest& operator=(
      const RenderWidgetHostViewAuraInputMethodTest&) = delete;

  ~RenderWidgetHostViewAuraInputMethodTest() override {}
  void SetUp() override {
    // TODO(crbug.com/40275284) Pass as unique_ptr<>.
    ui::SetUpInputMethodForTesting(new ui::MockInputMethod(nullptr));
    SetUpEnvironment();
    text_input_client_ = nullptr;
  }

  void TearDown() override {
    // text_input_client_ may point at |parent_view_| or something else owned by
    // RenderWidgetHostViewAuraTest and will get destroyed in TearDown().
    text_input_client_ = nullptr;
    RenderWidgetHostViewAuraTest::TearDown();
  }

  // Override from ui::InputMethodObserver.
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {
    text_input_client_ = client;
  }
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}

 protected:
  raw_ptr<const ui::TextInputClient> text_input_client_;
};

// This test is for notifying InputMethod for surrounding text changes.
TEST_F(RenderWidgetHostViewAuraInputMethodTest, OnCaretBoundsChanged) {
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  GetInputMethod()->SetFocusedTextInputClient(parent_view_);
  GetInputMethod()->AddObserver(this);

  parent_view_->SelectionChanged(std::u16string(), 0, gfx::Range());
  EXPECT_EQ(parent_view_, text_input_client_);

  text_input_client_ = nullptr;

  parent_view_->SelectionBoundsChanged(
      gfx::Rect(0, 0, 10, 10), base::i18n::LEFT_TO_RIGHT,
      gfx::Rect(10, 10, 10, 10), base::i18n::LEFT_TO_RIGHT,
      /*bounding_box=*/gfx::Rect(), true);
  EXPECT_EQ(parent_view_, text_input_client_);

  GetInputMethod()->RemoveObserver(this);
}

// The input method should still receive caret bounds changes even if inputmode
// is NONE. See crbug.com/1114559.
TEST_F(RenderWidgetHostViewAuraInputMethodTest,
       OnCaretBoundsChangedInputModeNone) {
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  GetInputMethod()->SetFocusedTextInputClient(parent_view_);
  GetInputMethod()->AddObserver(this);

  text_input_client_ = nullptr;

  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;
  state.mode = ui::TEXT_INPUT_MODE_NONE;
  state.value = u"a";
  state.selection = gfx::Range(1, 1);

  GetTextInputManager(parent_view_)->UpdateTextInputState(parent_view_, state);

  EXPECT_EQ(parent_view_, text_input_client_);

  GetInputMethod()->RemoveObserver(this);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
class MockVirtualKeyboardController final
    : public ui::VirtualKeyboardController {
 public:
  MockVirtualKeyboardController() = default;

  MockVirtualKeyboardController(const MockVirtualKeyboardController&) = delete;
  MockVirtualKeyboardController& operator=(
      const MockVirtualKeyboardController&) = delete;

  bool DisplayVirtualKeyboard() override {
    virtual_keyboard_requested_ = true;
    return virtual_keyboard_requested_;
  }

  void DismissVirtualKeyboard() override {
    virtual_keyboard_requested_ = false;
  }

  void AddObserver(ui::VirtualKeyboardControllerObserver* observer) override {
    observer_count_++;
  }

  void RemoveObserver(
      ui::VirtualKeyboardControllerObserver* observer) override {
    observer_count_--;
  }

  bool IsKeyboardVisible() override { return virtual_keyboard_requested_; }

  size_t observer_count() const { return observer_count_; }

 private:
  size_t observer_count_ = 0;
  bool virtual_keyboard_requested_ = false;
};

class RenderWidgetHostViewAuraKeyboardMockInputMethod
    : public ui::MockInputMethod {
 public:
  RenderWidgetHostViewAuraKeyboardMockInputMethod()
      : MockInputMethod(nullptr) {}

  RenderWidgetHostViewAuraKeyboardMockInputMethod(
      const RenderWidgetHostViewAuraKeyboardMockInputMethod&) = delete;
  RenderWidgetHostViewAuraKeyboardMockInputMethod& operator=(
      const RenderWidgetHostViewAuraKeyboardMockInputMethod&) = delete;

  ui::VirtualKeyboardController* GetVirtualKeyboardController() override {
    return &keyboard_controller_;
  }
  size_t keyboard_controller_observer_count() const {
    return keyboard_controller_.observer_count();
  }
  void SetVirtualKeyboardVisibilityIfEnabled(bool should_show) override {
    if (should_show) {
      keyboard_controller_.DisplayVirtualKeyboard();
    } else {
      keyboard_controller_.DismissVirtualKeyboard();
    }
  }
  bool IsKeyboardVisible() { return keyboard_controller_.IsKeyboardVisible(); }

 private:
  MockVirtualKeyboardController keyboard_controller_;
};

class RenderWidgetHostViewAuraKeyboardTest
    : public RenderWidgetHostViewAuraTest {
 public:
  RenderWidgetHostViewAuraKeyboardTest() = default;

  RenderWidgetHostViewAuraKeyboardTest(
      const RenderWidgetHostViewAuraKeyboardTest&) = delete;
  RenderWidgetHostViewAuraKeyboardTest& operator=(
      const RenderWidgetHostViewAuraKeyboardTest&) = delete;

  ~RenderWidgetHostViewAuraKeyboardTest() override {}
  void SetUp() override {
    // TODO(crbug.com/40275284) Pass as unique_ptr<>.
    ui::SetUpInputMethodForTesting(
        new RenderWidgetHostViewAuraKeyboardMockInputMethod());
    SetUpEnvironment();
  }

  RenderWidgetHostViewAuraKeyboardMockInputMethod* GetMockInputMethod() const {
    return static_cast<RenderWidgetHostViewAuraKeyboardMockInputMethod*>(
        GetInputMethod());
  }

  size_t keyboard_controller_observer_count() const {
    return GetMockInputMethod()->keyboard_controller_observer_count();
  }
  bool IsKeyboardVisible() const {
    return GetMockInputMethod()->IsKeyboardVisible();
  }
};
#endif

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(RenderWidgetHostViewAuraKeyboardTest,
       UpdateTextInputStateUpdatesVirtualKeyboardState) {
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);

  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;
  state.mode = ui::TEXT_INPUT_MODE_NONE;
  state.last_vk_visibility_request =
      ui::mojom::VirtualKeyboardVisibilityRequest::SHOW;

  EXPECT_EQ(IsKeyboardVisible(), false);

  GetTextInputManager(parent_view_)->UpdateTextInputState(parent_view_, state);

  EXPECT_EQ(IsKeyboardVisible(), true);

  state.last_vk_visibility_request =
      ui::mojom::VirtualKeyboardVisibilityRequest::HIDE;
  GetTextInputManager(parent_view_)->UpdateTextInputState(parent_view_, state);

  EXPECT_EQ(IsKeyboardVisible(), false);
}
#endif

#if BUILDFLAG(IS_WIN)
TEST_F(RenderWidgetHostViewAuraKeyboardTest, KeyboardObserverDestroyed) {
  parent_view_->SetLastPointerType(ui::EventPointerType::kTouch);
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_NE(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 1u);
  EXPECT_EQ(IsKeyboardVisible(), true);
  // Detach the RenderWidgetHostViewAura from the IME.
  parent_view_->DetachFromInputMethod(true);
  EXPECT_EQ(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 0u);
}

TEST_F(RenderWidgetHostViewAuraKeyboardTest, NoKeyboardObserverForMouseInput) {
  // Not checking for both touch and mouse inputs here as the user could use
  // mouse and touch input on a touch device. The keyboard observer shouldn't be
  // removed after it has been registered with a touch input and we received a
  // mouse event.
  // Do not show virtual keyboard for mouse inputs.
  parent_view_->SetLastPointerType(ui::EventPointerType::kMouse);
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(keyboard_controller_observer_count(), 0u);
  EXPECT_EQ(IsKeyboardVisible(), false);
}

TEST_F(RenderWidgetHostViewAuraKeyboardTest,
       KeyboardObserverForOnlyTouchInput) {
  // Show virtual keyboard for touch inputs.
  parent_view_->SetLastPointerType(ui::EventPointerType::kTouch);
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_NE(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 1u);
  EXPECT_EQ(IsKeyboardVisible(), true);
}

TEST_F(RenderWidgetHostViewAuraKeyboardTest,
       KeyboardObserverForFocusedNodeChanged) {
  // Show virtual keyboard for touch inputs.
  parent_view_->SetLastPointerType(ui::EventPointerType::kTouch);
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(IsKeyboardVisible(), true);
  EXPECT_NE(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 1u);

  // Change the focused node to a read-only node so the keyboard is dismissed,
  // but the keyboard observer should still be valid.
  parent_view_->FocusedNodeChanged(false, gfx::Rect());
  EXPECT_NE(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 1u);
  EXPECT_EQ(IsKeyboardVisible(), false);
  // Detaching the input method should destroy the keyboard observer.
  parent_view_->DetachFromInputMethod(true);
  EXPECT_EQ(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 0u);
}

TEST_F(RenderWidgetHostViewAuraKeyboardTest, KeyboardObserverForPenInput) {
  // Show virtual keyboard for pen inputs.
  parent_view_->SetLastPointerType(ui::EventPointerType::kPen);
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_NE(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 1u);
}

TEST_F(RenderWidgetHostViewAuraKeyboardTest,
       KeyboardObserverDetachDuringWindowDestroy) {
  parent_view_->SetLastPointerType(ui::EventPointerType::kTouch);
  ActivateViewForTextInputManager(parent_view_, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_NE(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 1u);
  EXPECT_EQ(IsKeyboardVisible(), true);
  // Detach the RenderWidgetHostViewAura from the IME, but don't destroy the
  // Vk controller as it might need to notify about VK hide so the sites can
  // reflow their content.
  parent_view_->DetachFromInputMethod(false);
  EXPECT_EQ(IsKeyboardVisible(), false);
  EXPECT_NE(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 1u);
  // Detach the keyboard observer as the window is getting destroyed.
  parent_view_->DetachFromInputMethod(true);
  EXPECT_EQ(parent_view_->virtual_keyboard_controller_win_.get(), nullptr);
  EXPECT_EQ(keyboard_controller_observer_count(), 0u);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
