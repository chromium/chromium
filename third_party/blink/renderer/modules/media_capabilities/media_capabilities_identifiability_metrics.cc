// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_identifiability_metrics.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_key_system_track_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_decoding_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_key_system_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_decoding_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_access.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_media_capability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {
namespace media_capabilities_identifiability_metrics {
namespace {

bool IsDecodingInfoTypeAllowed() {
  return IdentifiabilityStudySettings::Get()->ShouldSampleType(
      IdentifiableSurface::Type::kMediaCapabilities_DecodingInfo);
}

bool ShouldSampleDecodingInfoType() {
  return IdentifiabilityStudySettings::Get()->ShouldSampleType(
      IdentifiableSurface::Type::kMediaCapabilities_DecodingInfo);
}

void RecordDecodingIdentifiabilityMetric(ExecutionContext* context,
                                         IdentifiableToken input_token,
                                         IdentifiableToken output_token) {
  DCHECK(IsDecodingInfoTypeAllowed());
  IdentifiabilityMetricBuilder(context->UkmSourceID())
      .Add(IdentifiableSurface::FromTypeAndToken(
               IdentifiableSurface::Type::kMediaCapabilities_DecodingInfo,
               input_token),
           output_token)
      .Record(context->UkmRecorder());
}

// The various ComputeToken methods create digests of each of the objects,
// returning the special empty value when an input is nullptr.
IdentifiableToken ComputeToken(const VideoConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder
      .AddToken(IdentifiabilityBenignStringToken(configuration->contentType()))
      .AddValue(configuration->width())
      .AddValue(configuration->height())
      .AddValue(configuration->bitrate())
      .AddValue(configuration->framerate());

  // While the above are always present, we need to check the other properties'
  // presence explicitly.
  builder.AddValue(configuration->hasHdrMetadataType())
      .AddValue(configuration->hasColorGamut())
      .AddValue(configuration->hasTransferFunction())
      .AddValue(configuration->hasScalabilityMode());
  if (configuration->hasHdrMetadataType()) {
    builder.AddToken(IdentifiabilityBenignStringToken(
        configuration->hdrMetadataType().AsString()));
  }
  if (configuration->hasColorGamut()) {
    builder.AddToken(IdentifiabilityBenignStringToken(
        configuration->colorGamut().AsString()));
  }
  if (configuration->hasTransferFunction()) {
    builder.AddToken(IdentifiabilityBenignStringToken(
        configuration->transferFunction().AsString()));
  }
  if (configuration->hasScalabilityMode()) {
    builder.AddToken(
        IdentifiabilityBenignStringToken(configuration->scalabilityMode()));
  }
  return builder.GetToken();
}

IdentifiableToken ComputeToken(const AudioConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddToken(
      IdentifiabilityBenignStringToken(configuration->contentType()));

  // While the strings above will be null if not present, we need to check
  // the presence of numerical types explicitly.
  builder.AddValue(configuration->hasChannels())
      .AddValue(configuration->hasBitrate())
      .AddValue(configuration->hasSamplerate());
  if (configuration->hasChannels()) {
    builder.AddToken(
        IdentifiabilityBenignStringToken(configuration->channels()));
  }
  if (configuration->hasBitrate())
    builder.AddValue(configuration->bitrate());
  if (configuration->hasSamplerate())
    builder.AddValue(configuration->samplerate());
  return builder.GetToken();
}

IdentifiableToken ComputeToken(
    const KeySystemTrackConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddToken(
      IdentifiabilityBenignStringToken(configuration->robustness()));
  return builder.GetToken();
}

IdentifiableToken ComputeToken(
    const MediaKeySystemMediaCapability* capability) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!capability)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddToken(IdentifiabilityBenignStringToken(capability->contentType()))
      .AddToken(IdentifiabilityBenignStringToken(capability->robustness()))
      .AddToken(
          IdentifiabilityBenignStringToken(capability->encryptionScheme()));
  return builder.GetToken();
}

IdentifiableToken ComputeToken(
    const MediaKeySystemConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddToken(IdentifiabilityBenignStringToken(configuration->label()))
      .AddValue(configuration->hasInitDataTypes())
      .AddValue(configuration->hasAudioCapabilities())
      .AddValue(configuration->hasVideoCapabilities())
      .AddToken(IdentifiabilityBenignStringToken(
          configuration->distinctiveIdentifier().AsString()))
      .AddToken(IdentifiabilityBenignStringToken(
          configuration->persistentState().AsString()))
      .AddValue(configuration->hasSessionTypes());
  if (configuration->hasInitDataTypes()) {
    builder.AddToken(
        IdentifiabilityBenignStringVectorToken(configuration->initDataTypes()));
  }
  if (configuration->hasAudioCapabilities()) {
    const HeapVector<Member<MediaKeySystemMediaCapability>>&
        audio_capabilities = configuration->audioCapabilities();
    builder.AddValue(audio_capabilities.size());
    for (const auto& elem : audio_capabilities)
      builder.AddToken(ComputeToken(elem.Get()));
  }
  if (configuration->hasVideoCapabilities()) {
    const HeapVector<Member<MediaKeySystemMediaCapability>>&
        video_capabilities = configuration->videoCapabilities();
    builder.AddValue(video_capabilities.size());
    for (const auto& elem : video_capabilities)
      builder.AddToken(ComputeToken(elem.Get()));
  }
  if (configuration->hasSessionTypes()) {
    builder.AddToken(
        IdentifiabilityBenignStringVectorToken(configuration->sessionTypes()));
  }
  return builder.GetToken();
}

IdentifiableToken ComputeToken(const MediaKeySystemAccess* access) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!access)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddToken(IdentifiabilityBenignStringToken(access->keySystem()))
      .AddToken(ComputeToken(access->getConfiguration()));
  return builder.GetToken();
}

IdentifiableToken ComputeToken(
    const MediaCapabilitiesKeySystemConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddToken(IdentifiabilityBenignStringToken(configuration->keySystem()))
      .AddToken(IdentifiabilityBenignStringToken(configuration->initDataType()))
      .AddToken(IdentifiabilityBenignStringToken(
          configuration->distinctiveIdentifier().AsString()))
      .AddToken(IdentifiabilityBenignStringToken(
          configuration->persistentState().AsString()))
      .AddValue(configuration->hasSessionTypes())
      .AddValue(configuration->hasAudio())
      .AddValue(configuration->hasVideo());
  if (configuration->hasSessionTypes()) {
    builder.AddToken(
        IdentifiabilityBenignStringVectorToken(configuration->sessionTypes()));
  }
  if (configuration->hasAudio())
    builder.AddToken(ComputeToken(configuration->audio()));
  if (configuration->hasVideo())
    builder.AddToken(ComputeToken(configuration->video()));
  return builder.GetToken();
}

IdentifiableToken ComputeToken(
    const MediaDecodingConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder
      .AddToken(
          IdentifiabilityBenignStringToken(configuration->type().AsString()))
      .AddValue(configuration->hasKeySystemConfiguration())
      .AddValue(configuration->hasAudio())
      .AddValue(configuration->hasVideo());
  if (configuration->hasKeySystemConfiguration())
    builder.AddToken(ComputeToken(configuration->keySystemConfiguration()));
  if (configuration->hasAudio())
    builder.AddToken(ComputeToken(configuration->audio()));
  if (configuration->hasVideo())
    builder.AddToken(ComputeToken(configuration->video()));
  return builder.GetToken();
}

IdentifiableToken ComputeToken(const MediaCapabilitiesDecodingInfo* info) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!info)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddValue(info->supported())
      .AddValue(info->smooth())
      .AddValue(info->powerEfficient())
      .AddToken(ComputeToken(info->keySystemAccess()));
  return builder.GetToken();
}

}  // namespace

void ReportDecodingInfoResult(ExecutionContext* context,
                              const MediaDecodingConfiguration* input,
                              const MediaCapabilitiesDecodingInfo* output) {
  if (!IsDecodingInfoTypeAllowed() || !ShouldSampleDecodingInfoType())
    return;

  RecordDecodingIdentifiabilityMetric(context, ComputeToken(input),
                                      ComputeToken(output));
}

void ReportDecodingInfoResult(ExecutionContext* context,
                              std::optional<IdentifiableToken> input_token,
                              const MediaCapabilitiesDecodingInfo* output) {
  DCHECK_EQ(IsDecodingInfoTypeAllowed(), input_token.has_value());
  if (!input_token.has_value() || !ShouldSampleDecodingInfoType())
    return;

  RecordDecodingIdentifiabilityMetric(context, input_token.value(),
                                      IdentifiableToken());
}

std::optional<IdentifiableToken> ComputeDecodingInfoInputToken(
    const MediaDecodingConfiguration* input) {
  if (!IsDecodingInfoTypeAllowed() || !ShouldSampleDecodingInfoType())
    return std::nullopt;

  return ComputeToken(input);
}

}  // namespace media_capabilities_identifiability_metrics
}  // namespace blink
