// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_test_content_renderer_client.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/web_test_support.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/test_media_stream_renderer_factory.h"
#include "content/shell/renderer/web_test/test_websocket_handshake_throttle_provider.h"
#include "content/shell/renderer/web_test/web_test_render_frame_observer.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "media/base/audio_latency.h"
#include "media/base/mime_util.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/icc_profile.h"
#include "v8/include/v8.h"

using blink::WebAudioDevice;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebPlugin;
using blink::WebPluginParams;
using blink::WebRTCPeerConnectionHandler;
using blink::WebRTCPeerConnectionHandlerClient;
using blink::WebThemeEngine;

namespace content {

WebTestContentRendererClient::WebTestContentRendererClient() {
  EnableWebTestProxyCreation();
  SetWorkerRewriteURLFunction(RewriteWebTestsURL);
}

WebTestContentRendererClient::~WebTestContentRendererClient() {}

void WebTestContentRendererClient::RenderThreadStarted() {
  ShellContentRendererClient::RenderThreadStarted();
  shell_observer_.reset(new WebTestRenderThreadObserver());
}

void WebTestContentRendererClient::RenderFrameCreated(
    RenderFrame* render_frame) {
  new WebTestRenderFrameObserver(render_frame);
}

void WebTestContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new ShellRenderViewObserver(render_view);

  BlinkTestRunner* test_runner = BlinkTestRunner::Get(render_view);
  test_runner->Reset(false /* for_new_test */);
}

std::unique_ptr<blink::WebMediaStreamRendererFactory>
WebTestContentRendererClient::CreateMediaStreamRendererFactory() {
  return std::unique_ptr<blink::WebMediaStreamRendererFactory>(
      new TestMediaStreamRendererFactory());
}

std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
WebTestContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<TestWebSocketHandshakeThrottleProvider>();
}

void WebTestContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  blink::WebTestingSupport::InjectInternalsObject(context);
}

void WebTestContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // We always expose GC to web tests.
  std::string flags("--expose-gc");
  auto* command_line = base::CommandLine::ForCurrentProcess();
  v8::V8::SetFlagsFromString(flags.c_str(), flags.size());
  if (!command_line->HasSwitch(switches::kStableReleaseMode)) {
    blink::WebRuntimeFeatures::EnableTestOnlyFeatures(true);
  }
  if (command_line->HasSwitch(switches::kEnableFontAntialiasing)) {
    blink::SetFontAntialiasingEnabledForTest(true);
  }
  if (command_line->HasSwitch(
          switches::kDisableOriginTrialControlledBlinkFeatures)) {
    blink::WebRuntimeFeatures::EnableOriginTrialControlledFeatures(false);
  }
}

bool WebTestContentRendererClient::IsIdleMediaSuspendEnabled() {
  // Disable idle media suspend to avoid web tests getting into accidentally
  // bad states if they take too long to run.
  return false;
}

}  // namespace content
