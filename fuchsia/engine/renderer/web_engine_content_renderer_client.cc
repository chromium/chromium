// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/cast_streaming/renderer/public/resource_provider.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "components/media_control/renderer/media_playback_options.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "components/on_load_script_injector/renderer/on_load_script_injector.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "fuchsia/engine/common/cast_streaming.h"
#include "fuchsia/engine/features.h"
#include "fuchsia/engine/renderer/web_engine_media_renderer_factory.h"
#include "fuchsia/engine/renderer/web_engine_url_loader_throttle_provider.h"
#include "fuchsia/engine/switches.h"
#include "media/base/demuxer.h"
#include "media/base/eme_constants.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace {

// Returns true if the specified video format can be decoded on hardware.
bool IsSupportedHardwareVideoCodec(const media::VideoType& type) {
  // TODO(crbug.com/1013412): Replace these hardcoded checks with a query to the
  // fuchsia.mediacodec FIDL service.
  if (type.codec == media::VideoCodec::kH264 && type.level <= 41)
    return true;

  if (type.codec == media::VideoCodec::kVP9 && type.level <= 40)
    return true;

  return false;
}

class PlayreadyKeySystemProperties : public ::media::KeySystemProperties {
 public:
  PlayreadyKeySystemProperties(const std::string& key_system_name,
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

  media::EmeConfigRule GetRobustnessConfigRule(
      const std::string& /*key_system*/,
      media::EmeMediaType /*media_type*/,
      const std::string& requested_robustness,
      const bool* /*hw_secure_requirement*/) const override {
    // Only empty robustness string is currently supported.
    if (requested_robustness.empty()) {
      return media::EmeConfigRule::HW_SECURE_CODECS_REQUIRED;
    }

    return media::EmeConfigRule::NOT_SUPPORTED;
  }

  media::EmeSessionTypeSupport GetPersistentLicenseSessionSupport()
      const override {
    return media::EmeSessionTypeSupport::NOT_SUPPORTED;
  }

  media::EmeFeatureSupport GetPersistentStateSupport() const override {
    return media::EmeFeatureSupport::ALWAYS_ENABLED;
  }

  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return media::EmeFeatureSupport::ALWAYS_ENABLED;
  }

  media::EmeConfigRule GetEncryptionSchemeConfigRule(
      media::EncryptionScheme encryption_mode) const override {
    if (encryption_mode == ::media::EncryptionScheme::kCenc) {
      return media::EmeConfigRule::SUPPORTED;
    }

    return media::EmeConfigRule::NOT_SUPPORTED;
  }

 private:
  const std::string key_system_name_;
  const media::SupportedCodecs supported_codecs_;
};

}  // namespace

WebEngineContentRendererClient::WebEngineContentRendererClient()
    : cast_streaming_resource_provider_(
          cast_streaming::ResourceProvider::Create()) {}

WebEngineContentRendererClient::~WebEngineContentRendererClient() = default;

WebEngineRenderFrameObserver*
WebEngineContentRendererClient::GetWebEngineRenderFrameObserverForRenderFrameId(
    int render_frame_id) const {
  auto iter = render_frame_id_to_observer_map_.find(render_frame_id);
  DCHECK(iter != render_frame_id_to_observer_map_.end());
  return iter->second.get();
}

void WebEngineContentRendererClient::OnRenderFrameDeleted(int render_frame_id) {
  size_t count = render_frame_id_to_observer_map_.erase(render_frame_id);
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
    memory_pressure_monitor_->Start();
  }
}

void WebEngineContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // Add WebEngine services to the new RenderFrame.
  // The objects' lifetimes are bound to the RenderFrame's lifetime.
  new on_load_script_injector::OnLoadScriptInjector(render_frame);

  int render_frame_id = render_frame->GetRoutingID();

  auto render_frame_observer = std::make_unique<WebEngineRenderFrameObserver>(
      render_frame,
      base::BindOnce(&WebEngineContentRendererClient::OnRenderFrameDeleted,
                     base::Unretained(this)));
  auto render_frame_observer_iter = render_frame_id_to_observer_map_.emplace(
      render_frame_id, std::move(render_frame_observer));
  DCHECK(render_frame_observer_iter.second);

  // Call into the cast_streaming-specific frame creation logic.
  cast_streaming_resource_provider_->RenderFrameCreated(render_frame);

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new media_control::MediaPlaybackOptions(render_frame);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
WebEngineContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type) {
  // TODO(crbug.com/976975): Add support for service workers.
  if (type == blink::URLLoaderThrottleProviderType::kWorker)
    return nullptr;

  return std::make_unique<WebEngineURLLoaderThrottleProvider>(this);
}

void WebEngineContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  media::KeySystemPropertiesVector key_systems;
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

  if (IsSupportedHardwareVideoCodec(media::VideoType{
          media::VideoCodec::kH264, media::H264PROFILE_MAIN, kUnknownCodecLevel,
          media::VideoColorSpace::REC709()})) {
    supported_video_codecs |= media::EME_CODEC_AVC1;
  }

  media::SupportedCodecs supported_audio_codecs = media::EME_CODEC_AUDIO_ALL;

  media::SupportedCodecs supported_codecs =
      supported_video_codecs | supported_audio_codecs;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWidevine)) {
    base::flat_set<media::EncryptionScheme> encryption_schemes{
        media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs};

    // Fuchsia always decrypts audio into clear buffers and return them back to
    // Chromium. Hardware secured decoders are only available for supported
    // video codecs.
    // TODO(crbug.com/1013412): Replace these hardcoded values with a query to
    // the fuchsia.mediacodec FIDL service.
    key_systems.emplace_back(new cdm::WidevineKeySystemProperties(
        supported_codecs,    // codecs
        encryption_schemes,  // encryption schemes
        supported_codecs,    // hw secure codecs
        encryption_schemes,  // hw secure encryption schemes
        cdm::WidevineKeySystemProperties::Robustness::
            HW_SECURE_CRYPTO,  // max audio robustness
        cdm::WidevineKeySystemProperties::Robustness::
            HW_SECURE_ALL,                            // max video robustness
        media::EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent license
        media::EmeFeatureSupport::ALWAYS_ENABLED,     // persistent state
        media::EmeFeatureSupport::ALWAYS_ENABLED));   // distinctive identifier
  }

  std::string playready_key_system =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPlayreadyKeySystem);
  if (!playready_key_system.empty()) {
    key_systems.emplace_back(new PlayreadyKeySystemProperties(
        playready_key_system, supported_codecs));
  }

  std::move(cb).Run(std::move(key_systems));
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

// TODO(crbug.com/1067435): Look into the ChromiumContentRendererClient version
// of this method and how it may apply here.
bool WebEngineContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool has_played_media_before,
    base::OnceClosure closure) {
  return RunClosureWhenInForeground(render_frame, std::move(closure));
}

std::unique_ptr<media::Demuxer>
WebEngineContentRendererClient::OverrideDemuxerForUrl(
    content::RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) {
  if (!IsCastStreamingEnabled()) {
    return nullptr;
  }

  return cast_streaming_resource_provider_->OverrideDemuxerForUrl(
      render_frame, url, std::move(media_task_runner));
}

std::unique_ptr<media::RendererFactory>
WebEngineContentRendererClient::GetBaseRendererFactory(
    content::RenderFrame* render_frame,
    media::MediaLog* media_log,
    media::DecoderFactory* decoder_factory,
    base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>
        get_gpu_factories_cb) {
  auto* interface_broker = render_frame->GetBrowserInterfaceBroker();

  mojo::Remote<mojom::WebEngineMediaResourceProvider> media_resource_provider;
  interface_broker->GetInterface(
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
