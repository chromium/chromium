// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

#include "build/build_config.h"
#include "media/base/demuxer.h"
#include "media/base/renderer_factory.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "ui/gfx/icc_profile.h"
#include "url/gurl.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return nullptr;
}

SkBitmap* ContentRendererClient::GetSadWebViewBitmap() {
  return nullptr;
}

bool ContentRendererClient::IsPluginHandledExternally(
    RenderFrame* embedder_frame,
    const blink::WebElement& owner_element,
    const GURL& original_url,
    const std::string& original_mime_type) {
  return false;
}

v8::Local<v8::Object> ContentRendererClient::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
  return v8::Local<v8::Object>();
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

bool ContentRendererClient::ShouldTrackUseCounter(const GURL& url) {
  return true;
}

bool ContentRendererClient::DeferMediaLoad(RenderFrame* render_frame,
                                           bool has_played_media_before,
                                           base::OnceClosure closure) {
  std::move(closure).Run();
  return false;
}

std::unique_ptr<media::Demuxer> ContentRendererClient::OverrideDemuxerForUrl(
    RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

blink::WebThemeEngine* ContentRendererClient::OverrideThemeEngine() {
  return nullptr;
}

std::unique_ptr<WebSocketHandshakeThrottleProvider>
ContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
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

void ContentRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url,
    bool* force_ignore_site_for_cookies) {}

bool ContentRendererClient::IsPrefetchOnly(
    RenderFrame* render_frame,
    const blink::WebURLRequest& request) {
  return false;
}

uint64_t ContentRendererClient::VisitedLinkHash(const char* canonical_url,
                                                size_t length) {
  return 0;
}

bool ContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return false;
}

std::unique_ptr<blink::WebPrescientNetworking>
ContentRendererClient::CreatePrescientNetworking(RenderFrame* render_frame) {
  return nullptr;
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

bool ContentRendererClient::IsSupportedAudioType(const media::AudioType& type) {
  // Defer to media's default support.
  return ::media::IsDefaultSupportedAudioType(type);
}

bool ContentRendererClient::IsSupportedVideoType(const media::VideoType& type) {
  // Defer to media's default support.
  return ::media::IsDefaultSupportedVideoType(type);
}

bool ContentRendererClient::IsSupportedBitstreamAudioCodec(
    media::AudioCodec codec) {
  return false;
}

bool ContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) {
  return false;
}

std::unique_ptr<blink::WebContentSettingsClient>
ContentRendererClient::CreateWorkerContentSettingsClient(
    RenderFrame* render_frame) {
  return nullptr;
}

#if !defined(OS_ANDROID)
std::unique_ptr<media::SpeechRecognitionClient>
ContentRendererClient::CreateSpeechRecognitionClient(
    RenderFrame* render_frame,
    media::SpeechRecognitionClient::OnReadyCallback callback) {
  return nullptr;
}
#endif

bool ContentRendererClient::IsPluginAllowedToUseCameraDeviceAPI(
    const GURL& url) {
  return false;
}

bool ContentRendererClient::AllowScriptExtensionForServiceWorker(
    const url::Origin& script_origin) {
  return false;
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

bool ContentRendererClient::IsIdleMediaSuspendEnabled() {
  return true;
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

void ContentRendererClient::DidSetUserAgent(const std::string& user_agent) {}

bool ContentRendererClient::RequiresWebComponentsV0(const GURL& url) {
  return false;
}

base::Optional<::media::AudioRendererAlgorithmParameters>
ContentRendererClient::GetAudioRendererAlgorithmParameters(
    media::AudioParameters audio_parameters) {
  return base::nullopt;
}

void ContentRendererClient::MaybeProxyURLLoaderFactory(
    RenderFrame* render_frame,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver) {
}

}  // namespace content
