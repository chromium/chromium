// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_mixin.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/types/zip.h"
#include "base/unguessable_token.h"
#include "media/audio/application_loopback_device_helper.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "media/base/vector_math.h"
#include "services/audio/loopback_coordinator.h"
#include "services/audio/loopback_signal_provider.h"

namespace audio {

BASE_FEATURE(kRestrictOwnAudioAddChromiumBack,
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
std::unique_ptr<LoopbackMixin>
LoopbackMixin::MaybeCreateRestrictOwnAudioLoopbackMixin(
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& group_id,
    std::string_view device_id,
    const media::AudioParameters& params,
    OnDataCallback on_data_callback) {
  if (!(media::IsRestrictOwnAudioSupported() &&
        base::FeatureList::IsEnabled(kRestrictOwnAudioAddChromiumBack))) {
    return nullptr;
  }

  bool include_primary_source = true;
  if (media::IsRestrictOwnAudioBrowserLoopbackDeviceId(device_id)) {
    // This device id means that all audio from the browser should be captured,
    // except the audio originating from the current tab. We achieve this by
    // configuring LoopbackMixin to not include the primary source, which leaves
    // only the audio from the other tabs.
    include_primary_source = false;
  } else if (device_id !=
             media::AudioDeviceDescription::kLoopbackWithoutChromeId) {
    return nullptr;
  }

  // Not using make_unique due to the protected constructor.
  return base::WrapUnique(new LoopbackMixin(
      std::make_unique<LoopbackSignalProvider>(
          params, LoopbackGroupObserver::CreateExcludingGroupObserver(
                      coordinator, group_id)),
      params, include_primary_source, std::move(on_data_callback)));
}

LoopbackMixin::~LoopbackMixin() = default;

void LoopbackMixin::Start() {
  signal_provider_->Start();
}

void LoopbackMixin::OnData(const media::AudioBus* source,
                           base::TimeTicks capture_time,
                           double volume,
                           const media::AudioGlitchInfo& audio_glitch_info) {
  signal_provider_->PullLoopbackData(mix_bus_.get(), capture_time,
                                     /*volume=*/1);

  if (include_primary_source_) {
    for (auto [source_ch, mixed_ch] :
         base::zip(source->AllChannels(), mix_bus_->AllChannels())) {
      media::vector_math::FMAC(source_ch, 1.0f, mixed_ch);  // mixed += source
    }
  }

  // Pass the mixed audio data to the callback.
  on_data_callback_.Run(mix_bus_.get(), capture_time, volume,
                        audio_glitch_info);
}

LoopbackMixin::LoopbackMixin(
    std::unique_ptr<LoopbackSignalProviderInterface> signal_provider,
    const media::AudioParameters& params,
    bool include_primary_source,
    OnDataCallback on_data_callback)
    : signal_provider_(std::move(signal_provider)),
      mix_bus_(media::AudioBus::Create(params)),
      include_primary_source_(include_primary_source),
      on_data_callback_(std::move(on_data_callback)) {}

}  // namespace audio
