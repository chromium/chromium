// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_render_widget_host.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/local_frame_host_interceptor.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/history_entry.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/service_worker/service_worker_network_provider_for_frame.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/mock_keyboard.h"
#include "content/test/test_render_frame.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/mojom/base/text_direction.mojom-blink.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_origin_trials.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/range/range.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/x/x11.h"  // nogncheck
#endif

#if defined(USE_OZONE)
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

#if defined(USE_X11) && defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

using base::TimeDelta;
using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebString;
using blink::WebURLError;

namespace content {

namespace {

static const int kProxyRoutingId = 13;

#if (defined(USE_AURA) && defined(USE_X11)) || defined(USE_OZONE)
// Converts MockKeyboard::Modifiers to ui::EventFlags.
int ConvertMockKeyboardModifier(MockKeyboard::Modifiers modifiers) {
  static struct ModifierMap {
    MockKeyboard::Modifiers src;
    int dst;
  } kModifierMap[] = {
      {MockKeyboard::LEFT_SHIFT, ui::EF_SHIFT_DOWN},
      {MockKeyboard::RIGHT_SHIFT, ui::EF_SHIFT_DOWN},
      {MockKeyboard::LEFT_CONTROL, ui::EF_CONTROL_DOWN},
      {MockKeyboard::RIGHT_CONTROL, ui::EF_CONTROL_DOWN},
      {MockKeyboard::LEFT_ALT, ui::EF_ALT_DOWN},
      {MockKeyboard::RIGHT_ALT, ui::EF_ALT_DOWN},
  };
  int flags = 0;
  for (size_t i = 0; i < base::size(kModifierMap); ++i) {
    if (kModifierMap[i].src & modifiers) {
      flags |= kModifierMap[i].dst;
    }
  }
  return flags;
}
#endif

class WebUITestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    return nullptr;
  }
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    return WebUI::kNoWebUI;
  }
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return HasWebUIScheme(url);
  }
  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) override {
    return HasWebUIScheme(url);
  }
};

// FrameReplicationState is normally maintained in the browser process,
// but the function below provides a way for tests to construct a partial
// FrameReplicationState within the renderer process.  We say "partial",
// because some fields of FrameReplicationState cannot be filled out
// by content-layer, renderer code (still the constructed, partial
// FrameReplicationState is sufficiently complete to avoid trigerring
// asserts that a default/empty FrameReplicationState would).
FrameReplicationState ReconstructReplicationStateForTesting(
    TestRenderFrame* test_render_frame) {
  blink::WebLocalFrame* frame = test_render_frame->GetWebFrame();

  FrameReplicationState result;
  // can't recover result.scope - no way to get blink::mojom::TreeScopeType via
  // public blink API...
  result.name = frame->AssignedName().Utf8();
  result.unique_name = test_render_frame->unique_name();
  // result.should_enforce_strict_mixed_content_checking is calculated in the
  // browser...
  result.origin = frame->GetSecurityOrigin();

  return result;
}

// Returns mojom::CommonNavigationParams for a normal navigation to a data: url,
// with navigation_start set to Now() plus the given offset.
mojom::CommonNavigationParamsPtr MakeCommonNavigationParams(
    TimeDelta navigation_start_offset) {
  auto params = CreateCommonNavigationParams();
  params->url = GURL("data:text/html,<div>Page</div>");
  params->navigation_start = base::TimeTicks::Now() + navigation_start_offset;
  params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  params->transition = ui::PAGE_TRANSITION_TYPED;
  return params;
}

template <class MockedLocalFrameHostInterceptor>
class MockedLocalFrameHostInterceptorTestRenderFrame : public TestRenderFrame {
 public:
  static RenderFrameImpl* CreateTestRenderFrame(
      RenderFrameImpl::CreateParams params) {
    return new MockedLocalFrameHostInterceptorTestRenderFrame(
        std::move(params));
  }

  ~MockedLocalFrameHostInterceptorTestRenderFrame() override = default;

  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override {
    blink::AssociatedInterfaceProvider* associated_interface_provider =
        RenderFrameImpl::GetRemoteAssociatedInterfaces();

    // Attach our fake local frame host at the very first call to
    // GetRemoteAssociatedInterfaces.
    if (!local_frame_host_) {
      local_frame_host_ = std::make_unique<MockedLocalFrameHostInterceptor>(
          associated_interface_provider);
    }
    return associated_interface_provider;
  }

  MockedLocalFrameHostInterceptor* mock_local_frame_host() {
    return local_frame_host_.get();
  }

 private:
  explicit MockedLocalFrameHostInterceptorTestRenderFrame(
      RenderFrameImpl::CreateParams params)
      : TestRenderFrame(std::move(params)) {}

  std::unique_ptr<MockedLocalFrameHostInterceptor> local_frame_host_;
};

}  // namespace

class RenderViewImplTest : public RenderViewTest {
 public:
  explicit RenderViewImplTest(
      RenderFrameImpl::CreateRenderFrameImplFunction hook_function = nullptr)
      : RenderViewTest(/*hook_render_frame_creation=*/!hook_function) {
    if (hook_function)
      RenderFrameImpl::InstallCreateHook(hook_function);
    // Attach a pseudo keyboard device to this object.
    mock_keyboard_.reset(new MockKeyboard());
  }

  ~RenderViewImplTest() override {}

  RenderViewImpl* view() { return static_cast<RenderViewImpl*>(view_); }

  RenderWidget* main_widget() {
    return view()->GetMainRenderFrame()->GetLocalRootRenderWidget();
  }

  blink::WebFrameWidget* main_frame_widget() {
    return static_cast<blink::WebFrameWidget*>(view()
                                                   ->GetMainRenderFrame()
                                                   ->GetLocalRootRenderWidget()
                                                   ->GetWebWidget());
  }

  TestRenderFrame* frame() {
    return static_cast<TestRenderFrame*>(view()->GetMainRenderFrame());
  }

  blink::mojom::FrameWidgetInputHandler* GetFrameWidgetInputHandler() {
    return render_widget_host_->GetFrameWidgetInputHandler();
  }

  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() {
    return render_widget_host_->GetWidgetInputHandler();
  }

  RenderAccessibilityManager* GetRenderAccessibilityManager() {
    return frame()->GetRenderAccessibilityManager();
  }

  ui::AXMode GetAccessibilityMode() {
    return GetRenderAccessibilityManager()->GetAccessibilityMode();
  }

  const std::vector<gfx::Rect>& LastCompositionBounds() {
    render_widget_host_->GetWidgetInputHandler()->RequestCompositionUpdates(
        true, false);
    base::RunLoop().RunUntilIdle();
    return render_widget_host_->LastCompositionBounds();
  }

  void ReceiveDisableDeviceEmulation(RenderViewImpl* view) {
    view->GetWebView()->DisableDeviceEmulation();
  }

  void ReceiveEnableDeviceEmulation(
      RenderViewImpl* view,
      const blink::DeviceEmulationParams& params) {
    view->GetWebView()->EnableDeviceEmulation(params);
  }

  void GoToOffsetWithParams(int offset,
                            const PageState& state,
                            mojom::CommonNavigationParamsPtr common_params,
                            mojom::CommitNavigationParamsPtr commit_params) {
    EXPECT_TRUE(common_params->transition & ui::PAGE_TRANSITION_FORWARD_BACK);
    int pending_offset = offset + view()->history_list_offset_;

    commit_params->page_state = state;
    commit_params->nav_entry_id = pending_offset + 1;
    commit_params->pending_history_list_offset = pending_offset;
    commit_params->current_history_list_offset = view()->history_list_offset_;
    commit_params->current_history_list_length = view()->history_list_length_;
    frame()->Navigate(std::move(common_params), std::move(commit_params));

    // The load actually happens asynchronously, so we pump messages to process
    // the pending continuation.
    FrameLoadWaiter(frame()).Wait();
  }

  template <class T>
  typename T::Param ProcessAndReadIPC() {
    base::RunLoop().RunUntilIdle();
    const IPC::Message* message =
        render_thread_->sink().GetUniqueMessageMatching(T::ID);
    typename T::Param param;
    EXPECT_TRUE(message);
    if (message)
      T::Read(message, &param);
    return param;
  }

#if defined(USE_X11)
  int SendKeyEventX11(MockKeyboard::Layout layout,
                      int key_code,
                      MockKeyboard::Modifiers modifiers,
                      base::string16* output) {
    // We ignore |layout|, which means we are only testing the layout of the
    // current locale. TODO(mazda): fix this to respect |layout|.
    CHECK(output);
    const int flags = ConvertMockKeyboardModifier(modifiers);

    ui::ScopedXI2Event xevent;
    xevent.InitKeyEvent(ui::ET_KEY_PRESSED,
                        static_cast<ui::KeyboardCode>(key_code), flags);
    auto event1 = ui::BuildKeyEventFromXEvent(*xevent);
    NativeWebKeyboardEvent keydown_event(*event1);
    SendNativeKeyEvent(keydown_event);

    // X11 doesn't actually have native character events, but give the test
    // what it wants.
    xevent.InitKeyEvent(ui::ET_KEY_PRESSED,
                        static_cast<ui::KeyboardCode>(key_code), flags);
    auto event2 = ui::BuildKeyEventFromXEvent(*xevent);
    event2->set_character(
        DomCodeToUsLayoutCharacter(event2->code(), event2->flags()));
    ui::KeyEventTestApi test_event2(event2.get());
    test_event2.set_is_char(true);
    NativeWebKeyboardEvent char_event(*event2);
    SendNativeKeyEvent(char_event);

    xevent.InitKeyEvent(ui::ET_KEY_RELEASED,
                        static_cast<ui::KeyboardCode>(key_code), flags);
    auto event3 = ui::BuildKeyEventFromXEvent(*xevent);
    NativeWebKeyboardEvent keyup_event(*event3);
    SendNativeKeyEvent(keyup_event);

    base::char16 c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(static_cast<ui::KeyboardCode>(key_code)),
        flags);
    output->assign(1, static_cast<base::char16>(c));
    return 1;
  }
#endif

#if defined(USE_OZONE)
  int SendKeyEventOzone(MockKeyboard::Layout layout,
                        int key_code,
                        MockKeyboard::Modifiers modifiers,
                        base::string16* output) {
    int flags = ConvertMockKeyboardModifier(modifiers);

    ui::KeyEvent keydown_event(ui::ET_KEY_PRESSED,
                               static_cast<ui::KeyboardCode>(key_code), flags);
    NativeWebKeyboardEvent keydown_web_event(keydown_event);
    SendNativeKeyEvent(keydown_web_event);

    ui::KeyEvent char_event(keydown_event.GetCharacter(),
                            static_cast<ui::KeyboardCode>(key_code),
                            ui::DomCode::NONE, flags);
    NativeWebKeyboardEvent char_web_event(char_event);
    SendNativeKeyEvent(char_web_event);

    ui::KeyEvent keyup_event(ui::ET_KEY_RELEASED,
                             static_cast<ui::KeyboardCode>(key_code), flags);
    NativeWebKeyboardEvent keyup_web_event(keyup_event);
    SendNativeKeyEvent(keyup_web_event);

    base::char16 c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(static_cast<ui::KeyboardCode>(key_code)),
        flags);
    output->assign(1, static_cast<base::char16>(c));
    return 1;
  }
#endif

  // Sends IPC messages that emulates a key-press event.
  int SendKeyEvent(MockKeyboard::Layout layout,
                   int key_code,
                   MockKeyboard::Modifiers modifiers,
                   base::string16* output) {
#if defined(OS_WIN)
    // Retrieve the Unicode character for the given tuple (keyboard-layout,
    // key-code, and modifiers).
    // Exit when a keyboard-layout driver cannot assign a Unicode character to
    // the tuple to prevent sending an invalid key code to the RenderView
    // object.
    CHECK(mock_keyboard_.get());
    CHECK(output);
    int length =
        mock_keyboard_->GetCharacters(layout, key_code, modifiers, output);
    if (length != 1)
      return -1;

    // Create IPC messages from Windows messages and send them to our
    // back-end.
    // A keyboard event of Windows consists of three Windows messages:
    // WM_KEYDOWN, WM_CHAR, and WM_KEYUP.
    // WM_KEYDOWN and WM_KEYUP sends virtual-key codes. On the other hand,
    // WM_CHAR sends a composed Unicode character.
    MSG msg1 = {NULL, WM_KEYDOWN, key_code, 0};
    ui::KeyEvent evt1(msg1);
    NativeWebKeyboardEvent keydown_event(evt1);
    SendNativeKeyEvent(keydown_event);

    MSG msg2 = {NULL, WM_CHAR, (*output)[0], 0};
    ui::KeyEvent evt2(msg2);
    NativeWebKeyboardEvent char_event(evt2);
    SendNativeKeyEvent(char_event);

    MSG msg3 = {NULL, WM_KEYUP, key_code, 0};
    ui::KeyEvent evt3(msg3);
    NativeWebKeyboardEvent keyup_event(evt3);
    SendNativeKeyEvent(keyup_event);

    return length;
#elif defined(USE_X11)
#if defined(USE_OZONE)
    if (features::IsUsingOzonePlatform())
      return SendKeyEventOzone(layout, key_code, modifiers, output);
#endif
    return SendKeyEventX11(layout, key_code, modifiers, output);
#elif defined(USE_OZONE)
    return SendKeyEventOzone(layout, key_code, modifiers, output);
#else
    NOTIMPLEMENTED();
    return L'\0';
#endif
  }

  void EnablePreferredSizeMode() {
    blink::WebView* webview = view()->GetWebView();
    webview->EnablePreferredSizeChangedMode();
  }

  gfx::Size GetPreferredSize() {
    blink::WebView* webview = view()->GetWebView();
    webview->UpdatePreferredSize();
    return gfx::Size(webview->GetPreferredSizeForTest());
  }

  gfx::Size MainWidgetSizeInDIPS() {
    blink::WebSize widget_size = main_widget()->GetWebWidget()->Size();
    blink::WebRect widget_rect(0, 0, widget_size.width, widget_size.height);
    main_widget()->ConvertViewportToWindow(&widget_rect);
    return gfx::Rect(widget_rect).size();
  }

  int GetScrollbarWidth() {
    blink::WebView* webview = view()->GetWebView();
    return webview->MainFrameWidget()->Size().width -
           webview->MainFrame()->ToWebLocalFrame()->VisibleContentRect().width;
  }

 private:
  std::unique_ptr<MockKeyboard> mock_keyboard_;
};

class RenderViewImplBlinkSettingsTest : public RenderViewImplTest {
 public:
  virtual void DoSetUp() { RenderViewImplTest::SetUp(); }

  blink::WebSettings* settings() { return view()->GetWebView()->GetSettings(); }

 protected:
  // Blink settings may be specified on the command line, which must
  // be configured before RenderViewImplTest::SetUp runs. Thus we make
  // SetUp() a no-op, and expose RenderViewImplTest::SetUp() via
  // DoSetUp(), to allow tests to perform command line modifications
  // before RenderViewImplTest::SetUp is run. Each test must invoke
  // DoSetUp manually once pre-SetUp configuration is complete.
  void SetUp() override {}
};

// This test class enables UseZoomForDSF based on the platform default value.
class RenderViewImplScaleFactorTest : public RenderViewImplTest {
 protected:
  void SetUp() override {
    render_thread_ = std::make_unique<MockRenderThread>();
    SetUseZoomForDSFEnabled(content::IsUseZoomForDSFEnabled());
    RenderViewImplTest::SetUp();
  }

  void SetDeviceScaleFactor(float dsf) {
    RenderWidget* widget = main_widget();
    widget->GetWebWidget()->ApplyVisualProperties(
        MakeVisualPropertiesWithDeviceScaleFactor(dsf));

    ASSERT_EQ(dsf, view()->GetMainRenderFrame()->GetDeviceScaleFactor());
    ASSERT_EQ(
        dsf,
        widget->GetWebWidget()->GetOriginalScreenInfo().device_scale_factor);
  }

  blink::VisualProperties MakeVisualPropertiesWithDeviceScaleFactor(float dsf) {
    blink::VisualProperties visual_properties;
    visual_properties.screen_info.device_scale_factor = dsf;
    visual_properties.new_size = gfx::Size(100, 100);
    visual_properties.compositor_viewport_pixel_rect = gfx::Rect(200, 200);
    visual_properties.visible_viewport_size = visual_properties.new_size;
    visual_properties.auto_resize_enabled = view()->AutoResizeMode();
    visual_properties.min_size_for_auto_resize = min_size_for_autoresize_;
    visual_properties.max_size_for_auto_resize = max_size_for_autoresize_;
    visual_properties.local_surface_id =
        viz::LocalSurfaceId(1, 1, base::UnguessableToken::Create());
    return visual_properties;
  }

  void TestEmulatedSizeDprDsf(int width,
                              int height,
                              float dpr,
                              float compositor_dsf) {
    static base::string16 get_width =
        base::ASCIIToUTF16("Number(window.innerWidth)");
    static base::string16 get_height =
        base::ASCIIToUTF16("Number(window.innerHeight)");
    static base::string16 get_dpr =
        base::ASCIIToUTF16("Number(window.devicePixelRatio * 10)");

    int emulated_width, emulated_height;
    int emulated_dpr;
    blink::DeviceEmulationParams params;
    params.view_size = gfx::Size(width, height);
    params.device_scale_factor = dpr;
    ReceiveEnableDeviceEmulation(view(), params);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_width, &emulated_width));
    EXPECT_EQ(width, emulated_width);
    EXPECT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(get_height, &emulated_height));
    EXPECT_EQ(height, emulated_height);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_dpr, &emulated_dpr));
    EXPECT_EQ(static_cast<int>(dpr * 10), emulated_dpr);
    cc::LayerTreeHost* host = main_widget()->layer_tree_host();
    EXPECT_EQ(compositor_dsf, host->device_scale_factor());
  }

  void EnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size) {
    min_size_for_autoresize_ = min_size;
    max_size_for_autoresize_ = max_size;
    blink::WebView* webview = view()->GetWebView();
    webview->EnableAutoResizeForTesting(min_size, max_size);
  }

 private:
  gfx::Size min_size_for_autoresize_;
  gfx::Size max_size_for_autoresize_;
};

// This test class forces UseZoomForDSF to be on for all platforms.
class RenderViewImplEnableZoomForDSFTest
    : public RenderViewImplScaleFactorTest {
 protected:
  void SetUp() override {
    render_thread_ = std::make_unique<MockRenderThread>();
    SetUseZoomForDSFEnabled(true);
    RenderViewImplTest::SetUp();
  }
};

// This test class forces UseZoomForDSF to be off for all platforms.
class RenderViewImplDisableZoomForDSFTest
    : public RenderViewImplScaleFactorTest {
 protected:
  void SetUp() override {
    render_thread_ = std::make_unique<MockRenderThread>();
    SetUseZoomForDSFEnabled(false);
    RenderViewImplTest::SetUp();
  }
};

TEST_F(RenderViewImplTest, IsPinchGestureActivePropagatesToProxies) {
  LoadHTML(
      "<body style='min-height:1000px;'>"
      "  <iframe src='data:text/html,frame 1'></iframe>"
      "  <iframe src='data:text/html,frame 2'></iframe>"
      "</body>");

  // Verify child's proxy doesn't think we're pinching.
  blink::WebFrame* root_web_frame = frame()->GetWebFrame();
  ASSERT_TRUE(root_web_frame->FirstChild()->IsWebLocalFrame());
  TestRenderFrame* child_frame_1 =
      static_cast<TestRenderFrame*>(RenderFrame::FromWebFrame(
          root_web_frame->FirstChild()->ToWebLocalFrame()));
  ASSERT_TRUE(child_frame_1);
  TestRenderFrame* child_frame_2 =
      static_cast<TestRenderFrame*>(RenderFrame::FromWebFrame(
          root_web_frame->FirstChild()->NextSibling()->ToWebLocalFrame()));
  ASSERT_TRUE(child_frame_2);
  child_frame_1->Unload(kProxyRoutingId, true,
                        ReconstructReplicationStateForTesting(child_frame_1),
                        base::UnguessableToken::Create());
  EXPECT_TRUE(root_web_frame->FirstChild()->IsWebRemoteFrame());
  RenderFrameProxy* child_proxy_1 = RenderFrameProxy::FromWebFrame(
      root_web_frame->FirstChild()->ToWebRemoteFrame());
  ASSERT_TRUE(child_proxy_1);
  EXPECT_FALSE(child_proxy_1->is_pinch_gesture_active_for_testing());

  // Set the |is_pinch_gesture_active| flag.
  cc::ApplyViewportChangesArgs args;
  args.page_scale_delta = 1.f;
  args.is_pinch_gesture_active = true;
  args.top_controls_delta = 0.f;
  args.bottom_controls_delta = 0.f;
  args.browser_controls_constraint = cc::BrowserControlsState::kHidden;
  args.scroll_gesture_did_end = false;

  view()->GetWebView()->MainFrameWidget()->ApplyViewportChangesForTesting(args);
  EXPECT_TRUE(child_proxy_1->is_pinch_gesture_active_for_testing());

  // Create a new remote child, and get its proxy. Unloading will force creation
  // and registering of a new RenderFrameProxy, which should pick up the
  // existing setting.
  child_frame_2->Unload(kProxyRoutingId + 1, true,
                        ReconstructReplicationStateForTesting(child_frame_2),
                        base::UnguessableToken::Create());
  EXPECT_TRUE(root_web_frame->FirstChild()->NextSibling()->IsWebRemoteFrame());
  RenderFrameProxy* child_proxy_2 = RenderFrameProxy::FromWebFrame(
      root_web_frame->FirstChild()->NextSibling()->ToWebRemoteFrame());

  // Verify new child has the flag too.
  EXPECT_TRUE(child_proxy_2->is_pinch_gesture_active_for_testing());

  // Reset the flag, make sure both children respond.
  args.is_pinch_gesture_active = false;
  view()->GetWebView()->MainFrameWidget()->ApplyViewportChangesForTesting(args);
  EXPECT_FALSE(child_proxy_1->is_pinch_gesture_active_for_testing());
  EXPECT_FALSE(child_proxy_2->is_pinch_gesture_active_for_testing());
}

// Test that we get form state change notifications when input fields change.
TEST_F(RenderViewImplTest, OnNavStateChanged) {
  view()->set_send_content_state_immediately(true);
  LoadHTML("<input type=\"text\" id=\"elt_text\"></input>");

  // We should NOT have gotten a form state change notification yet.
  EXPECT_FALSE(frame()->IsPageStateUpdated());

  // Change the value of the input. We should have gotten an update state
  // notification. We need to spin the message loop to catch this update.
  ExecuteJavaScriptForTests(
      "document.getElementById('elt_text').value = 'foo';");
  base::RunLoop().RunUntilIdle();

  // Check the page state is updated after the value of the input is changed.
  EXPECT_TRUE(frame()->IsPageStateUpdated());
}

class RenderViewImplEmulatingPopupTest : public RenderViewImplTest {
 protected:
  blink::VisualProperties InitialVisualProperties() override {
    blink::VisualProperties visual_properties =
        RenderViewImplTest::InitialVisualProperties();
    visual_properties.screen_info.rect = gfx::Rect(800, 600);
    return visual_properties;
  }
};

// Popup RenderWidgets should inherit emulation params from the parent.
TEST_F(RenderViewImplEmulatingPopupTest, EmulatingPopupRect) {
  // Real screen rect set to 800x600.
  gfx::Rect screen_rect(800, 600);
  // Real widget and window screen rects.
  gfx::Rect window_screen_rect(1, 2, 137, 139);
  gfx::Rect widget_screen_rect(5, 7, 57, 59);

  // Verify screen rect will be set.
  EXPECT_EQ(gfx::Rect(main_widget()->GetWebWidget()->GetScreenInfo().rect),
            screen_rect);

  {
    // Make a popup widget.
    blink::WebPagePopup* popup = view()->CreatePopup(frame()->GetWebFrame());
    RenderWidget* popup_widget =
        static_cast<RenderWidget*>(popup->GetClientForTesting());
    ASSERT_TRUE(popup_widget);

    // Set its size.
    popup->SetScreenRects(widget_screen_rect, window_screen_rect);

    // The WindowScreenRect, WidgetScreenRect, and ScreenRect are all available
    // to the popup.
    EXPECT_EQ(window_screen_rect, gfx::Rect(popup->WindowRect()));
    EXPECT_EQ(widget_screen_rect, gfx::Rect(popup->ViewRect()));
    EXPECT_EQ(screen_rect, gfx::Rect(popup->GetScreenInfo().rect));

    // Close and destroy the widget.
    {
      WidgetMsg_Close msg(popup_widget->routing_id());
      popup_widget->OnMessageReceived(msg);
    }
  }

  // Enable device emulation on the parent widget.
  blink::DeviceEmulationParams emulation_params;
  gfx::Rect emulated_widget_rect(150, 160, 980, 1200);
  // In mobile emulation the WindowScreenRect and ScreenRect are both set to
  // match the WidgetScreenRect, which we set here.
  emulation_params.screen_type = blink::mojom::EmulatedScreenType::kMobile;
  emulation_params.view_size = emulated_widget_rect.size();
  emulation_params.view_position = emulated_widget_rect.origin();
  view()->GetWebView()->EnableDeviceEmulation(emulation_params);

  {
    // Make a popup again. It should inherit device emulation params.
    blink::WebPagePopup* popup = view()->CreatePopup(frame()->GetWebFrame());
    popup->InitializeForTesting(view()->GetWebView());
    RenderWidget* popup_widget =
        static_cast<RenderWidget*>(popup->GetClientForTesting());
    ASSERT_TRUE(popup_widget);

    // Set its size again.
    popup->SetScreenRects(widget_screen_rect, window_screen_rect);

    // This time, the position of the WidgetScreenRect and WindowScreenRect
    // should be affected by emulation params.
    // TODO(danakj): This means the popup sees the top level widget at the
    // emulated position *plus* the real position. Whereas the top level
    // widget will see itself at the emulation position. Why this inconsistency?
    int window_x = emulated_widget_rect.x() + window_screen_rect.x();
    int window_y = emulated_widget_rect.y() + window_screen_rect.y();
    EXPECT_EQ(window_x, popup->WindowRect().x());
    EXPECT_EQ(window_y, popup->WindowRect().y());

    int widget_x = emulated_widget_rect.x() + widget_screen_rect.x();
    int widget_y = emulated_widget_rect.y() + widget_screen_rect.y();
    EXPECT_EQ(widget_x, popup->ViewRect().x());
    EXPECT_EQ(widget_y, popup->ViewRect().y());

    // TODO(danakj): Why don't the sizes get changed by emulation? The comments
    // that used to be in this test suggest that the sizes used to change, and
    // we were testing for that. But now we only test for positions changing?
    EXPECT_EQ(window_screen_rect.width(), popup->WindowRect().width());
    EXPECT_EQ(window_screen_rect.height(), popup->WindowRect().height());
    EXPECT_EQ(widget_screen_rect.width(), popup->ViewRect().width());
    EXPECT_EQ(widget_screen_rect.height(), popup->ViewRect().height());
    EXPECT_EQ(emulated_widget_rect,
              gfx::Rect(main_widget()->GetWebWidget()->ViewRect()));
    EXPECT_EQ(emulated_widget_rect,
              gfx::Rect(main_widget()->GetWebWidget()->WindowRect()));

    // TODO(danakj): Why isn't the ScreenRect visible to the popup an emulated
    // value? The ScreenRect has been changed by emulation as demonstrated
    // below.
    EXPECT_EQ(gfx::Rect(800, 600),
              gfx::Rect(popup_widget->GetWebWidget()->GetScreenInfo().rect));
    EXPECT_EQ(emulated_widget_rect,
              gfx::Rect(main_widget()->GetWebWidget()->GetScreenInfo().rect));

    // Close and destroy the widget.
    {
      WidgetMsg_Close msg(popup_widget->routing_id());
      popup_widget->OnMessageReceived(msg);
    }
  }
}

TEST_F(RenderViewImplTest, OnNavigationHttpPost) {
  // An http url will trigger a resource load so cannot be used here.
  auto common_params = CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,<div>Page</div>");
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;
  common_params->method = "POST";

  // Set up post data.
  const char raw_data[] = "post \0\ndata";
  const size_t length = base::size(raw_data);
  scoped_refptr<network::ResourceRequestBody> post_data(
      new network::ResourceRequestBody);
  post_data->AppendBytes(raw_data, length);
  common_params->post_data = post_data;

  frame()->Navigate(std::move(common_params), CreateCommitNavigationParams());
  base::RunLoop().RunUntilIdle();

  auto last_commit_params = frame()->TakeLastCommitParams();
  ASSERT_TRUE(last_commit_params);
  EXPECT_EQ("POST", last_commit_params->method);

  // Check post data sent to browser matches
  EXPECT_TRUE(last_commit_params->page_state.IsValid());
  std::unique_ptr<HistoryEntry> entry =
      PageStateToHistoryEntry(last_commit_params->page_state);
  blink::WebHTTPBody body = entry->root().HttpBody();
  blink::WebHTTPBody::Element element;
  bool successful = body.ElementAt(0, element);
  EXPECT_TRUE(successful);
  EXPECT_EQ(blink::WebHTTPBody::Element::kTypeData, element.type);
  EXPECT_EQ(length, element.data.size());

  std::unique_ptr<char[]> flat_data(new char[element.data.size()]);
  element.data.ForEachSegment([&flat_data](const char* segment,
                                           size_t segment_size,
                                           size_t segment_offset) {
    std::copy(segment, segment + segment_size,
              flat_data.get() + segment_offset);
    return true;
  });
  EXPECT_EQ(0, memcmp(raw_data, flat_data.get(), length));
}

#if defined(OS_ANDROID)
namespace {
class UpdateTitleLocalFrameHost : public LocalFrameHostInterceptor {
 public:
  explicit UpdateTitleLocalFrameHost(
      blink::AssociatedInterfaceProvider* provider)
      : LocalFrameHostInterceptor(provider) {}

  MOCK_METHOD2(UpdateTitle,
               void(const base::Optional<::base::string16>& title,
                    base::i18n::TextDirection title_direction));
};
}  // namespace

class RenderViewImplUpdateTitleTest : public RenderViewImplTest {
 public:
  using MockedTestRenderFrame =
      MockedLocalFrameHostInterceptorTestRenderFrame<UpdateTitleLocalFrameHost>;

  RenderViewImplUpdateTitleTest()
      : RenderViewImplTest(&MockedTestRenderFrame::CreateTestRenderFrame) {}

  UpdateTitleLocalFrameHost* title_mock_frame_host() {
    return static_cast<MockedTestRenderFrame*>(frame())
        ->mock_local_frame_host();
  }
};

#if defined(OS_ANDROID)
// Failing on Android: http://crbug.com/1080328
#define MAYBE_OnNavigationLoadDataWithBaseURL \
  DISABLED_OnNavigationLoadDataWithBaseURL
#else
#define MAYBE_OnNavigationLoadDataWithBaseURL OnNavigationLoadDataWithBaseURL
#endif
TEST_F(RenderViewImplUpdateTitleTest, MAYBE_OnNavigationLoadDataWithBaseURL) {
  auto common_params = CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,");
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;
  common_params->base_url_for_data_url = GURL("about:blank");
  common_params->history_url_for_data_url = GURL("about:blank");
  auto commit_params = CreateCommitNavigationParams();
  commit_params->data_url_as_string =
      "data:text/html,<html><head><title>Data page</title></head></html>";
  FrameLoadWaiter waiter(frame());
  frame()->Navigate(std::move(common_params), std::move(commit_params));
  waiter.Wait();

  // While LocalFrame is initialized, it's called with an empty title.
  const base::Optional<::base::string16> null_title;
  EXPECT_CALL(*title_mock_frame_host(), UpdateTitle(null_title, testing::_))
      .Times(1);

  const base::Optional<::base::string16>& title =
      base::make_optional(base::ASCIIToUTF16("Data page"));
  EXPECT_CALL(*title_mock_frame_host(), UpdateTitle(title, testing::_))
      .Times(1);
}
#endif

TEST_F(RenderViewImplTest, BeginNavigation) {
  WebUITestWebUIControllerFactory factory;
  WebUIControllerFactory::RegisterFactory(&factory);

  blink::WebSecurityOrigin requestor_origin =
      blink::WebSecurityOrigin::Create(GURL("http://foo.com"));

  // Navigations to normal HTTP URLs.
  blink::WebURLRequest request(GURL("http://foo.com"));
  request.SetMode(network::mojom::RequestMode::kNavigate);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kInclude);
  request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetRequestorOrigin(requestor_origin);
  auto navigation_info = std::make_unique<blink::WebNavigationInfo>();
  navigation_info->url_request = std::move(request);
  navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kTopLevel;
  navigation_info->navigation_type = blink::kWebNavigationTypeLinkClicked;
  navigation_info->navigation_policy = blink::kWebNavigationPolicyCurrentTab;
  DCHECK(!navigation_info->url_request.RequestorOrigin().IsNull());
  frame()->BeginNavigation(std::move(navigation_info));
  // If this is a renderer-initiated navigation that just begun, it should
  // stop and be sent to the browser.
  EXPECT_TRUE(frame()->IsBrowserSideNavigationPending());

  // Form posts to WebUI URLs.
  auto form_navigation_info = std::make_unique<blink::WebNavigationInfo>();
  form_navigation_info->url_request = blink::WebURLRequest(GetWebUIURL("foo"));
  form_navigation_info->url_request.SetHttpMethod("POST");
  form_navigation_info->url_request.SetMode(
      network::mojom::RequestMode::kNavigate);
  form_navigation_info->url_request.SetRedirectMode(
      network::mojom::RedirectMode::kManual);
  form_navigation_info->url_request.SetRequestContext(
      blink::mojom::RequestContextType::INTERNAL);
  blink::WebHTTPBody post_body;
  post_body.Initialize();
  post_body.AppendData("blah");
  form_navigation_info->url_request.SetHttpBody(post_body);
  form_navigation_info->url_request.SetRequestorOrigin(requestor_origin);
  form_navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kTopLevel;
  form_navigation_info->navigation_type =
      blink::kWebNavigationTypeFormSubmitted;
  form_navigation_info->navigation_policy =
      blink::kWebNavigationPolicyCurrentTab;
  render_thread_->sink().ClearMessages();
  frame()->BeginNavigation(std::move(form_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());

  // Popup links to WebUI URLs.
  blink::WebURLRequest popup_request(GetWebUIURL("foo"));
  auto popup_navigation_info = std::make_unique<blink::WebNavigationInfo>();
  popup_navigation_info->url_request = blink::WebURLRequest(GetWebUIURL("foo"));
  popup_navigation_info->url_request.SetMode(
      network::mojom::RequestMode::kNavigate);
  popup_navigation_info->url_request.SetRedirectMode(
      network::mojom::RedirectMode::kManual);
  popup_navigation_info->url_request.SetRequestContext(
      blink::mojom::RequestContextType::INTERNAL);
  popup_navigation_info->url_request.SetRequestorOrigin(requestor_origin);
  popup_navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kAuxiliary;
  popup_navigation_info->navigation_type = blink::kWebNavigationTypeLinkClicked;
  popup_navigation_info->navigation_policy =
      blink::kWebNavigationPolicyNewForegroundTab;
  render_thread_->sink().ClearMessages();
  frame()->BeginNavigation(std::move(popup_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());
}

TEST_F(RenderViewImplTest, BeginNavigationHandlesAllTopLevel) {
  blink::mojom::RendererPreferences prefs = view()->renderer_preferences();
  prefs.browser_handles_all_top_level_requests = true;
  view()->OnSetRendererPrefs(prefs);

  const blink::WebNavigationType kNavTypes[] = {
      blink::kWebNavigationTypeLinkClicked,
      blink::kWebNavigationTypeFormSubmitted,
      blink::kWebNavigationTypeBackForward,
      blink::kWebNavigationTypeReload,
      blink::kWebNavigationTypeFormResubmitted,
      blink::kWebNavigationTypeOther,
  };

  for (size_t i = 0; i < base::size(kNavTypes); ++i) {
    auto navigation_info = std::make_unique<blink::WebNavigationInfo>();
    navigation_info->url_request = blink::WebURLRequest(GURL("http://foo.com"));
    navigation_info->url_request.SetRequestorOrigin(
        blink::WebSecurityOrigin::Create(GURL("http://foo.com")));
    navigation_info->frame_type =
        blink::mojom::RequestContextFrameType::kTopLevel;
    navigation_info->navigation_policy = blink::kWebNavigationPolicyCurrentTab;
    navigation_info->navigation_type = kNavTypes[i];

    render_thread_->sink().ClearMessages();
    frame()->BeginNavigation(std::move(navigation_info));
    EXPECT_TRUE(frame()->IsURLOpened());
  }
}

TEST_F(RenderViewImplTest, BeginNavigationForWebUI) {
  // Enable bindings to simulate a WebUI view.
  view()->GetMainRenderFrame()->AllowBindings(BINDINGS_POLICY_WEB_UI);

  blink::WebSecurityOrigin requestor_origin =
      blink::WebSecurityOrigin::Create(GURL("http://foo.com"));

  // Navigations to normal HTTP URLs.
  auto navigation_info = std::make_unique<blink::WebNavigationInfo>();
  navigation_info->url_request = blink::WebURLRequest(GURL("http://foo.com"));
  navigation_info->url_request.SetMode(network::mojom::RequestMode::kNavigate);
  navigation_info->url_request.SetRedirectMode(
      network::mojom::RedirectMode::kManual);
  navigation_info->url_request.SetRequestContext(
      blink::mojom::RequestContextType::INTERNAL);
  navigation_info->url_request.SetRequestorOrigin(requestor_origin);
  navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kTopLevel;
  navigation_info->navigation_type = blink::kWebNavigationTypeLinkClicked;
  navigation_info->navigation_policy = blink::kWebNavigationPolicyCurrentTab;

  render_thread_->sink().ClearMessages();
  frame()->BeginNavigation(std::move(navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());

  // Navigations to WebUI URLs.
  auto webui_navigation_info = std::make_unique<blink::WebNavigationInfo>();
  webui_navigation_info->url_request = blink::WebURLRequest(GetWebUIURL("foo"));
  webui_navigation_info->url_request.SetMode(
      network::mojom::RequestMode::kNavigate);
  webui_navigation_info->url_request.SetRedirectMode(
      network::mojom::RedirectMode::kManual);
  webui_navigation_info->url_request.SetRequestContext(
      blink::mojom::RequestContextType::INTERNAL);
  webui_navigation_info->url_request.SetRequestorOrigin(requestor_origin);
  webui_navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kTopLevel;
  webui_navigation_info->navigation_type = blink::kWebNavigationTypeLinkClicked;
  webui_navigation_info->navigation_policy =
      blink::kWebNavigationPolicyCurrentTab;
  render_thread_->sink().ClearMessages();
  frame()->BeginNavigation(std::move(webui_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());

  // Form posts to data URLs.
  auto data_navigation_info = std::make_unique<blink::WebNavigationInfo>();
  data_navigation_info->url_request =
      blink::WebURLRequest(GURL("data:text/html,foo"));
  data_navigation_info->url_request.SetMode(
      network::mojom::RequestMode::kNavigate);
  data_navigation_info->url_request.SetRedirectMode(
      network::mojom::RedirectMode::kManual);
  data_navigation_info->url_request.SetRequestContext(
      blink::mojom::RequestContextType::INTERNAL);
  data_navigation_info->url_request.SetRequestorOrigin(requestor_origin);
  data_navigation_info->url_request.SetHttpMethod("POST");
  blink::WebHTTPBody post_body;
  post_body.Initialize();
  post_body.AppendData("blah");
  data_navigation_info->url_request.SetHttpBody(post_body);
  data_navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kTopLevel;
  data_navigation_info->navigation_type =
      blink::kWebNavigationTypeFormSubmitted;
  data_navigation_info->navigation_policy =
      blink::kWebNavigationPolicyCurrentTab;
  render_thread_->sink().ClearMessages();
  frame()->BeginNavigation(std::move(data_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());

  // A popup that creates a view first and then navigates to a
  // normal HTTP URL.
  blink::WebURLRequest popup_request(GURL("http://foo.com"));
  popup_request.SetRequestorOrigin(requestor_origin);
  popup_request.SetMode(network::mojom::RequestMode::kNavigate);
  popup_request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  popup_request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  blink::WebView* new_web_view = view()->CreateView(
      GetMainFrame(), popup_request, blink::WebWindowFeatures(), "foo",
      blink::kWebNavigationPolicyNewForegroundTab,
      network::mojom::WebSandboxFlags::kNone,
      blink::FeaturePolicyFeatureState(),
      blink::AllocateSessionStorageNamespaceId());
  auto popup_navigation_info = std::make_unique<blink::WebNavigationInfo>();
  popup_navigation_info->url_request = std::move(popup_request);
  popup_navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kAuxiliary;
  popup_navigation_info->navigation_type = blink::kWebNavigationTypeLinkClicked;
  popup_navigation_info->navigation_policy =
      blink::kWebNavigationPolicyNewForegroundTab;
  render_thread_->sink().ClearMessages();
  RenderFrameImpl::FromWebFrame(new_web_view->MainFrame())
      ->render_view()
      ->GetMainRenderFrame()
      ->BeginNavigation(std::move(popup_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());
}

// This test verifies that when device emulation is enabled, RenderFrameProxy
// continues to receive the original ScreenInfo and not the emualted
// ScreenInfo.
TEST_F(RenderViewImplScaleFactorTest, DeviceEmulationWithOOPIF) {
  const float device_scale = 2.0f;
  float compositor_dsf =
      compositor_deps_->IsUseZoomForDSFEnabled() ? 1.f : device_scale;
  SetDeviceScaleFactor(device_scale);

  LoadHTML(
      "<body style='min-height:1000px;'>"
      "  <iframe src='data:text/html,frame 1'></iframe>"
      "</body>");

  WebFrame* web_frame = frame()->GetWebFrame();
  ASSERT_TRUE(web_frame->FirstChild()->IsWebLocalFrame());
  TestRenderFrame* child_frame = static_cast<TestRenderFrame*>(
      RenderFrame::FromWebFrame(web_frame->FirstChild()->ToWebLocalFrame()));
  ASSERT_TRUE(child_frame);

  child_frame->Unload(kProxyRoutingId + 1, true,
                      ReconstructReplicationStateForTesting(child_frame),
                      base::UnguessableToken::Create());
  EXPECT_TRUE(web_frame->FirstChild()->IsWebRemoteFrame());
  RenderFrameProxy* child_proxy = RenderFrameProxy::FromWebFrame(
      web_frame->FirstChild()->ToWebRemoteFrame());
  ASSERT_TRUE(child_proxy);

  // Verify that the system device scale factor has propagated into the
  // RenderFrameProxy.
  EXPECT_EQ(device_scale, view()->GetMainRenderFrame()->GetDeviceScaleFactor());
  EXPECT_EQ(device_scale, main_widget()
                              ->GetWebWidget()
                              ->GetOriginalScreenInfo()
                              .device_scale_factor);
  EXPECT_EQ(device_scale, child_proxy->screen_info().device_scale_factor);

  TestEmulatedSizeDprDsf(640, 480, 3.f, compositor_dsf);

  // Verify that the RenderFrameProxy device scale factor is still the same.
  EXPECT_EQ(3.f, view()->GetMainRenderFrame()->GetDeviceScaleFactor());
  EXPECT_EQ(device_scale, main_widget()
                              ->GetWebWidget()
                              ->GetOriginalScreenInfo()
                              .device_scale_factor);
  EXPECT_EQ(device_scale, child_proxy->screen_info().device_scale_factor);

  ReceiveDisableDeviceEmulation(view());

  blink::DeviceEmulationParams params;
  ReceiveEnableDeviceEmulation(view(), params);
  // Don't disable here to test that emulation is being shutdown properly.
}

// Verify that security origins are replicated properly to RenderFrameProxies
// when unloading.
TEST_F(RenderViewImplTest, OriginReplicationForUnload) {
  LoadHTML(
      "Hello <iframe src='data:text/html,frame 1'></iframe>"
      "<iframe src='data:text/html,frame 2'></iframe>");
  WebFrame* web_frame = frame()->GetWebFrame();
  TestRenderFrame* child_frame = static_cast<TestRenderFrame*>(
      RenderFrame::FromWebFrame(web_frame->FirstChild()->ToWebLocalFrame()));

  // Unload the child frame and pass a replicated origin to be set for
  // WebRemoteFrame.
  content::FrameReplicationState replication_state =
      ReconstructReplicationStateForTesting(child_frame);
  replication_state.origin = url::Origin::Create(GURL("http://foo.com"));
  child_frame->Unload(kProxyRoutingId, true, replication_state,
                      base::UnguessableToken::Create());

  // The child frame should now be a WebRemoteFrame.
  EXPECT_TRUE(web_frame->FirstChild()->IsWebRemoteFrame());

  // Expect the origin to be updated properly.
  blink::WebSecurityOrigin origin =
      web_frame->FirstChild()->GetSecurityOrigin();
  EXPECT_EQ(origin.ToString(),
            WebString::FromUTF8(replication_state.origin.Serialize()));

  // Now, unload the second frame using a unique origin and verify that it is
  // replicated correctly.
  replication_state.origin = url::Origin();
  TestRenderFrame* child_frame2 =
      static_cast<TestRenderFrame*>(RenderFrame::FromWebFrame(
          web_frame->FirstChild()->NextSibling()->ToWebLocalFrame()));
  child_frame2->Unload(kProxyRoutingId + 1, true, replication_state,
                       base::UnguessableToken::Create());
  EXPECT_TRUE(web_frame->FirstChild()->NextSibling()->IsWebRemoteFrame());
  EXPECT_TRUE(
      web_frame->FirstChild()->NextSibling()->GetSecurityOrigin().IsOpaque());
}

// Test that when navigating cross-origin, which creates a new main frame
// RenderWidget, that the device scale is set correctly for that RenderWidget
// the WebView and frames.
// See crbug.com/737777#c37.
TEST_F(RenderViewImplEnableZoomForDSFTest,
       DeviceScaleCorrectAfterCrossOriginNav) {
  const float device_scale = 3.0f;
  SetDeviceScaleFactor(device_scale);
  EXPECT_EQ(device_scale, view()->GetMainRenderFrame()->GetDeviceScaleFactor());

  LoadHTML("Hello world!");

  // Early grab testing values as the main-frame widget becomes inaccessible
  // when it unloads.
  blink::VisualProperties test_visual_properties =
      MakeVisualPropertiesWithDeviceScaleFactor(device_scale);

  // Unload the main frame after which it should become a WebRemoteFrame.
  content::FrameReplicationState replication_state =
      ReconstructReplicationStateForTesting(frame());
  // replication_state.origin = url::Origin(GURL("http://foo.com"));
  frame()->Unload(kProxyRoutingId, true, replication_state,
                  base::UnguessableToken::Create());
  EXPECT_TRUE(view()->GetWebView()->MainFrame()->IsWebRemoteFrame());

  // Do the remote-to-local transition for the proxy, which is to create a
  // provisional local frame.
  int routing_id = kProxyRoutingId + 1;
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      stub_interface_provider;
  ignore_result(stub_interface_provider.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      stub_browser_interface_broker;
  ignore_result(stub_browser_interface_broker.InitWithNewPipeAndPassReceiver());

  // The new frame is initialized with |device_scale| as the device scale
  // factor.
  mojom::CreateFrameWidgetParamsPtr widget_params =
      mojom::CreateFrameWidgetParams::New();
  widget_params->routing_id = kProxyRoutingId + 2;
  widget_params->visual_properties = test_visual_properties;

  mojo::AssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
      blink_frame_widget_receiver =
          blink_frame_widget.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> blink_frame_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetHost>
      blink_frame_widget_host_receiver =
          blink_frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();

  widget_params->frame_widget = std::move(blink_frame_widget_receiver);
  widget_params->frame_widget_host = blink_frame_widget_host.Unbind();

  RenderFrameImpl::CreateFrame(
      *agent_scheduling_group_, routing_id, std::move(stub_interface_provider),
      std::move(stub_browser_interface_broker), kProxyRoutingId, base::nullopt,
      MSG_ROUTING_NONE, MSG_ROUTING_NONE, base::UnguessableToken::Create(),
      base::UnguessableToken::Create(), replication_state,
      compositor_deps_.get(), std::move(widget_params),
      blink::mojom::FrameOwnerProperties::New(),
      /*has_committed_real_load=*/true);
  TestRenderFrame* provisional_frame =
      static_cast<TestRenderFrame*>(RenderFrameImpl::FromRoutingID(routing_id));
  EXPECT_TRUE(provisional_frame);

  // Navigate to other page, which triggers the swap in.
  auto common_params = CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,<div>Page</div>");
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;

  provisional_frame->Navigate(std::move(common_params),
                              CreateCommitNavigationParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(device_scale, view()->GetMainRenderFrame()->GetDeviceScaleFactor());
  EXPECT_EQ(device_scale,
            view()->GetWebView()->ZoomFactorForDeviceScaleFactor());

  double device_pixel_ratio;
  base::string16 get_dpr =
      base::ASCIIToUTF16("Number(window.devicePixelRatio)");
  EXPECT_TRUE(
      ExecuteJavaScriptAndReturnNumberValue(get_dpr, &device_pixel_ratio));
  EXPECT_EQ(device_scale, device_pixel_ratio);

  int width;
  base::string16 get_width =
      base::ASCIIToUTF16("Number(document.documentElement.clientWidth)");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_width, &width));
  EXPECT_EQ(view()->GetWebView()->MainFrameWidget()->Size().width,
            width * device_scale);
}

// Test that when a parent detaches a remote child after the provisional
// RenderFrame is created but before it is navigated, the RenderFrame is
// destroyed along with the proxy.  This protects against races in
// https://crbug.com/526304 and https://crbug.com/568676.
TEST_F(RenderViewImplTest, DetachingProxyAlsoDestroysProvisionalFrame) {
  LoadHTML("Hello <iframe src='data:text/html,frame 1'></iframe>");
  WebFrame* web_frame = frame()->GetWebFrame();
  TestRenderFrame* child_frame = static_cast<TestRenderFrame*>(
      RenderFrame::FromWebFrame(web_frame->FirstChild()->ToWebLocalFrame()));

  // Unload the child frame.
  FrameReplicationState replication_state =
      ReconstructReplicationStateForTesting(child_frame);
  child_frame->Unload(kProxyRoutingId, true, replication_state,
                      base::UnguessableToken::Create());
  EXPECT_TRUE(web_frame->FirstChild()->IsWebRemoteFrame());

  // Do the first step of a remote-to-local transition for the child proxy,
  // which is to create a provisional local frame.
  int routing_id = kProxyRoutingId + 1;
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      stub_interface_provider;
  ignore_result(stub_interface_provider.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      stub_browser_interface_broker;
  ignore_result(stub_browser_interface_broker.InitWithNewPipeAndPassReceiver());

  RenderFrameImpl::CreateFrame(
      *agent_scheduling_group_, routing_id, std::move(stub_interface_provider),
      std::move(stub_browser_interface_broker), kProxyRoutingId, base::nullopt,
      frame()->GetRoutingID(), MSG_ROUTING_NONE,
      base::UnguessableToken::Create(), base::UnguessableToken::Create(),
      replication_state, nullptr,
      /*widget_params=*/nullptr, blink::mojom::FrameOwnerProperties::New(),
      /*has_committed_real_load=*/true);
  {
    TestRenderFrame* provisional_frame = static_cast<TestRenderFrame*>(
        RenderFrameImpl::FromRoutingID(routing_id));
    EXPECT_TRUE(provisional_frame);
  }

  // Detach the child frame (currently remote) in the main frame.
  ExecuteJavaScriptForTests(
      "document.body.removeChild(document.querySelector('iframe'));");
  RenderFrameProxy* child_proxy =
      RenderFrameProxy::FromRoutingID(kProxyRoutingId);
  EXPECT_FALSE(child_proxy);

  // The provisional frame should have been deleted along with the proxy, and
  // thus any subsequent messages (such as OnNavigate) already in flight for it
  // should be dropped.
  {
    TestRenderFrame* provisional_frame = static_cast<TestRenderFrame*>(
        RenderFrameImpl::FromRoutingID(routing_id));
    EXPECT_FALSE(provisional_frame);
  }
}

// Verify that the renderer process doesn't crash when device scale factor
// changes after a cross-process navigation has commited.
// See https://crbug.com/571603.
TEST_F(RenderViewImplEnableZoomForDSFTest,
       SetZoomLevelAfterCrossProcessNavigation) {
  LoadHTML("Hello world!");

  // Unload the main frame after which it should become a WebRemoteFrame.
  TestRenderFrame* main_frame =
      static_cast<TestRenderFrame*>(view()->GetMainRenderFrame());
  main_frame->Unload(kProxyRoutingId, true,
                     ReconstructReplicationStateForTesting(main_frame),
                     base::UnguessableToken::Create());
  EXPECT_TRUE(view()->GetWebView()->MainFrame()->IsWebRemoteFrame());
}

class TextInputStateFakeRenderWidgetHost : public FakeRenderWidgetHost {
 public:
  void TextInputStateChanged(ui::mojom::TextInputStatePtr state) override {
    updated_states_.push_back(std::move(state));
  }

  const std::vector<ui::mojom::TextInputStatePtr>& updated_states() {
    return updated_states_;
  }

  void ClearState() { updated_states_.clear(); }

 private:
  std::vector<ui::mojom::TextInputStatePtr> updated_states_;
};

class RenderViewImplTextInputStateChanged : public RenderViewImplTest {
 public:
  std::unique_ptr<FakeRenderWidgetHost> CreateRenderWidgetHost() override {
    return std::make_unique<TextInputStateFakeRenderWidgetHost>();
  }

  const std::vector<ui::mojom::TextInputStatePtr>& updated_states() {
    return static_cast<TextInputStateFakeRenderWidgetHost*>(
               render_widget_host_.get())
        ->updated_states();
  }

  void ClearState() {
    static_cast<TextInputStateFakeRenderWidgetHost*>(render_widget_host_.get())
        ->ClearState();
  }
};

// Test that our IME backend sends a notification message when the input focus
// changes.
TEST_F(RenderViewImplTextInputStateChanged, OnImeTypeChanged) {
  // Load an HTML page consisting of two input fields.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test1\" type=\"text\" value=\"some text\"></input>"
      "<input id=\"test2\" type=\"password\"></input>"
      "<input id=\"test3\" type=\"text\" inputmode=\"none\"></input>"
      "<input id=\"test4\" type=\"text\" inputmode=\"text\"></input>"
      "<input id=\"test5\" type=\"text\" inputmode=\"tel\"></input>"
      "<input id=\"test6\" type=\"text\" inputmode=\"url\"></input>"
      "<input id=\"test7\" type=\"text\" inputmode=\"email\"></input>"
      "<input id=\"test8\" type=\"text\" inputmode=\"numeric\"></input>"
      "<input id=\"test9\" type=\"text\" inputmode=\"decimal\"></input>"
      "<input id=\"test10\" type=\"text\" inputmode=\"search\"></input>"
      "<input id=\"test11\" type=\"text\" inputmode=\"unknown\"></input>"
      "</body>"
      "</html>");

  struct InputModeTestCase {
    const char* input_id;
    ui::TextInputMode expected_mode;
  };
  static const InputModeTestCase kInputModeTestCases[] = {
      {"test1", ui::TEXT_INPUT_MODE_DEFAULT},
      {"test3", ui::TEXT_INPUT_MODE_NONE},
      {"test4", ui::TEXT_INPUT_MODE_TEXT},
      {"test5", ui::TEXT_INPUT_MODE_TEL},
      {"test6", ui::TEXT_INPUT_MODE_URL},
      {"test7", ui::TEXT_INPUT_MODE_EMAIL},
      {"test8", ui::TEXT_INPUT_MODE_NUMERIC},
      {"test9", ui::TEXT_INPUT_MODE_DECIMAL},
      {"test10", ui::TEXT_INPUT_MODE_SEARCH},
      {"test11", ui::TEXT_INPUT_MODE_DEFAULT},
  };

  const int kRepeatCount = 10;
  for (int i = 0; i < kRepeatCount; i++) {
    // Move the input focus to the first <input> element, where we should
    // activate IMEs.
    ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
    base::RunLoop().RunUntilIdle();
    ClearState();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to activate IMEs.
    main_widget()->UpdateTextInputState();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1u, updated_states().size());
    ui::TextInputType type = updated_states()[0]->type;
    ui::TextInputMode input_mode = updated_states()[0]->mode;
    bool can_compose_inline = updated_states()[0]->can_compose_inline;
    EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, type);
    EXPECT_EQ(true, can_compose_inline);

    // Move the input focus to the second <input> element, where we should
    // de-activate IMEs.
    ExecuteJavaScriptForTests("document.getElementById('test2').focus();");
    base::RunLoop().RunUntilIdle();
    ClearState();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to de-activate IMEs.
    main_widget()->UpdateTextInputState();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1u, updated_states().size());
    type = updated_states()[0]->type;
    input_mode = updated_states()[0]->mode;
    EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, type);

    for (size_t i = 0; i < base::size(kInputModeTestCases); i++) {
      const InputModeTestCase* test_case = &kInputModeTestCases[i];
      std::string javascript = base::StringPrintf(
          "document.getElementById('%s').focus();", test_case->input_id);
      // Move the input focus to the target <input> element, where we should
      // activate IMEs.
      ExecuteJavaScriptAndReturnIntValue(base::ASCIIToUTF16(javascript),
                                         nullptr);
      base::RunLoop().RunUntilIdle();
      ClearState();

      // Update the IME status and verify if our IME backend sends an IPC
      // message to activate IMEs.
      main_widget()->UpdateTextInputState();
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(1u, updated_states().size());
      type = updated_states()[0]->type;
      input_mode = updated_states()[0]->mode;
      EXPECT_EQ(test_case->expected_mode, input_mode);
    }
  }
}

TEST_F(RenderViewImplTextInputStateChanged,
       ShouldSuppressKeyboardIsPropagated) {
  class TestAutofillClient : public blink::WebAutofillClient {
   public:
    TestAutofillClient() = default;
    ~TestAutofillClient() override = default;

    bool ShouldSuppressKeyboard(const blink::WebFormControlElement&) override {
      return should_suppress_keyboard_;
    }

    void SetShouldSuppressKeyboard(bool should_suppress_keyboard) {
      should_suppress_keyboard_ = should_suppress_keyboard;
    }

   private:
    bool should_suppress_keyboard_ = false;
  };

  // Set-up the fake autofill client.
  TestAutofillClient client;
  GetMainFrame()->SetAutofillClient(&client);

  // Load an HTML page consisting of one input fields.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test\" type=\"text\"></input>"
      "</body>"
      "</html>");

  // Focus the text field, trigger a state update and check that the right IPC
  // is sent.
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_FALSE(updated_states()[0]->always_hide_ime);
  ClearState();

  // Tell the client to suppress the keyboard. Check whether always_hide_ime is
  // set correctly.
  client.SetShouldSuppressKeyboard(true);
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_TRUE(updated_states()[0]->always_hide_ime);

  // Explicitly clean-up the autofill client, as otherwise a use-after-free
  // happens.
  GetMainFrame()->SetAutofillClient(nullptr);
}

TEST_F(RenderViewImplTextInputStateChanged,
       EditContextGetLayoutBoundsAndInputPanelPolicy) {
  // Load an HTML page.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "</body>"
      "</html>");
  ClearState();
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests(
      "const editContext = new EditContext(); "
      "editContext.focus();editContext.inputPanelPolicy=\"auto\"; "
      "const control_bound = new DOMRect(10, 20, 30, 40); "
      "const selection_bound = new DOMRect(10, 20, 1, 5); "
      "editContext.updateLayout(control_bound, selection_bound);");
  // This RunLoop is waiting for EditContext to be created and layout bounds
  // to be updated in the EditContext.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();

  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  blink::WebRect edit_context_control_bounds_expected(10, 20, 30, 40);
  blink::WebRect edit_context_selection_bounds_expected(10, 20, 1, 5);
  main_widget()->ConvertViewportToWindow(&edit_context_control_bounds_expected);
  main_widget()->ConvertViewportToWindow(
      &edit_context_selection_bounds_expected);
  blink::WebRect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  blink::WebRect actual_active_element_selection_bounds(
      updated_states()[0]->edit_context_selection_bounds.value());
  EXPECT_EQ(edit_context_control_bounds_expected,
            actual_active_element_control_bounds);
  EXPECT_EQ(edit_context_selection_bounds_expected,
            actual_active_element_selection_bounds);
}

TEST_F(RenderViewImplTextInputStateChanged,
       EditContextGetLayoutBoundsWithFloatingValues) {
  // Load an HTML page.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "</body>"
      "</html>");
  ClearState();
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests(
      "const editContext = new EditContext(); "
      "editContext.focus();editContext.inputPanelPolicy=\"auto\"; "
      "const control_bound = new DOMRect(10.14, 20.25, 30.15, 40.50); "
      "const selection_bound = new DOMRect(10, 20, 1, 5); "
      "editContext.updateLayout(control_bound, selection_bound);");
  // This RunLoop is waiting for EditContext to be created and layout bounds
  // to be updated in the EditContext.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  blink::WebRect edit_context_control_bounds_expected(10, 20, 31, 41);
  blink::WebRect edit_context_selection_bounds_expected(10, 20, 1, 5);
  main_widget()->ConvertViewportToWindow(&edit_context_control_bounds_expected);
  main_widget()->ConvertViewportToWindow(
      &edit_context_selection_bounds_expected);
  blink::WebRect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  blink::WebRect actual_active_element_selection_bounds(
      updated_states()[0]->edit_context_selection_bounds.value());
  EXPECT_EQ(edit_context_control_bounds_expected,
            actual_active_element_control_bounds);
  EXPECT_EQ(edit_context_selection_bounds_expected,
            actual_active_element_selection_bounds);
}

TEST_F(RenderViewImplTextInputStateChanged,
       EditContextGetLayoutBoundsWithOverflowFloatingValues) {
  // Load an HTML page.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "</body>"
      "</html>");
  ClearState();
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests(
      "const editContext = new EditContext(); "
      "editContext.focus();editContext.inputPanelPolicy=\"auto\"; "
      "const control_bound = new DOMRect(-3964254814208.000000, "
      "-60129542144.000000, 674309865472.000000, 64424509440.000000); "
      "const selection_bound = new DOMRect(10, 20, 1, 5); "
      "editContext.updateLayout(control_bound, selection_bound);");
  // This RunLoop is waiting for EditContext to be created and layout bounds
  // to be updated in the EditContext.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  blink::WebRect edit_context_control_bounds_expected(-2147483648, -2147483648,
                                                      0, 2147483647);
  blink::WebRect edit_context_selection_bounds_expected(10, 20, 1, 5);
  main_widget()->ConvertViewportToWindow(&edit_context_control_bounds_expected);
  main_widget()->ConvertViewportToWindow(
      &edit_context_selection_bounds_expected);
  blink::WebRect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  blink::WebRect actual_active_element_selection_bounds(
      updated_states()[0]->edit_context_selection_bounds.value());
  EXPECT_EQ(edit_context_control_bounds_expected,
            actual_active_element_control_bounds);
  EXPECT_EQ(edit_context_selection_bounds_expected,
            actual_active_element_selection_bounds);
}

TEST_F(RenderViewImplTextInputStateChanged, ActiveElementGetLayoutBounds) {
  // Load an HTML page consisting of one input fields.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test\" type=\"text\"></input>"
      "</body>"
      "</html>");
  ClearState();
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  blink::WebRect expected_control_bounds;
  blink::WebRect temp_selection_bounds;
  controller->GetLayoutBounds(&expected_control_bounds, &temp_selection_bounds);
  main_widget()->ConvertViewportToWindow(&expected_control_bounds);
  blink::WebRect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  EXPECT_EQ(actual_active_element_control_bounds, expected_control_bounds);
}

TEST_F(RenderViewImplTextInputStateChanged, VirtualKeyboardPolicyAuto) {
  // Load an HTML page consisting of one input field.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test\" type=\"text\"></input>"
      "</body>"
      "</html>");
  ClearState();
  // Set focus on the editable element.
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Update the text input state and verify the virtualkeyboardpolicy attribute
  // value.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_EQ(updated_states()[0]->vk_policy,
            ui::mojom::VirtualKeyboardPolicy::AUTO);
}

TEST_F(RenderViewImplTextInputStateChanged, VirtualKeyboardPolicyAutoToManual) {
  // Load an HTML page consisting of one input field.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test\" type=\"text\" "
      "virtualkeyboardpolicy=\"manual\"></input>"
      "</body>"
      "</html>");
  ClearState();
  // Set focus on the editable element.
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_EQ(updated_states()[0]->vk_policy,
            ui::mojom::VirtualKeyboardPolicy::MANUAL);
  EXPECT_EQ(updated_states()[0]->last_vk_visibility_request,
            ui::mojom::VirtualKeyboardVisibilityRequest::NONE);
}

TEST_F(RenderViewImplTextInputStateChanged,
       VirtualKeyboardPolicyManualAndShowHideAPIsCalled) {
  // Load an HTML page consisting of two input fields.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test1\" type=\"text\" "
      "virtualkeyboardpolicy=\"manual\"></input>"
      "<input id=\"test2\" type=\"text\" "
      "virtualkeyboardpolicy=\"manual\"></input>"
      "</body>"
      "</html>");
  ExecuteJavaScriptForTests(
      "document.getElementById('test2').focus(); "
      "navigator.virtualKeyboard.show();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop1;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop1.QuitClosure());
  run_loop1.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element and the show
  // API call.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_EQ(updated_states()[0]->vk_policy,
            ui::mojom::VirtualKeyboardPolicy::MANUAL);
  EXPECT_EQ(updated_states()[0]->last_vk_visibility_request,
            ui::mojom::VirtualKeyboardVisibilityRequest::NONE);
  ExecuteJavaScriptForTests(
      "document.getElementById('test1').focus(); "
      "navigator.virtualKeyboard.hide();");
  base::RunLoop run_loop2;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop2.QuitClosure());
  run_loop2.Run();
  ClearState();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element and the hide
  // API call.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_EQ(updated_states()[0]->vk_policy,
            ui::mojom::VirtualKeyboardPolicy::MANUAL);
  EXPECT_EQ(updated_states()[0]->last_vk_visibility_request,
            ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
}

TEST_F(RenderViewImplTextInputStateChanged,
       VirtualKeyboardPolicyAutoAndShowHideAPIsCalled) {
  // Load an HTML page consisting of one input field.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test1\" type=\"text\" "
      "virtualkeyboardpolicy=\"auto\"></input>"
      "</body>"
      "</html>");
  render_thread_->sink().ClearMessages();
  ExecuteJavaScriptForTests(
      "document.getElementById('test1').focus(); "
      "navigator.virtualKeyboard.show();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element and the show
  // API call.
  main_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_EQ(updated_states()[0]->vk_policy,
            ui::mojom::VirtualKeyboardPolicy::AUTO);
  EXPECT_EQ(updated_states()[0]->last_vk_visibility_request,
            ui::mojom::VirtualKeyboardVisibilityRequest::NONE);
}

// Test that our IME backend can compose CJK words.
// Our IME front-end sends many platform-independent messages to the IME backend
// while it composes CJK words. This test sends the minimal messages captured
// on my local environment directly to the IME backend to verify if the backend
// can compose CJK words without any problems.
// This test uses an array of command sets because an IME composotion does not
// only depends on IME events, but also depends on window events, e.g. moving
// the window focus while composing a CJK text. To handle such complicated
// cases, this test should not only call IME-related functions in the
// RenderWidget class, but also call some RenderWidget members, e.g.
// ExecuteJavaScriptForTests(), RenderWidget::OnSetFocus(), etc.
TEST_F(RenderViewImplTest, ImeComposition) {
  enum ImeCommand {
    IME_INITIALIZE,
    IME_SETINPUTMODE,
    IME_SETFOCUS,
    IME_SETCOMPOSITION,
    IME_COMMITTEXT,
    IME_FINISHCOMPOSINGTEXT,
    IME_CANCELCOMPOSITION
  };
  struct ImeMessage {
    ImeCommand command;
    bool enable;
    int selection_start;
    int selection_end;
    const wchar_t* ime_string;
    const wchar_t* result;
  };
  static const ImeMessage kImeMessages[] = {
      // Scenario 1: input a Chinese word with Microsoft IME.
      {IME_INITIALIZE, true, 0, 0, nullptr, nullptr},
      {IME_SETINPUTMODE, true, 0, 0, nullptr, nullptr},
      {IME_SETFOCUS, true, 0, 0, nullptr, nullptr},
      {IME_SETCOMPOSITION, false, 1, 1, L"n", L"n"},
      {IME_SETCOMPOSITION, false, 2, 2, L"ni", L"ni"},
      {IME_SETCOMPOSITION, false, 3, 3, L"nih", L"nih"},
      {IME_SETCOMPOSITION, false, 4, 4, L"niha", L"niha"},
      {IME_SETCOMPOSITION, false, 5, 5, L"nihao", L"nihao"},
      {IME_COMMITTEXT, false, -1, -1, L"\x4F60\x597D", L"\x4F60\x597D"},
      // Scenario 2: input a Japanese word with Microsoft IME.
      {IME_INITIALIZE, true, 0, 0, nullptr, nullptr},
      {IME_SETINPUTMODE, true, 0, 0, nullptr, nullptr},
      {IME_SETFOCUS, true, 0, 0, nullptr, nullptr},
      {IME_SETCOMPOSITION, false, 0, 1, L"\xFF4B", L"\xFF4B"},
      {IME_SETCOMPOSITION, false, 0, 1, L"\x304B", L"\x304B"},
      {IME_SETCOMPOSITION, false, 0, 2, L"\x304B\xFF4E", L"\x304B\xFF4E"},
      {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\xFF4A",
       L"\x304B\x3093\xFF4A"},
      {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\x3058",
       L"\x304B\x3093\x3058"},
      {IME_SETCOMPOSITION, false, 0, 2, L"\x611F\x3058", L"\x611F\x3058"},
      {IME_SETCOMPOSITION, false, 0, 2, L"\x6F22\x5B57", L"\x6F22\x5B57"},
      {IME_FINISHCOMPOSINGTEXT, false, -1, -1, L"", L"\x6F22\x5B57"},
      {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\x6F22\x5B57"},
      // Scenario 3: input a Korean word with Microsot IME.
      {IME_INITIALIZE, true, 0, 0, nullptr, nullptr},
      {IME_SETINPUTMODE, true, 0, 0, nullptr, nullptr},
      {IME_SETFOCUS, true, 0, 0, nullptr, nullptr},
      {IME_SETCOMPOSITION, false, 0, 1, L"\x3147", L"\x3147"},
      {IME_SETCOMPOSITION, false, 0, 1, L"\xC544", L"\xC544"},
      {IME_SETCOMPOSITION, false, 0, 1, L"\xC548", L"\xC548"},
      {IME_FINISHCOMPOSINGTEXT, false, -1, -1, L"", L"\xC548"},
      {IME_SETCOMPOSITION, false, 0, 1, L"\x3134", L"\xC548\x3134"},
      {IME_SETCOMPOSITION, false, 0, 1, L"\xB140", L"\xC548\xB140"},
      {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
      {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\xC548"},
      {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
      {IME_FINISHCOMPOSINGTEXT, false, -1, -1, L"", L"\xC548\xB155"},
  };

  for (size_t i = 0; i < base::size(kImeMessages); i++) {
    const ImeMessage* ime_message = &kImeMessages[i];
    switch (ime_message->command) {
      case IME_INITIALIZE:
        // Load an HTML page consisting of a content-editable <div> element,
        // and move the input focus to the <div> element, where we can use
        // IMEs.
        LoadHTML(
            "<html>"
            "<head>"
            "</head>"
            "<body>"
            "<div id=\"test1\" contenteditable=\"true\"></div>"
            "</body>"
            "</html>");
        ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
        break;

      case IME_SETINPUTMODE:
        break;

      case IME_SETFOCUS:
        // Update the window focus.
        GetWidgetInputHandler()->SetFocus(ime_message->enable);
        break;

      case IME_SETCOMPOSITION:
        GetWidgetInputHandler()->ImeSetComposition(
            base::WideToUTF16(ime_message->ime_string),
            std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(),
            ime_message->selection_start, ime_message->selection_end);
        break;

      case IME_COMMITTEXT:
        GetWidgetInputHandler()->ImeCommitText(
            base::WideToUTF16(ime_message->ime_string),
            std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(), 0,
            base::DoNothing());
        break;

      case IME_FINISHCOMPOSINGTEXT:
        GetWidgetInputHandler()->ImeFinishComposingText(false);
        break;

      case IME_CANCELCOMPOSITION:
        GetWidgetInputHandler()->ImeSetComposition(
            base::string16(), std::vector<ui::ImeTextSpan>(),
            gfx::Range::InvalidRange(), 0, 0);
        break;
    }

    // Update the status of our IME back-end.
    // TODO(hbono): we should verify messages to be sent from the back-end.
    main_widget()->UpdateTextInputState();
    base::RunLoop().RunUntilIdle();
    render_thread_->sink().ClearMessages();

    if (ime_message->result) {
      // Retrieve the content of this page and compare it with the expected
      // result.
      const int kMaxOutputCharacters = 128;
      base::string16 output = WebFrameContentDumper::DumpWebViewAsText(
                                  view()->GetWebView(), kMaxOutputCharacters)
                                  .Utf16();
      EXPECT_EQ(base::WideToUTF16(ime_message->result), output);
    }
  }
}

// Test that the RenderView::OnSetTextDirection() function can change the text
// direction of the selected input element.
TEST_F(RenderViewImplTest, OnSetTextDirection) {
  // Load an HTML page consisting of a <textarea> element and a <div> element.
  // This test changes the text direction of the <textarea> element, and
  // writes the values of its 'dir' attribute and its 'direction' property to
  // verify that the text direction is changed.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<textarea id=\"test\"></textarea>"
      "<div id=\"result\" contenteditable=\"true\"></div>"
      "</body>"
      "</html>");
  render_thread_->sink().ClearMessages();

  static const struct {
    base::i18n::TextDirection direction;
    const wchar_t* expected_result;
  } kTextDirection[] = {
      {base::i18n::TextDirection::RIGHT_TO_LEFT, L"rtl,rtl"},
      {base::i18n::TextDirection::LEFT_TO_RIGHT, L"ltr,ltr"},
  };
  for (auto& test_case : kTextDirection) {
    // Set the text direction of the <textarea> element.
    ExecuteJavaScriptForTests("document.getElementById('test').focus();");
    GetMainFrame()->SetTextDirectionForTesting(test_case.direction);

    // Write the values of its DOM 'dir' attribute and its CSS 'direction'
    // property to the <div> element.
    ExecuteJavaScriptForTests(
        "var result = document.getElementById('result');"
        "var node = document.getElementById('test');"
        "var style = getComputedStyle(node, null);"
        "result.innerText ="
        "    node.getAttribute('dir') + ',' +"
        "    style.getPropertyValue('direction');");

    // Copy the document content to std::wstring and compare with the
    // expected result.
    const int kMaxOutputCharacters = 16;
    base::string16 output = WebFrameContentDumper::DumpWebViewAsText(
                                view()->GetWebView(), kMaxOutputCharacters)
                                .Utf16();
    EXPECT_EQ(base::WideToUTF16(test_case.expected_result), output);
  }
}

TEST_F(RenderViewImplTest, DroppedNavigationStaysInViewSourceMode) {
  GetMainFrame()->EnableViewSourceMode(true);
  WebURLError error(net::ERR_ABORTED, GURL("http://foo"));
  WebLocalFrame* web_frame = GetMainFrame();

  // Start a load that will reach provisional state synchronously,
  // but won't complete synchronously.
  auto common_params = CreateCommonNavigationParams();
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->url = GURL("data:text/html,test data");
  frame()->Navigate(std::move(common_params), CreateCommitNavigationParams());

  // A cancellation occurred.
  view()->GetMainRenderFrame()->OnDroppedNavigation();
  // Frame should stay in view-source mode.
  EXPECT_TRUE(web_frame->IsViewSourceModeEnabled());
}

TEST_F(RenderViewImplTest, SetHistoryLengthAndOffset) {
  // No history to merge; one committed page.
  view()->OnSetHistoryOffsetAndLength(0, 1);
  EXPECT_EQ(1, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);

  // History of length 1 to merge; one committed page.
  view()->OnSetHistoryOffsetAndLength(1, 2);
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
}

#if !defined(OS_ANDROID)
TEST_F(RenderViewImplTest, ContextMenu) {
  LoadHTML("<div>Page A</div>");

  // Create a right click in the center of the iframe. (I'm hoping this will
  // make this a bit more robust in case of some other formatting or other bug.)
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(250, 250);
  mouse_event.SetPositionInScreen(250, 250);

  SendWebMouseEvent(mouse_event);

  // Now simulate the corresponding up event which should display the menu
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  SendWebMouseEvent(mouse_event);

  EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
      FrameHostMsg_ContextMenu::ID));
}

#else
TEST_F(RenderViewImplTest, AndroidContextMenuSelectionOrdering) {
  LoadHTML("<div>Page A</div><div id=result>Not selected</div>");

  ExecuteJavaScriptForTests(
      "document.onselectionchange = function() { "
      "document.getElementById('result').innerHTML = 'Selected'}");

  // Create a long press in the center of the iframe. (I'm hoping this will
  // make this a bit more robust in case of some other formatting or other bug.)
  WebGestureEvent gesture_event(WebInputEvent::Type::kGestureLongPress,
                                WebInputEvent::kNoModifiers,
                                ui::EventTimeForNow());
  gesture_event.SetPositionInWidget(gfx::PointF(250, 250));

  SendWebGestureEvent(gesture_event);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
      FROM_HERE, message_loop_runner->QuitClosure());

  EXPECT_FALSE(render_thread_->sink().GetUniqueMessageMatching(
      FrameHostMsg_ContextMenu::ID));

  message_loop_runner->Run();

  EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
      FrameHostMsg_ContextMenu::ID));

  int did_select = -1;
  base::string16 check_did_select = base::ASCIIToUTF16(
      "Number(document.getElementById('result').innerHTML == 'Selected')");
  EXPECT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(check_did_select, &did_select));
  EXPECT_EQ(1, did_select);
}
#endif

TEST_F(RenderViewImplTest, TestBackForward) {
  LoadHTML("<div id=pagename>Page A</div>");
  PageState page_a_state = GetCurrentPageState();
  int was_page_a = -1;
  base::string16 check_page_a = base::ASCIIToUTF16(
      "Number(document.getElementById('pagename').innerHTML == 'Page A')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
  EXPECT_EQ(1, was_page_a);

  LoadHTML("<div id=pagename>Page B</div>");
  int was_page_b = -1;
  base::string16 check_page_b = base::ASCIIToUTF16(
      "Number(document.getElementById('pagename').innerHTML == 'Page B')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  PageState back_state = GetCurrentPageState();

  LoadHTML("<div id=pagename>Page C</div>");
  int was_page_c = -1;
  base::string16 check_page_c = base::ASCIIToUTF16(
      "Number(document.getElementById('pagename').innerHTML == 'Page C')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_c, &was_page_c));
  EXPECT_EQ(1, was_page_c);

  PageState forward_state = GetCurrentPageState();

  // Go back.
  GoBack(GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>"),
         back_state);

  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);
  PageState back_state2 = GetCurrentPageState();

  // Go forward.
  GoForward(GURL("data:text/html;charset=utf-8,<div id=pagename>Page C</div>"),
            forward_state);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_c, &was_page_c));
  EXPECT_EQ(1, was_page_c);

  // Go back.
  GoBack(GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>"),
         back_state2);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  forward_state = GetCurrentPageState();

  // Go back.
  GoBack(GURL("data:text/html;charset=utf-8,<div id=pagename>Page A</div>"),
         page_a_state);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
  EXPECT_EQ(1, was_page_a);

  // Go forward.
  GoForward(GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>"),
            forward_state);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);
}

#if defined(OS_MAC) || defined(USE_AURA)
TEST_F(RenderViewImplTest, GetCompositionCharacterBoundsTest) {
  LoadHTML("<textarea id=\"test\" cols=\"100\"></textarea>");
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");

  auto* widget_input_handler = GetWidgetInputHandler();
  const base::string16 empty_string;
  const std::vector<ui::ImeTextSpan> empty_ime_text_span;
  std::vector<gfx::Rect> bounds;
  widget_input_handler->SetFocus(true);

  // ASCII composition
  const base::string16 ascii_composition = base::UTF8ToUTF16("aiueo");
  widget_input_handler->ImeSetComposition(
      ascii_composition, empty_ime_text_span, gfx::Range::InvalidRange(), 0, 0);
  bounds = LastCompositionBounds();
  ASSERT_EQ(ascii_composition.size(), bounds.size());

  for (const gfx::Rect& r : bounds)
    EXPECT_LT(0, r.width());
  widget_input_handler->ImeCommitText(
      empty_string, std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(),
      0, base::DoNothing());

  // Non surrogate pair unicode character.
  const base::string16 unicode_composition = base::UTF8ToUTF16(
      "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A");
  widget_input_handler->ImeSetComposition(unicode_composition,
                                          empty_ime_text_span,
                                          gfx::Range::InvalidRange(), 0, 0);
  bounds = LastCompositionBounds();
  ASSERT_EQ(unicode_composition.size(), bounds.size());
  for (const gfx::Rect& r : bounds)
    EXPECT_LT(0, r.width());
  widget_input_handler->ImeCommitText(empty_string, empty_ime_text_span,
                                      gfx::Range::InvalidRange(), 0,
                                      base::DoNothing());

  // Surrogate pair character.
  const base::string16 surrogate_pair_char =
      base::UTF8ToUTF16("\xF0\xA0\xAE\x9F");
  widget_input_handler->ImeSetComposition(surrogate_pair_char,
                                          empty_ime_text_span,
                                          gfx::Range::InvalidRange(), 0, 0);
  bounds = LastCompositionBounds();
  ASSERT_EQ(surrogate_pair_char.size(), bounds.size());
  EXPECT_LT(0, bounds[0].width());
  EXPECT_EQ(0, bounds[1].width());
  widget_input_handler->ImeCommitText(empty_string, empty_ime_text_span,
                                      gfx::Range::InvalidRange(), 0,
                                      base::DoNothing());

  // Mixed string.
  const base::string16 surrogate_pair_mixed_composition =
      surrogate_pair_char + base::UTF8ToUTF16("\xE3\x81\x82") +
      surrogate_pair_char + base::UTF8ToUTF16("b") + surrogate_pair_char;
  const size_t utf16_length = 8UL;
  const bool is_surrogate_pair_empty_rect[8] = {false, true,  false, false,
                                                true,  false, false, true};
  widget_input_handler->ImeSetComposition(surrogate_pair_mixed_composition,
                                          empty_ime_text_span,
                                          gfx::Range::InvalidRange(), 0, 0);
  bounds = LastCompositionBounds();
  ASSERT_EQ(utf16_length, bounds.size());
  for (size_t i = 0; i < utf16_length; ++i) {
    if (is_surrogate_pair_empty_rect[i]) {
      EXPECT_EQ(0, bounds[i].width());
    } else {
      EXPECT_LT(0, bounds[i].width());
    }
  }
  widget_input_handler->ImeCommitText(empty_string, empty_ime_text_span,
                                      gfx::Range::InvalidRange(), 0,
                                      base::DoNothing());
}
#endif

TEST_F(RenderViewImplTest, SetEditableSelectionAndComposition) {
  // Load an HTML page consisting of an input field.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test1\" value=\"some test text hello\"></input>"
      "</body>"
      "</html>");
  auto* frame_widget_input_handler = GetFrameWidgetInputHandler();
  ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
  frame_widget_input_handler->SetEditableSelectionOffsets(4, 8);
  const std::vector<ui::ImeTextSpan> empty_ime_text_span;
  frame_widget_input_handler->SetCompositionFromExistingText(
      7, 10, empty_ime_text_span);
  base::RunLoop().RunUntilIdle();
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  blink::WebTextInputInfo info = controller->TextInputInfo();
  EXPECT_EQ(4, info.selection_start);
  EXPECT_EQ(8, info.selection_end);
  EXPECT_EQ(7, info.composition_start);
  EXPECT_EQ(10, info.composition_end);
  frame_widget_input_handler->CollapseSelection();
  base::RunLoop().RunUntilIdle();
  info = controller->TextInputInfo();
  EXPECT_EQ(8, info.selection_start);
  EXPECT_EQ(8, info.selection_end);
}

TEST_F(RenderViewImplTest, OnExtendSelectionAndDelete) {
  // Load an HTML page consisting of an input field.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test1\" value=\"abcdefghijklmnopqrstuvwxyz\"></input>"
      "</body>"
      "</html>");
  auto* frame_widget_input_handler = GetFrameWidgetInputHandler();
  ExecuteJavaScriptForTests("document.getElementById('test1').focus();");
  frame_widget_input_handler->SetEditableSelectionOffsets(10, 10);
  frame_widget_input_handler->ExtendSelectionAndDelete(3, 4);
  base::RunLoop().RunUntilIdle();
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  blink::WebTextInputInfo info = controller->TextInputInfo();
  EXPECT_EQ("abcdefgopqrstuvwxyz", info.value);
  EXPECT_EQ(7, info.selection_start);
  EXPECT_EQ(7, info.selection_end);
  frame_widget_input_handler->SetEditableSelectionOffsets(4, 8);
  frame_widget_input_handler->ExtendSelectionAndDelete(2, 5);
  base::RunLoop().RunUntilIdle();
  info = controller->TextInputInfo();
  EXPECT_EQ("abuvwxyz", info.value);
  EXPECT_EQ(2, info.selection_start);
  EXPECT_EQ(2, info.selection_end);
}

TEST_F(RenderViewImplTest, OnDeleteSurroundingText) {
  // Load an HTML page consisting of an input field.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<body>"
      "<input id=\"test1\" value=\"abcdefghijklmnopqrstuvwxyz\"></input>"
      "</body>"
      "</html>");
  ExecuteJavaScriptForTests("document.getElementById('test1').focus();");

  auto* frame_widget_input_handler = GetFrameWidgetInputHandler();
  frame_widget_input_handler->SetEditableSelectionOffsets(10, 10);
  frame_widget_input_handler->DeleteSurroundingText(3, 4);
  base::RunLoop().RunUntilIdle();
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  blink::WebTextInputInfo info = controller->TextInputInfo();
  EXPECT_EQ("abcdefgopqrstuvwxyz", info.value);
  EXPECT_EQ(7, info.selection_start);
  EXPECT_EQ(7, info.selection_end);

  frame_widget_input_handler->SetEditableSelectionOffsets(4, 8);
  frame_widget_input_handler->DeleteSurroundingText(2, 5);
  base::RunLoop().RunUntilIdle();
  info = controller->TextInputInfo();
  EXPECT_EQ("abefgouvwxyz", info.value);
  EXPECT_EQ(2, info.selection_start);
  EXPECT_EQ(6, info.selection_end);

  frame_widget_input_handler->SetEditableSelectionOffsets(5, 5);
  frame_widget_input_handler->DeleteSurroundingText(10, 0);
  base::RunLoop().RunUntilIdle();
  info = controller->TextInputInfo();
  EXPECT_EQ("ouvwxyz", info.value);
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);

  frame_widget_input_handler->DeleteSurroundingText(0, 10);
  base::RunLoop().RunUntilIdle();
  info = controller->TextInputInfo();
  EXPECT_EQ("", info.value);
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);

  frame_widget_input_handler->DeleteSurroundingText(10, 10);
  base::RunLoop().RunUntilIdle();
  info = controller->TextInputInfo();
  EXPECT_EQ("", info.value);

  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
}

#if defined(OS_ANDROID)
// Failing on Android M: http://crbug.com/873580
#define MAYBE_OnDeleteSurroundingTextInCodePoints \
  DISABLED_OnDeleteSurroundingTextInCodePoints
#else
#define MAYBE_OnDeleteSurroundingTextInCodePoints \
  OnDeleteSurroundingTextInCodePoints
#endif
TEST_F(RenderViewImplTest, MAYBE_OnDeleteSurroundingTextInCodePoints) {
  // Load an HTML page consisting of an input field.
  LoadHTML(
      // "ab" + trophy + space + "cdef" + trophy + space + "gh".
      "<input id=\"test1\" value=\"ab&#x1f3c6; cdef&#x1f3c6; gh\">");
  ExecuteJavaScriptForTests("document.getElementById('test1').focus();");

  auto* frame_widget_input_handler = GetFrameWidgetInputHandler();
  frame_widget_input_handler->SetEditableSelectionOffsets(4, 4);
  frame_widget_input_handler->DeleteSurroundingTextInCodePoints(2, 2);
  base::RunLoop().RunUntilIdle();
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  blink::WebTextInputInfo info = controller->TextInputInfo();
  // "a" + "def" + trophy + space + "gh".
  EXPECT_EQ(WebString::FromUTF8("adef\xF0\x9F\x8F\x86 gh"), info.value);
  EXPECT_EQ(1, info.selection_start);
  EXPECT_EQ(1, info.selection_end);

  frame_widget_input_handler->SetEditableSelectionOffsets(1, 3);
  frame_widget_input_handler->DeleteSurroundingTextInCodePoints(1, 4);
  base::RunLoop().RunUntilIdle();
  info = controller->TextInputInfo();
  EXPECT_EQ("deh", info.value);
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(2, info.selection_end);
}

// Test that the navigating specific frames works correctly.
TEST_F(RenderViewImplTest, NavigateSubframe) {
  // Load page A.
  LoadHTML("hello <iframe srcdoc='fail' name='frame'></iframe>");

  // Navigate the frame only.
  auto common_params = CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,world");
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;
  common_params->navigation_start = base::TimeTicks::FromInternalValue(1);
  auto commit_params = CreateCommitNavigationParams();
  commit_params->current_history_list_length = 1;
  commit_params->current_history_list_offset = 0;
  commit_params->pending_history_list_offset = 1;

  TestRenderFrame* subframe =
      static_cast<TestRenderFrame*>(RenderFrameImpl::FromWebFrame(
          frame()->GetWebFrame()->FindFrameByName("frame")));
  FrameLoadWaiter waiter(subframe);
  subframe->Navigate(std::move(common_params), std::move(commit_params));
  waiter.Wait();

  // Copy the document content to std::string and compare with the
  // expected result.
  const int kMaxOutputCharacters = 256;
  std::string output = WebFrameContentDumper::DumpWebViewAsText(
                           view()->GetWebView(), kMaxOutputCharacters)
                           .Utf8();
  EXPECT_EQ(output, "hello \n\nworld");
}

// This test ensures that a RenderFrame object is created for the top level
// frame in the RenderView.
TEST_F(RenderViewImplTest, BasicRenderFrame) {
  EXPECT_TRUE(view()->main_render_frame_);
}

namespace {
class MessageOrderFakeRenderWidgetHost : public FakeRenderWidgetHost {
 public:
  MOCK_METHOD1(TextInputStateChanged, void(ui::mojom::TextInputStatePtr state));
};

class TextSelectionChangedLocalFrameHost : public LocalFrameHostInterceptor {
 public:
  explicit TextSelectionChangedLocalFrameHost(
      blink::AssociatedInterfaceProvider* provider)
      : LocalFrameHostInterceptor(provider) {}
  MOCK_METHOD3(TextSelectionChanged,
               void(const base::string16& text,
                    uint32_t offset,
                    const gfx::Range& range));
};
}  // namespace

class RenderViewImplTextInputMessageOrder : public RenderViewImplTest {
 public:
  using MockedTestRenderFrame = MockedLocalFrameHostInterceptorTestRenderFrame<
      TextSelectionChangedLocalFrameHost>;

  RenderViewImplTextInputMessageOrder()
      : RenderViewImplTest(&MockedTestRenderFrame::CreateTestRenderFrame) {}

  std::unique_ptr<FakeRenderWidgetHost> CreateRenderWidgetHost() override {
    return std::make_unique<MessageOrderFakeRenderWidgetHost>();
  }

  MessageOrderFakeRenderWidgetHost* GetMessageOrderFakeRenderWidgetHost() {
    return static_cast<MessageOrderFakeRenderWidgetHost*>(
        render_widget_host_.get());
  }

  TextSelectionChangedLocalFrameHost* GetMockLocalFrameHost() {
    return static_cast<MockedTestRenderFrame*>(frame())
        ->mock_local_frame_host();
  }
};

TEST_F(RenderViewImplTextInputMessageOrder, MessageOrderInDidChangeSelection) {
  LoadHTML("<textarea id=\"test\"></textarea>");

  // TextInputStateChanged should be called earlier than TextSelectionChanged.
  testing::InSequence sequence;

  // TextInputStateChanged and TextSelectionChanged should be called once each.
  EXPECT_CALL(*GetMessageOrderFakeRenderWidgetHost(),
              TextInputStateChanged(testing::_))
      .Times(1);
  EXPECT_CALL(*GetMockLocalFrameHost(),
              TextSelectionChanged(testing::_, testing::_, testing::_))
      .Times(1);

  main_widget()->SetHandlingInputEvent(true);
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
}

class RendererErrorPageTest : public RenderViewImplTest {
 public:
  ContentRendererClient* CreateContentRendererClient() override {
    return new TestContentRendererClient;
  }

  RenderViewImpl* view() { return static_cast<RenderViewImpl*>(view_); }

  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(view()->GetMainRenderFrame());
  }

 private:
  class TestContentRendererClient : public ContentRendererClient {
   public:
    void PrepareErrorPage(content::RenderFrame* render_frame,
                          const blink::WebURLError& error,
                          const std::string& http_method,
                          std::string* error_html) override {
      if (error_html)
        *error_html = "A suffusion of yellow.";
    }

    void PrepareErrorPageForHttpStatusError(content::RenderFrame* render_frame,
                                            const GURL& unreachable_url,
                                            const std::string& http_method,
                                            int http_status,
                                            std::string* error_html) override {
      if (error_html)
        *error_html = "A suffusion of yellow.";
    }

    bool HasErrorPage(int http_status_code) override { return true; }
  };
};

TEST_F(RendererErrorPageTest, RegularError) {
  auto common_params = CreateCommonNavigationParams();
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->url = GURL("http://example.com/error-page");
  TestRenderFrame* main_frame = static_cast<TestRenderFrame*>(frame());
  main_frame->NavigateWithError(
      std::move(common_params), CreateCommitNavigationParams(),
      net::ERR_FILE_NOT_FOUND, net::ResolveErrorInfo(net::OK),
      "A suffusion of yellow.");

  // The error page itself is loaded asynchronously.
  FrameLoadWaiter(main_frame).Wait();
  const int kMaxOutputCharacters = 22;
  EXPECT_EQ("A suffusion of yellow.",
            WebFrameContentDumper::DumpWebViewAsText(view()->GetWebView(),
                                                     kMaxOutputCharacters)
                .Ascii());
}

TEST_F(RendererErrorPageTest, HttpStatusCodeErrorWithEmptyBody) {
  // Start a load that will reach provisional state synchronously,
  // but won't complete synchronously.
  auto common_params = CreateCommonNavigationParams();
  common_params->navigation_type = mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->url = GURL("data:text/html,test data");

  // Emulate a 503 main resource response with an empty body.
  auto head = network::mojom::URLResponseHead::New();
  std::string headers(
      "HTTP/1.1 503 SERVICE UNAVAILABLE\nContent-type: text/html\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));

  TestRenderFrame* main_frame = static_cast<TestRenderFrame*>(frame());
  main_frame->Navigate(std::move(head), std::move(common_params),
                       CreateCommitNavigationParams());
  main_frame->DidFinishDocumentLoad();
  main_frame->RunScriptsAtDocumentReady(true);

  // The error page itself is loaded asynchronously.
  FrameLoadWaiter(main_frame).Wait();
  const int kMaxOutputCharacters = 22;
  EXPECT_EQ("A suffusion of yellow.",
            WebFrameContentDumper::DumpWebViewAsText(view()->GetWebView(),
                                                     kMaxOutputCharacters)
                .Ascii());
}

TEST_F(RenderViewImplTest, SetAccessibilityMode) {
  ASSERT_TRUE(GetAccessibilityMode().is_mode_off());
  ASSERT_TRUE(GetRenderAccessibilityManager());
  ASSERT_FALSE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());

  GetRenderAccessibilityManager()->SetMode(ui::kAXModeWebContentsOnly.mode());
  ASSERT_TRUE(GetAccessibilityMode() == ui::kAXModeWebContentsOnly);
  ASSERT_TRUE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());

  GetRenderAccessibilityManager()->SetMode(0);
  ASSERT_TRUE(GetAccessibilityMode().is_mode_off());
  ASSERT_FALSE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());

  GetRenderAccessibilityManager()->SetMode(ui::kAXModeComplete.mode());
  ASSERT_TRUE(GetAccessibilityMode() == ui::kAXModeComplete);
  ASSERT_TRUE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());
}

TEST_F(RenderViewImplTest, AccessibilityModeOnClosingConnection) {
  // Force the RenderAccessibilityManager to bind a pending receiver so that we
  // can test what happens after closing the remote endpoint.
  mojo::AssociatedRemote<mojom::RenderAccessibility> remote;
  GetRenderAccessibilityManager()->BindReceiver(
      remote.BindNewEndpointAndPassReceiver());

  GetRenderAccessibilityManager()->SetMode(ui::kAXModeWebContentsOnly.mode());
  ASSERT_TRUE(GetAccessibilityMode() == ui::kAXModeWebContentsOnly);
  ASSERT_TRUE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());

  // Closing the remote endpoint of the mojo pipe gets accessibility disabled
  // for the frame and the RenderAccessibility object deleted.
  remote.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetRenderAccessibilityManager());
  ASSERT_TRUE(GetAccessibilityMode().is_mode_off());
  ASSERT_FALSE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());
}

// Checks that when a navigation starts in the renderer, |navigation_start| is
// recorded at an appropriate time and is passed in the corresponding message.
TEST_F(RenderViewImplTest, RendererNavigationStartTransmittedToBrowser) {
  base::TimeTicks lower_bound_navigation_start(base::TimeTicks::Now());
  FrameLoadWaiter waiter(frame());
  frame()->LoadHTMLString("hello world", GURL("data:text/html,"), "UTF-8",
                          GURL(), false /* replace_current_item */);
  waiter.Wait();
  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  EXPECT_FALSE(navigation_state->common_params().navigation_start.is_null());
  EXPECT_LE(lower_bound_navigation_start,
            navigation_state->common_params().navigation_start);
}

// Checks that a browser-initiated navigation in an initial document that was
// not accessed uses browser-side timestamp.
// This test assumes that |frame()| contains an unaccessed initial document at
// start.
TEST_F(RenderViewImplTest, BrowserNavigationStart) {
  auto common_params = MakeCommonNavigationParams(-TimeDelta::FromSeconds(1));

  FrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), CreateCommitNavigationParams());
  waiter.Wait();
  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  EXPECT_EQ(common_params->navigation_start,
            navigation_state->common_params().navigation_start);
}

// Sanity check for the Navigation Timing API |navigationStart| override. We
// are asserting only most basic constraints, as TimeTicks (passed as the
// override) are not comparable with the wall time (returned by the Blink API).
TEST_F(RenderViewImplTest, BrowserNavigationStartSanitized) {
  // Verify that a navigation that claims to have started in the future - 42
  // days from now is *not* reported as one that starts in the future; as we
  // sanitize the override allowing a maximum of ::Now().
  auto late_common_params = MakeCommonNavigationParams(TimeDelta::FromDays(42));
  late_common_params->method = "POST";

  frame()->Navigate(late_common_params.Clone(), CreateCommitNavigationParams());
  base::RunLoop().RunUntilIdle();
  base::Time after_navigation =
      base::Time::Now() + base::TimeDelta::FromDays(1);

  base::Time late_nav_reported_start =
      base::Time::FromDoubleT(GetMainFrame()->Performance().NavigationStart());
  EXPECT_LE(late_nav_reported_start, after_navigation);
}

// Checks that a browser-initiated navigation in an initial document that has
// been accessed uses browser-side timestamp (there may be arbitrary
// content and/or scripts injected, including beforeunload handler that shows
// a confirmation dialog).
TEST_F(RenderViewImplTest, NavigationStartWhenInitialDocumentWasAccessed) {
  // Trigger a didAccessInitialDocument notification.
  ExecuteJavaScriptForTests("document.title = 'Hi!';");

  auto common_params = MakeCommonNavigationParams(-TimeDelta::FromSeconds(1));
  FrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), CreateCommitNavigationParams());
  waiter.Wait();
  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  EXPECT_EQ(common_params->navigation_start,
            navigation_state->common_params().navigation_start);
}

TEST_F(RenderViewImplTest, NavigationStartForReload) {
  const char url_string[] = "data:text/html,<div>Page</div>";
  // Navigate once, then reload.
  LoadHTML(url_string);
  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  auto common_params = CreateCommonNavigationParams();
  common_params->url = GURL(url_string);
  common_params->navigation_type =
      mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL;
  common_params->transition = ui::PAGE_TRANSITION_RELOAD;

  // The browser navigation_start should not be used because beforeunload will
  // be fired during Navigate.
  FrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), CreateCommitNavigationParams());
  waiter.Wait();

  // The browser navigation_start is always used.
  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  EXPECT_EQ(common_params->navigation_start,
            navigation_state->common_params().navigation_start);
}

TEST_F(RenderViewImplTest, NavigationStartForSameProcessHistoryNavigation) {
  LoadHTML("<div id=pagename>Page A</div>");
  LoadHTML("<div id=pagename>Page B</div>");
  PageState back_state = GetCurrentPageState();
  LoadHTML("<div id=pagename>Page C</div>");
  PageState forward_state = GetCurrentPageState();
  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  // Go back.
  auto common_params_back = CreateCommonNavigationParams();
  common_params_back->url =
      GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>");
  common_params_back->transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  common_params_back->navigation_type =
      mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
  GoToOffsetWithParams(-1, back_state, common_params_back.Clone(),
                       CreateCommitNavigationParams());
  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());

  // The browser navigation_start is always used.
  EXPECT_EQ(common_params_back->navigation_start,
            navigation_state->common_params().navigation_start);

  // Go forward.
  auto common_params_forward = CreateCommonNavigationParams();
  common_params_forward->url =
      GURL("data:text/html;charset=utf-8,<div id=pagename>Page C</div>");
  common_params_forward->transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  common_params_forward->navigation_type =
      mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
  GoToOffsetWithParams(1, forward_state, common_params_forward.Clone(),
                       CreateCommitNavigationParams());
  navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  EXPECT_EQ(common_params_forward->navigation_start,
            navigation_state->common_params().navigation_start);
}

TEST_F(RenderViewImplTest, NavigationStartForCrossProcessHistoryNavigation) {
  auto common_params = MakeCommonNavigationParams(-TimeDelta::FromSeconds(1));
  common_params->transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  common_params->navigation_type =
      mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;

  auto commit_params = CreateCommitNavigationParams();
  commit_params->page_state =
      PageState::CreateForTesting(common_params->url, false, nullptr, nullptr);
  commit_params->nav_entry_id = 42;
  commit_params->pending_history_list_offset = 1;
  commit_params->current_history_list_offset = 0;
  commit_params->current_history_list_length = 1;
  FrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), std::move(commit_params));
  waiter.Wait();

  NavigationState* navigation_state = NavigationState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  EXPECT_EQ(common_params->navigation_start,
            navigation_state->common_params().navigation_start);
}

TEST_F(RenderViewImplTest, PreferredSizeZoomed) {
  LoadHTML(
      "<body style='margin:0;'>"
      "<div style='display:inline-block; "
      "width:400px; height:400px;'/></body>");

  // For unknown reasons, setting fixed scrollbar width using
  // ::-webkit-scrollbar makes Mac bots flaky (crbug.com/785088).
  // Measure native scrollbar width instead.
  int scrollbar_width = GetScrollbarWidth();
  EnablePreferredSizeMode();

  gfx::Size size = GetPreferredSize();
  EXPECT_EQ(gfx::Size(400 + scrollbar_width, 400), size);

  main_frame_widget()->SetZoomLevelForTesting(
      blink::PageZoomFactorToZoomLevel(2.0));
  size = GetPreferredSize();
  EXPECT_EQ(gfx::Size(800 + scrollbar_width, 800), size);
}

TEST_F(RenderViewImplScaleFactorTest, PreferredSizeWithScaleFactor) {
  LoadHTML(
      "<body style='margin:0;'><div style='display:inline-block; "
      "width:400px; height:400px;'/></body>");

  // For unknown reasons, setting fixed scrollbar width using
  // ::-webkit-scrollbar makes Mac bots flaky (crbug.com/785088).
  // Measure native scrollbar width instead.
  int scrollbar_width = GetScrollbarWidth();
  EnablePreferredSizeMode();

  gfx::Size size = GetPreferredSize();
  EXPECT_EQ(gfx::Size(400 + scrollbar_width, 400), size);

  // The size is in DIP. Changing the scale factor should not change
  // the preferred size. (Caveat: a page may apply different layout for
  // high DPI, in which case, the size may differ.)
  SetDeviceScaleFactor(2.f);
  size = GetPreferredSize();
  EXPECT_EQ(gfx::Size(400 + scrollbar_width, 400), size);
}

// Ensure the RenderViewImpl history list is properly updated when starting a
// new browser-initiated navigation.
TEST_F(RenderViewImplTest, HistoryIsProperlyUpdatedOnNavigation) {
  EXPECT_EQ(0, view()->HistoryBackListCount());
  EXPECT_EQ(0, view()->HistoryBackListCount() +
                   view()->HistoryForwardListCount() + 1);

  // Receive a CommitNavigation message with history parameters.
  auto commit_params = CreateCommitNavigationParams();
  commit_params->current_history_list_offset = 1;
  commit_params->current_history_list_length = 2;
  frame()->Navigate(CreateCommonNavigationParams(), std::move(commit_params));

  // The current history list in RenderView is updated.
  EXPECT_EQ(1, view()->HistoryBackListCount());
  EXPECT_EQ(2, view()->HistoryBackListCount() +
                   view()->HistoryForwardListCount() + 1);
}

// Ensure the RenderViewImpl history list is properly updated when starting a
// new history browser-initiated navigation.
TEST_F(RenderViewImplTest, HistoryIsProperlyUpdatedOnHistoryNavigation) {
  EXPECT_EQ(0, view()->HistoryBackListCount());
  EXPECT_EQ(0, view()->HistoryBackListCount() +
                   view()->HistoryForwardListCount() + 1);

  // Receive a CommitNavigation message with history parameters.
  auto commit_params = CreateCommitNavigationParams();
  commit_params->current_history_list_offset = 1;
  commit_params->current_history_list_length = 25;
  commit_params->pending_history_list_offset = 12;
  commit_params->nav_entry_id = 777;
  frame()->Navigate(CreateCommonNavigationParams(), std::move(commit_params));

  // The current history list in RenderView is updated.
  EXPECT_EQ(12, view()->HistoryBackListCount());
  EXPECT_EQ(25, view()->HistoryBackListCount() +
                    view()->HistoryForwardListCount() + 1);
}

// Ensure the RenderViewImpl history list is properly updated when starting a
// new history browser-initiated navigation with should_clear_history_list
TEST_F(RenderViewImplTest, HistoryIsProperlyUpdatedOnShouldClearHistoryList) {
  EXPECT_EQ(0, view()->HistoryBackListCount());
  EXPECT_EQ(0, view()->HistoryBackListCount() +
                   view()->HistoryForwardListCount() + 1);

  // Receive a CommitNavigation message with history parameters.
  auto commit_params = CreateCommitNavigationParams();
  commit_params->current_history_list_offset = 12;
  commit_params->current_history_list_length = 25;
  commit_params->should_clear_history_list = true;
  frame()->Navigate(CreateCommonNavigationParams(), std::move(commit_params));

  // The current history list in RenderView is updated.
  EXPECT_EQ(0, view()->HistoryBackListCount());
  EXPECT_EQ(1, view()->HistoryBackListCount() +
                   view()->HistoryForwardListCount() + 1);
}

// Tests that there's no UaF after dispatchBeforeUnloadEvent.
// See https://crbug.com/666714.
TEST_F(RenderViewImplTest, DispatchBeforeUnloadCanDetachFrame) {
  LoadHTML(
      "<script>window.onbeforeunload = function() { "
      "window.console.log('OnBeforeUnload called'); }</script>");

  // Create a callback that unloads the frame when the 'OnBeforeUnload called'
  // log is printed from the beforeunload handler.
  base::RunLoop run_loop;
  bool was_callback_run = false;
  frame()->SetDidAddMessageToConsoleCallback(
      base::BindOnce(base::BindLambdaForTesting([&](const base::string16& msg) {
        // Makes sure this happens during the beforeunload handler.
        EXPECT_EQ(base::UTF8ToUTF16("OnBeforeUnload called"), msg);

        // Unloads the main frame.
        frame()->OnMessageReceived(UnfreezableFrameMsg_Unload(
            frame()->GetRoutingID(), 1, false, FrameReplicationState(),
            base::UnguessableToken::Create()));

        was_callback_run = true;
        run_loop.Quit();
      })));

  // Simulate a BeforeUnload IPC received from the browser.
  frame()->SimulateBeforeUnload(false);

  run_loop.Run();
  ASSERT_TRUE(was_callback_run);
}

namespace {
class AlertDialogMockLocalFrameHost : public LocalFrameHostInterceptor {
 public:
  explicit AlertDialogMockLocalFrameHost(
      blink::AssociatedInterfaceProvider* provider)
      : LocalFrameHostInterceptor(provider) {}

  MOCK_METHOD2(RunModalAlertDialog,
               void(const base::string16& alert_message,
                    RunModalAlertDialogCallback callback));
};
}  // namespace

class RenderViewImplModalDialogTest : public RenderViewImplTest {
 public:
  using MockedTestRenderFrame = MockedLocalFrameHostInterceptorTestRenderFrame<
      AlertDialogMockLocalFrameHost>;

  RenderViewImplModalDialogTest()
      : RenderViewImplTest(&MockedTestRenderFrame::CreateTestRenderFrame) {}

  AlertDialogMockLocalFrameHost* alert_mock_frame_host() {
    return static_cast<MockedTestRenderFrame*>(frame())
        ->mock_local_frame_host();
  }
};

// Test that invoking one of the modal dialogs doesn't crash.
TEST_F(RenderViewImplModalDialogTest, ModalDialogs) {
  LoadHTML("<body></body>");
  base::string16 alert_message = base::UTF8ToUTF16("Please don't crash");
  EXPECT_CALL(*alert_mock_frame_host(),
              RunModalAlertDialog(alert_message, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>());
  frame()->GetWebFrame()->Alert(WebString::FromUTF16(alert_message));
}

TEST_F(RenderViewImplBlinkSettingsTest, Default) {
  DoSetUp();
  EXPECT_FALSE(settings()->ViewportEnabled());
}

TEST_F(RenderViewImplBlinkSettingsTest, CommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      blink::switches::kBlinkSettings, "viewportEnabled=true");
  DoSetUp();
  EXPECT_TRUE(settings()->ViewportEnabled());
}

// Ensure that setting default page scale limits immediately recomputes the
// minimum scale factor to the final value. With
// shrinks_viewport_contents_to_fit, Blink clamps the minimum cased on the
// content width. In this case, that'll always be 1.
TEST_F(RenderViewImplBlinkSettingsTest, DefaultPageScaleSettings) {
  DoSetUp();
  LoadHTML(
      "<style>"
      "    body,html {"
      "    margin: 0;"
      "    width:100%;"
      "    height:100%;"
      "}"
      "</style>");

  EXPECT_EQ(1.f, view()->GetWebView()->PageScaleFactor());
  EXPECT_EQ(1.f, view()->GetWebView()->MinimumPageScaleFactor());

  blink::web_pref::WebPreferences prefs;
  prefs.shrinks_viewport_contents_to_fit = true;
  prefs.default_minimum_page_scale_factor = 0.1f;
  prefs.default_maximum_page_scale_factor = 5.5f;
  view()->SetBlinkPreferences(prefs);

  EXPECT_EQ(1.f, view()->GetWebView()->PageScaleFactor());
  EXPECT_EQ(1.f, view()->GetWebView()->MinimumPageScaleFactor());
  EXPECT_EQ(5.5f, view()->GetWebView()->MaximumPageScaleFactor());
}

TEST_F(RenderViewImplDisableZoomForDSFTest,
       ConverViewportToWindowWithoutZoomForDSF) {
  SetDeviceScaleFactor(2.f);
  blink::WebRect rect(20, 10, 200, 100);
  main_widget()->ConvertViewportToWindow(&rect);
  EXPECT_EQ(20, rect.x);
  EXPECT_EQ(10, rect.y);
  EXPECT_EQ(200, rect.width);
  EXPECT_EQ(100, rect.height);
}

TEST_F(RenderViewImplScaleFactorTest, ScreenMetricsEmulationWithOriginalDSF1) {
  SetDeviceScaleFactor(1.f);

  LoadHTML("<body style='min-height:1000px;'></body>");
  {
    SCOPED_TRACE("327x415 1dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.f, 1.f);
  }
  {
    SCOPED_TRACE("327x415 1.5dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.5f, 1.f);
  }
  {
    SCOPED_TRACE("1005x1102 2dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 2.f, 1.f);
  }
  {
    SCOPED_TRACE("1005x1102 3dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 3.f, 1.f);
  }

  ReceiveDisableDeviceEmulation(view());

  blink::DeviceEmulationParams params;
  ReceiveEnableDeviceEmulation(view(), params);
  // Don't disable here to test that emulation is being shutdown properly.
}

TEST_F(RenderViewImplScaleFactorTest, ScreenMetricsEmulationWithOriginalDSF2) {
  SetDeviceScaleFactor(2.f);
  float compositor_dsf = compositor_deps_->IsUseZoomForDSFEnabled() ? 1.f : 2.f;

  LoadHTML("<body style='min-height:1000px;'></body>");
  {
    SCOPED_TRACE("327x415 1dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.f, compositor_dsf);
  }
  {
    SCOPED_TRACE("327x415 1.5dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.5f, compositor_dsf);
  }
  {
    SCOPED_TRACE("1005x1102 2dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 2.f, compositor_dsf);
  }
  {
    SCOPED_TRACE("1005x1102 3dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 3.f, compositor_dsf);
  }

  ReceiveDisableDeviceEmulation(view());

  blink::DeviceEmulationParams params;
  ReceiveEnableDeviceEmulation(view(), params);
  // Don't disable here to test that emulation is being shutdown properly.
}

TEST_F(RenderViewImplEnableZoomForDSFTest,
       ConverViewportToWindowWithZoomForDSF) {
  SetDeviceScaleFactor(1.f);
  {
    blink::WebRect rect(20, 10, 200, 100);
    main_widget()->ConvertViewportToWindow(&rect);
    EXPECT_EQ(20, rect.x);
    EXPECT_EQ(10, rect.y);
    EXPECT_EQ(200, rect.width);
    EXPECT_EQ(100, rect.height);
  }

  SetDeviceScaleFactor(2.f);
  {
    blink::WebRect rect(20, 10, 200, 100);
    main_widget()->ConvertViewportToWindow(&rect);
    EXPECT_EQ(10, rect.x);
    EXPECT_EQ(5, rect.y);
    EXPECT_EQ(100, rect.width);
    EXPECT_EQ(50, rect.height);
  }
}

#if defined(OS_MAC) || defined(USE_AURA)
TEST_F(RenderViewImplEnableZoomForDSFTest,
       DISABLED_GetCompositionCharacterBoundsTest) {  // http://crbug.com/582016
  SetDeviceScaleFactor(1.f);
#if defined(OS_WIN)
  // http://crbug.com/508747
  if (base::win::GetVersion() >= base::win::Version::WIN10)
    return;
#endif

  LoadHTML("<textarea id=\"test\"></textarea>");
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");

  auto* widget_input_handler = GetWidgetInputHandler();
  const base::string16 empty_string;
  const std::vector<ui::ImeTextSpan> empty_ime_text_span;
  std::vector<gfx::Rect> bounds_at_1x;
  widget_input_handler->SetFocus(true);

  // ASCII composition
  const base::string16 ascii_composition = base::UTF8ToUTF16("aiueo");
  widget_input_handler->ImeSetComposition(
      ascii_composition, empty_ime_text_span, gfx::Range::InvalidRange(), 0, 0);
  bounds_at_1x = LastCompositionBounds();
  ASSERT_EQ(ascii_composition.size(), bounds_at_1x.size());

  SetDeviceScaleFactor(2.f);
  std::vector<gfx::Rect> bounds_at_2x = LastCompositionBounds();
  ASSERT_EQ(bounds_at_1x.size(), bounds_at_2x.size());
  for (size_t i = 0; i < bounds_at_1x.size(); i++) {
    const gfx::Rect& b1 = bounds_at_1x[i];
    const gfx::Rect& b2 = bounds_at_2x[i];
    gfx::Vector2d origin_diff = b1.origin() - b2.origin();

    // The bounds may not be exactly same because the font metrics are different
    // at 1x and 2x. Just make sure that the difference is small.
    EXPECT_LT(origin_diff.x(), 2);
    EXPECT_LT(origin_diff.y(), 2);
    EXPECT_LT(std::abs(b1.width() - b2.width()), 3);
    EXPECT_LT(std::abs(b1.height() - b2.height()), 2);
  }
}
#endif

#if !defined(OS_ANDROID)
// No extensions/autoresize on Android.
namespace {

// Don't use text as it text will change the size in DIP at different
// scale factor.
const char kAutoResizeTestPage[] =
    "<div style='width=20px; height=20px'></div>";

}  // namespace

TEST_F(RenderViewImplEnableZoomForDSFTest, AutoResizeWithZoomForDSF) {
  EnableAutoResize(gfx::Size(5, 5), gfx::Size(1000, 1000));
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_1x = MainWidgetSizeInDIPS();
  ASSERT_FALSE(size_at_1x.IsEmpty());

  SetDeviceScaleFactor(2.f);
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_2x = MainWidgetSizeInDIPS();
  EXPECT_EQ(size_at_1x, size_at_2x);
}

TEST_F(RenderViewImplScaleFactorTest, AutoResizeWithoutZoomForDSF) {
  EnableAutoResize(gfx::Size(5, 5), gfx::Size(1000, 1000));
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_1x = MainWidgetSizeInDIPS();
  ASSERT_FALSE(size_at_1x.IsEmpty());

  SetDeviceScaleFactor(2.f);
  LoadHTML(kAutoResizeTestPage);
  gfx::Size size_at_2x = MainWidgetSizeInDIPS();
  EXPECT_EQ(size_at_1x, size_at_2x);
}

TEST_F(RenderViewImplTest, ZoomLevelUpdate) {
  // 0 will use the minimum zoom level, which is the default, nothing will
  // change.
  EXPECT_FLOAT_EQ(0u, view()->GetZoomLevel());

  double zoom_level = blink::PageZoomFactorToZoomLevel(0.25);
  // Change the zoom level to 25% and check if the view gets the change.
  main_frame_widget()->SetZoomLevelForTesting(zoom_level);
  // Use EXPECT_FLOAT_EQ here because view()->GetZoomLevel returns a float.
  EXPECT_FLOAT_EQ(zoom_level, view()->GetZoomLevel());
}

#endif

namespace {

// This is the public key which the test below will use to enable origin
// trial features. Trial tokens for use in tests can be created with the
// tool in /tools/origin_trials/generate_token.py, using the private key
// contained in /tools/origin_trials/eftest.key.
static const uint8_t kOriginTrialPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

}  // anonymous namespace

// Origin Trial Policy which vends the test public key so that the token
// can be validated.
class TestOriginTrialPolicy : public blink::OriginTrialPolicy {
 public:
  TestOriginTrialPolicy() {
    public_keys_.emplace_back(
        reinterpret_cast<const char*>(kOriginTrialPublicKey),
        base::size(kOriginTrialPublicKey));
  }
  bool IsOriginTrialsSupported() const override { return true; }
  std::vector<base::StringPiece> GetPublicKeys() const override {
    return public_keys_;
  }
  bool IsOriginSecure(const GURL& url) const override { return true; }

 private:
  std::vector<base::StringPiece> public_keys_;
};

TEST_F(RenderViewImplTest, OriginTrialDisabled) {
  // HTML Document with no origin trial.
  const char kHTMLWithNoOriginTrial[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<title>Origin Trial Test</title>"
      "</head>"
      "</html>";

  // Override the origin trial policy to use the test keys.
  TestOriginTrialPolicy policy;
  blink::TrialTokenValidator::SetOriginTrialPolicyGetter(base::BindRepeating(
      [](TestOriginTrialPolicy* policy_ptr) -> blink::OriginTrialPolicy* {
        return policy_ptr;
      },
      base::Unretained(&policy)));

  // Set the document URL.
  LoadHTMLWithUrlOverride(kHTMLWithNoOriginTrial, "https://example.test/");
  blink::WebFrame* web_frame = frame()->GetWebFrame();
  ASSERT_TRUE(web_frame);
  ASSERT_TRUE(web_frame->IsWebLocalFrame());
  blink::WebDocument web_doc = web_frame->ToWebLocalFrame()->GetDocument();
  EXPECT_FALSE(blink::WebOriginTrials::isTrialEnabled(&web_doc, "Frobulate"));
  // Reset the origin trial policy.
  blink::TrialTokenValidator::ResetOriginTrialPolicyGetter();
}

TEST_F(RenderViewImplTest, OriginTrialEnabled) {
  // HTML Document with an origin trial.
  // Note: The token below will expire in 2033. It was generated with the
  // command:
  // generate_token.py https://example.test Frobulate \
  //     -expire-timestamp=2000000000
  const char kHTMLWithOriginTrial[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<title>Origin Trial Test</title>"
      "<meta http-equiv=\"origin-trial\" "
      "content=\"AlrgXVXDH5RSr6sDZiO6/8Hejv3BIhODCSS/0zD8VmDDLNPn463JzEq/Cv/"
      "wqt8cRHacGD3cUhKkibGIGQbaXAMAAABUeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnR"
      "lc3Q6NDQzIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB"
      "9\">"
      "</head>"
      "</html>";

  // Override the origin trial policy to use the test keys.
  TestOriginTrialPolicy policy;
  blink::TrialTokenValidator::SetOriginTrialPolicyGetter(base::BindRepeating(
      [](TestOriginTrialPolicy* policy_ptr) -> blink::OriginTrialPolicy* {
        return policy_ptr;
      },
      base::Unretained(&policy)));

  // Set the document URL so the origin is correct for the trial.
  LoadHTMLWithUrlOverride(kHTMLWithOriginTrial, "https://example.test/");
  blink::WebFrame* web_frame = frame()->GetWebFrame();
  ASSERT_TRUE(web_frame);
  ASSERT_TRUE(web_frame->IsWebLocalFrame());
  blink::WebDocument web_doc = web_frame->ToWebLocalFrame()->GetDocument();
  EXPECT_TRUE(blink::WebOriginTrials::isTrialEnabled(&web_doc, "Frobulate"));
  // Reset the origin trial policy.
  blink::TrialTokenValidator::ResetOriginTrialPolicyGetter();
}

}  // namespace content
