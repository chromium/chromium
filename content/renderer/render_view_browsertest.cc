// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <tuple>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/layer_tree_host.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_render_widget_host.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/local_frame_host_interceptor.h"
#include "content/public/test/policy_container_utils.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/document_state.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/render_process.h"
#include "content/renderer/service_worker/service_worker_network_provider_for_frame.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/mock_keyboard.h"
#include "content/test/test_render_frame.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/mojom/base/text_direction.mojom-blink.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "skia/ext/legacy_display_globals.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_origin_trials.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/public/web/web_picture_in_picture_window_options.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/range/range.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

using blink::TestWebFrameContentDumper;
using blink::WebFrame;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebString;
using blink::WebURLError;

namespace content {

namespace {

static const int kProxyRoutingId = 13;

#if BUILDFLAG(IS_OZONE)
// Converts MockKeyboard::Modifiers to ui::EventFlags.
int ConvertMockKeyboardModifier(MockKeyboard::Modifiers modifiers) {
  struct ModifierMap {
    MockKeyboard::Modifiers src;
    int dst;
  };
  static const auto kMapping = std::to_array<ModifierMap>({
      {MockKeyboard::LEFT_SHIFT, ui::EF_SHIFT_DOWN},
      {MockKeyboard::RIGHT_SHIFT, ui::EF_SHIFT_DOWN},
      {MockKeyboard::LEFT_CONTROL, ui::EF_CONTROL_DOWN},
      {MockKeyboard::RIGHT_CONTROL, ui::EF_CONTROL_DOWN},
      {MockKeyboard::LEFT_ALT, ui::EF_ALT_DOWN},
      {MockKeyboard::RIGHT_ALT, ui::EF_ALT_DOWN},
  });
  int flags = 0;
  for (const auto& mapping : kMapping) {
    if (mapping.src & modifiers) {
      flags |= mapping.dst;
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
};

// FrameReplicationState is normally maintained in the browser process,
// but the function below provides a way for tests to construct a partial
// FrameReplicationState within the renderer process.  We say "partial",
// because some fields of FrameReplicationState cannot be filled out
// by content-layer, renderer code (still the constructed, partial
// FrameReplicationState is sufficiently complete to avoid trigerring
// asserts that a default/empty FrameReplicationState would).
blink::mojom::FrameReplicationStatePtr ReconstructReplicationStateForTesting(
    TestRenderFrame* test_render_frame) {
  blink::WebLocalFrame* frame = test_render_frame->GetWebFrame();

  auto result = blink::mojom::FrameReplicationState::New();
  // can't recover result.scope - no way to get blink::mojom::TreeScopeType via
  // public blink API...
  result->name = frame->AssignedName().Utf8();
  result->unique_name = test_render_frame->unique_name();
  // result.should_enforce_strict_mixed_content_checking is calculated in the
  // browser...
  result->origin = frame->GetSecurityOrigin();

  return result;
}

// Returns mojom::CommonNavigationParams for a normal navigation to a data: url,
// with navigation_start set to Now() plus the given offset.
blink::mojom::CommonNavigationParamsPtr MakeCommonNavigationParams(
    base::TimeDelta navigation_start_offset) {
  auto params = blink::CreateCommonNavigationParams();
  params->url = GURL("data:text/html,<div>Page</div>");
  params->navigation_start = base::TimeTicks::Now() + navigation_start_offset;
  params->navigation_type = blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
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

blink::mojom::CommitNavigationParamsPtr DummyCommitNavigationParams() {
  blink::mojom::CommitNavigationParamsPtr params =
      blink::CreateCommitNavigationParams();
  return params;
}

blink::mojom::RemoteFrameInterfacesFromBrowserPtr
CreateStubRemoteFrameInterfaces() {
  auto interfaces = blink::mojom::RemoteFrameInterfacesFromBrowser::New();

  mojo::AssociatedRemote<blink::mojom::RemoteFrame> frame;
  interfaces->frame_receiver = frame.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::RemoteFrameHost> frame_host;
  std::ignore = frame_host.BindNewEndpointAndPassDedicatedReceiver();
  interfaces->frame_host = frame_host.Unbind();

  return interfaces;
}

blink::mojom::RemoteMainFrameInterfacesPtr
CreateStubRemoteMainFrameInterfaces() {
  auto interfaces = blink::mojom::RemoteMainFrameInterfaces::New();

  mojo::AssociatedRemote<blink::mojom::RemoteMainFrame> main_frame;
  interfaces->main_frame = main_frame.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::RemoteMainFrameHost> main_frame_host;
  std::ignore = main_frame_host.BindNewEndpointAndPassDedicatedReceiver();
  interfaces->main_frame_host = main_frame_host.Unbind();

  return interfaces;
}

// Helper that collects the CommonNavigationParams off of WebDocumentLoader's
// NavigationState during commit. The NavigationState is cleared when commit
// notifications are done, so any assertions about the CommonNavigationParams
// post-commit require the CommonNavigationParams to be stored manually.
class CommonParamsFrameLoadWaiter : public FrameLoadWaiter {
 public:
  explicit CommonParamsFrameLoadWaiter(RenderFrameImpl* frame)
      : FrameLoadWaiter(frame), frame_(frame) {}

  const blink::mojom::CommonNavigationParamsPtr& common_params() {
    return common_params_;
  }

 private:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override {
    NavigationState* navigation_state =
        DocumentState::FromDocumentLoader(
            frame_->GetWebFrame()->GetDocumentLoader())
            ->navigation_state();
    common_params_ = navigation_state->common_params().Clone();
  }

  blink::mojom::CommonNavigationParamsPtr common_params_;
  raw_ptr<const RenderFrameImpl> frame_;
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
    mock_keyboard_ = std::make_unique<MockKeyboard>();
  }

  ~RenderViewImplTest() override {}

  blink::WebFrameWidget* main_frame_widget() {
    return frame()->GetLocalRootWebFrameWidget();
  }

  TestRenderFrame* frame() {
    return static_cast<TestRenderFrame*>(GetMainRenderFrame());
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

  void ReceiveDisableDeviceEmulation() { web_view_->DisableDeviceEmulation(); }

  void ReceiveEnableDeviceEmulation(
      const blink::DeviceEmulationParams& params) {
    web_view_->EnableDeviceEmulation(params);
  }

  blink::mojom::CommonNavigationParamsPtr GoToOffsetWithParams(
      int offset,
      const blink::PageState& state,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params) {
    EXPECT_TRUE(common_params->transition & ui::PAGE_TRANSITION_FORWARD_BACK);
    blink::WebView* webview = web_view_;
    int pending_offset = offset + webview->HistoryBackListCount();

    // The load actually happens asynchronously, so we pump messages to process
    // the pending continuation.
    CommonParamsFrameLoadWaiter waiter(frame());

    commit_params->page_state = state.ToEncodedData();
    commit_params->nav_entry_id = pending_offset + 1;
    commit_params->pending_history_list_offset = pending_offset;
    commit_params->current_history_list_offset =
        webview->HistoryBackListCount();
    commit_params->current_history_list_length =
        webview->HistoryForwardListCount() + webview->HistoryBackListCount() +
        1;
    frame()->Navigate(std::move(common_params), std::move(commit_params));

    waiter.Wait();
    return waiter.common_params()->Clone();
  }

#if BUILDFLAG(IS_OZONE)
  int SendKeyEventOzone(MockKeyboard::Layout layout,
                        int key_code,
                        MockKeyboard::Modifiers modifiers,
                        std::u16string* output) {
    int flags = ConvertMockKeyboardModifier(modifiers);

    ui::KeyEvent keydown_event(ui::EventType::kKeyPressed,
                               static_cast<ui::KeyboardCode>(key_code), flags);
    input::NativeWebKeyboardEvent keydown_web_event(keydown_event);
    SendNativeKeyEvent(keydown_web_event);

    ui::KeyEvent char_event = ui::KeyEvent::FromCharacter(
        keydown_event.GetCharacter(), static_cast<ui::KeyboardCode>(key_code),
        ui::DomCode::NONE, flags);
    input::NativeWebKeyboardEvent char_web_event(char_event);
    SendNativeKeyEvent(char_web_event);

    ui::KeyEvent keyup_event(ui::EventType::kKeyReleased,
                             static_cast<ui::KeyboardCode>(key_code), flags);
    input::NativeWebKeyboardEvent keyup_web_event(keyup_event);
    SendNativeKeyEvent(keyup_web_event);

    char16_t c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(static_cast<ui::KeyboardCode>(key_code)),
        flags);
    output->assign(1, c);
    return 1;
  }
#endif

  // Sends IPC messages that emulates a key-press event.
  int SendKeyEvent(MockKeyboard::Layout layout,
                   int key_code,
                   MockKeyboard::Modifiers modifiers,
                   std::u16string* output) {
#if BUILDFLAG(IS_WIN)
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
    CHROME_MSG msg1 = {NULL, WM_KEYDOWN, static_cast<WPARAM>(key_code), 0};
    ui::KeyEvent evt1(msg1);
    input::NativeWebKeyboardEvent keydown_event(evt1);
    SendNativeKeyEvent(keydown_event);

    CHROME_MSG msg2 = {NULL, WM_CHAR, (*output)[0], 0};
    ui::KeyEvent evt2(msg2);
    input::NativeWebKeyboardEvent char_event(evt2);
    SendNativeKeyEvent(char_event);

    CHROME_MSG msg3 = {NULL, WM_KEYUP, static_cast<WPARAM>(key_code), 0};
    ui::KeyEvent evt3(msg3);
    input::NativeWebKeyboardEvent keyup_event(evt3);
    SendNativeKeyEvent(keyup_event);

    return length;
#elif BUILDFLAG(IS_OZONE)
    return SendKeyEventOzone(layout, key_code, modifiers, output);
#else
    NOTIMPLEMENTED();
    return L'\0';
#endif
  }

  void EnablePreferredSizeMode() {
    web_view_->EnablePreferredSizeChangedMode();
  }

  gfx::Size GetPreferredSize() {
    web_view_->UpdatePreferredSize();
    return gfx::Size(web_view_->GetPreferredSizeForTest());
  }

  gfx::Size MainWidgetSizeInDIPS() {
    gfx::Rect widget_rect_in_dips =
        main_frame_widget()->BlinkSpaceToEnclosedDIPs(
            gfx::Rect(main_frame_widget()->Size()));
    return widget_rect_in_dips.size();
  }

  int GetScrollbarWidth() {
    return web_view_->MainFrameWidget()->Size().width() -
           web_view_->MainFrame()
               ->ToWebLocalFrame()
               ->VisibleContentRect()
               .width();
  }

 private:
  std::unique_ptr<MockKeyboard> mock_keyboard_;
};

class RenderViewImplBlinkSettingsTest : public RenderViewImplTest {
 public:
  virtual void DoSetUp() { RenderViewImplTest::SetUp(); }

  blink::WebSettings* settings() { return web_view_->GetSettings(); }

 protected:
  // Blink settings may be specified on the command line, which must
  // be configured before RenderViewImplTest::SetUp runs. Thus we make
  // SetUp() a no-op, and expose RenderViewImplTest::SetUp() via
  // DoSetUp(), to allow tests to perform command line modifications
  // before RenderViewImplTest::SetUp is run. Each test must invoke
  // DoSetUp manually once pre-SetUp configuration is complete.
  void SetUp() override {}
};

class RenderViewImplScaleFactorTest : public RenderViewImplTest {
 protected:
  void SetUp() override {
    render_thread_ = std::make_unique<MockRenderThread>();
    RenderViewImplTest::SetUp();
  }

  void SetDeviceScaleFactor(float dsf) {
    blink::WebFrameWidget* widget = main_frame_widget();
    widget->ApplyVisualProperties(
        MakeVisualPropertiesWithDeviceScaleFactor(dsf));

    ASSERT_EQ(dsf, GetMainRenderFrame()->GetDeviceScaleFactor());
    ASSERT_EQ(dsf, widget->GetOriginalScreenInfo().device_scale_factor);
  }

  blink::VisualProperties MakeVisualPropertiesWithDeviceScaleFactor(float dsf) {
    blink::VisualProperties visual_properties;
    visual_properties.screen_infos =
        display::ScreenInfos(display::ScreenInfo());
    visual_properties.screen_infos.mutable_current().device_scale_factor = dsf;
    visual_properties.new_size = gfx::Size(100, 100);
    visual_properties.compositor_viewport_pixel_rect = gfx::Rect(200, 200);
    visual_properties.visible_viewport_size = visual_properties.new_size;
    visual_properties.auto_resize_enabled = web_view_->AutoResizeMode();
    visual_properties.min_size_for_auto_resize = min_size_for_autoresize_;
    visual_properties.max_size_for_auto_resize = max_size_for_autoresize_;
    visual_properties.local_surface_id =
        viz::LocalSurfaceId(1, 1, base::UnguessableToken::Create());
    return visual_properties;
  }

  void TestEmulatedSizeDprDsf(int width, int height, float dpr, float dsf) {
    static std::u16string get_width = u"Number(window.innerWidth)";
    static std::u16string get_height = u"Number(window.innerHeight)";
    static std::u16string get_dpr = u"Number(window.devicePixelRatio * 10)";

    int emulated_width, emulated_height;
    int emulated_dpr;
    blink::DeviceEmulationParams params;
    params.view_size = gfx::Size(width, height);
    params.device_scale_factor = dpr;
    ReceiveEnableDeviceEmulation(params);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_width, &emulated_width));
    EXPECT_EQ(width, emulated_width);
    EXPECT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(get_height, &emulated_height));
    EXPECT_EQ(height, emulated_height);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_dpr, &emulated_dpr));
    EXPECT_EQ(static_cast<int>(dpr * 10), emulated_dpr);
    ASSERT_EQ(dsf,
              main_frame_widget()->GetOriginalScreenInfo().device_scale_factor);
  }

  void EnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size) {
    min_size_for_autoresize_ = min_size;
    max_size_for_autoresize_ = max_size;
    web_view_->EnableAutoResizeForTesting(min_size, max_size);
  }

 private:
  gfx::Size min_size_for_autoresize_;
  gfx::Size max_size_for_autoresize_;
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
  static_cast<mojom::Frame*>(child_frame_1)
      ->Unload(/*is_loading=*/true,
               ReconstructReplicationStateForTesting(child_frame_1),
               blink::RemoteFrameToken(), CreateStubRemoteFrameInterfaces(),
               CreateStubRemoteMainFrameInterfaces());
  EXPECT_TRUE(root_web_frame->FirstChild()->IsWebRemoteFrame());
  EXPECT_FALSE(root_web_frame->FirstChild()
                   ->ToWebRemoteFrame()
                   ->GetPendingVisualPropertiesForTesting()
                   .is_pinch_gesture_active);

  // Set the |is_pinch_gesture_active| flag.
  cc::ApplyViewportChangesArgs args;
  args.page_scale_delta = 1.f;
  args.is_pinch_gesture_active = true;
  args.top_controls_delta = 0.f;
  args.bottom_controls_delta = 0.f;
  args.browser_controls_constraint = cc::BrowserControlsState::kHidden;
  args.scroll_gesture_did_end = false;

  web_view_->MainFrameWidget()->ApplyViewportChangesForTesting(args);
  EXPECT_TRUE(root_web_frame->FirstChild()
                  ->ToWebRemoteFrame()
                  ->GetPendingVisualPropertiesForTesting()
                  .is_pinch_gesture_active);

  // Create a new remote child, and get its proxy. Unloading will force creation
  // and registering of a new WebRemoteFrame, which should pick up the
  // existing setting.
  static_cast<mojom::Frame*>(child_frame_2)
      ->Unload(/*is_loading=*/true,
               ReconstructReplicationStateForTesting(child_frame_2),
               blink::RemoteFrameToken(), CreateStubRemoteFrameInterfaces(),
               CreateStubRemoteMainFrameInterfaces());
  EXPECT_TRUE(root_web_frame->FirstChild()->NextSibling()->IsWebRemoteFrame());
  // Verify new child has the flag too.
  EXPECT_TRUE(root_web_frame->FirstChild()
                  ->NextSibling()
                  ->ToWebRemoteFrame()
                  ->GetPendingVisualPropertiesForTesting()
                  .is_pinch_gesture_active);

  // Reset the flag, make sure both children respond.
  args.is_pinch_gesture_active = false;
  web_view_->MainFrameWidget()->ApplyViewportChangesForTesting(args);
  EXPECT_FALSE(root_web_frame->FirstChild()
                   ->ToWebRemoteFrame()
                   ->GetPendingVisualPropertiesForTesting()
                   .is_pinch_gesture_active);
  EXPECT_FALSE(root_web_frame->FirstChild()
                   ->NextSibling()
                   ->ToWebRemoteFrame()
                   ->GetPendingVisualPropertiesForTesting()
                   .is_pinch_gesture_active);
}

// Test that we get form state change notifications when input fields change.
TEST_F(RenderViewImplTest, OnNavStateChanged) {
  frame()->set_send_content_state_immediately(true);
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

TEST_F(RenderViewImplTest, OnNavigationHttpPost) {
  // An http url will trigger a resource load so cannot be used here.
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,<div>Page</div>");
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;
  common_params->method = "POST";

  // Set up post data.
  const char raw_data[] = "post \0\ndata";
  scoped_refptr<network::ResourceRequestBody> post_data(
      new network::ResourceRequestBody);
  post_data->AppendBytes(raw_data, sizeof(raw_data));
  common_params->post_data = post_data;

  frame()->Navigate(std::move(common_params), DummyCommitNavigationParams());
  base::RunLoop().RunUntilIdle();

  auto last_commit_params = frame()->TakeLastCommitParams();
  ASSERT_TRUE(last_commit_params);
  EXPECT_EQ("POST", last_commit_params->method);

  // Check post data sent to browser matches
  EXPECT_TRUE(last_commit_params->page_state.IsValid());
  blink::WebHTTPBody body =
      blink::WebHistoryItem(last_commit_params->page_state).HttpBody();
  blink::WebHTTPBody::Element element;
  bool successful = body.ElementAt(0, element);
  EXPECT_TRUE(successful);
  EXPECT_EQ(blink::HTTPBodyElementType::kTypeData, element.type);

  auto flat_data = base::HeapArray<char>::Uninit(element.data.size());
  element.data.ForEachSegment([&flat_data](const char* segment,
                                           size_t segment_size,
                                           size_t segment_offset) {
    flat_data.subspan(segment_offset)
        .copy_prefix_from(
            // TODO(crbug.com/40284755): ForEachSegment should be given a span.
            UNSAFE_TODO(base::span(segment, segment_size)));
    return true;
  });
  EXPECT_EQ(base::span_with_nul_from_cstring(raw_data), flat_data.as_span());
}

#if BUILDFLAG(IS_ANDROID)
namespace {
class UpdateTitleLocalFrameHost : public LocalFrameHostInterceptor {
 public:
  explicit UpdateTitleLocalFrameHost(
      blink::AssociatedInterfaceProvider* provider)
      : LocalFrameHostInterceptor(provider) {}

  MOCK_METHOD2(UpdateTitle,
               void(const std::optional<::std::u16string>& title,
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

TEST_F(RenderViewImplUpdateTitleTest, OnNavigationLoadDataWithBaseURL) {
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,");
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;
  common_params->base_url_for_data_url = GURL("about:blank");
  auto commit_params = DummyCommitNavigationParams();
  commit_params->data_url_as_string =
      "data:text/html,<html><head><title>Data page</title></head></html>";

  const std::optional<::std::u16string>& title =
      std::make_optional(u"Data page");
  EXPECT_CALL(*title_mock_frame_host(), UpdateTitle(title, testing::_))
      .Times(1);
  FrameLoadWaiter waiter(frame());
  frame()->Navigate(std::move(common_params), std::move(commit_params));
  waiter.Wait();

  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(title_mock_frame_host());
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
  EXPECT_TRUE(frame()->IsRequestingNavigation());

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
  frame()->BeginNavigation(std::move(popup_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());
}

TEST_F(RenderViewImplTest, BeginNavigationHandlesAllTopLevel) {
  blink::RendererPreferences prefs = web_view_->GetRendererPreferences();
  prefs.browser_handles_all_top_level_requests = true;
  web_view_->SetRendererPreferences(prefs);

  const blink::WebNavigationType kNavTypes[] = {
      blink::kWebNavigationTypeLinkClicked,
      blink::kWebNavigationTypeFormSubmitted,
      blink::kWebNavigationTypeBackForward,
      blink::kWebNavigationTypeReload,
      blink::kWebNavigationTypeRestore,
      blink::kWebNavigationTypeFormResubmittedBackForward,
      blink::kWebNavigationTypeFormResubmittedReload,
      blink::kWebNavigationTypeOther,
  };

  for (const auto& nav_type : kNavTypes) {
    auto navigation_info = std::make_unique<blink::WebNavigationInfo>();
    navigation_info->url_request = blink::WebURLRequest(GURL("http://foo.com"));
    navigation_info->url_request.SetRequestorOrigin(
        blink::WebSecurityOrigin::Create(GURL("http://foo.com")));
    navigation_info->frame_type =
        blink::mojom::RequestContextFrameType::kTopLevel;
    navigation_info->navigation_policy = blink::kWebNavigationPolicyCurrentTab;
    navigation_info->navigation_type = nav_type;

    frame()->BeginNavigation(std::move(navigation_info));
    EXPECT_TRUE(frame()->IsURLOpened());
  }
}

TEST_F(RenderViewImplTest, BeginNavigationForWebUI) {
  // Enable bindings to simulate a WebUI view.
  frame()->AllowBindings(
      BindingsPolicySet({BindingsPolicyValue::kWebUi}).ToEnumBitmask());

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
  frame()->BeginNavigation(std::move(data_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());

  // A popup that creates a view first and then navigates to a
  // normal HTTP URL.
  bool consumed_user_gesture = false;
  blink::WebURLRequest popup_request(GURL("http://foo.com"));
  popup_request.SetRequestorOrigin(requestor_origin);
  popup_request.SetMode(network::mojom::RequestMode::kNavigate);
  popup_request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  popup_request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  blink::WebView* new_web_view = frame()->CreateNewWindow(
      popup_request, blink::WebWindowFeatures(), "foo",
      blink::kWebNavigationPolicyNewForegroundTab,
      network::mojom::WebSandboxFlags::kNone,
      blink::AllocateSessionStorageNamespaceId(), consumed_user_gesture,
      std::nullopt, std::nullopt, /*base_url=*/blink::WebURL());
  auto popup_navigation_info = std::make_unique<blink::WebNavigationInfo>();
  popup_navigation_info->url_request = std::move(popup_request);
  popup_navigation_info->frame_type =
      blink::mojom::RequestContextFrameType::kAuxiliary;
  popup_navigation_info->navigation_type = blink::kWebNavigationTypeLinkClicked;
  popup_navigation_info->navigation_policy =
      blink::kWebNavigationPolicyNewForegroundTab;
  RenderFrameImpl::FromWebFrame(new_web_view->MainFrame())
      ->BeginNavigation(std::move(popup_navigation_info));
  EXPECT_TRUE(frame()->IsURLOpened());
}

// This test verifies that when device emulation is enabled, WebRemoteFrame
// continues to receive the original ScreenInfo and not the emualted
// ScreenInfo.
TEST_F(RenderViewImplScaleFactorTest, DeviceEmulationWithOOPIF) {
  const float device_scale = 2.0f;
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

  static_cast<mojom::Frame*>(child_frame)
      ->Unload(/*is_loading=*/true,
               ReconstructReplicationStateForTesting(child_frame),
               blink::RemoteFrameToken(), CreateStubRemoteFrameInterfaces(),
               CreateStubRemoteMainFrameInterfaces());
  EXPECT_TRUE(web_frame->FirstChild()->IsWebRemoteFrame());

  // Verify that the system device scale factor has propagated into the
  // WebRemoteFrame.
  EXPECT_EQ(device_scale, GetMainRenderFrame()->GetDeviceScaleFactor());
  EXPECT_EQ(device_scale,
            main_frame_widget()->GetOriginalScreenInfo().device_scale_factor);

  TestEmulatedSizeDprDsf(640, 480, 3.f, device_scale);

  // Verify that the WebRemoteFrame device scale factor is still the same.
  EXPECT_EQ(3.f, GetMainRenderFrame()->GetDeviceScaleFactor());
  EXPECT_EQ(device_scale,
            main_frame_widget()->GetOriginalScreenInfo().device_scale_factor);

  ReceiveDisableDeviceEmulation();

  blink::DeviceEmulationParams params;
  ReceiveEnableDeviceEmulation(params);
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
  auto replication_state = ReconstructReplicationStateForTesting(child_frame);
  replication_state->origin = url::Origin::Create(GURL("http://foo.com"));
  static_cast<mojom::Frame*>(child_frame)
      ->Unload(/*is_loading=*/true, replication_state->Clone(),
               blink::RemoteFrameToken(), CreateStubRemoteFrameInterfaces(),
               CreateStubRemoteMainFrameInterfaces());

  // The child frame should now be a WebRemoteFrame.
  EXPECT_TRUE(web_frame->FirstChild()->IsWebRemoteFrame());

  // Expect the origin to be updated properly.
  blink::WebSecurityOrigin origin =
      web_frame->FirstChild()->GetSecurityOrigin();
  EXPECT_EQ(origin.ToString(),
            WebString::FromUTF8(replication_state->origin.Serialize()));

  // Now, unload the second frame using a unique origin and verify that it is
  // replicated correctly.
  replication_state->origin = url::Origin();
  TestRenderFrame* child_frame2 =
      static_cast<TestRenderFrame*>(RenderFrame::FromWebFrame(
          web_frame->FirstChild()->NextSibling()->ToWebLocalFrame()));
  static_cast<mojom::Frame*>(child_frame2)
      ->Unload(/*is_loading=*/true, std::move(replication_state),
               blink::RemoteFrameToken(), CreateStubRemoteFrameInterfaces(),
               CreateStubRemoteMainFrameInterfaces());
  EXPECT_TRUE(web_frame->FirstChild()->NextSibling()->IsWebRemoteFrame());
  EXPECT_TRUE(
      web_frame->FirstChild()->NextSibling()->GetSecurityOrigin().IsOpaque());
}

// Test that when navigating cross-origin, which creates a new main frame
// RenderWidget, that the device scale is set correctly for that RenderWidget
// the WebView and frames.
// See crbug.com/737777#c37.
TEST_F(RenderViewImplScaleFactorTest, DeviceScaleCorrectAfterCrossOriginNav) {
  const float device_scale = 3.0f;
  SetDeviceScaleFactor(device_scale);
  EXPECT_EQ(device_scale, GetMainRenderFrame()->GetDeviceScaleFactor());

  LoadHTML("Hello world!");

  // Early grab testing values as the main-frame widget becomes inaccessible
  // when it unloads.
  blink::VisualProperties test_visual_properties =
      MakeVisualPropertiesWithDeviceScaleFactor(device_scale);

  blink::RemoteFrameToken remote_child_frame_token = blink::RemoteFrameToken();
  // Unload the main frame after which it should become a WebRemoteFrame.
  auto replication_state = ReconstructReplicationStateForTesting(frame());
  // replication_state.origin = url::Origin(GURL("http://foo.com"));
  static_cast<mojom::Frame*>(frame())->Unload(
      /*is_loading=*/true, replication_state->Clone(), remote_child_frame_token,
      CreateStubRemoteFrameInterfaces(), CreateStubRemoteMainFrameInterfaces());
  EXPECT_TRUE(web_view_->MainFrame()->IsWebRemoteFrame());

  // Do the remote-to-local transition for the proxy, which is to create a
  // provisional local frame.
  int routing_id = kProxyRoutingId + 1;

  // The new frame is initialized with |device_scale| as the device scale
  // factor.
  mojom::CreateFrameWidgetParamsPtr widget_params =
      mojom::CreateFrameWidgetParams::New();
  widget_params->routing_id = kProxyRoutingId + 2;
  widget_params->visual_properties = test_visual_properties;

  mojo::AssociatedRemote<blink::mojom::Widget> blink_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget_receiver =
      blink_widget.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
  std::ignore = blink_widget_host.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
      blink_frame_widget_receiver =
          blink_frame_widget.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> blink_frame_widget_host;
  std::ignore =
      blink_frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();

  widget_params->frame_widget = std::move(blink_frame_widget_receiver);
  widget_params->frame_widget_host = blink_frame_widget_host.Unbind();
  widget_params->widget = std::move(blink_widget_receiver);
  widget_params->widget_host = blink_widget_host.Unbind();

  blink::LocalFrameToken frame_token;
  RenderFrameImpl::CreateFrame(
      *agent_scheduling_group_, frame_token, routing_id,
      TestRenderFrame::CreateStubFrameReceiver(),
      TestRenderFrame::CreateStubBrowserInterfaceBrokerRemote(),
      TestRenderFrame::CreateStubAssociatedInterfaceProviderRemote(),
      /*web_view=*/nullptr,
      /*previous_frame_token=*/remote_child_frame_token,
      /*opener_frame_token=*/std::nullopt,
      /*parent_frame_token=*/std::nullopt,
      /*previous_sibling_frame_token=*/std::nullopt,
      base::UnguessableToken::Create(), blink::mojom::TreeScopeType::kDocument,
      std::move(replication_state), std::move(widget_params),
      blink::mojom::FrameOwnerProperties::New(),
      /*is_on_initial_empty_document=*/true, blink::DocumentToken(),
      CreateStubPolicyContainer(), /*is_for_nested_main_frame=*/false);

  TestRenderFrame* provisional_frame =
      static_cast<TestRenderFrame*>(RenderFrameImpl::FromWebFrame(
          WebLocalFrame::FromFrameToken(frame_token)));
  EXPECT_TRUE(provisional_frame);

  // Navigate to other page, which triggers the swap in.
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,<div>Page</div>");
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;

  provisional_frame->Navigate(std::move(common_params),
                              DummyCommitNavigationParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(device_scale, GetMainRenderFrame()->GetDeviceScaleFactor());
  EXPECT_EQ(device_scale, web_view_->ZoomFactorForViewportLayout());

  double device_pixel_ratio;
  std::u16string get_dpr = u"Number(window.devicePixelRatio)";
  EXPECT_TRUE(
      ExecuteJavaScriptAndReturnNumberValue(get_dpr, &device_pixel_ratio));
  EXPECT_EQ(device_scale, device_pixel_ratio);

  int width;
  std::u16string get_width = u"Number(document.documentElement.clientWidth)";
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(get_width, &width));
  EXPECT_EQ(web_view_->MainFrameWidget()->Size().width(), width * device_scale);
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
  blink::RemoteFrameToken child_remote_frame_token = blink::RemoteFrameToken();
  auto replication_state = ReconstructReplicationStateForTesting(child_frame);
  static_cast<mojom::Frame*>(child_frame)
      ->Unload(/*is_loading=*/true, replication_state.Clone(),
               child_remote_frame_token, CreateStubRemoteFrameInterfaces(),
               CreateStubRemoteMainFrameInterfaces());
  EXPECT_TRUE(web_frame->FirstChild()->IsWebRemoteFrame());

  // Do the first step of a remote-to-local transition for the child proxy,
  // which is to create a provisional local frame.
  int routing_id = kProxyRoutingId + 1;
  blink::LocalFrameToken frame_token;
  RenderFrameImpl::CreateFrame(
      *agent_scheduling_group_, frame_token, routing_id,
      TestRenderFrame::CreateStubFrameReceiver(),
      TestRenderFrame::CreateStubBrowserInterfaceBrokerRemote(),
      TestRenderFrame::CreateStubAssociatedInterfaceProviderRemote(),
      /*web_view=*/nullptr, child_remote_frame_token,
      /*opener_frame_token=*/std::nullopt,
      /*parent_frame_token=*/web_frame->GetFrameToken(),
      /*previous_sibling_frame_token=*/std::nullopt,
      base::UnguessableToken::Create(), blink::mojom::TreeScopeType::kDocument,
      std::move(replication_state),
      /*widget_params=*/nullptr, blink::mojom::FrameOwnerProperties::New(),
      /*is_on_initial_empty_document=*/true, blink::DocumentToken(),
      CreateStubPolicyContainer(), /*is_for_nested_main_frame=*/false);
  {
    TestRenderFrame* provisional_frame =
        static_cast<TestRenderFrame*>(RenderFrameImpl::FromWebFrame(
            WebLocalFrame::FromFrameToken(frame_token)));
    EXPECT_TRUE(provisional_frame);
  }

  // Detach the child frame (currently remote) in the main frame.
  ExecuteJavaScriptForTests(
      "document.body.removeChild(document.querySelector('iframe'));");
  blink::WebRemoteFrame* child_remote_frame =
      blink::WebRemoteFrame::FromFrameToken(child_remote_frame_token);
  EXPECT_FALSE(child_remote_frame);

  // The provisional frame should have been deleted along with the proxy, and
  // thus any subsequent messages (such as OnNavigate) already in flight for it
  // should be dropped.
  {
    TestRenderFrame* provisional_frame =
        static_cast<TestRenderFrame*>(RenderFrameImpl::FromWebFrame(
            WebLocalFrame::FromFrameToken(frame_token)));
    EXPECT_FALSE(provisional_frame);
  }
}

// Verify that the renderer process doesn't crash when device scale factor
// changes after a cross-process navigation has commited.
// See https://crbug.com/571603.
TEST_F(RenderViewImplScaleFactorTest, SetZoomLevelAfterCrossProcessNavigation) {
  LoadHTML("Hello world!");

  // Unload the main frame after which it should become a WebRemoteFrame.
  TestRenderFrame* main_frame = frame();
  static_cast<mojom::Frame*>(main_frame)
      ->Unload(/*is_loading=*/true,
               ReconstructReplicationStateForTesting(main_frame),
               blink::RemoteFrameToken(), CreateStubRemoteFrameInterfaces(),
               CreateStubRemoteMainFrameInterfaces());
  EXPECT_TRUE(web_view_->MainFrame()->IsWebRemoteFrame());
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
    main_frame_widget()->UpdateTextInputState();
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
    main_frame_widget()->UpdateTextInputState();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1u, updated_states().size());
    type = updated_states()[0]->type;
    input_mode = updated_states()[0]->mode;
    EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, type);

    for (const auto test_case : kInputModeTestCases) {
      std::u16string javascript = base::ASCIIToUTF16(base::StringPrintf(
          "document.getElementById('%s').focus();", test_case.input_id));
      // Move the input focus to the target <input> element, where we should
      // activate IMEs.
      ExecuteJavaScriptAndReturnIntValue(javascript, nullptr);
      base::RunLoop().RunUntilIdle();
      ClearState();

      // Update the IME status and verify if our IME backend sends an IPC
      // message to activate IMEs.
      main_frame_widget()->UpdateTextInputState();
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(1u, updated_states().size());
      type = updated_states()[0]->type;
      input_mode = updated_states()[0]->mode;
      EXPECT_EQ(test_case.expected_mode, input_mode);
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
  main_frame_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_FALSE(updated_states()[0]->always_hide_ime);
  ClearState();

  // Tell the client to suppress the keyboard. Check whether always_hide_ime is
  // set correctly.
  client.SetShouldSuppressKeyboard(true);
  main_frame_widget()->UpdateTextInputState();
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
  GetWidgetInputHandler()->SetFocus(blink::mojom::FocusState::kFocused);
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests(
      "const editContext = new EditContext();"
      "document.body.editContext = editContext;"
      "document.body.focus();editContext.inputPanelPolicy=\"auto\";"
      "const control_bounds = new DOMRect(10, 20, 30, 40);"
      "const selection_bounds = new DOMRect(10, 20, 1, 5);"
      "editContext.updateControlBounds(control_bounds);"
      "editContext.updateSelectionBounds(selection_bounds);");
  // This RunLoop is waiting for EditContext to be created and layout bounds
  // to be updated in the EditContext.
  base::RunLoop().RunUntilIdle();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_frame_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  gfx::Rect edit_context_control_bounds_expected =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(gfx::Rect(10, 20, 30, 40));
  gfx::Rect edit_context_selection_bounds_expected =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(gfx::Rect(10, 20, 1, 5));
  gfx::Rect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  gfx::Rect actual_active_element_selection_bounds(
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
  GetWidgetInputHandler()->SetFocus(blink::mojom::FocusState::kFocused);
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests(
      "const editContext = new EditContext();"
      "document.body.editContext = editContext;"
      "document.body.focus();editContext.inputPanelPolicy=\"auto\";"
      "const control_bounds = new DOMRect(10.14, 20.25, 30.15, 40.50);"
      "const selection_bounds = new DOMRect(10, 20, 1, 5);"
      "editContext.updateControlBounds(control_bounds);"
      "editContext.updateSelectionBounds(selection_bounds);");
  // This RunLoop is waiting for EditContext to be created and layout bounds
  // to be updated in the EditContext.
  base::RunLoop().RunUntilIdle();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_frame_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  gfx::Rect edit_context_control_bounds_expected =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(gfx::Rect(10, 20, 31, 41));
  gfx::Rect edit_context_selection_bounds_expected =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(gfx::Rect(10, 20, 1, 5));
  gfx::Rect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  gfx::Rect actual_active_element_selection_bounds(
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
  GetWidgetInputHandler()->SetFocus(blink::mojom::FocusState::kFocused);
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests(
      "const editContext = new EditContext();"
      "document.body.editContext = editContext;"
      "document.body.focus(); editContext.inputPanelPolicy=\"auto\";"
      "const control_bounds = new DOMRect(-3964254814208.000000,"
      "-60129542144.000000, 674309865472.000000, 64424509440.000000);"
      "const selection_bounds = new DOMRect(10, 20, 1, 5);"
      "editContext.updateControlBounds(control_bounds);"
      "editContext.updateSelectionBounds(selection_bounds);");
  // This RunLoop is waiting for EditContext to be created and layout bounds
  // to be updated in the EditContext.
  base::RunLoop().RunUntilIdle();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_frame_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  gfx::Rect edit_context_control_bounds_expected =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(
          gfx::Rect(-2147483648, -1073741825, 0, 2147483647));
  gfx::Rect edit_context_selection_bounds_expected =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(gfx::Rect(10, 20, 1, 5));
  gfx::Rect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  gfx::Rect actual_active_element_selection_bounds(
      updated_states()[0]->edit_context_selection_bounds.value());
  EXPECT_EQ(edit_context_control_bounds_expected,
            actual_active_element_control_bounds);
  EXPECT_EQ(edit_context_selection_bounds_expected,
            actual_active_element_selection_bounds);
}

TEST_F(RenderViewImplTextInputStateChanged, ActiveElementGetLayoutBounds) {
  // Load an HTML page consisting of one input fields.
  LoadHTML(R"HTML(
    <style>
      input { position: fixed; }
    </style>
      <input id='test' type='text'></input>
    )HTML");
  ClearState();
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_frame_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  gfx::Rect expected_control_bounds;
  gfx::Rect temp_selection_bounds;
  controller->GetLayoutBounds(&expected_control_bounds, &temp_selection_bounds);
  gfx::Rect expected_control_bounds_in_dips =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(expected_control_bounds);
  gfx::Rect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  EXPECT_EQ(actual_active_element_control_bounds,
            expected_control_bounds_in_dips);

  // Update the position of the element and that should trigger control bounds
  // update to IME.
  ExecuteJavaScriptForTests(
      "document.getElementById('test').style.top = 50 + "
      "\"px\";document.getElementById('test').style.left = 350 + \"px\";");
  // This RunLoop is waiting for styles to be processed for the active element.
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_frame_widget()->UpdateTextInputState();
  // This RunLoop is to flush the TextInputState update message.
  base::RunLoop run_loop3;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop3.QuitClosure());
  run_loop3.Run();
  EXPECT_EQ(2u, updated_states().size());
  controller->GetLayoutBounds(&expected_control_bounds, &temp_selection_bounds);
  gfx::Rect expected_control_bounds_in_dips_updated =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(expected_control_bounds);
  actual_active_element_control_bounds =
      updated_states()[1]->edit_context_control_bounds.value();
  EXPECT_EQ(actual_active_element_control_bounds,
            expected_control_bounds_in_dips_updated);
  // Also check that the updated bounds are different from last reported bounds.
  EXPECT_NE(expected_control_bounds_in_dips_updated,
            expected_control_bounds_in_dips);
}

TEST_F(RenderViewImplTextInputStateChanged,
       ActiveElementMultipleLayoutBoundsUpdates) {
  // Load an HTML page consisting of one input fields.
  LoadHTML(R"HTML(
      <input id='test' type='text'></input>
    )HTML");
  ClearState();
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_frame_widget()->UpdateTextInputState();
  // This RunLoop is to flush the TextInputState update message.
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(1u, updated_states().size());
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  gfx::Rect expected_control_bounds;
  gfx::Rect temp_selection_bounds;
  controller->GetLayoutBounds(&expected_control_bounds, &temp_selection_bounds);
  gfx::Rect expected_control_bounds_in_dips =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(expected_control_bounds);
  gfx::Rect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  EXPECT_EQ(actual_active_element_control_bounds,
            expected_control_bounds_in_dips);

  // No updates in control bounds so this shouldn't trigger an update to IME.
  main_frame_widget()->UpdateTextInputState();
  // This RunLoop is to flush the TextInputState update message.
  base::RunLoop run_loop3;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop3.QuitClosure());
  run_loop3.Run();
  EXPECT_EQ(1u, updated_states().size());
}

TEST_F(RenderViewImplTextInputStateChanged,
       ActiveElementLayoutBoundsUpdatesDuringBrowserZoom) {
  // Load an HTML page consisting of one input fields.
  LoadHTML(R"HTML(
      <input id='test' type='text'></input>
    )HTML");
  ClearState();
  // Create an EditContext with control and selection bounds and set input
  // panel policy to auto.
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  double zoom_level = blink::ZoomFactorToZoomLevel(1.25);
  // Change the zoom level to 125% and check if the view gets the change.
  main_frame_widget()->SetZoomLevelForTesting(zoom_level);
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify layout bounds of the EditContext.
  main_frame_widget()->UpdateTextInputState();
  // This RunLoop is to flush the TextInputState update message.
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(1u, updated_states().size());
  blink::WebInputMethodController* controller =
      frame()->GetWebFrame()->GetInputMethodController();
  gfx::Rect expected_control_bounds;
  gfx::Rect temp_selection_bounds;
  controller->GetLayoutBounds(&expected_control_bounds, &temp_selection_bounds);
  gfx::Rect expected_control_bounds_in_dips =
      main_frame_widget()->BlinkSpaceToEnclosedDIPs(expected_control_bounds);
  gfx::Rect actual_active_element_control_bounds(
      updated_states()[0]->edit_context_control_bounds.value());
  EXPECT_EQ(actual_active_element_control_bounds,
            expected_control_bounds_in_dips);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  // Update the text input state and verify the virtualkeyboardpolicy attribute
  // value.
  main_frame_widget()->UpdateTextInputState();
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element.
  main_frame_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_EQ(updated_states()[0]->vk_policy,
            ui::mojom::VirtualKeyboardPolicy::MANUAL);
  EXPECT_EQ(updated_states()[0]->last_vk_visibility_request,
            ui::mojom::VirtualKeyboardVisibilityRequest::NONE);
}

TEST_F(RenderViewImplTextInputStateChanged,
       VirtualKeyboardPolicyManualAndShowHideAPIsCalledInInsecureContext) {
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element and the show
  // API call.
  main_frame_widget()->UpdateTextInputState();
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  ClearState();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element and the hide
  // API call.
  main_frame_widget()->UpdateTextInputState();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, updated_states().size());
  EXPECT_EQ(updated_states()[0]->vk_policy,
            ui::mojom::VirtualKeyboardPolicy::MANUAL);
  EXPECT_EQ(updated_states()[0]->last_vk_visibility_request,
            ui::mojom::VirtualKeyboardVisibilityRequest::NONE);
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
  ExecuteJavaScriptForTests(
      "document.getElementById('test1').focus(); "
      "navigator.virtualKeyboard.show();");
  // This RunLoop is waiting for focus to be processed for the active element.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  // Update the IME status and verify if our IME backend sends an IPC message
  // to notify virtualkeyboardpolicy change of the focused element and the show
  // API call.
  main_frame_widget()->UpdateTextInputState();
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

  for (const auto& ime_message : kImeMessages) {
    switch (ime_message.command) {
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
        GetWidgetInputHandler()->SetFocus(
            ime_message.enable
                ? blink::mojom::FocusState::kFocused
                : blink::mojom::FocusState::kNotFocusedAndActive);
        break;

      case IME_SETCOMPOSITION:
        GetWidgetInputHandler()->ImeSetComposition(
            base::WideToUTF16(ime_message.ime_string),
            std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(),
            ime_message.selection_start, ime_message.selection_end,
            base::DoNothing());
        break;

      case IME_COMMITTEXT:
        GetWidgetInputHandler()->ImeCommitText(
            base::WideToUTF16(ime_message.ime_string),
            std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(), 0,
            base::DoNothing());
        break;

      case IME_FINISHCOMPOSINGTEXT:
        GetWidgetInputHandler()->ImeFinishComposingText(false);
        break;

      case IME_CANCELCOMPOSITION:
        GetWidgetInputHandler()->ImeSetComposition(
            std::u16string(), std::vector<ui::ImeTextSpan>(),
            gfx::Range::InvalidRange(), 0, 0, base::DoNothing());
        break;
    }

    // Update the status of our IME back-end.
    // TODO(hbono): we should verify messages to be sent from the back-end.
    main_frame_widget()->UpdateTextInputState();
    base::RunLoop().RunUntilIdle();

    if (ime_message.result) {
      // Retrieve the content of this page and compare it with the expected
      // result.
      const int kMaxOutputCharacters = 128;
      std::u16string output = TestWebFrameContentDumper::DumpWebViewAsText(
                                  web_view_, kMaxOutputCharacters)
                                  .Utf16();
      EXPECT_EQ(base::WideToUTF16(ime_message.result), output);
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
    std::u16string output = TestWebFrameContentDumper::DumpWebViewAsText(
                                web_view_, kMaxOutputCharacters)
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
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->url = GURL("data:text/html,test data");
  frame()->Navigate(std::move(common_params), DummyCommitNavigationParams());

  // A cancellation occurred.
  frame()->OnDroppedNavigation();
  // Frame should stay in view-source mode.
  EXPECT_TRUE(web_frame->IsViewSourceModeEnabled());
}

namespace {

class ContextMenuFrameHost : public LocalFrameHostInterceptor {
 public:
  explicit ContextMenuFrameHost(blink::AssociatedInterfaceProvider* provider)
      : LocalFrameHostInterceptor(provider) {}

  MOCK_METHOD2(
      ShowContextMenu,
      void(
          mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient> client,
          const blink::UntrustworthyContextMenuParams& params));
};

}  // namespace

class RenderViewImplContextMenuTest : public RenderViewImplTest {
 public:
  using MockedTestRenderFrame =
      MockedLocalFrameHostInterceptorTestRenderFrame<ContextMenuFrameHost>;

  RenderViewImplContextMenuTest()
      : RenderViewImplTest(&MockedTestRenderFrame::CreateTestRenderFrame) {}

  ContextMenuFrameHost* context_menu_frame_host() {
    return static_cast<MockedTestRenderFrame*>(frame())
        ->mock_local_frame_host();
  }
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(RenderViewImplContextMenuTest, ContextMenu) {
  LoadHTML("<div>Page A</div>");

  // Create a right click in the center of the iframe. (I'm hoping this will
  // make this a bit more robust in case of some other formatting or other
  // bug.)
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(250, 250);
  mouse_event.SetPositionInScreen(250, 250);

  SendWebMouseEvent(mouse_event);

  // Now simulate the corresponding up event which should display the menu
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  SendWebMouseEvent(mouse_event);

  EXPECT_CALL(*context_menu_frame_host(),
              ShowContextMenu(testing::_, testing::_))
      .Times(1);
}

#else
TEST_F(RenderViewImplContextMenuTest, AndroidContextMenuSelectionOrdering) {
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

  EXPECT_CALL(*context_menu_frame_host(),
              ShowContextMenu(testing::_, testing::_))
      .Times(0);

  SendWebGestureEvent(gesture_event);

  EXPECT_CALL(*context_menu_frame_host(),
              ShowContextMenu(testing::_, testing::_))
      .Times(1);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
      FROM_HERE, message_loop_runner->QuitClosure());

  message_loop_runner->Run();

  int did_select = -1;
  std::u16string check_did_select =
      u"Number(document.getElementById('result').innerHTML == 'Selected')";
  EXPECT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(check_did_select, &did_select));
  EXPECT_EQ(1, did_select);
}
#endif

TEST_F(RenderViewImplTest, TestBackForward) {
  LoadHTML("<div id=pagename>Page A</div>");
  blink::PageState page_a_state = GetCurrentPageState();
  int was_page_a = -1;
  std::u16string check_page_a =
      u"Number(document.getElementById('pagename').innerHTML == 'Page A')";
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
  EXPECT_EQ(1, was_page_a);

  LoadHTML("<div id=pagename>Page B</div>");
  int was_page_b = -1;
  std::u16string check_page_b =
      u"Number(document.getElementById('pagename').innerHTML == 'Page B')";
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  blink::PageState back_state = GetCurrentPageState();

  LoadHTML("<div id=pagename>Page C</div>");
  int was_page_c = -1;
  std::u16string check_page_c =
      u"Number(document.getElementById('pagename').innerHTML == 'Page C')";
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_c, &was_page_c));
  EXPECT_EQ(1, was_page_c);

  blink::PageState forward_state = GetCurrentPageState();

  // Go back.
  GoBack(GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>"),
         back_state);

  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);
  blink::PageState back_state2 = GetCurrentPageState();

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

#if BUILDFLAG(IS_MAC) || defined(USE_AURA)
TEST_F(RenderViewImplTest, GetCompositionCharacterBoundsTest) {
  LoadHTML("<textarea id=\"test\" cols=\"100\"></textarea>");
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");

  auto* widget_input_handler = GetWidgetInputHandler();
  const std::u16string empty_string;
  const std::vector<ui::ImeTextSpan> empty_ime_text_span;
  std::vector<gfx::Rect> bounds;
  widget_input_handler->SetFocus(blink::mojom::FocusState::kFocused);

  // ASCII composition
  const std::u16string ascii_composition = u"aiueo";
  widget_input_handler->ImeSetComposition(
      ascii_composition, empty_ime_text_span, gfx::Range::InvalidRange(), 0, 0,
      base::DoNothing());
  bounds = LastCompositionBounds();
  ASSERT_EQ(ascii_composition.size(), bounds.size());

  for (const gfx::Rect& r : bounds)
    EXPECT_LT(0, r.width());
  widget_input_handler->ImeCommitText(
      empty_string, std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(),
      0, base::DoNothing());

  // Non surrogate pair unicode character.
  const std::u16string unicode_composition = u"あいうえお";
  widget_input_handler->ImeSetComposition(
      unicode_composition, empty_ime_text_span, gfx::Range::InvalidRange(), 0,
      0, base::DoNothing());
  bounds = LastCompositionBounds();
  ASSERT_EQ(unicode_composition.size(), bounds.size());
  for (const gfx::Rect& r : bounds)
    EXPECT_LT(0, r.width());
  widget_input_handler->ImeCommitText(empty_string, empty_ime_text_span,
                                      gfx::Range::InvalidRange(), 0,
                                      base::DoNothing());

  // Surrogate pair character.
  const std::u16string surrogate_pair_char = u"𠮟";
  widget_input_handler->ImeSetComposition(
      surrogate_pair_char, empty_ime_text_span, gfx::Range::InvalidRange(), 0,
      0, base::DoNothing());
  bounds = LastCompositionBounds();
  ASSERT_EQ(surrogate_pair_char.size(), bounds.size());
  EXPECT_LT(0, bounds[0].width());
  EXPECT_EQ(0, bounds[1].width());
  widget_input_handler->ImeCommitText(empty_string, empty_ime_text_span,
                                      gfx::Range::InvalidRange(), 0,
                                      base::DoNothing());

  // Mixed string.
  const std::u16string surrogate_pair_mixed_composition =
      surrogate_pair_char + u"あ" + surrogate_pair_char + u"b" +
      surrogate_pair_char;
  const size_t utf16_length = 8UL;
  const std::array<bool, 8> is_surrogate_pair_empty_rect = {
      false, true, false, false, true, false, false, true,
  };
  widget_input_handler->ImeSetComposition(
      surrogate_pair_mixed_composition, empty_ime_text_span,
      gfx::Range::InvalidRange(), 0, 0, base::DoNothing());
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

#if BUILDFLAG(IS_ANDROID)
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
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = GURL("data:text/html,world");
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->transition = ui::PAGE_TRANSITION_TYPED;
  common_params->navigation_start = base::TimeTicks::Now();
  auto commit_params = DummyCommitNavigationParams();
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
  std::string output = TestWebFrameContentDumper::DumpWebViewAsText(
                           web_view_, kMaxOutputCharacters)
                           .Utf8();
  EXPECT_EQ(output, "hello \n\nworld");
}

// This test ensures that a RenderFrame object is created for the top level
// frame in the RenderView.
TEST_F(RenderViewImplTest, BasicRenderFrame) {
  EXPECT_TRUE(GetMainRenderFrame());
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
               void(const std::u16string& text,
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

// Failing on Windows; see https://crbug.com/1134571.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MessageOrderInDidChangeSelection \
  DISABLED_MessageOrderInDidChangeSelection
#else
#define MAYBE_MessageOrderInDidChangeSelection MessageOrderInDidChangeSelection
#endif
TEST_F(RenderViewImplTextInputMessageOrder,
       MAYBE_MessageOrderInDidChangeSelection) {
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

  main_frame_widget()->SetHandlingInputEvent(true);
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");
}

class RendererErrorPageTest : public RenderViewImplTest {
 public:
  ContentRendererClient* CreateContentRendererClient() override {
    return new TestContentRendererClient;
  }

  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(GetMainRenderFrame());
  }

 private:
  class TestContentRendererClient : public ContentRendererClient {
   public:
    void PrepareErrorPage(content::RenderFrame* render_frame,
                          const blink::WebURLError& error,
                          const std::string& http_method,
                          content::mojom::AlternativeErrorPageOverrideInfoPtr
                              alternative_error_page_info,
                          std::string* error_html) override {
      if (error_html)
        *error_html = "A suffusion of yellow.";
    }
  };
};

TEST_F(RendererErrorPageTest, RegularError) {
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->url = GURL("http://example.com/error-page");
  auto commit_params = DummyCommitNavigationParams();
  commit_params->origin_to_commit =
      url::Origin::Create(common_params->url).DeriveNewOpaqueOrigin();
  TestRenderFrame* main_frame = static_cast<TestRenderFrame*>(frame());
  main_frame->NavigateWithError(
      std::move(common_params), std::move(commit_params),
      net::ERR_FILE_NOT_FOUND, net::ResolveErrorInfo(net::OK),
      "A suffusion of yellow.");

  // The error page itself is loaded asynchronously.
  FrameLoadWaiter(main_frame).Wait();
  const int kMaxOutputCharacters = 22;
  EXPECT_EQ("A suffusion of yellow.",
            TestWebFrameContentDumper::DumpWebViewAsText(web_view_,
                                                         kMaxOutputCharacters)
                .Ascii());
}

TEST_F(RenderViewImplTest, SetAccessibilityMode) {
  ASSERT_TRUE(GetAccessibilityMode().is_mode_off());
  ASSERT_TRUE(GetRenderAccessibilityManager());
  ASSERT_FALSE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());

  GetRenderAccessibilityManager()->SetMode(ui::kAXModeWebContentsOnly, 1);
  ASSERT_TRUE(GetAccessibilityMode() == ui::kAXModeWebContentsOnly);
  ASSERT_TRUE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());

  GetRenderAccessibilityManager()->SetMode(ui::AXMode::kNone, 0);
  ASSERT_TRUE(GetAccessibilityMode().is_mode_off());
  ASSERT_FALSE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());

  GetRenderAccessibilityManager()->SetMode(ui::kAXModeComplete, 1);
  ASSERT_TRUE(GetAccessibilityMode() == ui::kAXModeComplete);
  ASSERT_TRUE(GetRenderAccessibilityManager()->GetRenderAccessibilityImpl());
}

TEST_F(RenderViewImplTest, AccessibilityModeOnClosingConnection) {
  // Force the RenderAccessibilityManager to bind a pending receiver so that we
  // can test what happens after closing the remote endpoint.
  mojo::AssociatedRemote<blink::mojom::RenderAccessibility> remote;
  GetRenderAccessibilityManager()->BindReceiver(
      remote.BindNewEndpointAndPassReceiver());

  GetRenderAccessibilityManager()->SetMode(ui::kAXModeWebContentsOnly, 1);
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
  CommonParamsFrameLoadWaiter waiter(frame());
  frame()->LoadHTMLStringForTesting("hello world", GURL("data:text/html,"),
                                    "UTF-8", GURL(),
                                    false /* replace_current_item */);
  waiter.Wait();
  EXPECT_FALSE(waiter.common_params()->navigation_start.is_null());
  EXPECT_LE(lower_bound_navigation_start,
            waiter.common_params()->navigation_start);
}

// Checks that a browser-initiated navigation in an initial document that was
// not accessed uses browser-side timestamp.
// This test assumes that |frame()| contains an unaccessed initial document at
// start.
TEST_F(RenderViewImplTest, BrowserNavigationStart) {
  auto common_params = MakeCommonNavigationParams(-base::Seconds(1));

  CommonParamsFrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), DummyCommitNavigationParams());
  waiter.Wait();
  EXPECT_EQ(common_params->navigation_start,
            waiter.common_params()->navigation_start);
}

// Sanity check for the Navigation Timing API |navigationStart| override. We
// are asserting only most basic constraints, as TimeTicks (passed as the
// override) are not comparable with the wall time (returned by the Blink API).
TEST_F(RenderViewImplTest, BrowserNavigationStartSanitized) {
  // Verify that a navigation that claims to have started in the future - 42
  // days from now is *not* reported as one that starts in the future; as we
  // sanitize the override allowing a maximum of ::Now().
  auto late_common_params = MakeCommonNavigationParams(base::Days(42));
  late_common_params->method = "POST";

  frame()->Navigate(late_common_params.Clone(), DummyCommitNavigationParams());
  base::RunLoop().RunUntilIdle();
  base::Time after_navigation = base::Time::Now() + base::Days(1);

  base::Time late_nav_reported_start = base::Time::FromSecondsSinceUnixEpoch(
      GetMainFrame()->PerformanceMetricsForReporting().NavigationStart());
  EXPECT_LE(late_nav_reported_start, after_navigation);
}

// Checks that a browser-initiated navigation in an initial document that has
// been accessed uses browser-side timestamp (there may be arbitrary
// content and/or scripts injected, including beforeunload handler that shows
// a confirmation dialog).
TEST_F(RenderViewImplTest, NavigationStartWhenInitialDocumentWasAccessed) {
  // Trigger a didAccessInitialDocument notification.
  ExecuteJavaScriptForTests("document.title = 'Hi!';");

  auto common_params = MakeCommonNavigationParams(-base::Seconds(1));
  CommonParamsFrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), DummyCommitNavigationParams());
  waiter.Wait();
  EXPECT_EQ(common_params->navigation_start,
            waiter.common_params()->navigation_start);
}

TEST_F(RenderViewImplTest, NavigationStartForReload) {
  const char url_string[] = "data:text/html,<div>Page</div>";
  // Navigate once, then reload.
  LoadHTML(url_string);
  base::RunLoop().RunUntilIdle();

  auto common_params = blink::CreateCommonNavigationParams();
  common_params->url = GURL(url_string);
  common_params->navigation_type = blink::mojom::NavigationType::RELOAD;
  common_params->transition = ui::PAGE_TRANSITION_RELOAD;

  // The browser navigation_start should not be used because beforeunload will
  // be fired during Navigate.
  CommonParamsFrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), DummyCommitNavigationParams());
  waiter.Wait();

  // The browser navigation_start is always used.
  EXPECT_EQ(common_params->navigation_start,
            waiter.common_params()->navigation_start);
}

TEST_F(RenderViewImplTest, NavigationStartForSameProcessHistoryNavigation) {
  LoadHTML("<div id=pagename>Page A</div>");
  LoadHTML("<div id=pagename>Page B</div>");
  blink::PageState back_state = GetCurrentPageState();
  LoadHTML("<div id=pagename>Page C</div>");
  blink::PageState forward_state = GetCurrentPageState();
  base::RunLoop().RunUntilIdle();

  // Go back.
  auto common_params_back = blink::CreateCommonNavigationParams();
  common_params_back->url =
      GURL("data:text/html;charset=utf-8,<div id=pagename>Page B</div>");
  common_params_back->transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  common_params_back->navigation_type =
      blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
  auto final_common_params =
      GoToOffsetWithParams(-1, back_state, common_params_back.Clone(),
                           DummyCommitNavigationParams());

  // The browser navigation_start is always used.
  EXPECT_EQ(common_params_back->navigation_start,
            final_common_params->navigation_start);

  // Go forward.
  auto common_params_forward = blink::CreateCommonNavigationParams();
  common_params_forward->url =
      GURL("data:text/html;charset=utf-8,<div id=pagename>Page C</div>");
  common_params_forward->transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  common_params_forward->navigation_type =
      blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
  auto final_common_params2 =
      GoToOffsetWithParams(-1, back_state, common_params_back.Clone(),
                           DummyCommitNavigationParams());
  EXPECT_EQ(common_params_forward->navigation_start,
            final_common_params2->navigation_start);
}

TEST_F(RenderViewImplTest, NavigationStartForCrossProcessHistoryNavigation) {
  auto common_params = MakeCommonNavigationParams(-base::Seconds(1));
  common_params->transition = ui::PAGE_TRANSITION_FORWARD_BACK;
  common_params->navigation_type =
      blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;

  auto commit_params = DummyCommitNavigationParams();
  commit_params->page_state = blink::PageState::CreateForTesting(
                                  common_params->url, false, nullptr, nullptr)
                                  .ToEncodedData();
  commit_params->nav_entry_id = 42;
  commit_params->pending_history_list_offset = 1;
  commit_params->current_history_list_offset = 0;
  commit_params->current_history_list_length = 1;
  CommonParamsFrameLoadWaiter waiter(frame());
  frame()->Navigate(common_params.Clone(), std::move(commit_params));
  waiter.Wait();

  EXPECT_EQ(common_params->navigation_start,
            waiter.common_params()->navigation_start);
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
      blink::ZoomFactorToZoomLevel(2.0));
  web_view_->MainFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
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

// Ensure the `blink::WebView` history list is properly updated when starting a
// new browser-initiated navigation.
TEST_F(RenderViewImplTest, HistoryIsProperlyUpdatedOnNavigation) {
  blink::WebView* webview = web_view_;
  EXPECT_EQ(0, webview->HistoryBackListCount());
  EXPECT_EQ(0, webview->HistoryBackListCount() +
                   webview->HistoryForwardListCount() + 1);

  // Receive a CommitNavigation message with history parameters.
  auto commit_params = DummyCommitNavigationParams();
  commit_params->current_history_list_offset = 1;
  commit_params->current_history_list_length = 2;
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->should_replace_current_entry = true;
  frame()->Navigate(std::move(common_params), std::move(commit_params));

  // The current history list in `blink::WebView` is updated.
  EXPECT_EQ(1, webview->HistoryBackListCount());
  EXPECT_EQ(2, webview->HistoryBackListCount() +
                   webview->HistoryForwardListCount() + 1);
}

// Ensure the `blink::WebView` history list is properly updated when starting a
// new history browser-initiated navigation.
TEST_F(RenderViewImplTest, HistoryIsProperlyUpdatedOnHistoryNavigation) {
  blink::WebView* webview = web_view_;
  EXPECT_EQ(0, webview->HistoryBackListCount());
  EXPECT_EQ(0, webview->HistoryBackListCount() +
                   webview->HistoryForwardListCount() + 1);

  // Receive a CommitNavigation message with history parameters.
  auto commit_params = DummyCommitNavigationParams();
  commit_params->current_history_list_offset = 1;
  commit_params->current_history_list_length = 25;
  commit_params->pending_history_list_offset = 12;
  commit_params->nav_entry_id = 777;
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->navigation_type =
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->should_replace_current_entry = true;
  frame()->Navigate(std::move(common_params), std::move(commit_params));

  // The current history list in `blink::WebView` is updated.
  EXPECT_EQ(12, webview->HistoryBackListCount());
  EXPECT_EQ(25, webview->HistoryBackListCount() +
                    webview->HistoryForwardListCount() + 1);
}

// Ensure the `blink::WebView` history list is properly updated when starting a
// new history browser-initiated navigation with should_clear_history_list
TEST_F(RenderViewImplTest, HistoryIsProperlyUpdatedOnShouldClearHistoryList) {
  blink::WebView* webview = web_view_;
  EXPECT_EQ(0, webview->HistoryBackListCount());
  EXPECT_EQ(0, webview->HistoryBackListCount() +
                   webview->HistoryForwardListCount() + 1);

  // Receive a CommitNavigation message with history parameters.
  auto commit_params = DummyCommitNavigationParams();
  commit_params->current_history_list_offset = 12;
  commit_params->current_history_list_length = 25;
  commit_params->should_clear_history_list = true;
  frame()->Navigate(blink::CreateCommonNavigationParams(),
                    std::move(commit_params));

  // The current history list in `blink::WebView` is updated.
  EXPECT_EQ(0, webview->HistoryBackListCount());
  EXPECT_EQ(1, webview->HistoryBackListCount() +
                   webview->HistoryForwardListCount() + 1);
}

namespace {
class AddMessageToConsoleMockLocalFrameHost : public LocalFrameHostInterceptor {
 public:
  explicit AddMessageToConsoleMockLocalFrameHost(
      blink::AssociatedInterfaceProvider* provider)
      : LocalFrameHostInterceptor(provider) {}

  void DidAddMessageToConsole(
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& msg,
      uint32_t line_number,
      const std::optional<std::u16string>& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override {
    if (did_add_message_to_console_callback_) {
      std::move(did_add_message_to_console_callback_).Run(msg);
    }
  }

  void SetDidAddMessageToConsoleCallback(
      base::OnceCallback<void(const std::u16string& msg)> callback) {
    did_add_message_to_console_callback_ = std::move(callback);
  }

 private:
  base::OnceCallback<void(const std::u16string& msg)>
      did_add_message_to_console_callback_;
};
}  // namespace

class RenderViewImplAddMessageToConsoleTest : public RenderViewImplTest {
 public:
  using MockedTestRenderFrame = MockedLocalFrameHostInterceptorTestRenderFrame<
      AddMessageToConsoleMockLocalFrameHost>;

  RenderViewImplAddMessageToConsoleTest()
      : RenderViewImplTest(&MockedTestRenderFrame::CreateTestRenderFrame) {}

  AddMessageToConsoleMockLocalFrameHost* message_mock_frame_host() {
    return static_cast<MockedTestRenderFrame*>(frame())
        ->mock_local_frame_host();
  }
};

// Tests that there's no UaF after dispatchBeforeUnloadEvent.
// See https://crbug.com/666714.
TEST_F(RenderViewImplAddMessageToConsoleTest,
       DispatchBeforeUnloadCanDetachFrame) {
  LoadHTML(
      "<script>window.onbeforeunload = function() { "
      "window.console.log('OnBeforeUnload called'); }</script>");

  // Create a callback that unloads the frame when the 'OnBeforeUnload called'
  // log is printed from the beforeunload handler.
  base::RunLoop run_loop;
  bool was_callback_run = false;
  message_mock_frame_host()->SetDidAddMessageToConsoleCallback(
      base::BindLambdaForTesting([&](const std::u16string& msg) {
        // Makes sure this happens during the beforeunload handler.
        EXPECT_EQ(u"OnBeforeUnload called", msg);

        // Unloads the main frame.
        static_cast<mojom::Frame*>(frame())->Unload(
            /*is_loading=*/false, blink::mojom::FrameReplicationState::New(),
            blink::RemoteFrameToken(), CreateStubRemoteFrameInterfaces(),
            CreateStubRemoteMainFrameInterfaces());

        was_callback_run = true;
        run_loop.Quit();
      }));

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

  MOCK_METHOD3(RunModalAlertDialog,
               void(const std::u16string& alert_message,
                    bool disable_third_party_subframe_suppresion,
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
  std::u16string alert_message = u"Please don't crash";
  EXPECT_CALL(*alert_mock_frame_host(),
              RunModalAlertDialog(alert_message, false, testing::_))
      .WillOnce(base::test::RunOnceCallback<2>());
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

  EXPECT_EQ(1.f, web_view_->PageScaleFactor());
  EXPECT_EQ(1.f, web_view_->MinimumPageScaleFactor());

  blink::web_pref::WebPreferences prefs;
  prefs.shrinks_viewport_contents_to_fit = true;
  prefs.default_minimum_page_scale_factor = 0.1f;
  prefs.default_maximum_page_scale_factor = 5.5f;
  web_view_->SetWebPreferences(prefs);

  EXPECT_EQ(1.f, web_view_->PageScaleFactor());
  EXPECT_EQ(1.f, web_view_->MinimumPageScaleFactor());
  EXPECT_EQ(5.5f, web_view_->MaximumPageScaleFactor());
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

  ReceiveDisableDeviceEmulation();

  blink::DeviceEmulationParams params;
  ReceiveEnableDeviceEmulation(params);
  // Don't disable here to test that emulation is being shutdown properly.
}

TEST_F(RenderViewImplScaleFactorTest, ScreenMetricsEmulationWithOriginalDSF2) {
  float device_scale = 2.f;
  SetDeviceScaleFactor(device_scale);

  LoadHTML("<body style='min-height:1000px;'></body>");
  {
    SCOPED_TRACE("327x415 1dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.f, device_scale);
  }
  {
    SCOPED_TRACE("327x415 1.5dpr");
    TestEmulatedSizeDprDsf(327, 415, 1.5f, device_scale);
  }
  {
    SCOPED_TRACE("1005x1102 2dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 2.f, device_scale);
  }
  {
    SCOPED_TRACE("1005x1102 3dpr");
    TestEmulatedSizeDprDsf(1005, 1102, 3.f, device_scale);
  }

  ReceiveDisableDeviceEmulation();

  blink::DeviceEmulationParams params;
  ReceiveEnableDeviceEmulation(params);
  // Don't disable here to test that emulation is being shutdown properly.
}

TEST_F(RenderViewImplScaleFactorTest, ConvertViewportToWindow) {
  SetDeviceScaleFactor(1.f);
  {
    gfx::Rect rect(20, 10, 200, 100);
    gfx::Rect rect_in_dips =
        main_frame_widget()->BlinkSpaceToEnclosedDIPs(rect);
    EXPECT_EQ(rect, rect_in_dips);
  }

  SetDeviceScaleFactor(2.f);
  {
    gfx::Rect rect_in_dips = main_frame_widget()->BlinkSpaceToEnclosedDIPs(
        gfx::Rect(20, 10, 200, 100));
    EXPECT_EQ(10, rect_in_dips.x());
    EXPECT_EQ(5, rect_in_dips.y());
    EXPECT_EQ(100, rect_in_dips.width());
    EXPECT_EQ(50, rect_in_dips.height());
  }
}

#if BUILDFLAG(IS_MAC) || defined(USE_AURA)
TEST_F(RenderViewImplScaleFactorTest,
       DISABLED_GetCompositionCharacterBoundsTest) {  // http://crbug.com/582016
  SetDeviceScaleFactor(1.f);
  LoadHTML("<textarea id=\"test\"></textarea>");
  ExecuteJavaScriptForTests("document.getElementById('test').focus();");

  auto* widget_input_handler = GetWidgetInputHandler();
  const std::u16string empty_string;
  const std::vector<ui::ImeTextSpan> empty_ime_text_span;
  std::vector<gfx::Rect> bounds_at_1x;
  widget_input_handler->SetFocus(blink::mojom::FocusState::kFocused);

  // ASCII composition
  const std::u16string ascii_composition = u"aiueo";
  widget_input_handler->ImeSetComposition(
      ascii_composition, empty_ime_text_span, gfx::Range::InvalidRange(), 0, 0,
      base::DoNothing());
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

#if !BUILDFLAG(IS_ANDROID)
// No extensions/autoresize on Android.
namespace {

// Don't use text as it text will change the size in DIP at different
// scale factor.
const char kAutoResizeTestPage[] =
    "<div style='width=20px; height=20px'></div>";

}  // namespace

TEST_F(RenderViewImplScaleFactorTest, AutoResize) {
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
  EXPECT_FLOAT_EQ(0u, web_view_->MainFrameWidget()->GetZoomLevel());

  double zoom_level = blink::ZoomFactorToZoomLevel(0.25);
  // Change the zoom level to 25% and check if the view gets the change.
  main_frame_widget()->SetZoomLevelForTesting(zoom_level);
  // Use EXPECT_FLOAT_EQ here because view()->GetZoomLevel returns a float.
  EXPECT_FLOAT_EQ(zoom_level, web_view_->MainFrameWidget()->GetZoomLevel());
}

#endif

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
  blink::ScopedTestOriginTrialPolicy policy;

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
  blink::ScopedTestOriginTrialPolicy policy;

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

TEST_F(RenderViewImplTest, CollapseSelectionNotChangeFocus) {
  // https://crbug.com/1343298
  // Load an test HTML page consisting of an input field.
  LoadHTML(
      "<html>"
      "<head>"
      "</head>"
      "<style>"
      "html, body, input {"
      "    margin: 0;"
      "    width:100%;"
      "    height:100%;"
      "}"
      "</style>"
      "<body>"
      "<input id=\"test\" value=\"I love Cookie\"></input>"
      "</body>"
      "</html>");
  GetWidgetInputHandler()->SetFocus(blink::mojom::FocusState::kFocused);

  // Send a GestureTap event to change the value of the variable named
  // is_handle_visible_ of the class named FrameSelection to true
  WebGestureEvent gesture_event(WebInputEvent::Type::kGestureTap,
                                WebInputEvent::kNoModifiers,
                                ui::EventTimeForNow());
  gesture_event.SetPositionInWidget(gfx::PointF(250, 250));
  SendWebGestureEvent(gesture_event);

  // Check the input element was focused.
  int is_input = -1;
  std::u16string check_active_element_is_input =
      u"Number(document.activeElement.tagName == 'INPUT')";
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_active_element_is_input,
                                                 &is_input));
  EXPECT_EQ(1, is_input);

  // Blur the input element, the active element document should be BODY.
  ExecuteJavaScriptForTests("document.getElementById('test').blur();");

  int is_body = -1;
  std::u16string check_active_element_is_body =
      u"Number(document.activeElement.tagName == 'BODY')";
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_active_element_is_body,
                                                 &is_body));
  EXPECT_EQ(1, is_body);

  // Collapse selection, the active element document should be BODY too.
  auto* frame_widget_input_handler = GetFrameWidgetInputHandler();
  frame_widget_input_handler->CollapseSelection();
  base::RunLoop().RunUntilIdle();

  int is_body_again = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_active_element_is_body,
                                                 &is_body_again));
  EXPECT_EQ(1, is_body_again);
}

#if BUILDFLAG(IS_WIN)
class RenderViewImplContrastGammaSettingsTest : public RenderViewImplTest {
 protected:
  void SetUp() override {
    RenderViewImplTest::SetUp();
    feature_list_.InitAndEnableFeature(
        features::kUseGammaContrastRegistrySettings);
  }
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RenderViewImplContrastGammaSettingsTest,
       ContrastGammaSetRendererPreferences) {
  LoadHTML(R"HTML(
      <input id='test' type='text'></input>
    )HTML");

  // Use non-default values for contrast and gamma.
  constexpr float test_contrast = 0.95;
  static_assert(test_contrast != SK_GAMMA_CONTRAST);
  static_assert(test_contrast >= SkSurfaceProps::kMinContrastInclusive);
  static_assert(test_contrast <= SkSurfaceProps::kMaxContrastInclusive);

  constexpr float test_gamma = 3.99;
  static_assert(test_gamma != SK_GAMMA_EXPONENT);
  static_assert(test_gamma >= SkSurfaceProps::kMinGammaInclusive);
  static_assert(test_gamma < SkSurfaceProps::kMaxGammaExclusive);

  blink::RendererPreferences renderer_preferences =
      web_view_->GetRendererPreferences();
  EXPECT_NE(renderer_preferences.text_contrast, test_contrast);
  EXPECT_NE(renderer_preferences.text_gamma, test_gamma);

  // Set the non-default values on `RendererPreferences`.
  renderer_preferences.text_contrast = test_contrast;
  renderer_preferences.text_gamma = test_gamma;
  web_view_->SetRendererPreferences(renderer_preferences);

  // `GetSkSurfaceProps` should have the updated contrast and
  // gamma properties from above.
  SkSurfaceProps surface_props =
      skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  EXPECT_EQ(surface_props.textContrast(), test_contrast);
  EXPECT_EQ(surface_props.textGamma(), test_gamma);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
