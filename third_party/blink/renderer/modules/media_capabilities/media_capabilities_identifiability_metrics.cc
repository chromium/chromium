// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {
namespace media_capabilities_identifiability_metrics {
namespace {

bool IsDecodingInfoTypeAllowed() {
  return IdentifiabilityStudySettings::Get()->IsTypeAllowed(
      IdentifiableSurface::Type::kMediaCapabilities_DecodingInfo);
}

bool ShouldSampleDecodingInfoType() {
  return IdentifiabilityStudySettings::Get()->ShouldSample(
      IdentifiableSurface::Type::kMediaCapabilities_DecodingInfo);
}

void RecordDecodingIdentifiabilityMetric(ExecutionContext* context,
                                         IdentifiableToken input_token,
                                         IdentifiableToken output_token) {
  DCHECK(IsDecodingInfoTypeAllowed());
  IdentifiabilityMetricBuilder(context->UkmSourceID())
      .Set(IdentifiableSurface::FromTypeAndToken(
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
      .AddToken(
          IdentifiabilityBenignStringToken(configuration->hdrMetadataType()))
      .AddToken(IdentifiabilityBenignStringToken(configuration->colorGamut()))
      .AddToken(
          IdentifiabilityBenignStringToken(configuration->transferFunction()));

  // While the strings above will be null if not present, we need to check
  // the presence of numerical types explicitly.
  builder.AddValue(configuration->hasWidth())
      .AddValue(configuration->hasHeight())
      .AddValue(configuration->hasBitrate())
      .AddValue(configuration->hasFramerate());
  if (configuration->hasWidth())
    builder.AddValue(configuration->width());
  if (configuration->hasHeight())
    builder.AddValue(configuration->height());
  if (configuration->hasBitrate())
    builder.AddValue(configuration->bitrate());
  if (configuration->hasFramerate())
    builder.AddValue(configuration->framerate());
  return builder.GetToken();
}

IdentifiableToken ComputeToken(const AudioConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder
      .AddToken(IdentifiabilityBenignStringToken(configuration->contentType()))
      .AddToken(IdentifiabilityBenignStringToken(configuration->channels()));

  // While the strings above will be null if not present, we need to check
  // the presence of numerical types explicitly.
  builder.AddValue(configuration->hasBitrate())
      .AddValue(configuration->hasSamplerate());
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
          configuration->distinctiveIdentifier()))
      .AddToken(
          IdentifiabilityBenignStringToken(configuration->persistentState()))
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
          configuration->distinctiveIdentifier()))
      .AddValue(configuration->hasSessionTypes())
      .AddToken(ComputeToken(configuration->audio()))
      .AddToken(ComputeToken(configuration->video()));
  if (configuration->hasSessionTypes()) {
    builder.AddToken(
        IdentifiabilityBenignStringVectorToken(configuration->sessionTypes()));
  }
  return builder.GetToken();
}

IdentifiableToken ComputeToken(
    const MediaDecodingConfiguration* configuration) {
  DCHECK(IsDecodingInfoTypeAllowed());
  if (!configuration)
    return IdentifiableToken();

  IdentifiableTokenBuilder builder;
  builder.AddToken(IdentifiabilityBenignStringToken(configuration->type()))
      .AddToken(ComputeToken(configuration->keySystemConfiguration()))
      .AddToken(ComputeToken(configuration->video()))
      .AddToken(ComputeToken(configuration->audio()));
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
                              base::Optional<IdentifiableToken> input_token,
                              const MediaCapabilitiesDecodingInfo* output) {
  DCHECK_EQ(IsDecodingInfoTypeAllowed(), input_token.has_value());
  if (!input_token.has_value() || !ShouldSampleDecodingInfoType())
    return;

  RecordDecodingIdentifiabilityMetric(context, input_token.value(),
                                      IdentifiableToken());
}

base::Optional<IdentifiableToken> ComputeDecodingInfoInputToken(
    const MediaDecodingConfiguration* input) {
  if (!IsDecodingInfoTypeAllowed() || !ShouldSampleDecodingInfoType())
    return base::nullopt;

  return ComputeToken(input);
}

}  // namespace media_capabilities_identifiability_metrics
}  // namespace blink
