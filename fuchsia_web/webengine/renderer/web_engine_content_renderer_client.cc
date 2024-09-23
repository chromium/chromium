// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_content_renderer_client.h"

#include <optional>
#include <tuple>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "build/chromecast_buildflags.h"
#include "components/media_control/renderer/media_playback_options.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "components/on_load_script_injector/renderer/on_load_script_injector.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "fuchsia_web/webengine/features.h"
#include "fuchsia_web/webengine/renderer/web_engine_media_renderer_factory.h"
#include "fuchsia_web/webengine/renderer/web_engine_url_loader_throttle_provider.h"
#include "fuchsia_web/webengine/switches.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "components/cdm/renderer/widevine_key_system_info.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
#include "components/cast_streaming/renderer/public/resource_provider.h"  // nogncheck
#include "components/cast_streaming/renderer/public/resource_provider_factory.h"  // nogncheck
#include "fuchsia_web/webengine/common/cast_streaming.h"  // nogncheck
#endif

namespace {

// Returns true if the specified video format can be decoded on hardware.
bool IsSupportedHardwareVideoCodec(const media::VideoType& type) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // TODO(crbug.com/42050020): Replace these hardcoded checks with a query to
  // the fuchsia.mediacodec FIDL service.
  if (type.codec == media::VideoCodec::kH264 && type.level <= 41)
    return true;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // Only SD profiles are supported for VP9. HDR profiles (2 and 3) are not
  // supported.
  if (type.codec == media::VideoCodec::kVP9 &&
      (type.profile == media::VP9PROFILE_PROFILE0 ||
       type.profile == media::VP9PROFILE_PROFILE1)) {
    return true;
  }

  return false;
}

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_CAST_RECEIVER)
class PlayreadyKeySystemInfo : public ::media::KeySystemInfo {
 public:
  PlayreadyKeySystemInfo(const std::string& key_system_name,
                         media::SupportedCodecs supported_codecs)
      : key_system_name_(key_system_name),
        supported_codecs_(supported_codecs) {}

  std::string GetBaseKeySystemName() const override { return key_system_name_; }

  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override {
    return init_data_type == media::EmeInitDataType::CENC;
  }

  media::SupportedCodecs GetSupportedCodecs() const override {
    return supported_codecs_;
  }

  media::SupportedCodecs GetSupportedHwSecureCodecs() const override {
    return supported_codecs_;
  }

  media::EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& /*key_system*/,
      media::EmeMediaType /*media_type*/,
      const std::string& requested_robustness,
      const bool* /*hw_secure_requirement*/) const override {
    // Only empty robustness string is currently supported.
    // TODO(crbug.com/40180587): Add support for robustness strings.
    if (requested_robustness.empty()) {
      return media::EmeConfig{.hw_secure_codecs =
                                  media::EmeConfigRuleState::kRequired};
    }

    return media::EmeConfig::UnsupportedRule();
  }

  media::EmeConfig::Rule GetPersistentLicenseSessionSupport() const override {
    return media::EmeConfig::UnsupportedRule();
  }

  media::EmeFeatureSupport GetPersistentStateSupport() const override {
    return media::EmeFeatureSupport::ALWAYS_ENABLED;
  }

  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return media::EmeFeatureSupport::ALWAYS_ENABLED;
  }

  media::EmeConfig::Rule GetEncryptionSchemeConfigRule(
      media::EncryptionScheme encryption_mode) const override {
    if (encryption_mode == ::media::EncryptionScheme::kCenc) {
      return media::EmeConfig::SupportedRule();
    }

    return media::EmeConfig::UnsupportedRule();
  }

 private:
  const std::string key_system_name_;
  const media::SupportedCodecs supported_codecs_;
};
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_CAST_RECEIVER)

}  // namespace

WebEngineContentRendererClient::WebEngineContentRendererClient() = default;

WebEngineContentRendererClient::~WebEngineContentRendererClient() {
  base::AutoLock lock(observer_map_lock_);
  frame_token_to_observer_map_.clear();
}

scoped_refptr<url_rewrite::UrlRequestRewriteRules>
WebEngineContentRendererClient::GetRewriteRulesForFrameToken(
    const blink::LocalFrameToken& frame_token) const {
  base::AutoLock lock(observer_map_lock_);
  auto iter = frame_token_to_observer_map_.find(frame_token);
  if (iter == frame_token_to_observer_map_.end()) {
    return nullptr;
  }
  return iter->second->url_request_rules_receiver()->GetCachedRules();
}

void WebEngineContentRendererClient::OnRenderFrameDeleted(
    const blink::LocalFrameToken& frame_token) {
  base::AutoLock lock(observer_map_lock_);
  size_t count = frame_token_to_observer_map_.erase(frame_token);
  DCHECK_EQ(count, 1u);
}

void WebEngineContentRendererClient::RenderThreadStarted() {
  if (base::FeatureList::IsEnabled(features::kHandleMemoryPressureInRenderer) &&
      // Behavior of browser tests should not depend on things outside of their
      // control (like the amount of memory on the system running the tests).
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBrowserTest)) {
    memory_pressure_monitor_ =
        std::make_unique<memory_pressure::MultiSourceMemoryPressureMonitor>();
    memory_pressure_monitor_->MaybeStartPlatformVoter();
  }
}

void WebEngineContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Add WebEngine services to the new RenderFrame.
  // The objects' lifetimes are bound to the RenderFrame's lifetime.
  new on_load_script_injector::OnLoadScriptInjector(render_frame);

  auto frame_token = render_frame->GetWebFrame()->GetLocalFrameToken();

  auto render_frame_observer = std::make_unique<WebEngineRenderFrameObserver>(
      render_frame,
      base::BindOnce(&WebEngineContentRendererClient::OnRenderFrameDeleted,
                     base::Unretained(this)));
  {
    base::AutoLock lock(observer_map_lock_);
    auto render_frame_observer_iter = frame_token_to_observer_map_.emplace(
        frame_token, std::move(render_frame_observer));
    DCHECK(render_frame_observer_iter.second);
  }

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new media_control::MediaPlaybackOptions(render_frame);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
WebEngineContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type) {
  return std::make_unique<WebEngineURLLoaderThrottleProvider>(this);
}

std::unique_ptr<media::KeySystemSupportRegistration>
WebEngineContentRendererClient::GetSupportedKeySystems(
    content::RenderFrame* render_frame,
    media::GetSupportedKeySystemsCB cb) {
  media::KeySystemInfos key_systems;
  media::SupportedCodecs supported_video_codecs = 0;
  constexpr uint8_t kUnknownCodecLevel = 0;
  if (IsSupportedHardwareVideoCodec(media::VideoType{
          media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE0,
          kUnknownCodecLevel, media::VideoColorSpace::REC709()})) {
    supported_video_codecs |= media::EME_CODEC_VP9_PROFILE0;
  }

  if (IsSupportedHardwareVideoCodec(media::VideoType{
          media::VideoCodec::kVP9, media::VP9PROFILE_PROFILE2,
          kUnknownCodecLevel, media::VideoColorSpace::REC709()})) {
    supported_video_codecs |= media::EME_CODEC_VP9_PROFILE2;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsSupportedHardwareVideoCodec(media::VideoType{
          media::VideoCodec::kH264, media::H264PROFILE_MAIN, kUnknownCodecLevel,
          media::VideoColorSpace::REC709()})) {
    supported_video_codecs |= media::EME_CODEC_AVC1;
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  media::SupportedCodecs supported_audio_codecs = media::EME_CODEC_AUDIO_ALL;

  const media::SupportedCodecs supported_codecs =
      supported_video_codecs | supported_audio_codecs;

#if BUILDFLAG(ENABLE_WIDEVINE)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWidevine)) {
    const base::flat_set<media::EncryptionScheme> kSupportedEncryptionSchemes{
        media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs};

    const base::flat_set<media::CdmSessionType> kSupportedSessionTypes = {
        media::CdmSessionType::kTemporary};

    // Fuchsia always decrypts audio into clear buffers and return them back to
    // Chromium. Hardware secured decoders are only available for supported
    // video codecs.
    // TODO(crbug.com/42050020): Replace these hardcoded values with a query to
    // the fuchsia.mediacodec FIDL service.
    key_systems.push_back(std::make_unique<cdm::WidevineKeySystemInfo>(
        supported_codecs,             // codecs
        kSupportedEncryptionSchemes,  // encryption schemes
        kSupportedSessionTypes,       // session types
        supported_codecs,             // hw secure codecs
        kSupportedEncryptionSchemes,  // hw secure encryption schemes
        kSupportedSessionTypes,       // hw secure session types
        cdm::WidevineKeySystemInfo::Robustness::HW_SECURE_CRYPTO,  // max audio
                                                                   // robustness
        cdm::WidevineKeySystemInfo::Robustness::HW_SECURE_ALL,     // max video
                                                                   // robustness
        media::EmeFeatureSupport::ALWAYS_ENABLED,    // persistent state
        media::EmeFeatureSupport::ALWAYS_ENABLED));  // distinctive identifier
  }

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  std::string playready_key_system =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPlayreadyKeySystem);
  if (!playready_key_system.empty()) {
    key_systems.push_back(
        std::make_unique<PlayreadyKeySystemInfo>(playready_key_system, supported_codecs));
  }
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER)
#else
  std::ignore = supported_codecs;
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

  std::move(cb).Run(std::move(key_systems));
  return nullptr;
}

bool WebEngineContentRendererClient::IsSupportedVideoType(
    const media::VideoType& type) {
  // Fall back to default codec querying logic if software-only codecs are
  // enabled.
  if (base::FeatureList::IsEnabled(features::kEnableSoftwareOnlyVideoCodecs)) {
    return ContentRendererClient::IsSupportedVideoType(type);
  }

  return IsSupportedHardwareVideoCodec(type);
}

// TODO(crbug.com/40682958): Look into the ChromiumContentRendererClient version
// of this method and how it may apply here.
bool WebEngineContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool has_played_media_before,
    base::OnceClosure closure) {
  return RunClosureWhenInForeground(render_frame, std::move(closure));
}

std::unique_ptr<media::RendererFactory>
WebEngineContentRendererClient::GetBaseRendererFactory(
    content::RenderFrame* render_frame,
    media::MediaLog* media_log,
    media::DecoderFactory* decoder_factory,
    base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>
        get_gpu_factories_cb,
    int element_id) {
  mojo::Remote<mojom::WebEngineMediaResourceProvider> media_resource_provider;
  render_frame->GetBrowserInterfaceBroker().GetInterface(
      media_resource_provider.BindNewPipeAndPassReceiver());

  bool use_audio_consumer = false;
  if (!media_resource_provider->ShouldUseAudioConsumer(&use_audio_consumer) ||
      !use_audio_consumer) {
    return nullptr;
  }

  return std::make_unique<WebEngineMediaRendererFactory>(
      media_log, decoder_factory, std::move(get_gpu_factories_cb),
      std::move(media_resource_provider));
}

bool WebEngineContentRendererClient::RunClosureWhenInForeground(
    content::RenderFrame* render_frame,
    base::OnceClosure closure) {
  auto* playback_options =
      media_control::MediaPlaybackOptions::Get(render_frame);
  DCHECK(playback_options);
  return playback_options->RunWhenInForeground(std::move(closure));
}

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
std::unique_ptr<cast_streaming::ResourceProvider>
WebEngineContentRendererClient::CreateCastStreamingResourceProvider() {
  if (!IsCastStreamingEnabled()) {
    return nullptr;
  }

  return cast_streaming::CreateResourceProvider();
}
#endif
