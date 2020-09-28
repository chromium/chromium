// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_test_content_renderer_client.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "build/build_config.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_frame_observer.h"
#include "content/web_test/common/web_test_switches.h"
#include "content/web_test/renderer/blink_test_helpers.h"
#include "content/web_test/renderer/test_websocket_handshake_throttle_provider.h"
#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "content/web_test/renderer/web_test_render_thread_observer.h"
#include "content/web_test/renderer/web_view_test_proxy.h"
#include "content/web_test/renderer/web_widget_test_proxy.h"
#include "media/base/audio_latency.h"
#include "media/base/mime_util.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "ui/gfx/icc_profile.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#endif

#if defined(OS_FUCHSIA) || defined(OS_MAC)
#include "skia/ext/test_fonts.h"
#endif

namespace content {

namespace {

RenderViewImpl* CreateWebViewTestProxy(
    AgentSchedulingGroup& agent_scheduling_group,
    CompositorDependencies* compositor_deps,
    const mojom::CreateViewParams& params) {
  return new WebViewTestProxy(
      agent_scheduling_group, compositor_deps, params,
      WebTestRenderThreadObserver::GetInstance()->test_runner());
}

std::unique_ptr<RenderWidget> CreateWebWidgetTestProxy(
    AgentSchedulingGroup& agent_scheduling_group,
    int32_t routing_id,
    CompositorDependencies* compositor_deps) {
  return std::make_unique<WebWidgetTestProxy>(agent_scheduling_group,
                                              routing_id, compositor_deps);
}

RenderFrameImpl* CreateWebFrameTestProxy(RenderFrameImpl::CreateParams params) {
  return new WebFrameTestProxy(std::move(params));
}

}  // namespace

WebTestContentRendererClient::WebTestContentRendererClient() {
  // Web tests subclass these types, so we inject factory methods to replace
  // the creation of the production type with the subclasses.
  RenderViewImpl::InstallCreateHook(CreateWebViewTestProxy);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
  // For RenderWidgets, web tests only subclass the ones attached to frames.
  RenderWidget::InstallCreateForFrameHook(CreateWebWidgetTestProxy);

  blink::UniqueNameHelper::PreserveStableUniqueNameForTesting();
  WebWorkerFetchContextImpl::InstallRewriteURLFunction(RewriteWebTestsURL);
}

WebTestContentRendererClient::~WebTestContentRendererClient() = default;

void WebTestContentRendererClient::RenderThreadStarted() {
  ShellContentRendererClient::RenderThreadStarted();

  render_thread_observer_ = std::make_unique<WebTestRenderThreadObserver>();

#if defined(OS_FUCHSIA) || defined(OS_MAC)
  // On these platforms, fonts are set up in the renderer process. Other
  // platforms set up fonts as part of WebTestBrowserMainRunner in the
  // browser process, via WebTestBrowserPlatformInitialize().
  skia::ConfigureTestFont();
#elif defined(OS_WIN)
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

std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
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
