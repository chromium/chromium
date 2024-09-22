// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

#include <string_view>

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/public/common/content_switches.h"
#include "media/base/demuxer.h"
#include "media/base/renderer_factory.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "ui/gfx/icc_profile.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-initialization.h"

namespace content {

void ContentRendererClient::SetUpWebAssemblyTrapHandler() {
  constexpr bool use_v8_trap_handler =
#if BUILDFLAG(IS_WIN)
      // On Windows we use the default trap handler provided by V8.
      true
#elif BUILDFLAG(IS_MAC)
      // On macOS, Crashpad uses exception ports to handle signals in a
      // different process. As we cannot just pass a callback to this other
      // process, we ask V8 to install its own signal handler to deal with
      // WebAssembly traps.
      true
#else
      // The trap handler is set as the first chance handler for Crashpad's
      // signal handler.
      false
#endif
      ;
  v8::V8::EnableWebAssemblyTrapHandler(use_v8_trap_handler);
}

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

bool ContentRendererClient::IsDomStorageDisabled() const {
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

void ContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    int http_status,
    mojom::AlternativeErrorPageOverrideInfoPtr alternative_error_page_info,
    std::string* error_html) {
  PrepareErrorPage(render_frame, error, http_method,
                   std::move(alternative_error_page_info), error_html);
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
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
ContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return nullptr;
}

bool ContentRendererClient::ShouldUseCodeCacheWithHashing(
    const blink::WebURL& request_url) const {
  return true;
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

bool ContentRendererClient::ShouldNotifyServiceWorkerOnWebSocketActivity(
    v8::Local<v8::Context> context) {
  return false;
}

blink::ProtocolHandlerSecurityLevel
ContentRendererClient::GetProtocolHandlerSecurityLevel(
    const url::Origin& origin) {
  return blink::ProtocolHandlerSecurityLevel::kStrict;
}

#if BUILDFLAG(IS_ANDROID)
bool ContentRendererClient::HandleNavigation(
    RenderFrame* render_frame,
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
    const blink::WebURL& upstream_url,
    const blink::WebURL& target_url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {}

bool ContentRendererClient::IsPrefetchOnly(RenderFrame* render_frame) {
  return false;
}

uint64_t ContentRendererClient::VisitedLinkHash(
    std::string_view canonical_url) {
  return 0;
}

uint64_t ContentRendererClient::PartitionedVisitedLinkFingerprint(
    std::string_view canonical_link_url,
    const net::SchemefulSite& top_level_site,
    const url::Origin& frame_origin) {
  // Return the null-fingerprint value.
  return 0;
}

bool ContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return false;
}

void ContentRendererClient::AddOrUpdateVisitedLinkSalt(
    const url::Origin& origin,
    uint64_t salt) {}

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
  // Hosting plugins in-process is inherently incompatible with attempting to
  // process-isolate plugins from different origins.
  auto* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kPpapiInProcess))
    return false;

  return true;
}

std::unique_ptr<media::KeySystemSupportRegistration>
ContentRendererClient::GetSupportedKeySystems(
    RenderFrame* render_frame,
    media::GetSupportedKeySystemsCB cb) {
  std::move(cb).Run({});
  return nullptr;
}

bool ContentRendererClient::IsSupportedAudioType(const media::AudioType& type) {
  // Defer to media's default support.
  return ::media::IsDefaultSupportedAudioType(type);
}

bool ContentRendererClient::IsSupportedVideoType(const media::VideoType& type) {
  // Defer to media's default support.
  return ::media::IsDefaultSupportedVideoType(type);
}

media::ExternalMemoryAllocator* ContentRendererClient::GetMediaAllocator() {
  return nullptr;
}

bool ContentRendererClient::IsSupportedBitstreamAudioCodec(
    media::AudioCodec codec) {
  switch (codec) {
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    case media::AudioCodec::kAC3:
    case media::AudioCodec::kEAC3:
      return true;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    case media::AudioCodec::kDTS:
    case media::AudioCodec::kDTSXP2:
      return true;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    default:
      return false;
  }
}

bool ContentRendererClient::ShouldReportDetailedMessageForSource(
    const std::u16string& source) {
  return false;
}

std::unique_ptr<blink::WebContentSettingsClient>
ContentRendererClient::CreateWorkerContentSettingsClient(
    RenderFrame* render_frame) {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<media::SpeechRecognitionClient>
ContentRendererClient::CreateSpeechRecognitionClient(
    RenderFrame* render_frame) {
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

bool ContentRendererClient::ShouldEnforceWebRTCRoutingPreferences() {
  return true;
}

GURL ContentRendererClient::OverrideFlashEmbedWithHTML(const GURL& url) {
  return GURL();
}

bool ContentRendererClient::IsIdleMediaSuspendEnabled() {
  return true;
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
ContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return nullptr;
}

blink::WebFrame* ContentRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
  return nullptr;
}

bool ContentRendererClient::IsSafeRedirectTarget(const GURL& from_url,
                                                 const GURL& to_url) {
  return true;
}

void ContentRendererClient::DidSetUserAgent(const std::string& user_agent) {}

std::optional<::media::AudioRendererAlgorithmParameters>
ContentRendererClient::GetAudioRendererAlgorithmParameters(
    media::AudioParameters audio_parameters) {
  return std::nullopt;
}

void ContentRendererClient::AppendContentSecurityPolicy(
    const blink::WebURL& url,
    blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp) {}

std::unique_ptr<media::RendererFactory>
ContentRendererClient::GetBaseRendererFactory(
    content::RenderFrame* render_frame,
    media::MediaLog* media_log,
    media::DecoderFactory* decoder_factory,
    base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>
        get_gpu_factories_cb,
    int element_id) {
  return nullptr;
}

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
std::unique_ptr<cast_streaming::ResourceProvider>
ContentRendererClient::CreateCastStreamingResourceProvider() {
  return nullptr;
}
#endif

std::unique_ptr<blink::WebLinkPreviewTriggerer>
ContentRendererClient::CreateLinkPreviewTriggerer() {
  return nullptr;
}

}  // namespace content
