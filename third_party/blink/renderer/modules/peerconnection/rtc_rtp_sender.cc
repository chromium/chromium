// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoding_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_insertable_streams.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_resolution_restriction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtcp_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_codec_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_capability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_parameters.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_sender_sink_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_sender_source_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_sender_sink_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_sender_source_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/webrtc/api/video/resolution.h"

namespace blink {

namespace features {

// Killswitch for requesting key frames via setParameterOptions.
// TODO(crbug.com/1354101): remove after rollout.
BASE_FEATURE(kWebRtcRequestKeyFrameViaSetParameterOptions,
             "WebRtcRequestKeyFrameViaSetParameterOptions",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

namespace {

webrtc::RtpCodec ToWebrtcRtpCodec(const RTCRtpCodec* codec);

// This enum is used for logging and values must match the ones in
// tools/metrics/histograms/enums.xml
enum class WebRtcScalabilityMode {
  kInvalid = 0,
  kL1T1 = 1,
  kL1T2 = 2,
  kL1T3 = 3,
  kL2T1 = 4,
  kL2T1h = 5,
  kL2T1_KEY = 6,
  kL2T2 = 7,
  kL2T2h = 8,
  kL2T2_KEY = 9,
  kL2T2_KEY_SHIFT = 10,
  kL2T3 = 11,
  kL2T3h = 12,
  kL2T3_KEY = 13,
  kL3T1 = 14,
  kL3T1h = 15,
  kL3T1_KEY = 16,
  kL3T2 = 17,
  kL3T2h = 18,
  kL3T2_KEY = 19,
  kL3T3 = 20,
  kL3T3h = 21,
  kL3T3_KEY = 22,
  kS2T1 = 23,
  kS2T1h = 24,
  kS2T2 = 25,
  kS2T2h = 26,
  kS2T3 = 27,
  kS2T3h = 28,
  kS3T1 = 29,
  kS3T1h = 30,
  kS3T2 = 31,
  kS3T2h = 32,
  kS3T3 = 33,
  kS3T3h = 34,
  kMaxValue = kS3T3h,
};

WebRtcScalabilityMode ScalabilityModeStringToUMAMode(
    const std::string& scalability_mode_string) {
  if (scalability_mode_string == "L1T1") {
    return WebRtcScalabilityMode::kL1T1;
  }
  if (scalability_mode_string == "L1T2") {
    return WebRtcScalabilityMode::kL1T2;
  }
  if (scalability_mode_string == "L1T3") {
    return WebRtcScalabilityMode::kL1T3;
  }
  if (scalability_mode_string == "L2T1") {
    return WebRtcScalabilityMode::kL2T1;
  }
  if (scalability_mode_string == "L2T1h") {
    return WebRtcScalabilityMode::kL2T1h;
  }
  if (scalability_mode_string == "L2T1_KEY") {
    return WebRtcScalabilityMode::kL2T1_KEY;
  }
  if (scalability_mode_string == "L2T2") {
    return WebRtcScalabilityMode::kL2T2;
  }
  if (scalability_mode_string == "L2T2h") {
    return WebRtcScalabilityMode::kL2T2h;
  }
  if (scalability_mode_string == "L2T2_KEY") {
    return WebRtcScalabilityMode::kL2T2_KEY;
  }
  if (scalability_mode_string == "L2T2_KEY_SHIFT") {
    return WebRtcScalabilityMode::kL2T2_KEY_SHIFT;
  }
  if (scalability_mode_string == "L2T3") {
    return WebRtcScalabilityMode::kL2T3;
  }
  if (scalability_mode_string == "L2T3h") {
    return WebRtcScalabilityMode::kL2T3h;
  }
  if (scalability_mode_string == "L2T3_KEY") {
    return WebRtcScalabilityMode::kL2T3_KEY;
  }
  if (scalability_mode_string == "L3T1") {
    return WebRtcScalabilityMode::kL3T1;
  }
  if (scalability_mode_string == "L3T1h") {
    return WebRtcScalabilityMode::kL3T1h;
  }
  if (scalability_mode_string == "L3T1_KEY") {
    return WebRtcScalabilityMode::kL3T1_KEY;
  }
  if (scalability_mode_string == "L3T2") {
    return WebRtcScalabilityMode::kL3T2;
  }
  if (scalability_mode_string == "L3T2h") {
    return WebRtcScalabilityMode::kL3T2h;
  }
  if (scalability_mode_string == "L3T2_KEY") {
    return WebRtcScalabilityMode::kL3T2_KEY;
  }
  if (scalability_mode_string == "L3T3") {
    return WebRtcScalabilityMode::kL3T3;
  }
  if (scalability_mode_string == "L3T3h") {
    return WebRtcScalabilityMode::kL3T3h;
  }
  if (scalability_mode_string == "L3T3_KEY") {
    return WebRtcScalabilityMode::kL3T3_KEY;
  }
  if (scalability_mode_string == "S2T1") {
    return WebRtcScalabilityMode::kS2T1;
  }
  if (scalability_mode_string == "S2T1h") {
    return WebRtcScalabilityMode::kS2T1h;
  }
  if (scalability_mode_string == "S2T2") {
    return WebRtcScalabilityMode::kS2T2;
  }
  if (scalability_mode_string == "S2T2h") {
    return WebRtcScalabilityMode::kS2T2h;
  }
  if (scalability_mode_string == "S2T3") {
    return WebRtcScalabilityMode::kS2T3;
  }
  if (scalability_mode_string == "S2T3h") {
    return WebRtcScalabilityMode::kS2T3h;
  }
  if (scalability_mode_string == "S3T1") {
    return WebRtcScalabilityMode::kS3T1;
  }
  if (scalability_mode_string == "S3T1h") {
    return WebRtcScalabilityMode::kS3T1h;
  }
  if (scalability_mode_string == "S3T2") {
    return WebRtcScalabilityMode::kS3T2;
  }
  if (scalability_mode_string == "S3T2h") {
    return WebRtcScalabilityMode::kS3T2h;
  }
  if (scalability_mode_string == "S3T3") {
    return WebRtcScalabilityMode::kS3T3;
  }
  if (scalability_mode_string == "S3T3h") {
    return WebRtcScalabilityMode::kS3T3h;
  }

  return WebRtcScalabilityMode::kInvalid;
}

class ReplaceTrackRequest : public RTCVoidRequest {
 public:
  ReplaceTrackRequest(RTCRtpSender* sender,
                      MediaStreamTrack* with_track,
                      ScriptPromiseResolver<IDLUndefined>* resolver)
      : sender_(sender), with_track_(with_track), resolver_(resolver) {}
  ~ReplaceTrackRequest() override {}

  void RequestSucceeded() override {
    sender_->SetTrack(with_track_);
    resolver_->Resolve();
  }

  void RequestFailed(const webrtc::RTCError& error) override {
    RejectPromiseFromRTCError(error, resolver_);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(sender_);
    visitor->Trace(with_track_);
    visitor->Trace(resolver_);
    RTCVoidRequest::Trace(visitor);
  }

 private:
  Member<RTCRtpSender> sender_;
  Member<MediaStreamTrack> with_track_;
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
};

class SetParametersRequest : public RTCVoidRequest {
 public:
  SetParametersRequest(ScriptPromiseResolver<IDLUndefined>* resolver,
                       RTCRtpSender* sender)
      : resolver_(resolver), sender_(sender) {}

  void RequestSucceeded() override {
    sender_->ClearLastReturnedParameters();
    resolver_->Resolve();
  }

  void RequestFailed(const webrtc::RTCError& error) override {
    sender_->ClearLastReturnedParameters();
    RejectPromiseFromRTCError(error, resolver_);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(sender_);
    visitor->Trace(resolver_);
    RTCVoidRequest::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  Member<RTCRtpSender> sender_;
};

bool HasInvalidModification(const RTCRtpSendParameters* parameters,
                            const RTCRtpSendParameters* new_parameters) {
  if (parameters->hasTransactionId() != new_parameters->hasTransactionId() ||
      (parameters->hasTransactionId() &&
       parameters->transactionId() != new_parameters->transactionId())) {
    return true;
  }

  if (parameters->hasEncodings() != new_parameters->hasEncodings())
    return true;
  if (parameters->hasEncodings()) {
    if (parameters->encodings().size() != new_parameters->encodings().size())
      return true;

    for (wtf_size_t i = 0; i < parameters->encodings().size(); ++i) {
      const auto& encoding = parameters->encodings()[i];
      const auto& new_encoding = new_parameters->encodings()[i];
      if (encoding->hasRid() != new_encoding->hasRid() ||
          (encoding->hasRid() && encoding->rid() != new_encoding->rid())) {
        return true;
      }
    }
  }

  if (parameters->hasHeaderExtensions() !=
      new_parameters->hasHeaderExtensions())
    return true;

  if (parameters->hasHeaderExtensions()) {
    if (parameters->headerExtensions().size() !=
        new_parameters->headerExtensions().size())
      return true;

    for (wtf_size_t i = 0; i < parameters->headerExtensions().size(); ++i) {
      const auto& header_extension = parameters->headerExtensions()[i];
      const auto& new_header_extension = new_parameters->headerExtensions()[i];
      if (header_extension->hasUri() != new_header_extension->hasUri() ||
          (header_extension->hasUri() &&
           header_extension->uri() != new_header_extension->uri()) ||
          header_extension->hasId() != new_header_extension->hasId() ||
          (header_extension->hasId() &&
           header_extension->id() != new_header_extension->id()) ||
          header_extension->hasEncrypted() !=
              new_header_extension->hasEncrypted() ||
          (header_extension->hasEncrypted() &&
           header_extension->encrypted() !=
               new_header_extension->encrypted())) {
        return true;
      }
    }
  }

  if (parameters->hasRtcp() != new_parameters->hasRtcp() ||
      (parameters->hasRtcp() &&
       ((parameters->rtcp()->hasCname() != new_parameters->rtcp()->hasCname() ||
         (parameters->rtcp()->hasCname() &&
          parameters->rtcp()->cname() != new_parameters->rtcp()->cname())) ||
        (parameters->rtcp()->hasReducedSize() !=
             new_parameters->rtcp()->hasReducedSize() ||
         (parameters->rtcp()->hasReducedSize() &&
          parameters->rtcp()->reducedSize() !=
              new_parameters->rtcp()->reducedSize()))))) {
    return true;
  }

  if (parameters->hasCodecs() != new_parameters->hasCodecs())
    return true;

  if (parameters->hasCodecs()) {
    if (parameters->codecs().size() != new_parameters->codecs().size())
      return true;

    for (wtf_size_t i = 0; i < parameters->codecs().size(); ++i) {
      const auto& codec = parameters->codecs()[i];
      const auto& new_codec = new_parameters->codecs()[i];
      if (codec->hasPayloadType() != new_codec->hasPayloadType() ||
          (codec->hasPayloadType() &&
           codec->payloadType() != new_codec->payloadType()) ||
          codec->hasMimeType() != new_codec->hasMimeType() ||
          (codec->hasMimeType() &&
           codec->mimeType() != new_codec->mimeType()) ||
          codec->hasClockRate() != new_codec->hasClockRate() ||
          (codec->hasClockRate() &&
           codec->clockRate() != new_codec->clockRate()) ||
          codec->hasChannels() != new_codec->hasChannels() ||
          (codec->hasChannels() &&
           codec->channels() != new_codec->channels()) ||
          codec->hasSdpFmtpLine() != new_codec->hasSdpFmtpLine() ||
          (codec->hasSdpFmtpLine() &&
           codec->sdpFmtpLine() != new_codec->sdpFmtpLine())) {
        return true;
      }
    }
  }

  return false;
}

// Relative weights for each priority as defined in RTCWEB-DATA
// https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel
const double kPriorityWeightVeryLow = 0.5;
const double kPriorityWeightLow = 1;
const double kPriorityWeightMedium = 2;
const double kPriorityWeightHigh = 4;

V8RTCPriorityType::Enum PriorityFromDouble(double priority) {
  // Find the middle point between 2 priority weights to match them to a
  // WebRTC priority
  const double very_low_upper_bound =
      (kPriorityWeightVeryLow + kPriorityWeightLow) / 2;
  const double low_upper_bound =
      (kPriorityWeightLow + kPriorityWeightMedium) / 2;
  const double medium_upper_bound =
      (kPriorityWeightMedium + kPriorityWeightHigh) / 2;

  if (priority < webrtc::kDefaultBitratePriority * very_low_upper_bound) {
    return V8RTCPriorityType::Enum::kVeryLow;
  }
  if (priority < webrtc::kDefaultBitratePriority * low_upper_bound) {
    return V8RTCPriorityType::Enum::kLow;
  }
  if (priority < webrtc::kDefaultBitratePriority * medium_upper_bound) {
    return V8RTCPriorityType::Enum::kMedium;
  }
  return V8RTCPriorityType::Enum::kHigh;
}

double PriorityToDouble(V8RTCPriorityType::Enum priority) {
  switch (priority) {
    case V8RTCPriorityType::Enum::kVeryLow:
      return webrtc::kDefaultBitratePriority * kPriorityWeightVeryLow;
    case V8RTCPriorityType::Enum::kLow:
      return webrtc::kDefaultBitratePriority * kPriorityWeightLow;
    case V8RTCPriorityType::Enum::kMedium:
      return webrtc::kDefaultBitratePriority * kPriorityWeightMedium;
    case V8RTCPriorityType::Enum::kHigh:
      return webrtc::kDefaultBitratePriority * kPriorityWeightHigh;
  }
  NOTREACHED();
}

V8RTCPriorityType::Enum PriorityFromEnum(webrtc::Priority priority) {
  switch (priority) {
    case webrtc::Priority::kVeryLow:
      return V8RTCPriorityType::Enum::kVeryLow;
    case webrtc::Priority::kLow:
      return V8RTCPriorityType::Enum::kLow;
    case webrtc::Priority::kMedium:
      return V8RTCPriorityType::Enum::kMedium;
    case webrtc::Priority::kHigh:
      return V8RTCPriorityType::Enum::kHigh;
  }
  NOTREACHED();
}

webrtc::Priority PriorityToEnum(V8RTCPriorityType::Enum priority) {
  switch (priority) {
    case V8RTCPriorityType::Enum::kVeryLow:
      return webrtc::Priority::kVeryLow;
    case V8RTCPriorityType::Enum::kLow:
      return webrtc::Priority::kLow;
    case V8RTCPriorityType::Enum::kMedium:
      return webrtc::Priority::kMedium;
    case V8RTCPriorityType::Enum::kHigh:
      return webrtc::Priority::kHigh;
  }
  NOTREACHED();
}

std::tuple<Vector<webrtc::RtpEncodingParameters>,
           std::optional<webrtc::DegradationPreference>>
ToRtpParameters(ExecutionContext* context,
                const RTCRtpSendParameters* parameters,
                const String& kind) {
  Vector<webrtc::RtpEncodingParameters> encodings;
  if (parameters->hasEncodings()) {
    encodings.reserve(parameters->encodings().size());

    for (const auto& encoding : parameters->encodings()) {
      encodings.push_back(ToRtpEncodingParameters(context, encoding, kind));
    }
  }

  std::optional<webrtc::DegradationPreference> degradation_preference;

  if (parameters->hasDegradationPreference()) {
    if (parameters->degradationPreference() == "balanced") {
      degradation_preference = webrtc::DegradationPreference::BALANCED;
    } else if (parameters->degradationPreference() == "maintain-framerate") {
      degradation_preference =
          webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
    } else if (parameters->degradationPreference() == "maintain-resolution") {
      degradation_preference =
          webrtc::DegradationPreference::MAINTAIN_RESOLUTION;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  return std::make_tuple(encodings, degradation_preference);
}

webrtc::RtpCodec ToWebrtcRtpCodec(const RTCRtpCodec* codec) {
  webrtc::RtpCodec webrtc_codec;
  std::string mime_type = codec->mimeType().Utf8();
  auto slash_index = codec->mimeType().Find("/");
  if (slash_index == WTF::kNotFound) {
    webrtc_codec.kind = cricket::MEDIA_TYPE_UNSUPPORTED;
    return webrtc_codec;
  }
  webrtc_codec.name = codec->mimeType().Substring(slash_index + 1).Utf8();
  WTF::String codec_type = codec->mimeType().Substring(0, slash_index);

  if (codec_type == "video") {
    webrtc_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  } else if (codec_type == "audio") {
    webrtc_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  } else {
    webrtc_codec.kind = cricket::MEDIA_TYPE_UNSUPPORTED;
    return webrtc_codec;
  }

  webrtc_codec.clock_rate = codec->clockRate();
  if (codec->hasChannels()) {
    webrtc_codec.num_channels = codec->channels();
  }
  if (codec->hasSdpFmtpLine()) {
    WTF::Vector<WTF::String> fmtp_splits;
    codec->sdpFmtpLine().Split(";", true, fmtp_splits);
    for (const auto& fmtp_split : fmtp_splits) {
      WTF::String parameter = fmtp_split.StripWhiteSpace();
      auto equal_index = parameter.Find("=");
      std::string name, value;
      if (equal_index == WTF::kNotFound) {
        // Handle parameters without any equal signs, such as RED "111/111"
        name = "";
        value = parameter.Utf8();
      } else {
        // Handle parameters with an equal sign "foo=bar"
        name = parameter.Substring(0, equal_index).Utf8();
        value = parameter.Substring(equal_index + 1).Utf8();
      }
      webrtc_codec.parameters[name] = value;
    }
  }
  return webrtc_codec;
}

}  // namespace

webrtc::RtpEncodingParameters ToRtpEncodingParameters(
    ExecutionContext* context,
    const RTCRtpEncodingParameters* encoding,
    const String& kind) {
  webrtc::RtpEncodingParameters webrtc_encoding;
  if (encoding->hasRid()) {
    webrtc_encoding.rid = encoding->rid().Utf8();
  }
  webrtc_encoding.active = encoding->active();
  webrtc_encoding.bitrate_priority =
      PriorityToDouble(encoding->priority().AsEnum());
  webrtc_encoding.network_priority =
      PriorityToEnum(encoding->networkPriority().AsEnum());
  if (encoding->hasMaxBitrate()) {
    webrtc_encoding.max_bitrate_bps = ClampTo<int>(encoding->maxBitrate());
  }
  if (encoding->hasCodec()) {
    webrtc_encoding.codec = ToWebrtcRtpCodec(encoding->codec());
  }
  if (kind == "video") {
    if (encoding->hasScaleResolutionDownBy()) {
      webrtc_encoding.scale_resolution_down_by =
          encoding->scaleResolutionDownBy();
    }
    if (encoding->hasScaleResolutionDownTo()) {
      RTCResolutionRestriction* resolution_restriction =
          encoding->scaleResolutionDownTo();
      webrtc::Resolution requested_resolution;
      if (resolution_restriction->hasMaxWidth()) {
        requested_resolution.width = resolution_restriction->maxWidth();
      }
      if (resolution_restriction->hasMaxHeight()) {
        requested_resolution.height = resolution_restriction->maxHeight();
      }
      webrtc_encoding.requested_resolution = requested_resolution;
    }
    if (encoding->hasMaxFramerate()) {
      webrtc_encoding.max_framerate = encoding->maxFramerate();
    }
    // https://w3c.github.io/webrtc-svc/
    if (encoding->hasScalabilityMode()) {
      webrtc_encoding.scalability_mode = encoding->scalabilityMode().Utf8();
      WebRtcScalabilityMode scalability_mode =
          ScalabilityModeStringToUMAMode(*webrtc_encoding.scalability_mode);
      UMA_HISTOGRAM_ENUMERATION("WebRtcScalabilityMode", scalability_mode);
    }
  } else if (kind == "audio") {
    webrtc_encoding.adaptive_ptime = encoding->adaptivePtime();
  }
  return webrtc_encoding;
}

RTCRtpHeaderExtensionParameters* ToRtpHeaderExtensionParameters(
    const webrtc::RtpExtension& webrtc_header) {
  RTCRtpHeaderExtensionParameters* header =
      RTCRtpHeaderExtensionParameters::Create();
  header->setUri(webrtc_header.uri.c_str());
  header->setId(webrtc_header.id);
  header->setEncrypted(webrtc_header.encrypt);
  return header;
}

void SetRtpCodec(RTCRtpCodec& codec, const webrtc::RtpCodec& webrtc_codec) {
  codec.setMimeType(WTF::String::FromUTF8(webrtc_codec.mime_type()));
  if (webrtc_codec.clock_rate)
    codec.setClockRate(webrtc_codec.clock_rate.value());
  if (webrtc_codec.num_channels)
    codec.setChannels(webrtc_codec.num_channels.value());
  if (!webrtc_codec.parameters.empty()) {
    std::string sdp_fmtp_line;
    for (const auto& parameter : webrtc_codec.parameters) {
      if (!sdp_fmtp_line.empty())
        sdp_fmtp_line += ";";
      if (parameter.first.empty()) {
        sdp_fmtp_line += parameter.second;
      } else {
        sdp_fmtp_line += parameter.first + "=" + parameter.second;
      }
    }
    codec.setSdpFmtpLine(sdp_fmtp_line.c_str());
  }
}

RTCRtpCodec* ToRtpCodec(const webrtc::RtpCodec& webrtc_codec) {
  RTCRtpCodec* codec = RTCRtpCodec::Create();
  SetRtpCodec(*codec, webrtc_codec);
  return codec;
}

RTCRtpCodecParameters* ToRtpCodecParameters(
    const webrtc::RtpCodecParameters& webrtc_codec_parameters) {
  RTCRtpCodecParameters* codec = RTCRtpCodecParameters::Create();
  SetRtpCodec(*codec, webrtc_codec_parameters);
  codec->setPayloadType(webrtc_codec_parameters.payload_type);
  return codec;
}

RTCRtpSender::RTCRtpSender(RTCPeerConnection* pc,
                           std::unique_ptr<RTCRtpSenderPlatform> sender,
                           String kind,
                           MediaStreamTrack* track,
                           MediaStreamVector streams,
                           bool require_encoded_insertable_streams,
                           scoped_refptr<base::SequencedTaskRunner>
                               encoded_transform_shortcircuit_runner)
    : ExecutionContextLifecycleObserver(pc->GetExecutionContext()),
      pc_(pc),
      sender_(std::move(sender)),
      kind_(std::move(kind)),
      track_(track),
      streams_(std::move(streams)),
      encoded_audio_transformer_(
          kind_ == "audio"
              ? sender_->GetEncodedAudioStreamTransformer()->GetBroker()
              : nullptr),
      encoded_video_transformer_(
          kind_ == "video"
              ? sender_->GetEncodedVideoStreamTransformer()->GetBroker()
              : nullptr) {
  DCHECK(pc_);
  DCHECK(sender_);
  DCHECK(!track || kind_ == track->kind());
  LogMessage(base::StringPrintf(
      "%s({require_encoded_insertable_streams=%s})", __func__,
      require_encoded_insertable_streams ? "true" : "false"));
  if (!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
    if (encoded_audio_transformer_) {
      RegisterEncodedAudioStreamCallback();
    } else {
      CHECK(encoded_video_transformer_);
      RegisterEncodedVideoStreamCallback();
    }
  }

  if (!require_encoded_insertable_streams) {
    // Schedule a task to short circuit encoded streams if JS doesn't
    // synchronously create them.
    encoded_transform_shortcircuit_runner->PostTask(
        FROM_HERE, WTF::BindOnce(&RTCRtpSender::MaybeShortCircuitEncodedStreams,
                                 WrapPersistent(this)));
  }
}

MediaStreamTrack* RTCRtpSender::track() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return track_.Get();
}

RTCDtlsTransport* RTCRtpSender::transport() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return transport_.Get();
}

RTCDtlsTransport* RTCRtpSender::rtcpTransport() {
  // Chrome does not support turning off RTCP-mux.
  return nullptr;
}

ScriptPromise<IDLUndefined> RTCRtpSender::replaceTrack(
    ScriptState* script_state,
    MediaStreamTrack* with_track,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (pc_->IsClosed()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "The peer connection is closed.");
    return promise;
  }

  if (with_track && kind_ != with_track->kind()) {
    resolver->RejectWithTypeError("Track kind does not match Sender kind");
    return promise;
  }

  if (transceiver_ && transceiver_->stopped()) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "replaceTrack cannot be called on a stopped sender");
    return promise;
  }

  MediaStreamComponent* component = nullptr;
  if (with_track) {
    pc_->RegisterTrack(with_track);
    component = with_track->Component();
  }
  ReplaceTrackRequest* request =
      MakeGarbageCollected<ReplaceTrackRequest>(this, with_track, resolver);
  sender_->ReplaceTrack(component, request);
  return promise;
}

RTCRtpSendParameters* RTCRtpSender::getParameters() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RTCRtpSendParameters* parameters = RTCRtpSendParameters::Create();
  std::unique_ptr<webrtc::RtpParameters> webrtc_parameters =
      sender_->GetParameters();

  parameters->setTransactionId(webrtc_parameters->transaction_id.c_str());

  if (webrtc_parameters->degradation_preference.has_value()) {
    WTF::String degradation_preference_str;
    switch (webrtc_parameters->degradation_preference.value()) {
      case webrtc::DegradationPreference::MAINTAIN_FRAMERATE:
        degradation_preference_str = "maintain-framerate";
        break;
      case webrtc::DegradationPreference::MAINTAIN_RESOLUTION:
        degradation_preference_str = "maintain-resolution";
        break;
      case webrtc::DegradationPreference::BALANCED:
        degradation_preference_str = "balanced";
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    parameters->setDegradationPreference(degradation_preference_str);
  }
  RTCRtcpParameters* rtcp = RTCRtcpParameters::Create();
  rtcp->setCname(webrtc_parameters->rtcp.cname.c_str());
  rtcp->setReducedSize(webrtc_parameters->rtcp.reduced_size);
  parameters->setRtcp(rtcp);

  HeapVector<Member<RTCRtpEncodingParameters>> encodings;
  encodings.reserve(
      base::checked_cast<wtf_size_t>(webrtc_parameters->encodings.size()));
  for (const auto& webrtc_encoding : webrtc_parameters->encodings) {
    RTCRtpEncodingParameters* encoding = RTCRtpEncodingParameters::Create();
    if (!webrtc_encoding.rid.empty()) {
      encoding->setRid(String::FromUTF8(webrtc_encoding.rid));
    }
    encoding->setActive(webrtc_encoding.active);
    if (webrtc_encoding.max_bitrate_bps) {
      encoding->setMaxBitrate(webrtc_encoding.max_bitrate_bps.value());
    }
    encoding->setPriority(PriorityFromDouble(webrtc_encoding.bitrate_priority));
    encoding->setNetworkPriority(
        PriorityFromEnum(webrtc_encoding.network_priority));
    if (webrtc_encoding.codec) {
      encoding->setCodec(ToRtpCodec(*webrtc_encoding.codec));
    }
    if (kind_ == "video") {
      if (webrtc_encoding.scale_resolution_down_by) {
        encoding->setScaleResolutionDownBy(
            webrtc_encoding.scale_resolution_down_by.value());
      }
      if (webrtc_encoding.max_framerate) {
        encoding->setMaxFramerate(webrtc_encoding.max_framerate.value());
      }
      if (webrtc_encoding.scalability_mode) {
        encoding->setScalabilityMode(
            webrtc_encoding.scalability_mode.value().c_str());
      }
    } else if (kind_ == "audio") {
      encoding->setAdaptivePtime(webrtc_encoding.adaptive_ptime);
    }
    encodings.push_back(encoding);
  }
  parameters->setEncodings(encodings);

  HeapVector<Member<RTCRtpHeaderExtensionParameters>> headers;
  headers.reserve(base::checked_cast<wtf_size_t>(
      webrtc_parameters->header_extensions.size()));
  for (const auto& webrtc_header : webrtc_parameters->header_extensions) {
    headers.push_back(ToRtpHeaderExtensionParameters(webrtc_header));
  }
  parameters->setHeaderExtensions(headers);

  HeapVector<Member<RTCRtpCodecParameters>> codecs;
  codecs.reserve(
      base::checked_cast<wtf_size_t>(webrtc_parameters->codecs.size()));
  for (const auto& webrtc_codec : webrtc_parameters->codecs) {
    codecs.push_back(ToRtpCodecParameters(webrtc_codec));
  }
  parameters->setCodecs(codecs);

  last_returned_parameters_ = parameters;

  return parameters;
}

ScriptPromise<IDLUndefined> RTCRtpSender::setParameters(
    ScriptState* script_state,
    const RTCRtpSendParameters* parameters,
    const RTCSetParameterOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (!last_returned_parameters_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "getParameters() needs to be called before setParameters().");
    return promise;
  }
  // The specification mentions that some fields in the dictionary should not
  // be modified. Some of those checks are done in the lower WebRTC layer, but
  // there is no perfect 1-1 mapping between the Javascript layer and native.
  // So we save the last returned dictionary and enforce the check at this
  // level instead.
  if (HasInvalidModification(last_returned_parameters_, parameters)) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Read-only field modified in setParameters().");
    return promise;
  }

  // The only values that can be set by setParameters are in the encodings
  // field and the degradationPreference field. We just forward those to the
  // native layer without having to transform all the other read-only
  // parameters.
  Vector<webrtc::RtpEncodingParameters> encodings;
  std::optional<webrtc::DegradationPreference> degradation_preference;
  std::tie(encodings, degradation_preference) =
      ToRtpParameters(pc_->GetExecutionContext(), parameters, kind_);

  // If present, encode options must match the number of encodings.
  if (base::FeatureList::IsEnabled(
          features::kWebRtcRequestKeyFrameViaSetParameterOptions)) {
    const auto& encoding_options = options->encodingOptions();
    if (!encoding_options.empty()) {
      if (encoding_options.size() != encodings.size()) {
        resolver->RejectWithDOMException(
            DOMExceptionCode::kInvalidModificationError,
            "encodingOptions size must match number of encodings.");
      }
      for (wtf_size_t i = 0; i < encoding_options.size(); i++) {
        encodings[i].request_key_frame = encoding_options[i]->keyFrame();
      }
    }
  }

  auto* request = MakeGarbageCollected<SetParametersRequest>(resolver, this);
  sender_->SetParameters(std::move(encodings), degradation_preference, request);
  return promise;
}

void RTCRtpSender::ClearLastReturnedParameters() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  last_returned_parameters_ = nullptr;
}

ScriptPromise<RTCStatsReport> RTCRtpSender::getStats(
    ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<RTCStatsReport>>(script_state);
  auto promise = resolver->Promise();
  sender_->GetStats(WTF::BindOnce(WebRTCStatsReportCallbackResolver,
                                  WrapPersistent(resolver)));
  return promise;
}

RTCRtpSenderPlatform* RTCRtpSender::web_sender() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return sender_.get();
}

void RTCRtpSender::SetTrack(MediaStreamTrack* track) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  track_ = track;
  if (track) {
    if (kind_.IsNull()) {
      kind_ = track->kind();
    } else if (kind_ != track->kind()) {
      LOG(ERROR) << "Trying to set track to a different kind: Old " << kind_
                 << " new " << track->kind();
      NOTREACHED_IN_MIGRATION();
    }
  }
}

MediaStreamVector RTCRtpSender::streams() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return streams_;
}

void RTCRtpSender::set_streams(MediaStreamVector streams) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  streams_ = std::move(streams);
}

void RTCRtpSender::set_transceiver(RTCRtpTransceiver* transceiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transceiver_ = transceiver;
}

void RTCRtpSender::set_transport(RTCDtlsTransport* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_ = transport;
}

RTCDTMFSender* RTCRtpSender::dtmf() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Lazy initialization of dtmf_ to avoid overhead when not used.
  if (!dtmf_ && kind_ == "audio") {
    auto handler = sender_->GetDtmfSender();
    if (!handler) {
      LOG(ERROR) << "Unable to create DTMF sender attribute on an audio sender";
      return nullptr;
    }
    dtmf_ =
        RTCDTMFSender::Create(pc_->GetExecutionContext(), std::move(handler));
  }
  return dtmf_.Get();
}

void RTCRtpSender::setStreams(HeapVector<Member<MediaStream>> streams,
                              ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pc_->IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCPeerConnection's signalingState is 'closed'.");
    return;
  }
  Vector<String> stream_ids;
  for (auto stream : streams)
    stream_ids.emplace_back(stream->id());
  sender_->SetStreams(stream_ids);
}

RTCInsertableStreams* RTCRtpSender::createEncodedStreams(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LogMessage(base::StringPrintf("%s({transform_shortcircuited_=%s})", __func__,
                                transform_shortcircuited_ ? "true" : "false"));
  if (transform_shortcircuited_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Too late to create encoded streams");
    return nullptr;
  }
  if (encoded_streams_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Encoded streams already created");
    return nullptr;
  }
  if (kind_ == "audio") {
    return CreateEncodedAudioStreams(script_state);
  }
  CHECK_EQ(kind_, "video");
  return CreateEncodedVideoStreams(script_state);
}

void RTCRtpSender::ContextDestroyed() {
  {
    base::AutoLock locker(audio_underlying_source_lock_);
    audio_from_encoder_underlying_source_.Clear();
  }
  {
    base::AutoLock locker(audio_underlying_sink_lock_);
    audio_to_packetizer_underlying_sink_.Clear();
  }
  {
    base::AutoLock locker(video_underlying_source_lock_);
    video_from_encoder_underlying_source_.Clear();
  }
  {
    base::AutoLock locker(video_underlying_sink_lock_);
    video_to_packetizer_underlying_sink_.Clear();
  }
}

void RTCRtpSender::Trace(Visitor* visitor) const {
  visitor->Trace(pc_);
  visitor->Trace(track_);
  visitor->Trace(transport_);
  visitor->Trace(dtmf_);
  visitor->Trace(streams_);
  visitor->Trace(last_returned_parameters_);
  visitor->Trace(transceiver_);
  visitor->Trace(encoded_streams_);
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

RTCRtpCapabilities* RTCRtpSender::getCapabilities(ScriptState* state,
                                                  const String& kind) {
  if (!state->ContextIsValid())
    return nullptr;

  if (kind != "audio" && kind != "video")
    return nullptr;

  RTCRtpCapabilities* capabilities = RTCRtpCapabilities::Create();
  capabilities->setCodecs(HeapVector<Member<RTCRtpCodecCapability>>());
  capabilities->setHeaderExtensions(
      HeapVector<Member<RTCRtpHeaderExtensionCapability>>());

  std::unique_ptr<webrtc::RtpCapabilities> rtc_capabilities =
      PeerConnectionDependencyFactory::From(*ExecutionContext::From(state))
          .GetSenderCapabilities(kind);

  HeapVector<Member<RTCRtpCodecCapability>> codecs;
  codecs.ReserveInitialCapacity(
      base::checked_cast<wtf_size_t>(rtc_capabilities->codecs.size()));
  for (const auto& rtc_codec : rtc_capabilities->codecs) {
    auto* codec = RTCRtpCodecCapability::Create();
    codec->setMimeType(WTF::String::FromUTF8(rtc_codec.mime_type()));
    if (rtc_codec.clock_rate)
      codec->setClockRate(rtc_codec.clock_rate.value());

    if (rtc_codec.num_channels)
      codec->setChannels(rtc_codec.num_channels.value());
    if (!rtc_codec.parameters.empty()) {
      std::string sdp_fmtp_line;
      for (const auto& parameter : rtc_codec.parameters) {
        if (!sdp_fmtp_line.empty())
          sdp_fmtp_line += ";";
        if (parameter.first.empty()) {
          sdp_fmtp_line += parameter.second;
        } else {
          sdp_fmtp_line += parameter.first + "=" + parameter.second;
        }
      }
      codec->setSdpFmtpLine(sdp_fmtp_line.c_str());
    }
    codecs.push_back(codec);
  }
  capabilities->setCodecs(codecs);

  HeapVector<Member<RTCRtpHeaderExtensionCapability>> header_extensions;
  header_extensions.ReserveInitialCapacity(base::checked_cast<wtf_size_t>(
      rtc_capabilities->header_extensions.size()));
  for (const auto& rtc_header_extension : rtc_capabilities->header_extensions) {
    auto* header_extension = RTCRtpHeaderExtensionCapability::Create();
    header_extension->setUri(WTF::String::FromUTF8(rtc_header_extension.uri));
    header_extensions.push_back(header_extension);
  }
  capabilities->setHeaderExtensions(header_extensions);

  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kRtcRtpSenderGetCapabilities)) {
    IdentifiableTokenBuilder builder;
    IdentifiabilityAddRTCRtpCapabilitiesToBuilder(builder, *capabilities);
    IdentifiabilityMetricBuilder(ExecutionContext::From(state)->UkmSourceID())
        .Add(IdentifiableSurface::FromTypeAndToken(
                 IdentifiableSurface::Type::kRtcRtpSenderGetCapabilities,
                 IdentifiabilityBenignStringToken(kind)),
             builder.GetToken())
        .Record(ExecutionContext::From(state)->UkmRecorder());
  }
  return capabilities;
}

void RTCRtpSender::MaybeShortCircuitEncodedStreams() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!encoded_streams_ && !transform_) {
    transform_shortcircuited_ = true;
    LogMessage("Starting short circuiting of encoded transform");
    if (kind_ == "video") {
      encoded_video_transformer_->StartShortCircuiting();
    } else {
      CHECK_EQ(kind_, "audio");
      encoded_audio_transformer_->StartShortCircuiting();
    }
  }
}

void RTCRtpSender::RegisterEncodedAudioStreamCallback() {
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(kind_, "audio");
  encoded_audio_transformer_->SetTransformerCallback(
      WTF::CrossThreadBindRepeating(&RTCRtpSender::OnAudioFrameFromEncoder,
                                    WrapCrossThreadWeakPersistent(this)));
}

void RTCRtpSender::UnregisterEncodedAudioStreamCallback() {
  // Threadsafe as this might be called from the realm to which a stream has
  // been transferred.
  encoded_audio_transformer_->ResetTransformerCallback();
}

void RTCRtpSender::SetAudioUnderlyingSource(
    RTCEncodedAudioUnderlyingSource* new_underlying_source,
    scoped_refptr<base::SingleThreadTaskRunner> new_source_task_runner) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpSender, underlying
    // source(s), and transformer are about to be garbage collected, so there's
    // no reason to continue.
    return;
  }
  {
    base::AutoLock locker(audio_underlying_source_lock_);
    audio_from_encoder_underlying_source_->OnSourceTransferStarted();
    audio_from_encoder_underlying_source_ = new_underlying_source;
    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_audio_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedAudioUnderlyingSource::OnFrameFromSource,
              audio_from_encoder_underlying_source_));
    }
  }

  encoded_audio_transformer_->SetSourceTaskRunner(
      std::move(new_source_task_runner));
}

void RTCRtpSender::SetAudioUnderlyingSink(
    RTCEncodedAudioUnderlyingSink* new_underlying_sink) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpSender and underlying
    // sink(s) are about to be garbage collected, so there's no reason to
    // continue.
    return;
  }
  base::AutoLock locker(audio_underlying_sink_lock_);
  audio_to_packetizer_underlying_sink_ = new_underlying_sink;
}

RTCInsertableStreams* RTCRtpSender::CreateEncodedAudioStreams(
    ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!encoded_streams_);

  encoded_streams_ = RTCInsertableStreams::Create();

  {
    base::AutoLock locker(audio_underlying_source_lock_);
    DCHECK(!audio_from_encoder_underlying_source_);

    // Set up readable.
    audio_from_encoder_underlying_source_ =
        MakeGarbageCollected<RTCEncodedAudioUnderlyingSource>(
            script_state,
            WTF::CrossThreadBindOnce(
                &RTCRtpSender::UnregisterEncodedAudioStreamCallback,
                WrapCrossThreadWeakPersistent(this)));

    auto set_underlying_source =
        WTF::CrossThreadBindRepeating(&RTCRtpSender::SetAudioUnderlyingSource,
                                      WrapCrossThreadWeakPersistent(this));
    auto disconnect_callback = WTF::CrossThreadBindOnce(
        &RTCRtpSender::UnregisterEncodedAudioStreamCallback,
        WrapCrossThreadWeakPersistent(this));
    // The high water mark for the readable stream is set to 0 so that frames
    // are removed from the queue right away, without introducing a new buffer.
    ReadableStream* readable_stream =
        ReadableStream::CreateWithCountQueueingStrategy(
            script_state, audio_from_encoder_underlying_source_,
            /*high_water_mark=*/0, AllowPerChunkTransferring(false),
            std::make_unique<RtcEncodedAudioSenderSourceOptimizer>(
                std::move(set_underlying_source),
                std::move(disconnect_callback)));
    encoded_streams_->setReadable(readable_stream);

    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_audio_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedAudioUnderlyingSource::OnFrameFromSource,
              audio_from_encoder_underlying_source_));
    }
  }

  WritableStream* writable_stream;
  {
    base::AutoLock locker(audio_underlying_sink_lock_);
    DCHECK(!audio_to_packetizer_underlying_sink_);

    // Set up writable.
    audio_to_packetizer_underlying_sink_ =
        MakeGarbageCollected<RTCEncodedAudioUnderlyingSink>(
            script_state, encoded_audio_transformer_,
            /*detach_frame_data_on_write=*/false);

    auto set_underlying_sink =
        WTF::CrossThreadBindOnce(&RTCRtpSender::SetAudioUnderlyingSink,
                                 WrapCrossThreadWeakPersistent(this));

    // The high water mark for the stream is set to 1 so that the stream seems
    // ready to write, but without queuing frames.
    writable_stream = WritableStream::CreateWithCountQueueingStrategy(
        script_state, audio_to_packetizer_underlying_sink_,
        /*high_water_mark=*/1,
        std::make_unique<RtcEncodedAudioSenderSinkOptimizer>(
            std::move(set_underlying_sink), encoded_audio_transformer_));
  }

  encoded_streams_->setWritable(writable_stream);
  return encoded_streams_;
}

void RTCRtpSender::OnAudioFrameFromEncoder(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));

  base::AutoLock locker(audio_underlying_source_lock_);
  if (audio_from_encoder_underlying_source_) {
    audio_from_encoder_underlying_source_->OnFrameFromSource(std::move(frame));
  }
}

void RTCRtpSender::RegisterEncodedVideoStreamCallback() {
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(kind_, "video");
  encoded_video_transformer_->SetTransformerCallback(
      WTF::CrossThreadBindRepeating(&RTCRtpSender::OnVideoFrameFromEncoder,
                                    WrapCrossThreadWeakPersistent(this)));
}

void RTCRtpSender::UnregisterEncodedVideoStreamCallback() {
  // Threadsafe as this might be called from the realm to which a stream has
  // been transferred.
  encoded_video_transformer_->ResetTransformerCallback();
}

void RTCRtpSender::SetVideoUnderlyingSource(
    RTCEncodedVideoUnderlyingSource* new_underlying_source,
    scoped_refptr<base::SingleThreadTaskRunner> new_source_task_runner) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpSender, underlying
    // source(s), and transformer are about to be garbage collected, so there's
    // no reason to continue.
    return;
  }
  {
    base::AutoLock locker(video_underlying_source_lock_);
    video_from_encoder_underlying_source_->OnSourceTransferStarted();
    video_from_encoder_underlying_source_ = new_underlying_source;
    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_video_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedVideoUnderlyingSource::OnFrameFromSource,
              video_from_encoder_underlying_source_));
    }
  }

  encoded_video_transformer_->SetSourceTaskRunner(
      std::move(new_source_task_runner));
}

void RTCRtpSender::SetVideoUnderlyingSink(
    RTCEncodedVideoUnderlyingSink* new_underlying_sink) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpSender and underlying
    // sink(s) are about to be garbage collected, so there's no reason to
    // continue.
    return;
  }
  base::AutoLock locker(video_underlying_sink_lock_);
  video_to_packetizer_underlying_sink_ = new_underlying_sink;
}

RTCInsertableStreams* RTCRtpSender::CreateEncodedVideoStreams(
    ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!encoded_streams_);

  encoded_streams_ = RTCInsertableStreams::Create();

  {
    base::AutoLock locker(video_underlying_source_lock_);
    DCHECK(!video_from_encoder_underlying_source_);

    // Set up readable.
    video_from_encoder_underlying_source_ =
        MakeGarbageCollected<RTCEncodedVideoUnderlyingSource>(
            script_state,
            WTF::CrossThreadBindOnce(
                &RTCRtpSender::UnregisterEncodedVideoStreamCallback,
                WrapCrossThreadWeakPersistent(this)));

    auto set_underlying_source =
        WTF::CrossThreadBindRepeating(&RTCRtpSender::SetVideoUnderlyingSource,
                                      WrapCrossThreadWeakPersistent(this));
    auto disconnect_callback = WTF::CrossThreadBindOnce(
        &RTCRtpSender::UnregisterEncodedVideoStreamCallback,
        WrapCrossThreadWeakPersistent(this));
    // The high water mark for the readable stream is set to 0 so that frames
    // are removed from the queue right away, without introducing a new buffer.
    ReadableStream* readable_stream =
        ReadableStream::CreateWithCountQueueingStrategy(
            script_state, video_from_encoder_underlying_source_,
            /*high_water_mark=*/0, AllowPerChunkTransferring(false),
            std::make_unique<RtcEncodedVideoSenderSourceOptimizer>(
                std::move(set_underlying_source),
                std::move(disconnect_callback)));
    encoded_streams_->setReadable(readable_stream);

    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_video_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedVideoUnderlyingSource::OnFrameFromSource,
              video_from_encoder_underlying_source_));
    }
  }

  WritableStream* writable_stream;
  {
    base::AutoLock locker(video_underlying_sink_lock_);
    DCHECK(!video_to_packetizer_underlying_sink_);

    // Set up writable.
    video_to_packetizer_underlying_sink_ =
        MakeGarbageCollected<RTCEncodedVideoUnderlyingSink>(
            script_state, encoded_video_transformer_,
            /*detach_frame_data_on_write=*/false);

    auto set_underlying_sink =
        WTF::CrossThreadBindOnce(&RTCRtpSender::SetVideoUnderlyingSink,
                                 WrapCrossThreadWeakPersistent(this));

    // The high water mark for the stream is set to 1 so that the stream seems
    // ready to write, but without queuing frames.
    writable_stream = WritableStream::CreateWithCountQueueingStrategy(
        script_state, video_to_packetizer_underlying_sink_,
        /*high_water_mark=*/1,
        std::make_unique<RtcEncodedVideoSenderSinkOptimizer>(
            std::move(set_underlying_sink), encoded_video_transformer_));
  }

  encoded_streams_->setWritable(writable_stream);
  return encoded_streams_;
}

void RTCRtpSender::setTransform(RTCRtpScriptTransform* transform,
                                ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (transform_ == transform) {
    return;
  }
  if (!transform) {
    transform_->Detach();
    transform_ = nullptr;
    return;
  }
  if (transform->IsAttached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Transform is already in use");
    return;
  }
  if (transform_) {
    transform_->Detach();
  }
  transform_ = transform;
  transform_->Attach();
  if (kind_ == "audio") {
    transform_->CreateAudioUnderlyingSourceAndSink(
        WTF::CrossThreadBindOnce(
            &RTCRtpSender::UnregisterEncodedAudioStreamCallback,
            WrapCrossThreadWeakPersistent(this)),
        encoded_audio_transformer_);
    return;
  }
  CHECK_EQ(kind_, "video");
  transform_->CreateVideoUnderlyingSourceAndSink(
      WTF::CrossThreadBindOnce(
          &RTCRtpSender::UnregisterEncodedVideoStreamCallback,
          WrapCrossThreadWeakPersistent(this)),
      encoded_video_transformer_);
}

void RTCRtpSender::OnVideoFrameFromEncoder(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));

  base::AutoLock locker(video_underlying_source_lock_);
  if (video_from_encoder_underlying_source_) {
    video_from_encoder_underlying_source_->OnFrameFromSource(std::move(frame));
  }
}

void RTCRtpSender::LogMessage(const std::string& message) {
  blink::WebRtcLogMessage(
      base::StringPrintf("RtpSndr::%s [this=0x%" PRIXPTR "]", message.c_str(),
                         reinterpret_cast<uintptr_t>(this)));
}

}  // namespace blink
