// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

#include "content/public/renderer/media_stream_renderer_factory.h"
#include "media/base/renderer_factory.h"
#include "third_party/blink/public/platform/modules/webmidi/web_midi_accessor.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_media_stream_center.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/public/platform/web_speech_synthesizer.h"
#include "ui/gfx/icc_profile.h"
#include "url/gurl.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return nullptr;
}

SkBitmap* ContentRendererClient::GetSadWebViewBitmap() {
  return nullptr;
}

bool ContentRendererClient::MaybeCreateMimeHandlerView(
    RenderFrame* embedder_frame,
    const blink::WebElement& owner_element,
    const GURL& original_url,
    const std::string& original_mime_type,
    int32_t instance_id_to_use) {
  return false;
}

bool ContentRendererClient::OverrideCreatePlugin(
    RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  return false;
}

blink::WebPlugin* ContentRendererClient::CreatePluginReplacement(
    RenderFrame* render_frame,
    const base::FilePath& plugin_path) {
  return nullptr;
}

bool ContentRendererClient::HasErrorPage(int http_status_code) {
  return false;
}

bool ContentRendererClient::ShouldSuppressErrorPage(RenderFrame* render_frame,
                                                    const GURL& url) {
  return false;
}

bool ContentRendererClient::ShouldTrackUseCounter(const GURL& url) {
  return true;
}

bool ContentRendererClient::DeferMediaLoad(RenderFrame* render_frame,
                                           bool has_played_media_before,
                                           base::OnceClosure closure) {
  std::move(closure).Run();
  return false;
}

std::unique_ptr<blink::WebMIDIAccessor>
ContentRendererClient::OverrideCreateMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  return nullptr;
}

blink::WebThemeEngine* ContentRendererClient::OverrideThemeEngine() {
  return nullptr;
}

std::unique_ptr<WebSocketHandshakeThrottleProvider>
ContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return nullptr;
}

std::unique_ptr<blink::WebSpeechSynthesizer>
ContentRendererClient::OverrideSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return nullptr;
}

void ContentRendererClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner* io_thread_task_runner) {}

void ContentRendererClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* compositor_thread_task_runner) {}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup() {
  return false;
}

#if defined(OS_ANDROID)
bool ContentRendererClient::HandleNavigation(
    RenderFrame* render_frame,
    bool is_content_initiated,
    bool render_view_was_created_by_renderer,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  return false;
}
#endif

bool ContentRendererClient::ShouldFork(blink::WebLocalFrame* frame,
                                       const GURL& url,
                                       const std::string& http_method,
                                       bool is_initial_navigation,
                                       bool is_server_redirect) {
  return false;
}

void ContentRendererClient::WillSendRequest(blink::WebLocalFrame* frame,
                                            ui::PageTransition transition_type,
                                            const blink::WebURL& url,
                                            const url::Origin* initiator_origin,
                                            GURL* new_url,
                                            bool* attach_same_site_cookies) {}

bool ContentRendererClient::IsPrefetchOnly(
    RenderFrame* render_frame,
    const blink::WebURLRequest& request) {
  return false;
}

unsigned long long ContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool ContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

blink::WebPrescientNetworking*
ContentRendererClient::GetPrescientNetworking() {
  return nullptr;
}

bool ContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderFrame* render_frame,
    blink::mojom::PageVisibilityState* override_state) {
  return false;
}

bool ContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  return false;
}

bool ContentRendererClient::IsOriginIsolatedPepperPlugin(
    const base::FilePath& plugin_path) {
  return false;
}

void ContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) {}

bool ContentRendererClient::IsKeySystemsUpdateNeeded() {
  return false;
}

bool ContentRendererClient::IsSupportedAudioConfig(
    const media::AudioConfig& config) {
  // Defer to media's default support.
  return ::media::IsSupportedAudioConfig(config);
}

bool ContentRendererClient::IsSupportedVideoConfig(
    const media::VideoConfig& config) {
  // Defer to media's default support.
  return ::media::IsSupportedVideoConfig(config);
}

bool ContentRendererClient::IsSupportedBitstreamAudioCodec(
    media::AudioCodec codec) {
  return false;
}

std::unique_ptr<MediaStreamRendererFactory>
ContentRendererClient::CreateMediaStreamRendererFactory() {
  return nullptr;
}

bool ContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) const {
  return false;
}

std::unique_ptr<blink::WebContentSettingsClient>
ContentRendererClient::CreateWorkerContentSettingsClient(
    RenderFrame* render_frame) {
  return nullptr;
}

bool ContentRendererClient::IsPluginAllowedToUseCameraDeviceAPI(
    const GURL& url) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToUseCompositorAPI(const GURL& url) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
  return false;
}

BrowserPluginDelegate* ContentRendererClient::CreateBrowserPluginDelegate(
    RenderFrame* render_frame,
    const WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url) {
  return nullptr;
}

bool ContentRendererClient::IsExcludedHeaderForServiceWorkerFetchEvent(
    const std::string& header_name) {
  return false;
}

bool ContentRendererClient::ShouldEnforceWebRTCRoutingPreferences() {
  return true;
}

base::Optional<std::string>
ContentRendererClient::WebRTCPlatformSpecificAudioProcessingConfiguration() {
  return base::Optional<std::string>();
}

GURL ContentRendererClient::OverrideFlashEmbedWithHTML(const GURL& url) {
  return GURL();
}

std::unique_ptr<base::TaskScheduler::InitParams>
ContentRendererClient::GetTaskSchedulerInitParams() {
  return nullptr;
}

bool ContentRendererClient::IsIdleMediaSuspendEnabled() {
  return true;
}

bool ContentRendererClient::IsBackgroundMediaSuspendEnabled(
    RenderFrame* render_frame) {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

bool ContentRendererClient::OverrideLegacySymantecCertConsoleMessage(
    const GURL& url,
    std::string* console_messsage) {
  return false;
}

std::unique_ptr<URLLoaderThrottleProvider>
ContentRendererClient::CreateURLLoaderThrottleProvider(
    URLLoaderThrottleProviderType provider_type) {
  return nullptr;
}

blink::WebFrame* ContentRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
  return nullptr;
}

bool ContentRendererClient::IsSafeRedirectTarget(const GURL& url) {
  return true;
}

}  // namespace content
