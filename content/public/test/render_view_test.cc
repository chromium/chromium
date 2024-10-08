// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_view_test.h"

#include <stddef.h>

#include <optional>
#include <string_view>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/test/test_task_graph_runner.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/app/mojo/mojo_init.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/frame.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/fake_render_widget_host.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/policy_container_utils.h"
#include "content/renderer/mock_agent_scheduling_group.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_frame.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/page/browsing_context_group_info.mojom.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_v8_features.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/native_theme/native_theme_features.h"
#include "v8/include/v8.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

#if BUILDFLAG(IS_WIN)
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

class FailingURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  FailingURLLoaderFactory() = default;

  FailingURLLoaderFactory(const FailingURLLoaderFactory&) = delete;
  FailingURLLoaderFactory& operator=(const FailingURLLoaderFactory&) = delete;

  // SharedURLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    mojo::Remote<network::mojom::URLLoaderClient> remote(std::move(client));
    remote->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    receivers_.Add(this, std::move(receiver), this);
  }
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    return std::make_unique<PendingFactory>();
  }

 private:
  class PendingFactory : public network::PendingSharedURLLoaderFactory {
   public:
    PendingFactory() = default;

    PendingFactory(const PendingFactory&) = delete;
    PendingFactory& operator=(const PendingFactory&) = delete;

    ~PendingFactory() override = default;

    scoped_refptr<SharedURLLoaderFactory> CreateFactory() override {
      return base::MakeRefCounted<FailingURLLoaderFactory>();
    }
  };

  ~FailingURLLoaderFactory() override = default;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    scoped_refptr<FailingURLLoaderFactory>>
      receivers_;
};

class MockColorProviderSource : public ui::ColorProviderSource {
 public:
  explicit MockColorProviderSource() = default;
  MockColorProviderSource(const MockColorProviderSource&) = delete;
  MockColorProviderSource& operator=(const MockColorProviderSource&) = delete;
  ~MockColorProviderSource() override = default;

  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override {
    return &provider_;
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    auto key = GetColorProviderKey();
    key.color_mode = color_mode;
    key.forced_colors = forced_colors;
    ui::ColorProvider* color_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    CHECK(color_provider);
    return ui::CreateRendererColorMap(*color_provider);
  }

  ui::ColorProviderKey GetColorProviderKey() const override { return key_; }

 private:
  ui::ColorProvider provider_;
  ui::ColorProviderKey key_;
};

// Converts |ascii_character| into |key_code| and returns true on success.
// Handles only the characters needed by tests.
bool GetWindowsKeyCode(char ascii_character, int* key_code) {
  if (absl::ascii_isalnum(static_cast<unsigned char>(ascii_character))) {
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
    case ui::VKEY_END:
      *key_code = ui::VKEY_END;
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

#if BUILDFLAG(IS_ANDROID)
  void SetPrivateMemoryFootprint(
      uint64_t private_memory_footprint_bytes) override {}
#endif
};

class RenderFrameWasShownWaiter : public RenderFrameObserver {
 public:
  explicit RenderFrameWasShownWaiter(RenderFrame* frame)
      : RenderFrameObserver(frame) {}

  void Wait() {
    if (was_shown_)
      return;

    run_loop_.Run();
  }

 private:
  // RenderFrameObserver implementation.
  void WasShown() override {
    was_shown_ = true;
    if (run_loop_.running())
      run_loop_.Quit();
  }
  void OnDestruct() override {}

  bool was_shown_ = false;
  base::RunLoop run_loop_;
};

RenderViewTest::RendererBlinkPlatformImplTestOverride::
    RendererBlinkPlatformImplTestOverride() {
  InitializeMojo();
}

RenderViewTest::RendererBlinkPlatformImplTestOverride::
    ~RendererBlinkPlatformImplTestOverride() = default;

RendererBlinkPlatformImpl*
RenderViewTest::RendererBlinkPlatformImplTestOverride::Get() const {
  return blink_platform_impl_.get();
}

void RenderViewTest::RendererBlinkPlatformImplTestOverride::Initialize() {
  blink::Platform::InitializeBlink();
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

RenderViewTest::RenderViewTest(bool hook_render_frame_creation)
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  // Overrides creation of RenderFrameImpl. Subclasses may wish to do this
  // themselves and it can only be done once.
  if (hook_render_frame_creation)
    RenderFrameImpl::InstallCreateHook(&TestRenderFrame::CreateTestRenderFrame);
}

RenderViewTest::~RenderViewTest() = default;

WebLocalFrame* RenderViewTest::GetMainFrame() {
  return web_view_->MainFrame()->ToWebLocalFrame();
}

RenderFrame* RenderViewTest::GetMainRenderFrame() {
  return RenderFrame::FromWebFrame(GetMainFrame());
}

v8::Isolate* RenderViewTest::Isolate() {
  return GetMainFrame()->GetAgentGroupScheduler()->Isolate();
}

void RenderViewTest::ExecuteJavaScriptForTests(std::string_view js) {
  GetMainFrame()->ExecuteScript(WebScriptSource(WebString::FromUTF8(js)));
}

bool RenderViewTest::ExecuteJavaScriptAndReturnIntValue(
    const std::u16string& script,
    int* int_result) {
  v8::HandleScope handle_scope(Isolate());
  v8::Local<v8::Value> result = GetMainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource(blink::WebString::FromUTF16(script)));
  if (result.IsEmpty() || !result->IsInt32())
    return false;

  if (int_result)
    *int_result = result.As<v8::Int32>()->Value();

  return true;
}

bool RenderViewTest::ExecuteJavaScriptAndReturnNumberValue(
    const std::u16string& script,
    double* number_result) {
  v8::HandleScope handle_scope(Isolate());
  v8::Local<v8::Value> result = GetMainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource(blink::WebString::FromUTF16(script)));
  if (result.IsEmpty() || !result->IsNumber())
    return false;

  if (number_result)
    *number_result = result.As<v8::Number>()->Value();

  return true;
}

void RenderViewTest::LoadHTML(std::string_view html) {
  FrameLoadWaiter waiter(GetMainRenderFrame());
  std::string url_string = "data:text/html;charset=utf-8,";
  url_string.append(base::EscapeQueryParamValue(html, false));
  RenderFrame::FromWebFrame(GetMainFrame())
      ->LoadHTMLStringForTesting(html, GURL(url_string), "UTF-8", GURL(),
                                 /*replace_current_item=*/false);
  // The load may happen asynchronously, so we pump messages to process
  // the pending continuation.
  waiter.Wait();
  web_view_->MainFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

void RenderViewTest::LoadHTMLWithUrlOverride(std::string_view html,
                                             std::string_view url_override) {
  FrameLoadWaiter waiter(GetMainRenderFrame());
  RenderFrame::FromWebFrame(GetMainFrame())
      ->LoadHTMLStringForTesting(html, GURL(url_override), "UTF-8", GURL(),
                                 /*replace_current_item=*/false);
  // The load may happen asynchronously, so we pump messages to process
  // the pending continuation.
  waiter.Wait();
  web_view_->MainFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

blink::PageState RenderViewTest::GetCurrentPageState() {
  // This returns a PageState object for the main frame, excluding subframes.
  // This could be extended to all local frames if needed by tests, but it
  // cannot include out-of-process frames.
  auto* frame = GetMainFrame();
  return frame->CurrentHistoryItemToPageState();
}

void RenderViewTest::GoBack(const GURL& url, const blink::PageState& state) {
  GoToOffset(-1, url, state);
}

void RenderViewTest::GoForward(const GURL& url, const blink::PageState& state) {
  GoToOffset(1, url, state);
}

void RenderViewTest::SetUp() {
  ContentTestSuiteBase::InitializeResourceBundle();

  // Ensure that this looks like the renderer process based on the command line.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kProcessType, switches::kRendererProcess);

  // Enable Blink's experimental and test only features so that test code
  // does not have to bother enabling each feature.
  blink::WebRuntimeFeatures::EnableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::EnableTestOnlyFeatures(true);
  blink::WebRuntimeFeatures::EnableOverlayScrollbars(
      ui::IsOverlayScrollbarEnabled());
  blink::WebV8Features::InitializeMojoJSAllowedProtectedMemory();

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

  render_thread_->SetIOTaskRunner(test_io_thread_->task_runner());

  // Setting flags and really doing anything with WebKit is fairly fragile and
  // hacky, but this is the world we live in...
  // Note that we need to set flags *before* initializing V8 (as part of blink),
  // as it's not allowed to modify them later.
  v8::V8::SetFlagsFromString("--expose-gc");

  // ContentClient must be initialized before Blink, because Blink now eagerly
  // loads the default stylesheets, which are fetched from the resource bundle
  // using ContentClient.
  content_client_.reset(CreateContentClient());
  SetContentClient(content_client_.get());

#if BUILDFLAG(IS_WIN)
  // This needs to happen sometime before PlatformInitialize.
  // This isn't actually necessary for most tests: most tests are able to
  // connect to their browser process which runs the real proxy host. However,
  // some tests route IPCs to MockRenderThread, which is unable to process the
  // font IPCs, causing all font loading to fail.
  SetDWriteFontProxySenderForTesting(CreateFakeCollectionSender());
#endif

#if BUILDFLAG(IS_MAC)
  autorelease_pool_.emplace();
#endif
  command_line_ =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  params_ = std::make_unique<MainFunctionParams>(command_line_.get());
  platform_ = std::make_unique<RendererMainPlatformDelegate>(*params_);
  platform_->PlatformInitialize();

  // Blink needs to be initialized before calling CreateContentRendererClient()
  // because it uses Blink internally.
  blink_platform_impl_.Initialize();
  blink::Initialize(blink_platform_impl_.Get(), &binders_,
                    blink_platform_impl_.GetMainThreadScheduler());

  content_browser_client_.reset(CreateContentBrowserClient());
  content_renderer_client_.reset(CreateContentRendererClient());
  SetBrowserClientForTesting(content_browser_client_.get());
  SetRendererClientForTesting(content_renderer_client_.get());

  agent_scheduling_group_ = MockAgentSchedulingGroup::Create(*render_thread_);
  render_widget_host_ = CreateRenderWidgetHost();

  // Ensure that we register any necessary schemes when initializing WebKit,
  // since we are using a MockRenderThread.
  RenderThreadImpl::RegisterSchemes();

  // This check is needed because when run under content_browsertests,
  // ResourceBundle isn't initialized (since we have to use a diferent test
  // suite implementation than for content_unittests). For browser_tests, this
  // is already initialized.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  process_ = std::make_unique<RenderProcess>();

  // This is used to get the renderer color maps for the purpose of creating the
  // color providers in Blink::Page.
  MockColorProviderSource mock_color_provider_source_ =
      MockColorProviderSource();

  blink::ColorProviderColorMaps color_maps = blink::ColorProviderColorMaps{
      mock_color_provider_source_.GetRendererColorMap(
          ui::ColorProviderKey::ColorMode::kLight,
          ui::ColorProviderKey::ForcedColors::kNone),
      mock_color_provider_source_.GetRendererColorMap(
          ui::ColorProviderKey::ColorMode::kDark,
          ui::ColorProviderKey::ForcedColors::kNone),
      mock_color_provider_source_.GetRendererColorMap(
          mock_color_provider_source_.GetColorMode(),
          ui::ColorProviderKey::ForcedColors::kActive)};

  mojom::CreateViewParamsPtr view_params = mojom::CreateViewParams::New();
  view_params->opener_frame_token = std::nullopt;
  view_params->window_was_opened_by_another_window = false;
  view_params->renderer_preferences = blink::RendererPreferences();
  view_params->web_preferences = blink::web_pref::WebPreferences();
  view_params->color_provider_colors = color_maps;
  view_params->replication_state = blink::mojom::FrameReplicationState::New();
  view_params->blink_page_broadcast =
      page_broadcast_.BindNewEndpointAndPassDedicatedReceiver();

  auto main_frame_params = mojom::CreateLocalMainFrameParams::New();
  main_frame_params->routing_id = render_thread_->GetNextRoutingID();
  main_frame_params->frame = TestRenderFrame::CreateStubFrameReceiver();
  // Ignoring the returned PendingReceiver because it is not bound to anything
  std::ignore =
      main_frame_params->interface_broker.InitWithNewPipeAndPassReceiver();
  main_frame_params->associated_interface_provider_remote =
      TestRenderFrame::CreateStubAssociatedInterfaceProviderRemote();
  policy_container_host_ = std::make_unique<MockPolicyContainerHost>();
  main_frame_params->policy_container =
      policy_container_host_->CreatePolicyContainerForBlink();

  auto widget_params = mojom::CreateFrameWidgetParams::New();
  widget_params->routing_id = render_thread_->GetNextRoutingID();
  std::tie(widget_params->widget_host, widget_params->widget) =
      render_widget_host_->BindNewWidgetInterfaces();
  std::tie(widget_params->frame_widget_host, widget_params->frame_widget) =
      render_widget_host_->BindNewFrameWidgetInterfaces();
  widget_params->visual_properties = InitialVisualProperties();
  main_frame_params->widget_params = std::move(widget_params);
  main_frame_params->subresource_loader_factories =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>();

  view_params->main_frame =
      mojom::CreateMainFrameUnion::NewLocalParams(std::move(main_frame_params));

  view_params->session_storage_namespace_id =
      blink::AllocateSessionStorageNamespaceId();
  view_params->hidden = false;
  view_params->never_composited = false;

  view_params->browsing_context_group_info =
      blink::BrowsingContextGroupInfo::CreateUnique();

  web_view_ =
      agent_scheduling_group_->CreateWebView(std::move(view_params),
                                             /*was_created_by_renderer=*/false,
                                             /*base_url=*/blink::WebURL());

  RenderFrameWasShownWaiter waiter(
      RenderFrame::FromWebFrame(web_view_->MainFrame()->ToWebLocalFrame()));
  render_widget_host_->widget_remote_for_testing()->WasShown(
      /*was_evicted=*/false,
      blink::mojom::RecordContentToVisibleTimeRequestPtr());
  waiter.Wait();
}

void RenderViewTest::TearDown() {
  // Run the loop so the release task from the renderwidget executes.
  base::RunLoop().RunUntilIdle();

  mojo::Remote<blink::mojom::LeakDetector> leak_detector;
  mojo::GenericPendingReceiver receiver(
      leak_detector.BindNewPipeAndPassReceiver());
  std::ignore = binders_.TryBind(&receiver);

  render_thread_->ReleaseAllWebViews();

  // Resetting `page_broadcast_` will cause the WebView to close itself.
  page_broadcast_.reset();

  web_view_ = nullptr;
  process_.reset();

  // After telling the view to close and resetting process_ we may get
  // some new tasks which need to be processed before shutting down WebKit
  // (http://crbug.com/21508).
  base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_WIN)
  ClearDWriteFontProxySenderForTesting();
#endif

#if BUILDFLAG(IS_MAC)
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
          EXPECT_EQ(0u,
                    result->number_of_live_context_lifecycle_state_observers);
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
    const input::NativeWebKeyboardEvent& key_event) {
  SendWebKeyboardEvent(key_event);
}

void RenderViewTest::SendInputEvent(const blink::WebInputEvent& input_event) {
  GetWebFrameWidget()->ProcessInputEventSynchronouslyForTesting(
      blink::WebCoalescedInputEvent(input_event, ui::LatencyInfo()));
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

  v8::Isolate* isolate = Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> value = GetMainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUTF8(script)));
  if (value.IsEmpty() || !value->IsArray())
    return gfx::Rect();

  v8::Local<v8::Array> array = value.As<v8::Array>();
  v8::Local<v8::Context> v8_context =
      array->GetCreationContext(isolate).ToLocalChecked();
  v8::Context::Scope v8_context_scope(v8_context);
  if (array->Length() != 4)
    return gfx::Rect();
  std::vector<int> coords;
  for (int i = 0; i < 4; ++i) {
    v8::Local<v8::Number> index = v8::Number::New(isolate, i);
    if (!array->Get(v8_context, index).ToLocal(&value) || !value->IsInt32()) {
      return gfx::Rect();
    }
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
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  mouse_event.click_count = 1;
  GetWebFrameWidget()->ProcessInputEventSynchronouslyForTesting(
      blink::WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  GetWebFrameWidget()->ProcessInputEventSynchronouslyForTesting(
      blink::WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
}

bool RenderViewTest::SimulateElementRightClick(const std::string& element_id) {
  gfx::Rect bounds = GetElementBounds(element_id);
  if (bounds.IsEmpty())
    return false;
  SimulatePointRightClick(bounds.CenterPoint());
  return true;
}

void RenderViewTest::SimulatePointRightClick(const gfx::Point& point) {
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  mouse_event.click_count = 1;
  GetWebFrameWidget()->ProcessInputEventSynchronouslyForTesting(
      blink::WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  GetWebFrameWidget()->ProcessInputEventSynchronouslyForTesting(
      blink::WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
}

void RenderViewTest::SimulateRectTap(const gfx::Rect& rect) {
  WebGestureEvent gesture_event(
      WebInputEvent::Type::kGestureTap, WebInputEvent::kNoModifiers,
      ui::EventTimeForNow(), blink::WebGestureDevice::kTouchscreen);
  gesture_event.SetPositionInWidget(gfx::PointF(rect.CenterPoint()));
  gesture_event.data.tap.tap_count = 1;
  gesture_event.data.tap.width = rect.width();
  gesture_event.data.tap.height = rect.height();
  GetWebFrameWidget()->ProcessInputEventSynchronouslyForTesting(
      blink::WebCoalescedInputEvent(gesture_event, ui::LatencyInfo()));
}

void RenderViewTest::SetFocused(const blink::WebElement& element) {
  auto* frame = RenderFrameImpl::FromWebFrame(element.GetDocument().GetFrame());
  if (frame)
    frame->FocusedElementChanged(element);
}

void RenderViewTest::ChangeFocusToNull(const blink::WebDocument& document) {
  auto* frame = RenderFrameImpl::FromWebFrame(document.GetFrame());
  if (frame)
    frame->FocusedElementChanged(blink::WebElement());
}

void RenderViewTest::Reload(const GURL& url) {
  auto common_params = blink::mojom::CommonNavigationParams::New(
      url, /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, blink::mojom::Referrer::New(),
      ui::PAGE_TRANSITION_LINK, blink::mojom::NavigationType::RELOAD,
      blink::NavigationDownloadPolicy(), false, GURL(), base::TimeTicks::Now(),
      "GET", nullptr, network::mojom::SourceLocation::New(),
      false /* started_from_context_menu */, false /* has_user_gesture */,
      false /* has_text_fragment_token */,
      network::mojom::CSPDisposition::CHECK, std::vector<int>(), std::string(),
      false /* is_history_navigation_in_new_child_frame */,
      base::TimeTicks() /* input_start */,
      network::mojom::RequestDestination::kDocument);
  auto commit_params = blink::CreateCommitNavigationParams();
  TestRenderFrame* frame = static_cast<TestRenderFrame*>(GetMainRenderFrame());
  FrameLoadWaiter waiter(frame);
  frame->Navigate(std::move(common_params), std::move(commit_params));
  waiter.Wait();
  web_view_->MainFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

void RenderViewTest::Resize(gfx::Size new_size, bool is_fullscreen_granted) {
  blink::VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());
  visual_properties.new_size = new_size;
  visual_properties.compositor_viewport_pixel_rect = gfx::Rect(new_size);
  visual_properties.is_fullscreen_granted = is_fullscreen_granted;
  visual_properties.display_mode = blink::mojom::DisplayMode::kBrowser;

  GetWebFrameWidget()->ApplyVisualProperties(visual_properties);
}

void RenderViewTest::SimulateUserTypingASCIICharacter(char ascii_character,
                                                      bool flush_message_loop) {
  int modifiers = blink::WebInputEvent::kNoModifiers;
  if (absl::ascii_isupper(static_cast<unsigned char>(ascii_character)) ||
      ascii_character == '@' || ascii_character == '_') {
    modifiers = blink::WebKeyboardEvent::kShiftKey;
  }

  blink::WebKeyboardEvent event(blink::WebKeyboardEvent::Type::kRawKeyDown,
                                modifiers, ui::EventTimeForNow());
  event.text[0] = ascii_character;
  ASSERT_TRUE(GetWindowsKeyCode(ascii_character, &event.windows_key_code));
  SendWebKeyboardEvent(event);

  event.SetType(blink::WebKeyboardEvent::Type::kChar);
  SendWebKeyboardEvent(event);

  event.SetType(blink::WebKeyboardEvent::Type::kKeyUp);
  SendWebKeyboardEvent(event);

  if (flush_message_loop) {
    // Processing is delayed because of a Blink bug:
    // https://bugs.webkit.org/show_bug.cgi?id=16976 See
    // PasswordAutofillAgent::TextDidChangeInTextField() for details.
    base::RunLoop().RunUntilIdle();
  }
}

void RenderViewTest::SimulateUserInputChangeForElement(
    blink::WebInputElement input,
    std::string_view new_value) {
  ASSERT_TRUE(base::IsStringASCII(new_value));
  while (!input.Focused()) {
    input.GetDocument().GetFrame()->View()->AdvanceFocus(false);
  }
  SimulateUserTypingASCIICharacter(ui::VKEY_END, false);

  size_t previous_length = input.Value().length();
  for (size_t i = 0; i < previous_length; ++i) {
    SimulateUserTypingASCIICharacter(ui::VKEY_BACK, false);
  }
  EXPECT_TRUE(input.Value().Utf8().empty());
  for (char c : new_value) {
    SimulateUserTypingASCIICharacter(c, false);
  }
  // Compare only beginning, because autocomplete may have filled out the
  // form.
  EXPECT_EQ(new_value, input.Value().Utf8().substr(0, new_value.length()));

  base::RunLoop().RunUntilIdle();
}

void RenderViewTest::SimulateUserInputChangeForElementById(
    std::string_view id,
    std::string_view new_value) {
  blink::WebInputElement element =
      GetMainFrame()
          ->GetDocument()
          .GetElementById(WebString(base::UTF8ToUTF16(id)))
          .DynamicTo<blink::WebInputElement>();
  ASSERT_TRUE(element);
  SimulateUserInputChangeForElement(element, new_value);
}

void RenderViewTest::OnSameDocumentNavigation(blink::WebLocalFrame* frame,
                                              bool is_new_navigation) {
  static_cast<RenderFrameImpl*>(GetMainRenderFrame())
      ->DidFinishSameDocumentNavigation(
          is_new_navigation ? blink::kWebStandardCommit
                            : blink::kWebHistoryInertCommit,
          true /* is_synchronously_committed */,
          blink::mojom::SameDocumentNavigationType::kFragment,
          false /* is_client_redirect */,
          /*screenshot_destination=*/std::nullopt);
}

blink::WebFrameWidget* RenderViewTest::GetWebFrameWidget() {
  return web_view_->MainFrameWidget();
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

std::unique_ptr<FakeRenderWidgetHost> RenderViewTest::CreateRenderWidgetHost() {
  return std::make_unique<FakeRenderWidgetHost>();
}

blink::VisualProperties RenderViewTest::InitialVisualProperties() {
  blink::VisualProperties initial_visual_properties;
  initial_visual_properties.screen_infos =
      display::ScreenInfos(display::ScreenInfo());
  // Ensure the view has some size so tests involving scrolling bounds work.
  initial_visual_properties.new_size = gfx::Size(400, 300);
  initial_visual_properties.visible_viewport_size = gfx::Size(400, 300);
  return initial_visual_properties;
}

void RenderViewTest::GoToOffset(int offset,
                                const GURL& url,
                                const blink::PageState& state) {
  blink::WebView* webview = web_view_;
  int history_list_length =
      webview->HistoryBackListCount() + webview->HistoryForwardListCount() + 1;
  int pending_offset = offset + webview->HistoryBackListCount();

  auto common_params = blink::mojom::CommonNavigationParams::New(
      url, /* initiator_origin= */ std::nullopt,
      /* initiator_base_url= */ std::nullopt, blink::mojom::Referrer::New(),
      ui::PAGE_TRANSITION_FORWARD_BACK,
      blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT,
      blink::NavigationDownloadPolicy(), false, GURL(), base::TimeTicks::Now(),
      "GET", nullptr, network::mojom::SourceLocation::New(),
      false /* started_from_context_menu */, false /* has_user_gesture */,
      false /* has_text_fragment_token */,
      network::mojom::CSPDisposition::CHECK, std::vector<int>(), std::string(),
      false /* is_history_navigation_in_new_child_frame */,
      base::TimeTicks() /* input_start */,
      network::mojom::RequestDestination::kDocument);
  auto commit_params = blink::CreateCommitNavigationParams();
  commit_params->page_state = state.ToEncodedData();
  commit_params->nav_entry_id = pending_offset + 1;
  commit_params->pending_history_list_offset = pending_offset;
  commit_params->current_history_list_offset = webview->HistoryBackListCount();
  commit_params->current_history_list_length = history_list_length;

  auto* frame = static_cast<TestRenderFrame*>(GetMainRenderFrame());
  FrameLoadWaiter waiter(frame);
  frame->Navigate(std::move(common_params), std::move(commit_params));
  // The load may actually happen asynchronously, so we pump messages to process
  // the pending continuation.
  waiter.Wait();
  webview->MainFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

void RenderViewTest::CreateFakeURLLoaderFactory() {
  RenderFrameImpl* main_frame =
      static_cast<RenderFrameImpl*>(GetMainRenderFrame());
  DCHECK(main_frame);
  main_frame->SetURLLoaderFactoryOverrideForTest(
      base::MakeRefCounted<FailingURLLoaderFactory>());
}

}  // namespace content
