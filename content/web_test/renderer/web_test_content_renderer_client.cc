// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_test_content_renderer_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_frame_observer.h"
#include "content/web_test/common/web_test_switches.h"
#include "content/web_test/renderer/blink_test_helpers.h"
#include "content/web_test/renderer/test_websocket_handshake_throttle_provider.h"
#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "content/web_test/renderer/web_test_render_thread_observer.h"
#include "media/base/audio_latency.h"
#include "media/base/mime_util.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_dedicated_or_shared_worker_fetch_context.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/test/frame_widget_test_helper.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "ui/gfx/icc_profile.h"
#include "v8/include/v8.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#endif

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC)
#include "skia/ext/test_fonts.h"
#endif

namespace content {

namespace {

RenderFrameImpl* CreateWebFrameTestProxy(RenderFrameImpl::CreateParams params) {
  return new WebFrameTestProxy(
      std::move(params),
      WebTestRenderThreadObserver::GetInstance()->test_runner());
}

blink::WebFrameWidget* CreateWebTestWebFrameWidget(
    base::PassKey<blink::WebLocalFrame> pass_key,
    blink::CrossVariantMojoAssociatedRemote<
        blink::mojom::FrameWidgetHostInterfaceBase> frame_widget_host,
    blink::CrossVariantMojoAssociatedReceiver<
        blink::mojom::FrameWidgetInterfaceBase> frame_widget,
    blink::CrossVariantMojoAssociatedRemote<
        blink::mojom::WidgetHostInterfaceBase> widget_host,
    blink::CrossVariantMojoAssociatedReceiver<blink::mojom::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited,
    bool is_for_child_local_root,
    bool is_for_nested_main_frame,
    bool is_for_scalable_page) {
  return blink::FrameWidgetTestHelper::CreateTestWebFrameWidget(
      std::move(pass_key), std::move(frame_widget_host),
      std::move(frame_widget), std::move(widget_host), std::move(widget),
      std::move(task_runner), frame_sink_id, hidden, never_composited,
      is_for_child_local_root, is_for_nested_main_frame, is_for_scalable_page,
      WebTestRenderThreadObserver::GetInstance()->test_runner());
}

}  // namespace

WebTestContentRendererClient::WebTestContentRendererClient() {
  // Web tests subclass these types, so we inject factory methods to replace
  // the creation of the production type with the subclasses.
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
  create_widget_callback_ = base::BindRepeating(&CreateWebTestWebFrameWidget);
  blink::InstallCreateWebFrameWidgetHook(&create_widget_callback_);

  blink::UniqueNameHelper::PreserveStableUniqueNameForTesting();
  blink::WebDedicatedOrSharedWorkerFetchContext::InstallRewriteURLFunction(
      RewriteWebTestsURL);
}

WebTestContentRendererClient::~WebTestContentRendererClient() {
  blink::InstallCreateWebFrameWidgetHook(nullptr);
}

void WebTestContentRendererClient::RenderThreadStarted() {
  ShellContentRendererClient::RenderThreadStarted();

  render_thread_observer_ = std::make_unique<WebTestRenderThreadObserver>();

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC)
  // On these platforms, fonts are set up in the renderer process. Other
  // platforms set up fonts as part of WebTestBrowserMainRunner in the
  // browser process, via WebTestBrowserPlatformInitialize().
  skia::InitializeSkFontMgrForTest();
#elif BUILDFLAG(IS_WIN)
  // DirectWrite only has access to %WINDIR%\Fonts by default. For developer
  // side-loading, support kRegisterFontFiles to allow access to additional
  // fonts. The browser process sets these files and punches a hole in the
  // sandbox for the renderer to load them here.
  {
    sk_sp<SkFontMgr> fontmgr = SkFontMgr_New_DirectWrite();
    for (const auto& file : switches::GetSideloadFontFiles()) {
      sk_sp<SkTypeface> typeface = fontmgr->makeFromFile(file.c_str());
      blink::WebFontRendering::AddSideloadedFontForTesting(std::move(typeface));
    }
  }
#endif
}

void WebTestContentRendererClient::RenderFrameCreated(
    RenderFrame* render_frame) {
  // Intentionally doesn't call the base class, as we only use web test
  // observers.
  // TODO(danakj): The ShellRenderFrameObserver is doing stuff only for
  // browser tests. If we only create that for browser tests then this
  // override is not needed.
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
WebTestContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<TestWebSocketHandshakeThrottleProvider>();
}

void WebTestContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  blink::WebTestingSupport::InjectInternalsObject(context);

  // Intentionally doesn't call the base class to avoid injecting twice.
  // TODO(danakj): The ShellRenderFrameObserver is doing stuff only for
  // browser tests. If we only create that for browser tests then we don't
  // need to avoid the base class.
}

void WebTestContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // PerformanceManager is used by measure-memory web platform tests.
  blink::WebRuntimeFeatures::EnablePerformanceManagerInstrumentation(true);
  // We always expose GC to web tests.
  std::string flags("--expose-gc");
  auto* command_line = base::CommandLine::ForCurrentProcess();
  v8::V8::SetFlagsFromString(flags.c_str(), flags.size());
  if (command_line->HasSwitch(switches::kEnableFontAntialiasing)) {
    blink::SetFontAntialiasingEnabledForTest(true);
  }
}

bool WebTestContentRendererClient::IsIdleMediaSuspendEnabled() {
  // Disable idle media suspend to avoid web tests getting into accidentally
  // bad states if they take too long to run.
  return false;
}

}  // namespace content
