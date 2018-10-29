// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_view_test.h"

#include <stddef.h>

#include <cctype>

#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/app/mojo/mojo_init.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/renderer.mojom.h"
#include "content/common/visual_properties.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/mock_render_process.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame.h"
#include "net/base/escape.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if defined(OS_WIN)
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_init_impl_win.h"
#include "content/test/dwrite_font_fake_sender_win.h"
#endif

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebScriptSource;
using blink::WebString;
using blink::WebURLRequest;

namespace content {

namespace {

class CloseMessageSendingRenderViewVisitor : public RenderViewVisitor {
 public:
  CloseMessageSendingRenderViewVisitor() = default;
  ~CloseMessageSendingRenderViewVisitor() override = default;

 protected:
  bool Visit(RenderView* render_view) override {
    // Simulate the Widget receiving a close message. This should result on
    // releasing the internal reference counts and destroying the internal
    // state.
    WidgetMsg_Close msg(render_view->GetRoutingID());
    RenderWidget* render_widget =
        static_cast<RenderViewImpl*>(render_view)->GetWidget();
    render_widget->OnMessageReceived(msg);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CloseMessageSendingRenderViewVisitor);
};

// Converts |ascii_character| into |key_code| and returns true on success.
// Handles only the characters needed by tests.
bool GetWindowsKeyCode(char ascii_character, int* key_code) {
  if (isalnum(ascii_character)) {
    *key_code = base::ToUpperASCII(ascii_character);
    return true;
  }

  switch (ascii_character) {
    case '@':
      *key_code = '2';
      return true;
    case '_':
      *key_code = ui::VKEY_OEM_MINUS;
      return true;
    case '.':
      *key_code = ui::VKEY_OEM_PERIOD;
      return true;
    case ui::VKEY_BACK:
      *key_code = ui::VKEY_BACK;
      return true;
    default:
      return false;
  }
}

}  // namespace

class RendererBlinkPlatformImplTestOverrideImpl
    : public RendererBlinkPlatformImpl {
 public:
  explicit RendererBlinkPlatformImplTestOverrideImpl(
      blink::scheduler::WebThreadScheduler* scheduler)
      : RendererBlinkPlatformImpl(scheduler) {}

  // Get rid of the dependency to the sandbox, which is not available in
  // RenderViewTest.
  blink::WebSandboxSupport* GetSandboxSupport() override { return nullptr; }
};

RenderViewTest::RendererBlinkPlatformImplTestOverride::
    RendererBlinkPlatformImplTestOverride() {
  InitializeMojo();
}

RenderViewTest::RendererBlinkPlatformImplTestOverride::
    ~RendererBlinkPlatformImplTestOverride() {
}

RendererBlinkPlatformImpl*
RenderViewTest::RendererBlinkPlatformImplTestOverride::Get() const {
  return blink_platform_impl_.get();
}

void RenderViewTest::RendererBlinkPlatformImplTestOverride::Initialize() {
  main_thread_scheduler_ =
      blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler();
  blink_platform_impl_ =
      std::make_unique<RendererBlinkPlatformImplTestOverrideImpl>(
          main_thread_scheduler_.get());
}

void RenderViewTest::RendererBlinkPlatformImplTestOverride::Shutdown() {
  main_thread_scheduler_->Shutdown();
  blink_platform_impl_->Shutdown();
}

RenderViewTest::RenderViewTest() {
  // Overrides creation of RenderFrameImpl, but does not need to
  // override behaviour of WebWidgetClient.
  RenderFrameImpl::InstallCreateHook(&TestRenderFrame::CreateTestRenderFrame,
                                     nullptr, nullptr);
}

RenderViewTest::~RenderViewTest() {
}

WebLocalFrame* RenderViewTest::GetMainFrame() {
  return view_->GetWebView()->MainFrame()->ToWebLocalFrame();
}

void RenderViewTest::ExecuteJavaScriptForTests(const char* js) {
  GetMainFrame()->ExecuteScript(WebScriptSource(WebString::FromUTF8(js)));
}

bool RenderViewTest::ExecuteJavaScriptAndReturnIntValue(
    const base::string16& script,
    int* int_result) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result = GetMainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource(blink::WebString::FromUTF16(script)));
  if (result.IsEmpty() || !result->IsInt32())
    return false;

  if (int_result)
    *int_result = result.As<v8::Int32>()->Value();

  return true;
}

bool RenderViewTest::ExecuteJavaScriptAndReturnNumberValue(
    const base::string16& script,
    double* number_result) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result = GetMainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource(blink::WebString::FromUTF16(script)));
  if (result.IsEmpty() || !result->IsNumber())
    return false;

  if (number_result)
    *number_result = result.As<v8::Number>()->Value();

  return true;
}

void RenderViewTest::LoadHTML(const char* html) {
  std::string url_string = "data:text/html;charset=utf-8,";
  url_string.append(net::EscapeQueryParamValue(html, false));
  GetMainFrame()->LoadHTMLString(std::string(html),
                                 blink::WebURL(GURL(url_string)));
  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  FrameLoadWaiter(view_->GetMainRenderFrame()).Wait();
  view_->GetWebView()->UpdateAllLifecyclePhases();
}

void RenderViewTest::LoadHTMLWithUrlOverride(const char* html,
                                             const char* url_override) {
  GetMainFrame()->LoadHTMLString(std::string(html),
                                 blink::WebURL(GURL(url_override)));
  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  FrameLoadWaiter(view_->GetMainRenderFrame()).Wait();
  view_->GetWebView()->UpdateAllLifecyclePhases();
}

PageState RenderViewTest::GetCurrentPageState() {
  RenderViewImpl* view_impl = static_cast<RenderViewImpl*>(view_);

  // This returns a PageState object for the main frame, excluding subframes.
  // This could be extended to all local frames if needed by tests, but it
  // cannot include out-of-process frames.
  TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(view_impl->GetMainRenderFrame());
  return SingleHistoryItemToPageState(frame->current_history_item());
}

void RenderViewTest::GoBack(const GURL& url, const PageState& state) {
  GoToOffset(-1, url, state);
}

void RenderViewTest::GoForward(const GURL& url, const PageState& state) {
  GoToOffset(1, url, state);
}

void RenderViewTest::SetUp() {
  // Ensure that this looks like the renderer process based on the command line.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kProcessType, switches::kRendererProcess);

  test_io_thread_ =
      std::make_unique<base::TestIOThread>(base::TestIOThread::kAutoStart);
  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      test_io_thread_->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  // Subclasses can set render_thread_ with their own implementation before
  // calling RenderViewTest::SetUp().
  // The render thread needs to exist before blink::Initialize. It also mirrors
  // the order on Chromium initialization.
  if (!render_thread_)
    render_thread_ = std::make_unique<MockRenderThread>();

  // Blink needs to be initialized before calling CreateContentRendererClient()
  // because it uses blink internally.
  blink_platform_impl_.Initialize();
  blink::Initialize(blink_platform_impl_.Get(), &binder_registry_,
                    blink_platform_impl_.GetMainThreadScheduler());

  content_client_.reset(CreateContentClient());
  content_browser_client_.reset(CreateContentBrowserClient());
  content_renderer_client_.reset(CreateContentRendererClient());
  SetContentClient(content_client_.get());
  SetBrowserClientForTesting(content_browser_client_.get());
  SetRendererClientForTesting(content_renderer_client_.get());

#if defined(OS_WIN)
  // This needs to happen sometime before PlatformInitialize.
  // This isn't actually necessary for most tests: most tests are able to
  // connect to their browser process which runs the real proxy host. However,
  // some tests route IPCs to MockRenderThread, which is unable to process the
  // font IPCs, causing all font loading to fail.
  SetDWriteFontProxySenderForTesting(CreateFakeCollectionSender());
#endif

#if defined(OS_MACOSX)
  autorelease_pool_ = std::make_unique<base::mac::ScopedNSAutoreleasePool>();
#endif
  command_line_ =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
  // We don't use the descriptor here anyways so it's ok to pass -1.
  base::FieldTrialList::CreateTrialsFromCommandLine(
      *command_line_, switches::kFieldTrialHandle, -1);
  params_ = std::make_unique<MainFunctionParams>(*command_line_);
  platform_ = std::make_unique<RendererMainPlatformDelegate>(*params_);
  platform_->PlatformInitialize();

  // Setting flags and really doing anything with WebKit is fairly fragile and
  // hacky, but this is the world we live in...
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));

  // Ensure that we register any necessary schemes when initializing WebKit,
  // since we are using a MockRenderThread.
  RenderThreadImpl::RegisterSchemes();

  RenderThreadImpl::SetRendererBlinkPlatformImplForTesting(
      blink_platform_impl_.Get());

  // This check is needed because when run under content_browsertests,
  // ResourceBundle isn't initialized (since we have to use a diferent test
  // suite implementation than for content_unittests). For browser_tests, this
  // is already initialized.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  compositor_deps_ = CreateCompositorDependencies();
  mock_process_ = std::make_unique<MockRenderProcess>();

  mojom::CreateViewParamsPtr view_params = mojom::CreateViewParams::New();
  view_params->opener_frame_route_id = MSG_ROUTING_NONE;
  view_params->window_was_created_with_opener = false;
  view_params->renderer_preferences = RendererPreferences();
  view_params->web_preferences = WebPreferences();
  view_params->view_id = render_thread_->GetNextRoutingID();
  view_params->main_frame_widget_routing_id =
      render_thread_->GetNextRoutingID();
  view_params->main_frame_routing_id = render_thread_->GetNextRoutingID();
  render_thread_->PassInitialInterfaceProviderRequestForFrame(
      view_params->main_frame_routing_id,
      mojo::MakeRequest(&view_params->main_frame_interface_provider));
  view_params->session_storage_namespace_id =
      blink::AllocateSessionStorageNamespaceId();
  view_params->swapped_out = false;
  view_params->replicated_frame_state = FrameReplicationState();
  view_params->proxy_routing_id = MSG_ROUTING_NONE;
  view_params->hidden = false;
  view_params->never_visible = false;
  view_params->visual_properties = *InitialVisualProperties();

  view_ = RenderViewImpl::Create(compositor_deps_.get(), std::move(view_params),
                                 RenderWidget::ShowCallback(),
                                 base::ThreadTaskRunnerHandle::Get());
}

void RenderViewTest::TearDown() {
  // Run the loop so the release task from the renderwidget executes.
  base::RunLoop().RunUntilIdle();

  blink::mojom::LeakDetectorPtr leak_detector;
  BindInterface(&binder_registry_, &leak_detector);

  // Close the main |view_| as well as any other windows that might have been
  // opened by the test.
  CloseMessageSendingRenderViewVisitor closing_visitor;
  RenderView::ForEach(&closing_visitor);

  // |view_| is ref-counted and deletes itself during the RunUntilIdle() call
  // below.
  view_ = nullptr;
  mock_process_.reset();

  // After telling the view to close and resetting mock_process_ we may get
  // some new tasks which need to be processed before shutting down WebKit
  // (http://crbug.com/21508).
  base::RunLoop().RunUntilIdle();

  RenderThreadImpl::SetRendererBlinkPlatformImplForTesting(nullptr);

#if defined(OS_WIN)
  ClearDWriteFontProxySenderForTesting();
#endif

#if defined(OS_MACOSX)
  autorelease_pool_.reset();
#endif

  {
    base::RunLoop run_loop;
    leak_detector->PerformLeakDetection(base::BindOnce(
        [](base::OnceClosure closure,
           blink::mojom::LeakDetectionResultPtr result) {
          EXPECT_EQ(0u, result->number_of_live_audio_nodes);
          EXPECT_EQ(0u, result->number_of_live_documents);
          EXPECT_EQ(0u, result->number_of_live_nodes);
          EXPECT_EQ(0u, result->number_of_live_layout_objects);
          EXPECT_EQ(0u, result->number_of_live_resources);
          EXPECT_EQ(0u, result->number_of_live_pausable_objects);
          EXPECT_EQ(0u, result->number_of_live_script_promises);
          EXPECT_EQ(0u, result->number_of_live_frames);
          EXPECT_EQ(0u, result->number_of_live_v8_per_context_data);
          EXPECT_EQ(0u, result->number_of_worker_global_scopes);
          EXPECT_EQ(0u, result->number_of_live_resource_fetchers);
          std::move(closure).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();
  }

  blink_platform_impl_.Shutdown();
  platform_->PlatformUninitialize();
  platform_.reset();
  params_.reset();
  command_line_.reset();

  test_io_thread_.reset();
  ipc_support_.reset();
}

void RenderViewTest::SendNativeKeyEvent(
    const NativeWebKeyboardEvent& key_event) {
  SendWebKeyboardEvent(key_event);
}

void RenderViewTest::SendInputEvent(const blink::WebInputEvent& input_event) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->HandleInputEvent(blink::WebCoalescedInputEvent(input_event),
                         ui::LatencyInfo(), HandledEventCallback());
}

void RenderViewTest::SendWebKeyboardEvent(
    const blink::WebKeyboardEvent& key_event) {
  SendInputEvent(key_event);
}

void RenderViewTest::SendWebMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  SendInputEvent(mouse_event);
}

void RenderViewTest::SendWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  SendInputEvent(gesture_event);
}

gfx::Rect RenderViewTest::GetElementBounds(const std::string& element_id) {
  static constexpr char kGetCoordinatesScript[] =
      "(function() {"
      "  function GetCoordinates(elem) {"
      "    if (!elem)"
      "      return [ 0, 0];"
      "    var coordinates = [ elem.offsetLeft, elem.offsetTop];"
      "    var parent_coordinates = GetCoordinates(elem.offsetParent);"
      "    coordinates[0] += parent_coordinates[0];"
      "    coordinates[1] += parent_coordinates[1];"
      "    return [ Math.round(coordinates[0]),"
      "             Math.round(coordinates[1])];"
      "  };"
      "  var elem = document.getElementById('$1');"
      "  if (!elem)"
      "    return null;"
      "  var bounds = GetCoordinates(elem);"
      "  bounds[2] = Math.round(elem.offsetWidth);"
      "  bounds[3] = Math.round(elem.offsetHeight);"
      "  return bounds;"
      "})();";
  std::vector<std::string> params;
  params.push_back(element_id);
  std::string script =
      base::ReplaceStringPlaceholders(kGetCoordinatesScript, params, nullptr);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> value = GetMainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUTF8(script)));
  if (value.IsEmpty() || !value->IsArray())
    return gfx::Rect();

  v8::Local<v8::Array> array = value.As<v8::Array>();
  if (array->Length() != 4)
    return gfx::Rect();
  std::vector<int> coords;
  for (int i = 0; i < 4; ++i) {
    v8::Local<v8::Number> index = v8::Number::New(isolate, i);
    v8::Local<v8::Value> value = array->Get(index);
    if (value.IsEmpty() || !value->IsInt32())
      return gfx::Rect();
    coords.push_back(value.As<v8::Int32>()->Value());
  }
  return gfx::Rect(coords[0], coords[1], coords[2], coords[3]);
}

bool RenderViewTest::SimulateElementClick(const std::string& element_id) {
  gfx::Rect bounds = GetElementBounds(element_id);
  if (bounds.IsEmpty())
    return false;
  SimulatePointClick(bounds.CenterPoint());
  return true;
}

void RenderViewTest::SimulatePointClick(const gfx::Point& point) {
  WebMouseEvent mouse_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  mouse_event.click_count = 1;
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->HandleInputEvent(blink::WebCoalescedInputEvent(mouse_event),
                         ui::LatencyInfo(), HandledEventCallback());
  mouse_event.SetType(WebInputEvent::kMouseUp);
  impl->HandleInputEvent(blink::WebCoalescedInputEvent(mouse_event),
                         ui::LatencyInfo(), HandledEventCallback());
}


bool RenderViewTest::SimulateElementRightClick(const std::string& element_id) {
  gfx::Rect bounds = GetElementBounds(element_id);
  if (bounds.IsEmpty())
    return false;
  SimulatePointRightClick(bounds.CenterPoint());
  return true;
}

void RenderViewTest::SimulatePointRightClick(const gfx::Point& point) {
  WebMouseEvent mouse_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  mouse_event.click_count = 1;
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->HandleInputEvent(blink::WebCoalescedInputEvent(mouse_event),
                         ui::LatencyInfo(), HandledEventCallback());
  mouse_event.SetType(WebInputEvent::kMouseUp);
  impl->HandleInputEvent(blink::WebCoalescedInputEvent(mouse_event),
                         ui::LatencyInfo(), HandledEventCallback());
}

void RenderViewTest::SimulateRectTap(const gfx::Rect& rect) {
  WebGestureEvent gesture_event(
      WebInputEvent::kGestureTap, WebInputEvent::kNoModifiers,
      ui::EventTimeForNow(), blink::kWebGestureDeviceTouchscreen);
  gesture_event.SetPositionInWidget(gfx::PointF(rect.CenterPoint()));
  gesture_event.data.tap.tap_count = 1;
  gesture_event.data.tap.width = rect.width();
  gesture_event.data.tap.height = rect.height();
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->HandleInputEvent(blink::WebCoalescedInputEvent(gesture_event),
                         ui::LatencyInfo(), HandledEventCallback());
  impl->FocusChangeComplete();
}

void RenderViewTest::SetFocused(const blink::WebNode& node) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->FocusedNodeChanged(blink::WebNode(), node);
}

void RenderViewTest::Reload(const GURL& url) {
  CommonNavigationParams common_params(
      url, Referrer(), ui::PAGE_TRANSITION_LINK, FrameMsg_Navigate_Type::RELOAD,
      true, false, GURL(), GURL(), PREVIEWS_UNSPECIFIED, base::TimeTicks::Now(),
      "GET", nullptr, base::Optional<SourceLocation>(),
      false /* started_from_context_menu */, false /* has_user_gesture */,
      InitiatorCSPInfo());
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(impl->GetMainRenderFrame());
  frame->Navigate(common_params, RequestNavigationParams());
  FrameLoadWaiter(frame).Wait();
  view_->GetWebView()->UpdateAllLifecyclePhases();
}

void RenderViewTest::Resize(gfx::Size new_size,
                            bool is_fullscreen_granted) {
  VisualProperties visual_properties;
  visual_properties.screen_info = ScreenInfo();
  visual_properties.new_size = new_size;
  visual_properties.compositor_viewport_pixel_size = new_size;
  visual_properties.top_controls_height = 0.f;
  visual_properties.browser_controls_shrink_blink_size = false;
  visual_properties.is_fullscreen_granted = is_fullscreen_granted;
  visual_properties.display_mode = blink::kWebDisplayModeBrowser;
  std::unique_ptr<IPC::Message> resize_message(
      new WidgetMsg_SynchronizeVisualProperties(0, visual_properties));
  OnMessageReceived(*resize_message);
}

void RenderViewTest::SimulateUserTypingASCIICharacter(char ascii_character,
                                                      bool flush_message_loop) {
  int modifiers = blink::WebInputEvent::kNoModifiers;
  if (isupper(ascii_character) || ascii_character == '@' ||
      ascii_character == '_') {
    modifiers = blink::WebKeyboardEvent::kShiftKey;
  }

  blink::WebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown, modifiers,
                                ui::EventTimeForNow());
  event.text[0] = ascii_character;
  ASSERT_TRUE(GetWindowsKeyCode(ascii_character, &event.windows_key_code));
  SendWebKeyboardEvent(event);

  event.SetType(blink::WebKeyboardEvent::kChar);
  SendWebKeyboardEvent(event);

  event.SetType(blink::WebKeyboardEvent::kKeyUp);
  SendWebKeyboardEvent(event);

  if (flush_message_loop) {
    // Processing is delayed because of a Blink bug:
    // https://bugs.webkit.org/show_bug.cgi?id=16976 See
    // PasswordAutofillAgent::TextDidChangeInTextField() for details.
    base::RunLoop().RunUntilIdle();
  }
}

void RenderViewTest::SimulateUserInputChangeForElement(
    blink::WebInputElement* input,
    const std::string& new_value) {
  ASSERT_TRUE(base::IsStringASCII(new_value));
  while (!input->Focused())
    input->GetDocument().GetFrame()->View()->AdvanceFocus(false);

  size_t previous_length = input->Value().length();
  for (size_t i = 0; i < previous_length; ++i)
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, false);

  EXPECT_TRUE(input->Value().Utf8().empty());
  for (size_t i = 0; i < new_value.size(); ++i)
    SimulateUserTypingASCIICharacter(new_value[i], false);

  // Compare only beginning, because autocomplete may have filled out the
  // form.
  EXPECT_EQ(new_value, input->Value().Utf8().substr(0, new_value.length()));

  base::RunLoop().RunUntilIdle();
}

bool RenderViewTest::OnMessageReceived(const IPC::Message& msg) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  return impl->OnMessageReceived(msg);
}

void RenderViewTest::OnSameDocumentNavigation(blink::WebLocalFrame* frame,
                                              bool is_new_navigation) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  blink::WebHistoryItem item;
  item.Initialize();

  // Set the document sequence number to be the same as the current page.
  const blink::WebHistoryItem& current_item =
      impl->GetMainRenderFrame()->current_history_item();
  DCHECK(!current_item.IsNull());
  item.SetDocumentSequenceNumber(current_item.DocumentSequenceNumber());

  impl->GetMainRenderFrame()->DidFinishSameDocumentNavigation(
      item,
      is_new_navigation ? blink::kWebStandardCommit
                        : blink::kWebHistoryInertCommit,
      false /* content_initiated */);
}

blink::WebWidget* RenderViewTest::GetWebWidget() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  return impl->GetWidget()->GetWebWidget();
}

ContentClient* RenderViewTest::CreateContentClient() {
  return new TestContentClient;
}

ContentBrowserClient* RenderViewTest::CreateContentBrowserClient() {
  return new ContentBrowserClient;
}

ContentRendererClient* RenderViewTest::CreateContentRendererClient() {
  return new ContentRendererClient;
}

std::unique_ptr<VisualProperties> RenderViewTest::InitialVisualProperties() {
  auto initial_visual_properties = std::make_unique<VisualProperties>();
  // Ensure the view has some size so tests involving scrolling bounds work.
  initial_visual_properties->new_size = gfx::Size(400, 300);
  initial_visual_properties->visible_viewport_size = gfx::Size(400, 300);
  return initial_visual_properties;
}

std::unique_ptr<CompositorDependencies>
RenderViewTest::CreateCompositorDependencies() {
  return std::make_unique<FakeCompositorDependencies>();
}

void RenderViewTest::GoToOffset(int offset,
                                const GURL& url,
                                const PageState& state) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);

  int history_list_length =
      impl->HistoryBackListCount() + impl->HistoryForwardListCount() + 1;
  int pending_offset = offset + impl->history_list_offset_;

  CommonNavigationParams common_params(
      url, Referrer(), ui::PAGE_TRANSITION_FORWARD_BACK,
      FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT, true, false, GURL(),
      GURL(), PREVIEWS_UNSPECIFIED, base::TimeTicks::Now(), "GET", nullptr,
      base::Optional<SourceLocation>(), false /* started_from_context_menu */,
      false /* has_user_gesture */, InitiatorCSPInfo());
  RequestNavigationParams request_params;
  request_params.page_state = state;
  request_params.nav_entry_id = pending_offset + 1;
  request_params.pending_history_list_offset = pending_offset;
  request_params.current_history_list_offset = impl->history_list_offset_;
  request_params.current_history_list_length = history_list_length;

  TestRenderFrame* frame =
      static_cast<TestRenderFrame*>(impl->GetMainRenderFrame());
  frame->Navigate(common_params, request_params);

  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  FrameLoadWaiter(frame).Wait();
  view_->GetWebView()->UpdateAllLifecyclePhases();
}

}  // namespace content
