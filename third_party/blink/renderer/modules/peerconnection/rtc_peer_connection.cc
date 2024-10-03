/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_answer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_data_channel_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_candidate_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_server.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_offer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_transceiver_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_mediastreamtrack_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/modules/crypto/crypto_result_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_event.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate_generator.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_error_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_sctp_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_promise_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_track_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_promise_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_answer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/data_channel_interface.h"
#include "third_party/webrtc/api/dtls_transport_interface.h"
#include "third_party/webrtc/api/jsep.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/priority.h"
#include "third_party/webrtc/rtc_base/ssl_identity.h"

namespace blink {

namespace {

const char kSignalingStateClosedMessage[] =
    "The RTCPeerConnection's signalingState is 'closed'.";
const char kModifiedSdpMessage[] =
    "The SDP does not match the previously generated SDP for this type";

base::LazyInstance<RTCPeerConnection::RtcPeerConnectionHandlerFactoryCallback>::
    Leaky g_create_rpc_peer_connection_handler_callback_ =
        LAZY_INSTANCE_INITIALIZER;

// The maximum number of PeerConnections that can exist simultaneously.
const int64_t kMaxPeerConnections = 500;

bool ThrowExceptionIfSignalingStateClosed(
    webrtc::PeerConnectionInterface::SignalingState state,
    ExceptionState* exception_state) {
  if (state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    exception_state->ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                       kSignalingStateClosedMessage);
    return true;
  }

  return false;
}

void AsyncCallErrorCallback(ExecutionContext* context,
                            V8RTCPeerConnectionErrorCallback* error_callback,
                            DOMException* exception) {
  DCHECK(error_callback);
  context->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
      &V8RTCPeerConnectionErrorCallback::InvokeAndReportException,
      WrapPersistent(error_callback), nullptr, WrapPersistent(exception)));
}

bool CallErrorCallbackIfSignalingStateClosed(
    ExecutionContext* context,
    webrtc::PeerConnectionInterface::SignalingState state,
    V8RTCPeerConnectionErrorCallback* error_callback) {
  if (state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    if (error_callback) {
      AsyncCallErrorCallback(context, error_callback,
                             MakeGarbageCollected<DOMException>(
                                 DOMExceptionCode::kInvalidStateError,
                                 kSignalingStateClosedMessage));
    }
    return true;
  }

  return false;
}

bool IsIceCandidateMissingSdpMidAndMLineIndex(
    const RTCIceCandidateInit* candidate) {
  return (candidate->sdpMid().IsNull() &&
          !candidate->hasSdpMLineIndexNonNull());
}

RTCOfferOptionsPlatform* ConvertToRTCOfferOptionsPlatform(
    const RTCOfferOptions* options) {
  if (!options)
    return nullptr;
  return MakeGarbageCollected<RTCOfferOptionsPlatform>(
      options->hasOfferToReceiveVideo()
          ? std::max(options->offerToReceiveVideo(), 0)
          : -1,
      options->hasOfferToReceiveAudio()
          ? std::max(options->offerToReceiveAudio(), 0)
          : -1,
      options->hasVoiceActivityDetection() ? options->voiceActivityDetection()
                                           : true,
      options->hasIceRestart() ? options->iceRestart() : false);
}

RTCAnswerOptionsPlatform* ConvertToRTCAnswerOptionsPlatform(
    const RTCAnswerOptions* options) {
  if (!options)
    return nullptr;
  return MakeGarbageCollected<RTCAnswerOptionsPlatform>(
      options->hasVoiceActivityDetection() ? options->voiceActivityDetection()
                                           : true);
}

RTCIceCandidatePlatform* ConvertToRTCIceCandidatePlatform(
    ExecutionContext* context,
    const RTCIceCandidateInit* candidate) {
  // TODO(guidou): Change default value to -1. crbug.com/614958.
  uint16_t sdp_m_line_index = 0;
  if (candidate->hasSdpMLineIndexNonNull()) {
    sdp_m_line_index = candidate->sdpMLineIndexNonNull();
  } else {
    UseCounter::Count(context,
                      WebFeature::kRTCIceCandidateDefaultSdpMLineIndex);
  }
  return MakeGarbageCollected<RTCIceCandidatePlatform>(
      candidate->candidate(), candidate->sdpMid(), sdp_m_line_index,
      candidate->usernameFragment(),
      /*url can not be reconstruncted*/ std::nullopt);
}

webrtc::PeerConnectionInterface::IceTransportsType IceTransportPolicyFromEnum(
    V8RTCIceTransportPolicy::Enum policy) {
  switch (policy) {
    case V8RTCIceTransportPolicy::Enum::kRelay:
      return webrtc::PeerConnectionInterface::kRelay;
    case V8RTCIceTransportPolicy::Enum::kAll:
      return webrtc::PeerConnectionInterface::kAll;
  }
  NOTREACHED();
}

bool IsValidStunURL(const KURL& url) {
  if (!url.ProtocolIs("stun") && !url.ProtocolIs("stuns")) {
    return false;
  }
  if (!url.Query().empty()) {
    return false;
  }
  return true;
}

bool IsValidTurnURL(const KURL& url) {
  if (!url.ProtocolIs("turn") && !url.ProtocolIs("turns")) {
    return false;
  }
  if (!url.Query().empty()) {
    Vector<String> query_parts;
    url.Query().ToString().Split("=", query_parts);
    if (query_parts.size() < 2 || query_parts[0] != "transport") {
      return false;
    }
  }
  return true;
}

webrtc::PeerConnectionInterface::RTCConfiguration ParseConfiguration(
    ExecutionContext* context,
    const RTCConfiguration* configuration,
    ExceptionState* exception_state) {
  DCHECK(context);

  webrtc::PeerConnectionInterface::RTCConfiguration web_configuration;

  if (configuration->hasIceTransportPolicy()) {
    UseCounter::Count(context, WebFeature::kRTCConfigurationIceTransportPolicy);
    web_configuration.type = IceTransportPolicyFromEnum(
        configuration->iceTransportPolicy().AsEnum());
  } else if (configuration->hasIceTransports()) {
    UseCounter::Count(context, WebFeature::kRTCConfigurationIceTransports);
    web_configuration.type =
        IceTransportPolicyFromEnum(configuration->iceTransports().AsEnum());
  }

  if (configuration->bundlePolicy() == "max-compat") {
    web_configuration.bundle_policy =
        webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
  } else if (configuration->bundlePolicy() == "max-bundle") {
    web_configuration.bundle_policy =
        webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
  } else {
    DCHECK_EQ(configuration->bundlePolicy(), "balanced");
  }

  if (configuration->rtcpMuxPolicy() == "negotiate") {
    web_configuration.rtcp_mux_policy =
        webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
    Deprecation::CountDeprecation(context, WebFeature::kRtcpMuxPolicyNegotiate);
  } else {
    DCHECK_EQ(configuration->rtcpMuxPolicy(), "require");
  }

  if (configuration->hasIceServers()) {
    WebVector<webrtc::PeerConnectionInterface::IceServer> ice_servers;
    for (const RTCIceServer* ice_server : configuration->iceServers()) {
      Vector<String> url_strings;
      std::vector<std::string> converted_urls;
      if (ice_server->hasUrls()) {
        UseCounter::Count(context, WebFeature::kRTCIceServerURLs);
        switch (ice_server->urls()->GetContentType()) {
          case V8UnionStringOrStringSequence::ContentType::kString:
            url_strings.push_back(ice_server->urls()->GetAsString());
            break;
          case V8UnionStringOrStringSequence::ContentType::kStringSequence:
            url_strings = ice_server->urls()->GetAsStringSequence();
            break;
        }
      } else if (ice_server->hasUrl()) {
        UseCounter::Count(context, WebFeature::kRTCIceServerURL);
        url_strings.push_back(ice_server->url());
      } else {
        exception_state->ThrowTypeError("Malformed RTCIceServer");
        return {};
      }

      for (const String& url_string : url_strings) {
        KURL url(NullURL(), url_string);
        if (!url.IsValid()) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kSyntaxError,
              "'" + url_string + "' is not a valid URL.");
          return {};
        }
        bool is_valid_turn = IsValidTurnURL(url);
        if (!is_valid_turn && !IsValidStunURL(url)) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kSyntaxError,
              "'" + url_string + "' is not a valid stun or turn URL.");
          return {};
        }
        if (is_valid_turn &&
            (!ice_server->hasUsername() || !ice_server->hasCredential())) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kInvalidAccessError,
              "Both username and credential are "
              "required when the URL scheme is "
              "\"turn\" or \"turns\".");
        }

        converted_urls.push_back(String(url).Utf8());
      }

      auto converted_ice_server = webrtc::PeerConnectionInterface::IceServer();
      converted_ice_server.urls = std::move(converted_urls);
      if (ice_server->hasUsername()) {
        converted_ice_server.username = ice_server->username().Utf8();
      }
      if (ice_server->hasCredential()) {
        converted_ice_server.password = ice_server->credential().Utf8();
      }
      ice_servers.emplace_back(std::move(converted_ice_server));
    }
    web_configuration.servers = ice_servers.ReleaseVector();
  }

  if (configuration->hasCertificates()) {
    const HeapVector<Member<RTCCertificate>>& certificates =
        configuration->certificates();
    WebVector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates_copy(
        certificates.size());
    for (wtf_size_t i = 0; i < certificates.size(); ++i) {
      certificates_copy[i] = certificates[i]->Certificate();
    }
    web_configuration.certificates = certificates_copy.ReleaseVector();
  }

  web_configuration.ice_candidate_pool_size =
      configuration->iceCandidatePoolSize();

  if (configuration->hasRtcAudioJitterBufferMaxPackets()) {
    UseCounter::Count(context, WebFeature::kRTCMaxAudioBufferSize);
    web_configuration.audio_jitter_buffer_max_packets =
        static_cast<int>(configuration->rtcAudioJitterBufferMaxPackets());
  }

  if (configuration->hasRtcAudioJitterBufferFastAccelerate()) {
    UseCounter::Count(context, WebFeature::kRTCMaxAudioBufferSize);
    web_configuration.audio_jitter_buffer_fast_accelerate =
        configuration->hasRtcAudioJitterBufferFastAccelerate();
  }

  if (configuration->hasRtcAudioJitterBufferMinDelayMs()) {
    UseCounter::Count(context, WebFeature::kRTCMaxAudioBufferSize);
    web_configuration.audio_jitter_buffer_min_delay_ms =
        static_cast<int>(configuration->rtcAudioJitterBufferMinDelayMs());
  }

  return web_configuration;
}

bool SdpMismatch(String old_sdp, String new_sdp, String attribute) {
  // Look for an attribute that is present in both old and new SDP
  // and is modified which is not allowed.
  String attribute_with_prefix = "\na=" + attribute + ":";
  const wtf_size_t new_attribute_pos = new_sdp.Find(attribute_with_prefix);
  if (new_attribute_pos == kNotFound) {
    return true;
  }
  const wtf_size_t old_attribute_pos = old_sdp.Find(attribute_with_prefix);
  if (old_attribute_pos == kNotFound) {
    return true;
  }
  wtf_size_t old_attribute_end = old_sdp.Find("\r\n", old_attribute_pos + 1);
  if (old_attribute_end == kNotFound) {
    old_attribute_end = old_sdp.Find("\n", old_attribute_pos + 1);
  }
  wtf_size_t new_attribute_end = new_sdp.Find("\r\n", new_attribute_pos + 1);
  if (new_attribute_end == kNotFound) {
    new_attribute_end = new_sdp.Find("\n", new_attribute_pos + 1);
  }
  return old_sdp.Substring(old_attribute_pos,
                           old_attribute_end - old_attribute_pos) !=
         new_sdp.Substring(new_attribute_pos,
                           new_attribute_end - new_attribute_pos);
}

bool IceUfragPwdMismatch(String old_sdp, String new_sdp) {
  return SdpMismatch(old_sdp, new_sdp, "ice-ufrag") ||
         SdpMismatch(old_sdp, new_sdp, "ice-pwd");
}

bool FingerprintMismatch(String old_sdp, String new_sdp) {
  // Check special case of externally generated SDP without fingerprints.
  // It's impossible to generate a valid fingerprint without createOffer
  // or createAnswer, so this only applies when there are no fingerprints.
  // This is allowed.
  const wtf_size_t new_fingerprint_pos = new_sdp.Find("\na=fingerprint:");
  if (new_fingerprint_pos == kNotFound) {
    return false;
  }
  // Look for fingerprint having been added. Not allowed.
  const wtf_size_t old_fingerprint_pos = old_sdp.Find("\na=fingerprint:");
  if (old_fingerprint_pos == kNotFound) {
    return true;
  }
  // Look for fingerprint being modified. Not allowed.  Handle differences in
  // line endings ('\r\n' vs, '\n' when looking for the end of the fingerprint).
  wtf_size_t old_fingerprint_end =
      old_sdp.Find("\r\n", old_fingerprint_pos + 1);
  if (old_fingerprint_end == kNotFound) {
    old_fingerprint_end = old_sdp.Find("\n", old_fingerprint_pos + 1);
  }
  wtf_size_t new_fingerprint_end =
      new_sdp.Find("\r\n", new_fingerprint_pos + 1);
  if (new_fingerprint_end == kNotFound) {
    new_fingerprint_end = new_sdp.Find("\n", new_fingerprint_pos + 1);
  }
  return old_sdp.Substring(old_fingerprint_pos,
                           old_fingerprint_end - old_fingerprint_pos) !=
         new_sdp.Substring(new_fingerprint_pos,
                           new_fingerprint_end - new_fingerprint_pos);
}

bool ContainsLegacySimulcast(String sdp) {
  // Looks for the non-spec simulcast that іs enabled via SDP munging.
  return sdp.Find("\na=ssrc-group:SIM") != kNotFound;
}

bool ContainsLegacyRtpDataChannel(String sdp) {
  // Looks for the non-spec legacy RTP data channel.
  return sdp.Find("google-data/90000") != kNotFound;
}

bool ContainsCandidate(String sdp) {
  return sdp.Find("\na=candidate") != kNotFound;
}

bool ContainsOpusStereo(String sdp) {
  return sdp.Find("stereo=1") != kNotFound;
}

// Keep in sync with tools/metrics/histograms/metadata/web_rtc/enums.xml
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GenerateCertificateAlgorithms {
  kEcDsaP256 = 0,
  kRsa1024,
  kRsa2048,
  kRsa4096,
  kRsa8192,
  kRsaOther,
  kMaxValue = kRsaOther,
};

void MeasureGenerateCertificateKeyType(
    const std::optional<rtc::KeyParams>& key_params) {
  if (!key_params.has_value()) {
    return;
  }
  GenerateCertificateAlgorithms bucket =
      GenerateCertificateAlgorithms::kEcDsaP256;
  if (key_params->type() == rtc::KT_RSA) {
    switch (key_params->rsa_params().mod_size) {
      case 1024:
        bucket = GenerateCertificateAlgorithms::kRsa1024;
        break;
      case 2048:
        bucket = GenerateCertificateAlgorithms::kRsa2048;
        break;
      case 4096:
        bucket = GenerateCertificateAlgorithms::kRsa4096;
        break;
      case 8192:
        bucket = GenerateCertificateAlgorithms::kRsa8192;
        break;
      default:
        bucket = GenerateCertificateAlgorithms::kRsaOther;
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION(
      "WebRTC.PeerConnection.GenerateCertificate.Algorithms", bucket,
      GenerateCertificateAlgorithms::kMaxValue);
}

}  // namespace

RTCPeerConnection::EventWrapper::EventWrapper(Event* event,
                                              BoolFunction function)
    : event_(event), setup_function_(std::move(function)) {}

bool RTCPeerConnection::EventWrapper::Setup() {
  if (setup_function_) {
    return std::move(setup_function_).Run();
  }
  return true;
}

void RTCPeerConnection::EventWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(event_);
}

RTCPeerConnection* RTCPeerConnection::Create(
    ExecutionContext* context,
    const RTCConfiguration* rtc_configuration,
    ExceptionState& exception_state) {
  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "PeerConnections may not be created in detached documents.");
    return nullptr;
  }

  // Count number of PeerConnections that could potentially be impacted by CSP
  auto* content_security_policy = context->GetContentSecurityPolicy();
  if (content_security_policy &&
      content_security_policy->IsActiveForConnections()) {
    UseCounter::Count(context, WebFeature::kRTCPeerConnectionWithActiveCsp);
    // Count number of PeerConnections that would be blocked by CSP connect-src
    // or one of the directive it inherits from.
    // This is intended for evaluating whether introducing a "webrtc-src"
    // on-off switch that inherits from connect-csp would be harmful or not.
    // TODO(crbug.com/1225968): Remove code when decision is made.
    if (!content_security_policy->AllowConnectToSource(
            KURL("https://example.org"), KURL("https://example.org"),
            RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting)) {
      UseCounter::Count(context, WebFeature::kRTCPeerConnectionWithBlockingCsp);
    }
  }
  // TODO(https://crbug.com/1318448): figure out if this counter should be
  // retired - the other alternative is removed.
  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionConstructorCompliant);

  webrtc::PeerConnectionInterface::RTCConfiguration configuration =
      ParseConfiguration(context, rtc_configuration, &exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Make sure no certificates have expired.
  if (!configuration.certificates.empty()) {
    DOMTimeStamp now = ConvertSecondsToDOMTimeStamp(
        base::Time::Now().InSecondsFSinceUnixEpoch());
    for (const rtc::scoped_refptr<rtc::RTCCertificate>& certificate :
         configuration.certificates) {
      DOMTimeStamp expires = certificate->Expires();
      if (expires <= now) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                          "Expired certificate(s).");
        return nullptr;
      }
    }
  }

  RTCPeerConnection* peer_connection = MakeGarbageCollected<RTCPeerConnection>(
      context, std::move(configuration),
      rtc_configuration->encodedInsertableStreams(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  return peer_connection;
}

RTCPeerConnection::RTCPeerConnection(
    ExecutionContext* context,
    webrtc::PeerConnectionInterface::RTCConfiguration configuration,
    bool encoded_insertable_streams,
    ExceptionState& exception_state)
    : ActiveScriptWrappable<RTCPeerConnection>({}),
      ExecutionContextLifecycleObserver(context),
      pending_local_description_(nullptr),
      current_local_description_(nullptr),
      pending_remote_description_(nullptr),
      current_remote_description_(nullptr),
      signaling_state_(
          webrtc::PeerConnectionInterface::SignalingState::kStable),
      ice_gathering_state_(webrtc::PeerConnectionInterface::kIceGatheringNew),
      ice_connection_state_(webrtc::PeerConnectionInterface::kIceConnectionNew),
      peer_connection_state_(
          webrtc::PeerConnectionInterface::PeerConnectionState::kNew),
      peer_handler_unregistered_(true),
      closed_(true),
      suppress_events_(true),
      encoded_insertable_streams_(encoded_insertable_streams),
      rtp_transport_(RuntimeEnabledFeatures::RTCRtpTransportEnabled(context)
                         ? MakeGarbageCollected<RTCRtpTransport>(context)
                         : nullptr) {
  LocalDOMWindow* window = To<LocalDOMWindow>(context);

  // WebRTC peer connections are not allowed in fenced frames.
  // Given the complex scaffolding for setting up fenced frames testing, this
  // is tested in the following locations:
  // * third_party/blink/web_tests/external/wpt/fenced-frame/webrtc-peer-connection.https.html
  // * content/browser/fenced_frame/fenced_frame_browsertest.cc
  if (RuntimeEnabledFeatures::
          FencedFramesLocalUnpartitionedDataAccessEnabled() &&
      window->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "RTCPeerConnection is not allowed in fenced frames.");
    return;
  }

  InstanceCounters::IncrementCounter(
      InstanceCounters::kRTCPeerConnectionCounter);
  // If we fail, set |m_closed| and |m_stopped| to true, to avoid hitting the
  // assert in the destructor.
  if (InstanceCounters::CounterValue(
          InstanceCounters::kRTCPeerConnectionCounter) > kMaxPeerConnections) {
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "Cannot create so many PeerConnections");
    return;
  }

  // Tests might need a custom RtcPeerConnectionHandler implementation.
  PeerConnectionDependencyFactory& dependency_factory =
      PeerConnectionDependencyFactory::From(*context);
  if (!g_create_rpc_peer_connection_handler_callback_.Get().is_null()) {
    peer_handler_ =
        std::move(g_create_rpc_peer_connection_handler_callback_.Get()).Run();
  } else {
    peer_handler_ = dependency_factory.CreateRTCPeerConnectionHandler(
        this, window->GetTaskRunner(TaskType::kInternalMedia),
        encoded_insertable_streams_);
  }

  if (!peer_handler_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "No PeerConnection handler can be "
                                      "created, perhaps WebRTC is disabled?");
    return;
  }

  auto* web_frame =
      static_cast<WebLocalFrame*>(WebFrame::FromCoreFrame(window->GetFrame()));
  if (!peer_handler_->Initialize(context, configuration, web_frame,
                                 exception_state, rtp_transport_)) {
    DCHECK(exception_state.HadException());
    return;
  }
  // After Initialize() with a real `peer_handler_`, WebRTC threads exist.
  scoped_refptr<base::SingleThreadTaskRunner> worker_thread =
      dependency_factory.GetWebRtcWorkerTaskRunner();
  if (!worker_thread) {
    // This path is only used in some unit test environments with a fake
    // `peer_handler_` that does not ensure WebRTC threads exist.
    worker_thread =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});
  }
  rtp_contributing_source_cache_.emplace(this, std::move(worker_thread));
  // The RTCPeerConnection was successfully constructed.
  closed_ = false;
  peer_handler_unregistered_ = false;
  suppress_events_ = false;
}

RTCPeerConnection::~RTCPeerConnection() {
  // This checks that close() or stop() is called before the destructor.
  // We are assuming that a wrapper is always created when RTCPeerConnection is
  // created.
  DCHECK(closed_ || peer_handler_unregistered_);
  InstanceCounters::DecrementCounter(
      InstanceCounters::kRTCPeerConnectionCounter);
  DCHECK_GE(InstanceCounters::CounterValue(
                InstanceCounters::kRTCPeerConnectionCounter),
            0);
}

void RTCPeerConnection::Dispose() {
  // Promptly clears the handler so that content doesn't access it in a lazy
  // sweeping phase. Other references to the handler use a weak pointer,
  // preventing access.
  if (peer_handler_) {
    peer_handler_.reset();
  }
  // Memory owned by RTCPeerConnection must not be touched after Dispose().
  // Shut down the cache to cancel any in-flight tasks that may otherwise have
  // used the cache.
  if (rtp_contributing_source_cache_.has_value()) {
    rtp_contributing_source_cache_.value().Shutdown();
  }
}

ScriptPromise<RTCSessionDescriptionInit> RTCPeerConnection::createOffer(
    ScriptState* script_state,
    const RTCOfferOptions* options,
    ExceptionState& exception_state) {
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSignalingStateClosedMessage);
    return EmptyPromise();
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<RTCSessionDescriptionInit>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestPromiseImpl::Create(this, resolver);

  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionCreateOffer);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionCreateOfferPromise);
  if (options->hasOfferToReceiveAudio() || options->hasOfferToReceiveVideo()) {
    UseCounter::Count(
        context,
        WebFeature::kRTCPeerConnectionCreateOfferOptionsOfferToReceive);
  }

  auto platform_transceivers = peer_handler_->CreateOffer(
      request, ConvertToRTCOfferOptionsPlatform(options));
  for (auto& platform_transceiver : platform_transceivers)
    CreateOrUpdateTransceiver(std::move(platform_transceiver));
  return promise;
}

ScriptPromise<IDLUndefined> RTCPeerConnection::createOffer(
    ScriptState* script_state,
    V8RTCSessionDescriptionCallback* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback,
    const RTCOfferOptions* options,
    ExceptionState& exception_state) {
  DCHECK(success_callback);
  DCHECK(error_callback);
  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionCreateOffer);
  UseCounter::Count(
      context, WebFeature::kRTCPeerConnectionCreateOfferLegacyFailureCallback);
  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionCreateOfferLegacyCompliant);
  if (CallErrorCallbackIfSignalingStateClosed(context, signaling_state_,
                                              error_callback))
    return ToResolvedUndefinedPromise(script_state);

  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestImpl::Create(
          GetExecutionContext(), this, success_callback, error_callback);

  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> platform_transceivers =
      peer_handler_->CreateOffer(request,
                                 ConvertToRTCOfferOptionsPlatform(options));
  for (auto& platform_transceiver : platform_transceivers)
    CreateOrUpdateTransceiver(std::move(platform_transceiver));

  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<RTCSessionDescriptionInit> RTCPeerConnection::createAnswer(
    ScriptState* script_state,
    const RTCAnswerOptions* options,
    ExceptionState& exception_state) {
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSignalingStateClosedMessage);
    return EmptyPromise();
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionCreateAnswer);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionCreateAnswerPromise);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<RTCSessionDescriptionInit>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestPromiseImpl::Create(this, resolver);
  peer_handler_->CreateAnswer(request,
                              ConvertToRTCAnswerOptionsPlatform(options));
  return promise;
}

ScriptPromise<IDLUndefined> RTCPeerConnection::createAnswer(
    ScriptState* script_state,
    V8RTCSessionDescriptionCallback* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback,
    ExceptionState&) {
  DCHECK(success_callback);
  DCHECK(error_callback);
  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionCreateAnswer);
  UseCounter::Count(
      context, WebFeature::kRTCPeerConnectionCreateAnswerLegacyFailureCallback);
  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionCreateAnswerLegacyCompliant);

  if (CallErrorCallbackIfSignalingStateClosed(context, signaling_state_,
                                              error_callback))
    return ToResolvedUndefinedPromise(script_state);

  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestImpl::Create(
          GetExecutionContext(), this, success_callback, error_callback);
  peer_handler_->CreateAnswer(request, nullptr);
  return ToResolvedUndefinedPromise(script_state);
}

DOMException* RTCPeerConnection::checkSdpForStateErrors(
    ExecutionContext* context,
    const ParsedSessionDescription& parsed_sdp) {
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kSignalingStateClosedMessage);
  }

  if (parsed_sdp.type() == "offer") {
    if (parsed_sdp.sdp() != last_offer_) {
      if (FingerprintMismatch(last_offer_, parsed_sdp.sdp())) {
        return MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidModificationError, kModifiedSdpMessage);
      } else {
        UseCounter::Count(context, WebFeature::kRTCLocalSdpModification);
        if (ContainsLegacySimulcast(parsed_sdp.sdp())) {
          UseCounter::Count(context,
                            WebFeature::kRTCLocalSdpModificationSimulcast);
        }
        if (IceUfragPwdMismatch(last_offer_, parsed_sdp.sdp())) {
          UseCounter::Count(context,
                            WebFeature::kRTCLocalSdpModificationIceUfragPwd);
        }
        if (ContainsOpusStereo(parsed_sdp.sdp()) &&
            !ContainsOpusStereo(last_offer_)) {
          UseCounter::Count(context,
                            WebFeature::kRTCLocalSdpModificationOpusStereo);
        }
        return nullptr;
        // TODO(https://crbug.com/823036): Return failure for all modification.
      }
    }
  } else if (parsed_sdp.type() == "answer" || parsed_sdp.type() == "pranswer") {
    if (parsed_sdp.sdp() != last_answer_) {
      if (FingerprintMismatch(last_answer_, parsed_sdp.sdp())) {
        return MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidModificationError, kModifiedSdpMessage);
      } else {
        UseCounter::Count(context, WebFeature::kRTCLocalSdpModification);
        if (ContainsLegacySimulcast(parsed_sdp.sdp())) {
          UseCounter::Count(context,
                            WebFeature::kRTCLocalSdpModificationSimulcast);
        }
        if (IceUfragPwdMismatch(last_answer_, parsed_sdp.sdp())) {
          UseCounter::Count(context,
                            WebFeature::kRTCLocalSdpModificationIceUfragPwd);
        }
        if (ContainsOpusStereo(parsed_sdp.sdp()) &&
            !ContainsOpusStereo(last_offer_)) {
          UseCounter::Count(context,
                            WebFeature::kRTCLocalSdpModificationOpusStereo);
        }
        return nullptr;
        // TODO(https://crbug.com/823036): Return failure for all modification.
      }
    }
  }
  return nullptr;
}

HeapHashSet<Member<RTCIceTransport>> RTCPeerConnection::ActiveIceTransports()
    const {
  HeapHashSet<Member<RTCIceTransport>> active_transports;
  for (auto transceiver : transceivers_) {
    auto* sender = transceiver->sender();
    if (sender) {
      auto* dtls_transport = transceiver->sender()->transport();
      if (dtls_transport) {
        auto* ice_transport = dtls_transport->iceTransport();
        if (ice_transport) {
          active_transports.insert(ice_transport);
        }
      }
    }
  }
  if (sctp_transport_) {
    auto* dtls_transport = sctp_transport_->transport();
    if (dtls_transport) {
      auto* ice_transport = dtls_transport->iceTransport();
      if (ice_transport) {
        active_transports.insert(ice_transport);
      }
    }
  }
  return active_transports;
}

void RTCPeerConnection::GenerateCertificateCompleted(
    ScriptPromiseResolver<RTCCertificate>* resolver,
    rtc::scoped_refptr<rtc::RTCCertificate> certificate) {
  if (!certificate) {
    resolver->Reject();
    return;
  }

  resolver->Resolve(
      MakeGarbageCollected<RTCCertificate>(std::move(certificate)));
}

void RTCPeerConnection::UpdateIceConnectionState() {
  auto new_state = ComputeIceConnectionState();
  if (ice_connection_state_ != new_state) {
    peer_handler_->TrackIceConnectionStateChange(new_state);
  }
  ChangeIceConnectionState(new_state);
}

ScriptPromise<IDLUndefined> RTCPeerConnection::setLocalDescription(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(script_state->ContextIsValid());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* request =
      MakeGarbageCollected<RTCVoidRequestPromiseImpl>(this, resolver);
  peer_handler_->SetLocalDescription(request);
  return promise;
}

ScriptPromise<IDLUndefined> RTCPeerConnection::setLocalDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit* session_description_init,
    ExceptionState& exception_state) {
  if (closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSignalingStateClosedMessage);
    return EmptyPromise();
  }

  DCHECK(script_state->ContextIsValid());
  if (!session_description_init->hasType()) {
    return setLocalDescription(script_state, exception_state);
  }
  String sdp = session_description_init->sdp();
  // https://w3c.github.io/webrtc-pc/#dom-peerconnection-setlocaldescription
  // step 4.4 and 4.5: If SDP is empty, return the last created offer or answer.
  if (sdp.empty()) {
    switch (session_description_init->type().AsEnum()) {
      case V8RTCSdpType::Enum::kOffer:
        sdp = last_offer_;
        break;
      case V8RTCSdpType::Enum::kPranswer:
      case V8RTCSdpType::Enum::kAnswer:
        sdp = last_answer_;
        break;
      case V8RTCSdpType::Enum::kRollback:
        break;
    }
  }
  ParsedSessionDescription parsed_sdp = ParsedSessionDescription::Parse(
      session_description_init->type().AsString(), sdp);
  if (session_description_init->type() != V8RTCSdpType::Enum::kRollback) {
    DOMException* exception = checkSdpForStateErrors(
        ExecutionContext::From(script_state), parsed_sdp);
    if (exception) {
      exception_state.ThrowDOMException(
          static_cast<DOMExceptionCode>(exception->code()),
          exception->message());
      return EmptyPromise();
    }
  }
  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionSetLocalDescription);
  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionSetLocalDescriptionPromise);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* request =
      MakeGarbageCollected<RTCVoidRequestPromiseImpl>(this, resolver);
  peer_handler_->SetLocalDescription(request, std::move(parsed_sdp));
  return promise;
}

ScriptPromise<IDLUndefined> RTCPeerConnection::setLocalDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit* session_description_init,
    V8VoidFunction* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback) {
  if (CallErrorCallbackIfSignalingStateClosed(
          ExecutionContext::From(script_state), signaling_state_,
          error_callback)) {
    return ToResolvedUndefinedPromise(script_state);
  }

  DCHECK(script_state->ContextIsValid());
  String sdp = session_description_init->sdp();
  // https://w3c.github.io/webrtc-pc/#dom-peerconnection-setlocaldescription
  // step 4.4 and 4.5: If SDP is empty, return the last created offer or answer.
  if (sdp.empty() && session_description_init->hasType()) {
    switch (session_description_init->type().AsEnum()) {
      case V8RTCSdpType::Enum::kOffer:
        sdp = last_offer_;
        break;
      case V8RTCSdpType::Enum::kPranswer:
      case V8RTCSdpType::Enum::kAnswer:
        sdp = last_answer_;
        break;
      case V8RTCSdpType::Enum::kRollback:
        break;
    }
  }
  ParsedSessionDescription parsed_sdp = ParsedSessionDescription::Parse(
      session_description_init->hasType()
          ? session_description_init->type().AsString()
          : String(),
      sdp);
  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context, WebFeature::kRTCPeerConnectionSetLocalDescription);
  if (success_callback && error_callback) {
    UseCounter::Count(
        context,
        WebFeature::kRTCPeerConnectionSetLocalDescriptionLegacyCompliant);
  } else {
    if (!success_callback)
      UseCounter::Count(
          context,
          WebFeature::
              kRTCPeerConnectionSetLocalDescriptionLegacyNoSuccessCallback);
    if (!error_callback)
      UseCounter::Count(
          context,
          WebFeature::
              kRTCPeerConnectionSetLocalDescriptionLegacyNoFailureCallback);
  }
  if (!session_description_init->hasType() ||
      session_description_init->type() != V8RTCSdpType::Enum::kRollback) {
    DOMException* exception = checkSdpForStateErrors(context, parsed_sdp);
    if (exception) {
      if (error_callback)
        AsyncCallErrorCallback(context, error_callback, exception);
      return ToResolvedUndefinedPromise(script_state);
    }
  }
  auto* request = MakeGarbageCollected<RTCVoidRequestImpl>(
      GetExecutionContext(), this, success_callback, error_callback);
  peer_handler_->SetLocalDescription(request, std::move(parsed_sdp));
  return ToResolvedUndefinedPromise(script_state);
}

RTCSessionDescription* RTCPeerConnection::localDescription() const {
  return pending_local_description_ ? pending_local_description_
                                    : current_local_description_;
}

RTCSessionDescription* RTCPeerConnection::currentLocalDescription() const {
  return current_local_description_.Get();
}

RTCSessionDescription* RTCPeerConnection::pendingLocalDescription() const {
  return pending_local_description_.Get();
}

ScriptPromise<IDLUndefined> RTCPeerConnection::setRemoteDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit* session_description_init,
    ExceptionState& exception_state) {
  if (closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSignalingStateClosedMessage);
    return EmptyPromise();
  }

  DCHECK(script_state->ContextIsValid());
  ParsedSessionDescription parsed_sdp =
      ParsedSessionDescription::Parse(session_description_init);
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSignalingStateClosedMessage);
    return EmptyPromise();
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionSetRemoteDescription);
  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionSetRemoteDescriptionPromise);

  if (ContainsLegacyRtpDataChannel(session_description_init->sdp())) {
    UseCounter::Count(context, WebFeature::kRTCLegacyRtpDataChannelNegotiated);
  }

  if (ContainsCandidate(session_description_init->sdp()))
    DisableBackForwardCache(context);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* request =
      MakeGarbageCollected<RTCVoidRequestPromiseImpl>(this, resolver);
  peer_handler_->SetRemoteDescription(request, std::move(parsed_sdp));
  return promise;
}

ScriptPromise<IDLUndefined> RTCPeerConnection::setRemoteDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit* session_description_init,
    V8VoidFunction* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback) {
  if (CallErrorCallbackIfSignalingStateClosed(
          ExecutionContext::From(script_state), signaling_state_,
          error_callback)) {
    return ToResolvedUndefinedPromise(script_state);
  }

  DCHECK(script_state->ContextIsValid());
  ParsedSessionDescription parsed_sdp =
      ParsedSessionDescription::Parse(session_description_init);
  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionSetRemoteDescription);
  if (success_callback && error_callback) {
    UseCounter::Count(
        context,
        WebFeature::kRTCPeerConnectionSetRemoteDescriptionLegacyCompliant);
  } else {
    if (!success_callback)
      UseCounter::Count(
          context,
          WebFeature::
              kRTCPeerConnectionSetRemoteDescriptionLegacyNoSuccessCallback);
    if (!error_callback)
      UseCounter::Count(
          context,
          WebFeature::
              kRTCPeerConnectionSetRemoteDescriptionLegacyNoFailureCallback);
  }

  if (ContainsLegacyRtpDataChannel(session_description_init->sdp())) {
    UseCounter::Count(context, WebFeature::kRTCLegacyRtpDataChannelNegotiated);
  }

  if (ContainsCandidate(session_description_init->sdp()))
    DisableBackForwardCache(context);

  if (CallErrorCallbackIfSignalingStateClosed(context, signaling_state_,
                                              error_callback))
    return ToResolvedUndefinedPromise(script_state);

  auto* request = MakeGarbageCollected<RTCVoidRequestImpl>(
      GetExecutionContext(), this, success_callback, error_callback);
  peer_handler_->SetRemoteDescription(request, std::move(parsed_sdp));
  return ToResolvedUndefinedPromise(script_state);
}

RTCSessionDescription* RTCPeerConnection::remoteDescription() const {
  return pending_remote_description_ ? pending_remote_description_
                                     : current_remote_description_;
}

RTCSessionDescription* RTCPeerConnection::currentRemoteDescription() const {
  return current_remote_description_.Get();
}

RTCSessionDescription* RTCPeerConnection::pendingRemoteDescription() const {
  return pending_remote_description_.Get();
}

RTCConfiguration* RTCPeerConnection::getConfiguration(
    ScriptState* script_state) const {
  RTCConfiguration* result = RTCConfiguration::Create();
  const auto& webrtc_configuration = peer_handler_->GetConfiguration();

  switch (webrtc_configuration.type) {
    case webrtc::PeerConnectionInterface::kRelay:
      result->setIceTransportPolicy("relay");
      break;
    case webrtc::PeerConnectionInterface::kAll:
      result->setIceTransportPolicy("all");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  switch (webrtc_configuration.bundle_policy) {
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
      result->setBundlePolicy("max-compat");
      break;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
      result->setBundlePolicy("max-bundle");
      break;
    case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
      result->setBundlePolicy("balanced");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  switch (webrtc_configuration.rtcp_mux_policy) {
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
      result->setRtcpMuxPolicy("negotiate");
      break;
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
      result->setRtcpMuxPolicy("require");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  HeapVector<Member<RTCIceServer>> ice_servers;
  ice_servers.reserve(
      base::checked_cast<wtf_size_t>(webrtc_configuration.servers.size()));
  for (const auto& webrtc_server : webrtc_configuration.servers) {
    auto* ice_server = RTCIceServer::Create();

    Vector<String> url_vector;
    url_vector.reserve(
        base::checked_cast<wtf_size_t>(webrtc_server.urls.size()));
    for (const auto& url : webrtc_server.urls) {
      url_vector.emplace_back(url.c_str());
    }
    auto* urls = MakeGarbageCollected<V8UnionStringOrStringSequence>(
        std::move(url_vector));

    ice_server->setUrls(urls);
    ice_server->setUsername(webrtc_server.username.c_str());
    ice_server->setCredential(webrtc_server.password.c_str());
    ice_servers.push_back(ice_server);
  }
  result->setIceServers(ice_servers);

  if (!webrtc_configuration.certificates.empty()) {
    HeapVector<blink::Member<RTCCertificate>> certificates;
    certificates.reserve(base::checked_cast<wtf_size_t>(
        webrtc_configuration.certificates.size()));
    for (const auto& webrtc_certificate : webrtc_configuration.certificates) {
      certificates.emplace_back(
          MakeGarbageCollected<RTCCertificate>(webrtc_certificate));
    }
    result->setCertificates(certificates);
  }

  result->setIceCandidatePoolSize(webrtc_configuration.ice_candidate_pool_size);

  const auto* context = ExecutionContext::From(script_state);
  if (RuntimeEnabledFeatures::RtcAudioJitterBufferMaxPacketsEnabled(context)) {
    int audio_jitter_buffer_max_packets =
        webrtc_configuration.audio_jitter_buffer_max_packets;
    result->setRtcAudioJitterBufferMaxPackets(
        static_cast<int32_t>(audio_jitter_buffer_max_packets));
    result->setRtcAudioJitterBufferFastAccelerate(
        webrtc_configuration.audio_jitter_buffer_fast_accelerate);
    int audio_jitter_buffer_min_delay_ms =
        webrtc_configuration.audio_jitter_buffer_min_delay_ms;
    result->setRtcAudioJitterBufferMinDelayMs(
        static_cast<int32_t>(audio_jitter_buffer_min_delay_ms));
  }
  result->setEncodedInsertableStreams(
      peer_handler_->encoded_insertable_streams());

  return result;
}

void RTCPeerConnection::setConfiguration(
    ScriptState* script_state,
    const RTCConfiguration* rtc_configuration,
    ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, &exception_state))
    return;

  webrtc::PeerConnectionInterface::RTCConfiguration configuration =
      ParseConfiguration(ExecutionContext::From(script_state),
                         rtc_configuration, &exception_state);

  if (exception_state.HadException())
    return;

  if (peer_handler_->encoded_insertable_streams() !=
      rtc_configuration->encodedInsertableStreams()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Attempted to modify the PeerConnection's "
        "configuration in an unsupported way.");
  }

  webrtc::RTCErrorType error = peer_handler_->SetConfiguration(configuration);
  if (error == webrtc::RTCErrorType::NONE) {
    return;
  } else if (error == webrtc::RTCErrorType::INVALID_MODIFICATION) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Attempted to modify the PeerConnection's configuration in an "
        "unsupported way.");
  } else if (error == webrtc::RTCErrorType::SYNTAX_ERROR) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The given configuration has a syntax error.");
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Could not update the PeerConnection with the given configuration.");
  }
}

ScriptPromise<RTCCertificate> RTCPeerConnection::generateCertificate(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* keygen_algorithm,
    ExceptionState& exception_state) {
  // Normalize |keygenAlgorithm| with WebCrypto, making sure it is a recognized
  // AlgorithmIdentifier.
  WebCryptoAlgorithm crypto_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), keygen_algorithm,
                          kWebCryptoOperationGenerateKey, crypto_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  // Check if |keygenAlgorithm| contains the optional DOMTimeStamp |expires|
  // attribute.
  std::optional<DOMTimeStamp> expires;
  if (keygen_algorithm->IsObject()) {
    Dictionary keygen_algorithm_dict(script_state->GetIsolate(),
                                     keygen_algorithm->GetAsObject().V8Value(),
                                     exception_state);
    if (exception_state.HadException())
      return EmptyPromise();

    bool has_expires =
        keygen_algorithm_dict.HasProperty("expires", exception_state);
    if (exception_state.HadException())
      return EmptyPromise();

    if (has_expires) {
      v8::Local<v8::Value> expires_value;
      keygen_algorithm_dict.Get("expires", expires_value);
      if (expires_value->IsNumber()) {
        double expires_double =
            expires_value
                ->ToNumber(script_state->GetIsolate()->GetCurrentContext())
                .ToLocalChecked()
                ->Value();
        if (expires_double >= 0) {
          expires = static_cast<DOMTimeStamp>(expires_double);
        } else {
          exception_state.ThrowTypeError(
              "Negative value for expires attribute.");
          return EmptyPromise();
        }
      } else {
        exception_state.ThrowTypeError("Invalid type for expires attribute.");
        return EmptyPromise();
      }
    }
  }

  // Convert from WebCrypto representation to recognized WebRTCKeyParams. WebRTC
  // supports a small subset of what are valid AlgorithmIdentifiers.
  const char* unsupported_params_string =
      "The 1st argument provided is an AlgorithmIdentifier with a supported "
      "algorithm name, but the parameters are not supported.";
  std::optional<rtc::KeyParams> key_params;
  switch (crypto_algorithm.Id()) {
    case kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5: {
      // name: "RSASSA-PKCS1-v1_5"
      std::optional<uint32_t> public_exponent =
          crypto_algorithm.RsaHashedKeyGenParams()->PublicExponentAsU32();
      unsigned modulus_length =
          crypto_algorithm.RsaHashedKeyGenParams()->ModulusLengthBits();
      // Parameters must fit in int to be passed to rtc::KeyParams::RSA. The
      // only recognized "hash" is "SHA-256".
      // TODO(bugs.webrtc.org/364338811): deprecate 1024 bit keys.
      if (public_exponent &&
          base::IsValueInRangeForNumericType<int>(*public_exponent) &&
          base::IsValueInRangeForNumericType<int>(modulus_length) &&
          crypto_algorithm.RsaHashedKeyGenParams()->GetHash().Id() ==
              kWebCryptoAlgorithmIdSha256) {
        key_params =
            rtc::KeyParams::RSA(base::checked_cast<int>(modulus_length),
                                base::checked_cast<int>(*public_exponent));
      } else {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          unsupported_params_string);
        return EmptyPromise();
      }
      break;
    }
    case kWebCryptoAlgorithmIdEcdsa:
      // name: "ECDSA"
      // The only recognized "namedCurve" is "P-256".
      if (crypto_algorithm.EcKeyGenParams()->NamedCurve() ==
          kWebCryptoNamedCurveP256) {
        key_params = rtc::KeyParams::ECDSA(rtc::EC_NIST_P256);
      } else {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          unsupported_params_string);
        return EmptyPromise();
      }
      break;
    default:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "The 1st argument provided is an "
                                        "AlgorithmIdentifier, but the "
                                        "algorithm is not supported.");
      return EmptyPromise();
  }
  DCHECK(key_params.has_value());
  MeasureGenerateCertificateKeyType(key_params);

  auto certificate_generator = std::make_unique<RTCCertificateGenerator>();

  // |keyParams| was successfully constructed, but does the certificate
  // generator support these parameters?
  if (!certificate_generator->IsSupportedKeyParams(key_params.value())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      unsupported_params_string);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<RTCCertificate>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // Helper closure callback for RTCPeerConnection::generateCertificate.
  auto completion_callback =
      WTF::BindOnce(RTCPeerConnection::GenerateCertificateCompleted,
                    WrapPersistent(resolver));

  // Generate certificate. The |certificateObserver| will resolve the promise
  // asynchronously upon completion. The observer will manage its own
  // destruction as well as the resolver's destruction.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalMedia);
  if (!expires) {
    certificate_generator->GenerateCertificate(
        key_params.value(), std::move(completion_callback),
        *ExecutionContext::From(script_state), task_runner);
  } else {
    certificate_generator->GenerateCertificateWithExpiration(
        key_params.value(), expires.value(), std::move(completion_callback),
        *ExecutionContext::From(script_state), task_runner);
  }

  return promise;
}

ScriptPromise<IDLUndefined> RTCPeerConnection::addIceCandidate(
    ScriptState* script_state,
    const RTCIceCandidateInit* candidate,
    ExceptionState& exception_state) {
  DCHECK(script_state->ContextIsValid());
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSignalingStateClosedMessage);
    return EmptyPromise();
  }

  if (candidate->hasCandidate() && candidate->candidate().empty()) {
    // Temporary mitigation to avoid throwing an exception when candidate is
    // empty or nothing was passed.
    // TODO(crbug.com/978582): Remove this mitigation when the WebRTC layer
    // handles the empty candidate field or the null candidate correctly.
    return ToResolvedUndefinedPromise(script_state);
  }

  RTCIceCandidatePlatform* platform_candidate =
      ConvertToRTCIceCandidatePlatform(ExecutionContext::From(script_state),
                                       candidate);

  if (IsIceCandidateMissingSdpMidAndMLineIndex(candidate)) {
    exception_state.ThrowTypeError(
        "Candidate missing values for both sdpMid and sdpMLineIndex");
    return EmptyPromise();
  }

  DisableBackForwardCache(GetExecutionContext());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* request =
      MakeGarbageCollected<RTCVoidRequestPromiseImpl>(this, resolver);
  peer_handler_->AddIceCandidate(request, std::move(platform_candidate));
  return promise;
}

ScriptPromise<IDLUndefined> RTCPeerConnection::addIceCandidate(
    ScriptState* script_state,
    const RTCIceCandidateInit* candidate,
    V8VoidFunction* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback,
    ExceptionState& exception_state) {
  DCHECK(script_state->ContextIsValid());
  DCHECK(success_callback);
  DCHECK(error_callback);

  if (CallErrorCallbackIfSignalingStateClosed(
          ExecutionContext::From(script_state), signaling_state_,
          error_callback))
    return ToResolvedUndefinedPromise(script_state);

  if (IsIceCandidateMissingSdpMidAndMLineIndex(candidate)) {
    exception_state.ThrowTypeError(
        "Candidate missing values for both sdpMid and sdpMLineIndex");
    return EmptyPromise();
  }

  RTCIceCandidatePlatform* platform_candidate =
      ConvertToRTCIceCandidatePlatform(ExecutionContext::From(script_state),
                                       candidate);

  // Temporary mitigation to avoid throwing an exception when candidate is
  // empty.
  // TODO(crbug.com/978582): Remove this mitigation when the WebRTC layer
  // handles the empty candidate field or the null candidate correctly.
  if (platform_candidate->Candidate().empty())
    return ToResolvedUndefinedPromise(script_state);

  DisableBackForwardCache(GetExecutionContext());

  auto* request = MakeGarbageCollected<RTCVoidRequestImpl>(
      GetExecutionContext(), this, success_callback, error_callback);
  peer_handler_->AddIceCandidate(request, std::move(platform_candidate));
  return ToResolvedUndefinedPromise(script_state);
}

String RTCPeerConnection::signalingState() const {
  return String(
      webrtc::PeerConnectionInterface::AsString(signaling_state_).data());
}

String RTCPeerConnection::iceGatheringState() const {
  return String(
      webrtc::PeerConnectionInterface::AsString(ice_gathering_state_).data());
}

String RTCPeerConnection::iceConnectionState() const {
  if (closed_) {
    return "closed";
  }
  return String(
      webrtc::PeerConnectionInterface::AsString(ice_connection_state_).data());
}

String RTCPeerConnection::connectionState() const {
  if (closed_) {
    return "closed";
  }
  return String(
      webrtc::PeerConnectionInterface::AsString(peer_connection_state_).data());
}

std::optional<bool> RTCPeerConnection::canTrickleIceCandidates() const {
  if (closed_ || !remoteDescription()) {
    return std::nullopt;
  }
  webrtc::PeerConnectionInterface* native_connection =
      peer_handler_->NativePeerConnection();
  if (!native_connection) {
    return std::nullopt;
  }
  std::optional<bool> can_trickle =
      native_connection->can_trickle_ice_candidates();
  if (!can_trickle) {
    return std::nullopt;
  }
  return *can_trickle;
}

void RTCPeerConnection::restartIce() {
  if (closed_)
    return;
  peer_handler_->RestartIce();
}

void RTCPeerConnection::addStream(ScriptState* script_state,
                                  MediaStream* stream,
                                  ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, &exception_state))
    return;

  MediaStreamVector streams;
  streams.push_back(stream);
  for (const auto& track : stream->getTracks()) {
    addTrack(track, streams, IGNORE_EXCEPTION);
  }

  stream->RegisterObserver(this);
}

void RTCPeerConnection::removeStream(MediaStream* stream,
                                     ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, &exception_state))
    return;
  for (const auto& track : stream->getTracks()) {
    auto* sender = FindSenderForTrackAndStream(track, stream);
    if (!sender)
      continue;
    removeTrack(sender, IGNORE_EXCEPTION);
  }
  stream->UnregisterObserver(this);
}

MediaStreamVector RTCPeerConnection::getLocalStreams() const {
  MediaStreamVector local_streams;
  for (const auto& transceiver : transceivers_) {
    if (!transceiver->DirectionHasSend())
      continue;
    for (const auto& stream : transceiver->sender()->streams()) {
      if (!local_streams.Contains(stream))
        local_streams.push_back(stream);
    }
  }
  return local_streams;
}

MediaStreamVector RTCPeerConnection::getRemoteStreams() const {
  MediaStreamVector remote_streams;
  for (const auto& transceiver : transceivers_) {
    if (!transceiver->DirectionHasRecv())
      continue;
    for (const auto& stream : transceiver->receiver()->streams()) {
      if (!remote_streams.Contains(stream))
        remote_streams.push_back(stream);
    }
  }
  return remote_streams;
}

ScriptPromise<RTCStatsReport> RTCPeerConnection::getStats(
    ScriptState* script_state,
    MediaStreamTrack* selector,
    ExceptionState& exception_state) {
  if (!selector) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    UseCounter::Count(context, WebFeature::kRTCPeerConnectionGetStats);

    if (!peer_handler_) {
      LOG(ERROR) << "Internal error: peer_handler_ has been discarded";
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Internal error: release in progress");
      return EmptyPromise();
    }
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<RTCStatsReport>>(
            script_state, exception_state.GetContext());
    auto promise = resolver->Promise();
    if (peer_handler_unregistered_) {
      LOG(ERROR) << "Internal error: context is destroyed";
      // This is needed to have the resolver release its internal resources
      // while leaving the associated promise pending as specified.
      resolver->Detach();
    } else {
      peer_handler_->GetStats(WTF::BindOnce(WebRTCStatsReportCallbackResolver,
                                            WrapPersistent(resolver)));
    }
    return promise;
  }

  // Find the sender or receiver that represent the selector.
  size_t track_uses = 0u;
  RTCRtpSender* track_sender = nullptr;
  for (const auto& sender : rtp_senders_) {
    if (sender->track() == selector) {
      ++track_uses;
      track_sender = sender;
    }
  }
  RTCRtpReceiver* track_receiver = nullptr;
  for (const auto& receiver : rtp_receivers_) {
    if (receiver->track() == selector) {
      ++track_uses;
      track_receiver = receiver;
    }
  }
  if (track_uses == 0u) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "There is no sender or receiver for the track.");
    return EmptyPromise();
  }
  if (track_uses > 1u) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "There are more than one sender or receiver for the track.");
    return EmptyPromise();
  }
  // There is just one use of the track, a sender or receiver.
  if (track_sender) {
    DCHECK(!track_receiver);
    return track_sender->getStats(script_state);
  }
  DCHECK(track_receiver);
  return track_receiver->getStats(script_state);
}

const HeapVector<Member<RTCRtpTransceiver>>&
RTCPeerConnection::getTransceivers() const {
  return transceivers_;
}

const HeapVector<Member<RTCRtpSender>>& RTCPeerConnection::getSenders() const {
  return rtp_senders_;
}

const HeapVector<Member<RTCRtpReceiver>>& RTCPeerConnection::getReceivers()
    const {
  return rtp_receivers_;
}

RtpContributingSourceCache& RTCPeerConnection::GetRtpContributingSourceCache() {
  DCHECK(rtp_contributing_source_cache_.has_value());
  return rtp_contributing_source_cache_.value();
}

std::optional<webrtc::RtpTransceiverInit> ValidateRtpTransceiverInit(
    ExecutionContext* execution_context,
    ExceptionState& exception_state,
    const RTCRtpTransceiverInit* init,
    const String kind) {
  auto webrtc_init = ToRtpTransceiverInit(execution_context, init, kind);
  // Validate sendEncodings.
  for (auto& encoding : webrtc_init.send_encodings) {
    if (encoding.rid.length() > 16) {
      exception_state.ThrowTypeError("Illegal length of rid");
      return std::nullopt;
    }
    // Allowed characters: a-z 0-9 _ and -
    if (encoding.rid.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM"
                                       "NOPQRSTUVWXYZ0123456789-_") !=
        std::string::npos) {
      exception_state.ThrowTypeError("Illegal character in rid");
      return std::nullopt;
    }
  }
  return webrtc_init;
}

RTCRtpTransceiver* RTCPeerConnection::addTransceiver(
    const V8UnionMediaStreamTrackOrString* track_or_kind,
    const RTCRtpTransceiverInit* init,
    ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_,
                                           &exception_state)) {
    return nullptr;
  }
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>> result =
      webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION);
  switch (track_or_kind->GetContentType()) {
    case V8UnionMediaStreamTrackOrString::ContentType::kMediaStreamTrack: {
      MediaStreamTrack* track = track_or_kind->GetAsMediaStreamTrack();

      auto webrtc_init = ValidateRtpTransceiverInit(
          GetExecutionContext(), exception_state, init, track->kind());
      if (!webrtc_init) {
        return nullptr;
      }

      RegisterTrack(track);
      result = peer_handler_->AddTransceiverWithTrack(track->Component(),
                                                      std::move(*webrtc_init));
      break;
    }
    case V8UnionMediaStreamTrackOrString::ContentType::kString: {
      const String& kind_string = track_or_kind->GetAsString();
      // TODO(hbos): Make cricket::MediaType an allowed identifier in
      // rtc_peer_connection.cc and use that instead of a boolean.
      String kind;
      if (kind_string == "audio") {
        kind = webrtc::MediaStreamTrackInterface::kAudioKind;
      } else if (kind_string == "video") {
        kind = webrtc::MediaStreamTrackInterface::kVideoKind;
      } else {
        exception_state.ThrowTypeError(
            "The argument provided as parameter 1 is not a valid "
            "MediaStreamTrack kind ('audio' or 'video').");
        return nullptr;
      }

      auto webrtc_init = ValidateRtpTransceiverInit(
          GetExecutionContext(), exception_state, init, kind);
      if (!webrtc_init) {
        return nullptr;
      }

      result = peer_handler_->AddTransceiverWithKind(std::move(kind),
                                                     std::move(*webrtc_init));
      break;
    }
  }
  if (!result.ok()) {
    ThrowExceptionFromRTCError(result.error(), exception_state);
    return nullptr;
  }
  return CreateOrUpdateTransceiver(result.MoveValue());
}

RTCRtpSender* RTCPeerConnection::addTrack(MediaStreamTrack* track,
                                          MediaStreamVector streams,
                                          ExceptionState& exception_state) {
  DCHECK(track);
  DCHECK(track->Component());
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, &exception_state))
    return nullptr;
  for (const auto& sender : rtp_senders_) {
    if (sender->track() == track) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "A sender already exists for the track.");
      return nullptr;
    }
  }

  MediaStreamDescriptorVector descriptors(streams.size());
  for (wtf_size_t i = 0; i < streams.size(); ++i) {
    descriptors[i] = streams[i]->Descriptor();
  }
  webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
      error_or_transceiver =
          peer_handler_->AddTrack(track->Component(), descriptors);
  if (!error_or_transceiver.ok()) {
    ThrowExceptionFromRTCError(error_or_transceiver.error(), exception_state);
    return nullptr;
  }

  auto platform_transceiver = error_or_transceiver.MoveValue();

  // The track must be known to the peer connection when performing
  // CreateOrUpdateSender() below.
  RegisterTrack(track);

  auto stream_ids = platform_transceiver->Sender()->StreamIds();
  RTCRtpTransceiver* transceiver =
      CreateOrUpdateTransceiver(std::move(platform_transceiver));
  RTCRtpSender* sender = transceiver->sender();
  // Newly created senders have no streams set, we have to set it ourselves.
  sender->set_streams(streams);
  // The native sender may have filtered out duplicates.
  DCHECK_LE(stream_ids.size(), streams.size());
  return sender;
}

void RTCPeerConnection::removeTrack(RTCRtpSender* sender,
                                    ExceptionState& exception_state) {
  DCHECK(sender);
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, &exception_state))
    return;
  auto it = FindSender(*sender->web_sender());
  if (it == rtp_senders_.end()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The sender was not created by this peer connection.");
    return;
  }

  auto error_or_transceiver = peer_handler_->RemoveTrack(sender->web_sender());
  if (!error_or_transceiver.ok()) {
    ThrowExceptionFromRTCError(error_or_transceiver.error(), exception_state);
    return;
  }
  if (!error_or_transceiver.value()) {
    // There is no transceiver to update - the operation was cancelled, such
    // as if the transceiver was rolled back.
    return;
  }
  CreateOrUpdateTransceiver(error_or_transceiver.MoveValue());
}

RTCSctpTransport* RTCPeerConnection::sctp() const {
  return sctp_transport_.Get();
}

RTCDataChannel* RTCPeerConnection::createDataChannel(
    ScriptState* script_state,
    String label,
    const RTCDataChannelInit* data_channel_dict,
    ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, &exception_state))
    return nullptr;

  webrtc::DataChannelInit init;
  // TODO(jiayl): remove the deprecated reliable field once Libjingle is updated
  // to handle that.
  init.reliable = false;
  init.ordered = data_channel_dict->ordered();
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (data_channel_dict->hasMaxPacketLifeTime()) {
    UseCounter::Count(
        context,
        WebFeature::kRTCPeerConnectionCreateDataChannelMaxPacketLifeTime);
    init.maxRetransmitTime = data_channel_dict->maxPacketLifeTime();
  }
  if (data_channel_dict->hasMaxRetransmits()) {
    UseCounter::Count(
        context, WebFeature::kRTCPeerConnectionCreateDataChannelMaxRetransmits);
    init.maxRetransmits = data_channel_dict->maxRetransmits();
  }
  init.protocol = data_channel_dict->protocol().Utf8();
  init.negotiated = data_channel_dict->negotiated();
  if (data_channel_dict->hasId())
    init.id = data_channel_dict->id();
  if (data_channel_dict->hasPriority()) {
    init.priority = [&] {
      if (data_channel_dict->priority() == "very-low") {
        return webrtc::PriorityValue(webrtc::Priority::kVeryLow);
      }
      if (data_channel_dict->priority() == "low") {
        return webrtc::PriorityValue(webrtc::Priority::kLow);
      }
      if (data_channel_dict->priority() == "medium") {
        return webrtc::PriorityValue(webrtc::Priority::kMedium);
      }
      if (data_channel_dict->priority() == "high") {
        return webrtc::PriorityValue(webrtc::Priority::kHigh);
      }
      NOTREACHED();
    }();
  }
  // Checks from WebRTC specification section 6.1
  // If [[DataChannelLabel]] is longer than 65535 bytes, throw a
  // TypeError.
  if (label.Utf8().length() > 65535) {
    exception_state.ThrowTypeError("RTCDataChannel label too long");
    return nullptr;
  }
  // If [[DataChannelProtocol]] is longer than 65535 bytes long, throw a
  // TypeError.
  if (init.protocol.length() > 65535) {
    exception_state.ThrowTypeError("RTCDataChannel protocol too long");
    return nullptr;
  }
  // If [[Negotiated]] is true and [[DataChannelId]] is null, throw a TypeError.
  if (init.negotiated && init.id == -1) {
    exception_state.ThrowTypeError(
        "RTCDataChannel must have id set if negotiated is true");
    return nullptr;
  }
  // If both [[MaxPacketLifeTime]] and [[MaxRetransmits]] attributes are set
  // (not null), throw a TypeError.
  if (init.maxRetransmitTime >= 0 && init.maxRetransmits >= 0) {
    exception_state.ThrowTypeError(
        "RTCDataChannel cannot have both max retransmits and max lifetime");
    return nullptr;
  }
  // If [[DataChannelId]] is equal to 65535, which is greater than the maximum
  // allowed ID of 65534 but still qualifies as an unsigned short, throw a
  // TypeError.
  if (init.id >= 65535) {
    exception_state.ThrowTypeError("RTCDataChannel cannot have id > 65534");
    return nullptr;
  }
  // Further checks of DataChannelId are done in the webrtc layer.

  rtc::scoped_refptr<webrtc::DataChannelInterface> webrtc_channel =
      peer_handler_->CreateDataChannel(label, init);
  if (!webrtc_channel) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "RTCDataChannel creation failed");
    return nullptr;
  }
  auto* channel = MakeGarbageCollected<RTCDataChannel>(
      GetExecutionContext(), std::move(webrtc_channel));

  return channel;
}

MediaStreamTrack* RTCPeerConnection::GetTrackForTesting(
    MediaStreamComponent* component) const {
  auto it = tracks_.find(component);
  if (it != tracks_.end()) {
    return it->value.Get();
  } else {
    return nullptr;
  }
}

RTCRtpSender* RTCPeerConnection::FindSenderForTrackAndStream(
    MediaStreamTrack* track,
    MediaStream* stream) {
  for (const auto& rtp_sender : rtp_senders_) {
    if (rtp_sender->track() == track) {
      auto streams = rtp_sender->streams();
      if (streams.size() == 1u && streams[0] == stream)
        return rtp_sender.Get();
    }
  }
  return nullptr;
}

HeapVector<Member<RTCRtpSender>>::iterator RTCPeerConnection::FindSender(
    const RTCRtpSenderPlatform& web_sender) {
  return base::ranges::find_if(rtp_senders_, [&](const auto& sender) {
    return sender->web_sender()->Id() == web_sender.Id();
  });
}

HeapVector<Member<RTCRtpReceiver>>::iterator RTCPeerConnection::FindReceiver(
    const RTCRtpReceiverPlatform& platform_receiver) {
  return base::ranges::find_if(rtp_receivers_, [&](const auto& receiver) {
    return receiver->platform_receiver()->Id() == platform_receiver.Id();
  });
}

HeapVector<Member<RTCRtpTransceiver>>::iterator
RTCPeerConnection::FindTransceiver(
    const RTCRtpTransceiverPlatform& platform_transceiver) {
  return base::ranges::find_if(transceivers_, [&](const auto& transceiver) {
    return transceiver->platform_transceiver()->Id() ==
           platform_transceiver.Id();
  });
}

RTCRtpSender* RTCPeerConnection::CreateOrUpdateSender(
    std::unique_ptr<RTCRtpSenderPlatform> rtp_sender_platform,
    String kind) {
  // The track corresponding to |web_track| must already be known to us by being
  // in |tracks_|, as is a prerequisite of CreateOrUpdateSender().
  MediaStreamComponent* component = rtp_sender_platform->Track();
  MediaStreamTrack* track = nullptr;
  if (component) {
    track = tracks_.at(component);
    DCHECK(track);
  }

  // Create or update sender. If the web sender has stream IDs the sender's
  // streams need to be set separately outside of this method.
  auto sender_it = FindSender(*rtp_sender_platform);
  RTCRtpSender* sender;
  if (sender_it == rtp_senders_.end()) {
    // Create new sender (with empty stream set).
    sender = MakeGarbageCollected<RTCRtpSender>(
        this, std::move(rtp_sender_platform), kind, track, MediaStreamVector(),
        encoded_insertable_streams_,
        GetExecutionContext()->GetTaskRunner(TaskType::kInternalMedia));
    rtp_senders_.push_back(sender);
  } else {
    // Update existing sender (not touching the stream set).
    sender = *sender_it;
    DCHECK_EQ(sender->web_sender()->Id(), rtp_sender_platform->Id());
    sender->SetTrack(track);
  }
  sender->set_transport(CreateOrUpdateDtlsTransport(
      sender->web_sender()->DtlsTransport(),
      sender->web_sender()->DtlsTransportInformation()));
  return sender;
}

RTCRtpReceiver* RTCPeerConnection::CreateOrUpdateReceiver(
    std::unique_ptr<RTCRtpReceiverPlatform> platform_receiver) {
  auto receiver_it = FindReceiver(*platform_receiver);
  // Create track.
  MediaStreamTrack* track;
  if (receiver_it == rtp_receivers_.end()) {
    track = MakeGarbageCollected<MediaStreamTrackImpl>(
        GetExecutionContext(), platform_receiver->Track());
    RegisterTrack(track);
  } else {
    track = (*receiver_it)->track();
  }

  // Create or update receiver. If the web receiver has stream IDs the
  // receiver's streams need to be set separately outside of this method.
  RTCRtpReceiver* receiver;
  if (receiver_it == rtp_receivers_.end()) {
    // Create new receiver.
    receiver = MakeGarbageCollected<RTCRtpReceiver>(
        this, std::move(platform_receiver), track, MediaStreamVector(),
        encoded_insertable_streams_,
        GetExecutionContext()->GetTaskRunner(TaskType::kInternalMedia));
    // Receiving tracks should be muted by default. SetReadyState() propagates
    // the related state changes to ensure it is muted on all layers. It also
    // fires events - which is not desired - but because they fire synchronously
    // there are no listeners to detect this so this is indistinguishable from
    // having constructed the track in an already muted state.
    receiver->track()->Component()->Source()->SetReadyState(
        MediaStreamSource::kReadyStateMuted);
    rtp_receivers_.push_back(receiver);
  } else {
    // Update existing receiver is a no-op.
    receiver = *receiver_it;
    DCHECK_EQ(receiver->platform_receiver()->Id(), platform_receiver->Id());
    DCHECK_EQ(receiver->track(), track);  // Its track should never change.
  }
  receiver->set_transport(CreateOrUpdateDtlsTransport(
      receiver->platform_receiver()->DtlsTransport(),
      receiver->platform_receiver()->DtlsTransportInformation()));
  return receiver;
}

RTCRtpTransceiver* RTCPeerConnection::CreateOrUpdateTransceiver(
    std::unique_ptr<RTCRtpTransceiverPlatform> platform_transceiver) {
  String kind = (platform_transceiver->Receiver()->Track()->GetSourceType() ==
                 MediaStreamSource::kTypeAudio)
                    ? "audio"
                    : "video";
  RTCRtpSender* sender =
      CreateOrUpdateSender(platform_transceiver->Sender(), kind);
  RTCRtpReceiver* receiver =
      CreateOrUpdateReceiver(platform_transceiver->Receiver());

  RTCRtpTransceiver* transceiver;
  auto transceiver_it = FindTransceiver(*platform_transceiver);
  if (transceiver_it == transceivers_.end()) {
    // Create new tranceiver.
    transceiver = MakeGarbageCollected<RTCRtpTransceiver>(
        this, std::move(platform_transceiver), sender, receiver);
    transceivers_.push_back(transceiver);
  } else {
    // Update existing transceiver.
    transceiver = *transceiver_it;
    // The sender and receiver have already been updated above.
    DCHECK_EQ(transceiver->sender(), sender);
    DCHECK_EQ(transceiver->receiver(), receiver);
    transceiver->UpdateMembers();
  }
  return transceiver;
}

RTCDtlsTransport* RTCPeerConnection::CreateOrUpdateDtlsTransport(
    rtc::scoped_refptr<webrtc::DtlsTransportInterface> native_transport,
    const webrtc::DtlsTransportInformation& information) {
  if (!native_transport.get()) {
    return nullptr;
  }
  auto& transport = dtls_transports_by_native_transport_
                        .insert(native_transport.get(), nullptr)
                        .stored_value->value;
  if (!transport) {
    RTCIceTransport* ice_transport =
        CreateOrUpdateIceTransport(native_transport->ice_transport());
    transport = MakeGarbageCollected<RTCDtlsTransport>(
        GetExecutionContext(), std::move(native_transport), ice_transport);
  }
  transport->ChangeState(information);
  return transport.Get();
}

RTCIceTransport* RTCPeerConnection::CreateOrUpdateIceTransport(
    rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport) {
  if (!ice_transport.get()) {
    return nullptr;
  }
  auto& transport =
      ice_transports_by_native_transport_.insert(ice_transport.get(), nullptr)
          .stored_value->value;
  if (!transport) {
    transport = RTCIceTransport::Create(GetExecutionContext(),
                                        std::move(ice_transport), this);
  }
  return transport.Get();
}

RTCDTMFSender* RTCPeerConnection::createDTMFSender(
    MediaStreamTrack* track,
    ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, &exception_state))
    return nullptr;
  if (track->kind() != "audio") {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "track.kind is not 'audio'.");
    return nullptr;
  }
  RTCRtpSender* found_rtp_sender = nullptr;
  for (const auto& rtp_sender : rtp_senders_) {
    if (rtp_sender->track() == track) {
      found_rtp_sender = rtp_sender;
      break;
    }
  }
  if (!found_rtp_sender) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "No RTCRtpSender is available for the track provided.");
    return nullptr;
  }
  RTCDTMFSender* dtmf_sender = found_rtp_sender->dtmf();
  if (!dtmf_sender) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Unable to create DTMF sender for track");
    return nullptr;
  }
  return dtmf_sender;
}

void RTCPeerConnection::close() {
  suppress_events_ = true;
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    return;
  }
  CloseInternal();
}

void RTCPeerConnection::RegisterTrack(MediaStreamTrack* track) {
  DCHECK(track);
  tracks_.insert(track->Component(), track);
}

void RTCPeerConnection::NoteSdpCreated(const RTCSessionDescriptionInit& desc) {
  if (desc.type() == "offer") {
    last_offer_ = desc.sdp();
  } else if (desc.type() == "answer") {
    last_answer_ = desc.sdp();
  }
}

void RTCPeerConnection::OnStreamAddTrack(MediaStream* stream,
                                         MediaStreamTrack* track,
                                         ExceptionState& exception_state) {
  MediaStreamVector streams;
  streams.push_back(stream);
  addTrack(track, streams, exception_state);
}

void RTCPeerConnection::OnStreamRemoveTrack(MediaStream* stream,
                                            MediaStreamTrack* track,
                                            ExceptionState& exception_state) {
  auto* sender = FindSenderForTrackAndStream(track, stream);
  if (sender) {
    removeTrack(sender, exception_state);
  }
}

void RTCPeerConnection::NegotiationNeeded() {
  DCHECK(!closed_);
  MaybeDispatchEvent(Event::Create(event_type_names::kNegotiationneeded));
}

void RTCPeerConnection::DidGenerateICECandidate(
    RTCIceCandidatePlatform* platform_candidate) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  DCHECK(platform_candidate);
  RTCIceCandidate* ice_candidate = RTCIceCandidate::Create(platform_candidate);
  MaybeDispatchEvent(RTCPeerConnectionIceEvent::Create(ice_candidate));
}

void RTCPeerConnection::DidFailICECandidate(const String& address,
                                            std::optional<uint16_t> port,
                                            const String& host_candidate,
                                            const String& url,
                                            int error_code,
                                            const String& error_text) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  MaybeDispatchEvent(RTCPeerConnectionIceErrorEvent::Create(
      address, port, host_candidate, url, error_code, error_text));
}

void RTCPeerConnection::DidChangeSessionDescriptions(
    RTCSessionDescriptionPlatform* pending_local_description,
    RTCSessionDescriptionPlatform* current_local_description,
    RTCSessionDescriptionPlatform* pending_remote_description,
    RTCSessionDescriptionPlatform* current_remote_description) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  pending_local_description_ =
      pending_local_description
          ? RTCSessionDescription::Create(pending_local_description)
          : nullptr;
  current_local_description_ =
      current_local_description
          ? RTCSessionDescription::Create(current_local_description)
          : nullptr;
  pending_remote_description_ =
      pending_remote_description
          ? RTCSessionDescription::Create(pending_remote_description)
          : nullptr;
  current_remote_description_ =
      current_remote_description
          ? RTCSessionDescription::Create(current_remote_description)
          : nullptr;
}

void RTCPeerConnection::DidChangeIceGatheringState(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  ChangeIceGatheringState(new_state);
}

void RTCPeerConnection::DidChangePeerConnectionState(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  ChangePeerConnectionState(new_state);
}

void RTCPeerConnection::DidModifySctpTransport(
    WebRTCSctpTransportSnapshot snapshot) {
  if (!snapshot.transport) {
    sctp_transport_ = nullptr;
    return;
  }
  if (!sctp_transport_ ||
      sctp_transport_->native_transport() != snapshot.transport) {
    sctp_transport_ = MakeGarbageCollected<RTCSctpTransport>(
        GetExecutionContext(), snapshot.transport);
  }
  if (!sctp_transport_->transport() ||
      sctp_transport_->transport()->native_transport() !=
          snapshot.sctp_transport_state.dtls_transport()) {
    sctp_transport_->SetTransport(CreateOrUpdateDtlsTransport(
        snapshot.sctp_transport_state.dtls_transport(),
        snapshot.dtls_transport_state));
  }
  sctp_transport_->ChangeState(snapshot.sctp_transport_state);
}

void RTCPeerConnection::DidModifyTransceivers(
    webrtc::PeerConnectionInterface::SignalingState signaling_state,
    Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> platform_transceivers,
    Vector<uintptr_t> removed_transceiver_ids,
    bool is_remote_description_or_rollback) {
  HeapVector<Member<MediaStreamTrack>> mute_tracks;
  HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>
      remove_list;
  HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>> add_list;
  HeapVector<Member<RTCRtpTransceiver>> track_events;
  MediaStreamVector previous_streams = getRemoteStreams();
  // Remove transceivers and update their states to reflect that they are
  // necessarily stopped.
  for (auto id : removed_transceiver_ids) {
    for (auto it = transceivers_.begin(); it != transceivers_.end(); ++it) {
      if ((*it)->platform_transceiver()->Id() == id) {
        // All streams are removed on stop, update `remove_list` if necessary.
        auto* track = (*it)->receiver()->track();
        for (const auto& stream : (*it)->receiver()->streams()) {
          if (stream->getTracks().Contains(track)) {
            remove_list.push_back(std::make_pair(stream, track));
          }
        }
        (*it)->OnTransceiverStopped();
        transceivers_.erase(it);
        break;
      }
    }
  }
  for (auto& platform_transceiver : platform_transceivers) {
    auto it = FindTransceiver(*platform_transceiver);
    bool previously_had_recv =
        (it != transceivers_.end()) ? (*it)->FiredDirectionHasRecv() : false;
    RTCRtpTransceiver* transceiver =
        CreateOrUpdateTransceiver(std::move(platform_transceiver));

    size_t add_list_prev_size = add_list.size();
    // "Set the associated remote streams".
    // https://w3c.github.io/webrtc-pc/#set-associated-remote-streams
    SetAssociatedMediaStreams(
        transceiver->receiver(),
        transceiver->platform_transceiver()->Receiver()->StreamIds(),
        &remove_list, &add_list);
    // The transceiver is now up-to-date. Check if the receiver's track is now
    // considered added or removed (though a receiver's track is never truly
    // removed). A track event indicates either that the track was "added" in
    // the sense that FiredDirectionHasRecv() changed, or that a new remote
    // stream was added containing the receiver's track.
    if (is_remote_description_or_rollback &&
        ((!previously_had_recv && transceiver->FiredDirectionHasRecv()) ||
         add_list_prev_size != add_list.size())) {
      // "Process the addition of a remote track".
      // https://w3c.github.io/webrtc-pc/#process-remote-track-addition
      track_events.push_back(transceiver);
    }
    if (previously_had_recv && !transceiver->FiredDirectionHasRecv()) {
      // "Process the removal of a remote track".
      // https://w3c.github.io/webrtc-pc/#process-remote-track-removal
      if (!transceiver->receiver()->track()->muted())
        mute_tracks.push_back(transceiver->receiver()->track());
    }
  }
  // Update the rtp_senders_ and rtp_receivers_ members to only contain
  // senders and receivers that are in the current set of transceivers.
  rtp_senders_.clear();
  rtp_receivers_.clear();
  for (auto& transceiver : transceivers_) {
    rtp_senders_.push_back(transceiver->sender());
    rtp_receivers_.push_back(transceiver->receiver());
  }

  MediaStreamVector current_streams = getRemoteStreams();

  // Modify and fire "pc.onsignalingchange" synchronously.
  if (signaling_state_ == webrtc::PeerConnectionInterface::kHaveLocalOffer &&
      signaling_state == webrtc::PeerConnectionInterface::kHaveRemoteOffer) {
    // Inject missing kStable in case of implicit rollback.
    ChangeSignalingState(webrtc::PeerConnectionInterface::kStable, true);
  }
  ChangeSignalingState(signaling_state, true);

  // Mute the tracks, this fires "track.onmute" synchronously.
  for (auto& track : mute_tracks) {
    track->Component()->Source()->SetReadyState(
        MediaStreamSource::kReadyStateMuted);
  }
  // Remove/add tracks to streams, this fires "stream.onremovetrack" and
  // "stream.onaddtrack" synchronously.
  for (auto& pair : remove_list) {
    auto& stream = pair.first;
    auto& track = pair.second;
    if (stream->getTracks().Contains(track)) {
      stream->RemoveTrackAndFireEvents(
          track,
          MediaStreamDescriptorClient::DispatchEventTiming::kImmediately);
    }
  }
  for (auto& pair : add_list) {
    auto& stream = pair.first;
    auto& track = pair.second;
    if (!stream->getTracks().Contains(track)) {
      stream->AddTrackAndFireEvents(
          track,
          MediaStreamDescriptorClient::DispatchEventTiming::kImmediately);
    }
  }

  // Legacy APIs: "pc.onaddstream" and "pc.onremovestream".
  for (const auto& current_stream : current_streams) {
    if (!previous_streams.Contains(current_stream)) {
      MaybeDispatchEvent(MakeGarbageCollected<MediaStreamEvent>(
          event_type_names::kAddstream, current_stream));
    }
  }
  for (const auto& previous_stream : previous_streams) {
    if (!current_streams.Contains(previous_stream)) {
      MaybeDispatchEvent(MakeGarbageCollected<MediaStreamEvent>(
          event_type_names::kRemovestream, previous_stream));
    }
  }

  // Fire "pc.ontrack" synchronously.
  for (auto& transceiver : track_events) {
    auto* track_event = MakeGarbageCollected<RTCTrackEvent>(
        transceiver->receiver(), transceiver->receiver()->track(),
        transceiver->receiver()->streams(), transceiver);
    MaybeDispatchEvent(track_event);
  }

  // Unmute "pc.ontrack" tracks. Fires "track.onunmute" synchronously.
  // TODO(https://crbug.com/889487): The correct thing to do is to unmute in
  // response to receiving RTP packets.
  for (auto& transceiver : track_events) {
    transceiver->receiver()->track()->Component()->Source()->SetReadyState(
        MediaStreamSource::kReadyStateLive);
  }

  // Transceiver modifications can cause changes in the set of ICE
  // transports, which may affect ICE transport state.
  // Note - this must be done every time the set of ICE transports happens.
  // At the moment this only happens in SLD/SRD, and this function is called
  // whenever these functions complete.
  UpdateIceConnectionState();
}

void RTCPeerConnection::SetAssociatedMediaStreams(
    RTCRtpReceiver* receiver,
    const Vector<String>& stream_ids,
    HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
        remove_list,
    HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
        add_list) {
  MediaStreamVector known_streams = getRemoteStreams();

  MediaStreamVector streams;
  for (const auto& stream_id : stream_ids) {
    MediaStream* curr_stream = nullptr;
    for (const auto& known_stream : known_streams) {
      if (known_stream->id() == stream_id) {
        curr_stream = known_stream;
        break;
      }
    }
    if (!curr_stream) {
      curr_stream = MediaStream::Create(
          GetExecutionContext(),
          MakeGarbageCollected<MediaStreamDescriptor>(
              static_cast<String>(stream_id), MediaStreamComponentVector(),
              MediaStreamComponentVector()));
    }
    streams.push_back(curr_stream);
  }

  const MediaStreamVector& prev_streams = receiver->streams();
  if (remove_list) {
    for (const auto& stream : prev_streams) {
      if (!streams.Contains(stream))
        remove_list->push_back(std::make_pair(stream, receiver->track()));
    }
  }
  if (add_list) {
    for (const auto& stream : streams) {
      if (!prev_streams.Contains(stream))
        add_list->push_back(std::make_pair(stream, receiver->track()));
    }
  }
  receiver->set_streams(std::move(streams));
}

void RTCPeerConnection::DidAddRemoteDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());

  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed)
    return;

  auto* blink_channel = MakeGarbageCollected<RTCDataChannel>(
      GetExecutionContext(), std::move(channel));
  blink_channel->SetStateToOpenWithoutEvent();
  MaybeDispatchEvent(MakeGarbageCollected<RTCDataChannelEvent>(
      event_type_names::kDatachannel, blink_channel));
  // The event handler might have closed the channel.
  if (blink_channel->readyState() == "open") {
    blink_channel->DispatchOpenEvent();
  }
}

void RTCPeerConnection::DidNoteInterestingUsage(int usage_pattern) {
  if (!GetExecutionContext())
    return;
  ukm::SourceId source_id = GetExecutionContext()->UkmSourceID();
  ukm::builders::WebRTC_AddressHarvesting(source_id)
      .SetUsagePattern(usage_pattern)
      .Record(GetExecutionContext()->UkmRecorder());
}

void RTCPeerConnection::UnregisterPeerConnectionHandler() {
  if (peer_handler_unregistered_) {
    DCHECK(scheduled_events_.empty())
        << "Undelivered events can cause memory leaks due to "
        << "WrapPersistent(this) in setup function callbacks";
    return;
  }

  peer_handler_unregistered_ = true;
  ice_connection_state_ = webrtc::PeerConnectionInterface::kIceConnectionClosed;
  signaling_state_ = webrtc::PeerConnectionInterface::SignalingState::kClosed;

  peer_handler_->CloseAndUnregister();
  dispatch_scheduled_events_task_handle_.Cancel();
  scheduled_events_.clear();
  feature_handle_for_scheduler_.reset();
}

void RTCPeerConnection::ClosePeerConnection() {
  DCHECK(signaling_state_ !=
         webrtc::PeerConnectionInterface::SignalingState::kClosed);
  CloseInternal();
}

const AtomicString& RTCPeerConnection::InterfaceName() const {
  return event_target_names::kRTCPeerConnection;
}

ExecutionContext* RTCPeerConnection::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void RTCPeerConnection::ContextDestroyed() {
  suppress_events_ = true;
  if (!closed_) {
    CloseInternal();
  }
  UnregisterPeerConnectionHandler();
}

void RTCPeerConnection::ChangeSignalingState(
    webrtc::PeerConnectionInterface::SignalingState signaling_state,
    bool dispatch_event_immediately) {
  if (signaling_state_ == signaling_state)
    return;
  if (signaling_state_ !=
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    signaling_state_ = signaling_state;
    Event* event = Event::Create(event_type_names::kSignalingstatechange);
    if (dispatch_event_immediately)
      MaybeDispatchEvent(event);
    else
      ScheduleDispatchEvent(event);
  }
}

void RTCPeerConnection::ChangeIceGatheringState(
    webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state) {
  if (ice_connection_state_ !=
      webrtc::PeerConnectionInterface::kIceConnectionClosed) {
    ScheduleDispatchEvent(
        Event::Create(event_type_names::kIcegatheringstatechange),
        WTF::BindOnce(&RTCPeerConnection::SetIceGatheringState,
                      WrapPersistent(this), ice_gathering_state));
    if (ice_gathering_state ==
        webrtc::PeerConnectionInterface::kIceGatheringComplete) {
      // If ICE gathering is completed, generate a null ICE candidate, to
      // signal end of candidates.
      ScheduleDispatchEvent(RTCPeerConnectionIceEvent::Create(nullptr));
    }
  }
}

bool RTCPeerConnection::SetIceGatheringState(
    webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state) {
  if (ice_connection_state_ !=
          webrtc::PeerConnectionInterface::kIceConnectionClosed &&
      ice_gathering_state_ != ice_gathering_state) {
    ice_gathering_state_ = ice_gathering_state;
    return true;
  }
  return false;
}

void RTCPeerConnection::ChangeIceConnectionState(
    webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state) {
  if (closed_) {
    return;
  }
  if (ice_connection_state_ == ice_connection_state) {
    return;
  }
  ice_connection_state_ = ice_connection_state;
  MaybeDispatchEvent(
      Event::Create(event_type_names::kIceconnectionstatechange));
}

webrtc::PeerConnectionInterface::IceConnectionState
RTCPeerConnection::ComputeIceConnectionState() {
  if (closed_)
    return webrtc::PeerConnectionInterface::kIceConnectionClosed;
  if (HasAnyFailedIceTransport())
    return webrtc::PeerConnectionInterface::kIceConnectionFailed;
  if (HasAnyDisconnectedIceTransport())
    return webrtc::PeerConnectionInterface::kIceConnectionDisconnected;
  if (HasAllNewOrClosedIceTransports())
    return webrtc::PeerConnectionInterface::kIceConnectionNew;
  if (HasAnyNewOrCheckingIceTransport())
    return webrtc::PeerConnectionInterface::kIceConnectionChecking;
  if (HasAllCompletedOrClosedIceTransports())
    return webrtc::PeerConnectionInterface::kIceConnectionCompleted;
  if (HasAllConnectedCompletedOrClosedIceTransports())
    return webrtc::PeerConnectionInterface::kIceConnectionConnected;

  return ice_connection_state_;
}

bool RTCPeerConnection::HasAnyFailedIceTransport() const {
  for (auto& transport : ActiveIceTransports()) {
    if (transport->GetState() == webrtc::IceTransportState::kFailed)
      return true;
  }
  return false;
}

bool RTCPeerConnection::HasAnyDisconnectedIceTransport() const {
  for (auto& transport : ActiveIceTransports()) {
    if (transport->GetState() == webrtc::IceTransportState::kDisconnected)
      return true;
  }
  return false;
}

bool RTCPeerConnection::HasAllNewOrClosedIceTransports() const {
  for (auto& transport : ActiveIceTransports()) {
    if (transport->GetState() != webrtc::IceTransportState::kNew &&
        transport->GetState() != webrtc::IceTransportState::kClosed)
      return false;
  }
  return true;
}

bool RTCPeerConnection::HasAnyNewOrCheckingIceTransport() const {
  for (auto& transport : ActiveIceTransports()) {
    if (transport->GetState() == webrtc::IceTransportState::kNew ||
        transport->GetState() == webrtc::IceTransportState::kChecking)
      return true;
  }
  return false;
}

bool RTCPeerConnection::HasAllCompletedOrClosedIceTransports() const {
  for (auto& transport : ActiveIceTransports()) {
    if (transport->GetState() != webrtc::IceTransportState::kCompleted &&
        transport->GetState() != webrtc::IceTransportState::kClosed)
      return false;
  }
  return true;
}

bool RTCPeerConnection::HasAllConnectedCompletedOrClosedIceTransports() const {
  for (auto& transport : ActiveIceTransports()) {
    if (transport->GetState() != webrtc::IceTransportState::kConnected &&
        transport->GetState() != webrtc::IceTransportState::kCompleted &&
        transport->GetState() != webrtc::IceTransportState::kClosed)
      return false;
  }
  return true;
}

void RTCPeerConnection::ChangePeerConnectionState(
    webrtc::PeerConnectionInterface::PeerConnectionState
        peer_connection_state) {
  if (peer_connection_state_ !=
      webrtc::PeerConnectionInterface::PeerConnectionState::kClosed) {
    ScheduleDispatchEvent(
        Event::Create(event_type_names::kConnectionstatechange),
        WTF::BindOnce(&RTCPeerConnection::SetPeerConnectionState,
                      WrapPersistent(this), peer_connection_state));
  }
}

bool RTCPeerConnection::SetPeerConnectionState(
    webrtc::PeerConnectionInterface::PeerConnectionState
        peer_connection_state) {
  if (peer_connection_state_ !=
          webrtc::PeerConnectionInterface::PeerConnectionState::kClosed &&
      peer_connection_state_ != peer_connection_state) {
    peer_connection_state_ = peer_connection_state;
    return true;
  }
  return false;
}

void RTCPeerConnection::CloseInternal() {
  DCHECK(signaling_state_ !=
         webrtc::PeerConnectionInterface::SignalingState::kClosed);
  peer_handler_->Close();
  closed_ = true;

  ChangeIceConnectionState(
      webrtc::PeerConnectionInterface::kIceConnectionClosed);
  SetPeerConnectionState(
      webrtc::PeerConnectionInterface::PeerConnectionState::kClosed);
  ChangeSignalingState(webrtc::PeerConnectionInterface::SignalingState::kClosed,
                       false);
  for (auto& transceiver : transceivers_) {
    transceiver->OnTransceiverStopped();
  }
  if (sctp_transport_) {
    sctp_transport_->Close();
  }
  // Since Close() can trigger JS-level callbacks, iterate over a copy
  // of the transports list.
  auto dtls_transports_copy = dtls_transports_by_native_transport_;
  for (auto& dtls_transport_iter : dtls_transports_copy) {
    // Since "value" is a WeakPtr, check if it's still valid.
    if (dtls_transport_iter.value) {
      dtls_transport_iter.value->Close();
    }
  }

  feature_handle_for_scheduler_.reset();
}

void RTCPeerConnection::MaybeDispatchEvent(Event* event) {
  if (suppress_events_)
    return;
  DispatchEvent(*event);
}

void RTCPeerConnection::ScheduleDispatchEvent(Event* event) {
  ScheduleDispatchEvent(event, BoolFunction());
}

void RTCPeerConnection::ScheduleDispatchEvent(Event* event,
                                              BoolFunction setup_function) {
  if (peer_handler_unregistered_) {
    DCHECK(scheduled_events_.empty())
        << "Undelivered events can cause memory leaks due to "
        << "WrapPersistent(this) in setup function callbacks";
    return;
  }
  if (suppress_events_) {
    // If suppressed due to closing we also want to ignore the event, but we
    // don't need to crash.
    return;
  }

  scheduled_events_.push_back(
      MakeGarbageCollected<EventWrapper>(event, std::move(setup_function)));

  if (dispatch_scheduled_events_task_handle_.IsActive())
    return;

  if (auto* context = GetExecutionContext()) {
    if (dispatch_events_task_created_callback_for_testing_) {
      context->GetTaskRunner(TaskType::kNetworking)
          ->PostTask(
              FROM_HERE,
              std::move(dispatch_events_task_created_callback_for_testing_));
    }

    // WebRTC spec specifies kNetworking as task source.
    // https://www.w3.org/TR/webrtc/#operation
    dispatch_scheduled_events_task_handle_ = PostCancellableTask(
        *context->GetTaskRunner(TaskType::kNetworking), FROM_HERE,
        WTF::BindOnce(&RTCPeerConnection::DispatchScheduledEvents,
                      WrapPersistent(this)));
  }
}

void RTCPeerConnection::DispatchScheduledEvents() {
  if (peer_handler_unregistered_) {
    DCHECK(scheduled_events_.empty())
        << "Undelivered events can cause memory leaks due to "
        << "WrapPersistent(this) in setup function callbacks";
    return;
  }
  if (suppress_events_) {
    // If suppressed due to closing we also want to ignore the event, but we
    // don't need to crash.
    return;
  }

  HeapVector<Member<EventWrapper>> events;
  events.swap(scheduled_events_);

  HeapVector<Member<EventWrapper>>::iterator it = events.begin();
  for (; it != events.end(); ++it) {
    if ((*it)->Setup()) {
      DispatchEvent(*(*it)->event_.Release());
    }
  }

  events.clear();
}

void RTCPeerConnection::Trace(Visitor* visitor) const {
  visitor->Trace(pending_local_description_);
  visitor->Trace(current_local_description_);
  visitor->Trace(pending_remote_description_);
  visitor->Trace(current_remote_description_);
  visitor->Trace(tracks_);
  visitor->Trace(rtp_senders_);
  visitor->Trace(rtp_receivers_);
  visitor->Trace(transceivers_);
  visitor->Trace(scheduled_events_);
  visitor->Trace(dtls_transports_by_native_transport_);
  visitor->Trace(ice_transports_by_native_transport_);
  visitor->Trace(sctp_transport_);
  visitor->Trace(rtp_transport_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  MediaStreamObserver::Trace(visitor);
}

// static
void RTCPeerConnection::SetRtcPeerConnectionHandlerFactoryForTesting(
    RtcPeerConnectionHandlerFactoryCallback callback) {
  DCHECK(g_create_rpc_peer_connection_handler_callback_.Get().is_null());
  g_create_rpc_peer_connection_handler_callback_.Get() = std::move(callback);
}

int RTCPeerConnection::PeerConnectionCount() {
  return InstanceCounters::CounterValue(
      InstanceCounters::kRTCPeerConnectionCounter);
}

int RTCPeerConnection::PeerConnectionCountLimit() {
  return kMaxPeerConnections;
}

void RTCPeerConnection::DisableBackForwardCache(ExecutionContext* context) {
  LocalDOMWindow* window = To<LocalDOMWindow>(context);
  // Two features are registered here:
  // - `kWebRTC`: a non-sticky feature that will disable BFCache for any page.
  // It will be reset after the `RTCPeerConnection` is closed.
  // - `kWebRTCSticky`: a sticky feature that will only disable BFCache for the
  // page containing "Cache-Control: no-store" header. It won't be reset even if
  // the `RTCPeerConnection` is closed.
  feature_handle_for_scheduler_ =
      window->GetFrame()->GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebRTC,
          SchedulingPolicy{SchedulingPolicy::DisableBackForwardCache()});
  window->GetFrame()->GetFrameScheduler()->RegisterStickyFeature(
      SchedulingPolicy::Feature::kWebRTCSticky,
      SchedulingPolicy{SchedulingPolicy::DisableBackForwardCache()});
}

}  // namespace blink
