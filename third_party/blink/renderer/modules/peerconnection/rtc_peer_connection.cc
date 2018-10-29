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

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_rtc_answer_options.h"
#include "third_party/blink/public/platform/web_rtc_certificate_generator.h"
#include "third_party/blink/public/platform/web_rtc_data_channel_handler.h"
#include "third_party/blink/public/platform/web_rtc_data_channel_init.h"
#include "third_party/blink/public/platform/web_rtc_ice_candidate.h"
#include "third_party/blink/public/platform/web_rtc_key_params.h"
#include "third_party/blink/public/platform/web_rtc_offer_options.h"
#include "third_party/blink/public/platform/web_rtc_session_description.h"
#include "third_party/blink/public/platform/web_rtc_session_description_request.h"
#include "third_party/blink/public/platform/web_rtc_stats_request.h"
#include "third_party/blink/public/platform/web_rtc_void_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/modules/v8/media_stream_track_or_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/rtc_ice_candidate_init_or_rtc_ice_candidate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_stats_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/modules/crypto/crypto_result_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_answer_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_configuration.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_server.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_offer_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_promise_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_request_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_track_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_promise_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/internals_rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_answer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/webrtc/api/jsep.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/pc/sessiondescription.h"

namespace blink {

namespace {

const char kSignalingStateClosedMessage[] =
    "The RTCPeerConnection's signalingState is 'closed'.";
const char kModifiedSdpMessage[] =
    "The SDP does not match the previously generated SDP for this type";
const char kOnlySupportedInUnifiedPlanMessage[] =
    "This operation is only supported in 'unified-plan'. 'unified-plan' will "
    "become the default behavior in the future, but it is currently "
    "experimental. To try it out, construct the RTCPeerConnection with "
    "sdpSemantics:'unified-plan' present in the RTCConfiguration argument.";

// The maximum number of PeerConnections that can exist simultaneously.
const long kMaxPeerConnections = 500;

bool ThrowExceptionIfSignalingStateClosed(
    webrtc::PeerConnectionInterface::SignalingState state,
    ExceptionState& exception_state) {
  if (state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSignalingStateClosedMessage);
    return true;
  }

  return false;
}

void AsyncCallErrorCallback(V8RTCPeerConnectionErrorCallback* error_callback,
                            DOMException* exception) {
  DCHECK(error_callback);
  Microtask::EnqueueMicrotask(
      WTF::Bind(&V8PersistentCallbackFunction<
                    V8RTCPeerConnectionErrorCallback>::InvokeAndReportException,
                WrapPersistent(ToV8PersistentCallbackFunction(error_callback)),
                nullptr, WrapPersistent(exception)));
}

bool CallErrorCallbackIfSignalingStateClosed(
    webrtc::PeerConnectionInterface::SignalingState state,
    V8RTCPeerConnectionErrorCallback* error_callback) {
  if (state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    if (error_callback) {
      AsyncCallErrorCallback(
          error_callback,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kSignalingStateClosedMessage));
    }
    return true;
  }

  return false;
}

bool IsIceCandidateMissingSdp(
    const RTCIceCandidateInitOrRTCIceCandidate& candidate) {
  if (candidate.IsRTCIceCandidateInit()) {
    const RTCIceCandidateInit& ice_candidate_init =
        candidate.GetAsRTCIceCandidateInit();
    return !ice_candidate_init.hasSdpMid() &&
           !ice_candidate_init.hasSdpMLineIndex();
  }

  DCHECK(candidate.IsRTCIceCandidate());
  return false;
}

WebRTCOfferOptions ConvertToWebRTCOfferOptions(const RTCOfferOptions& options) {
  return WebRTCOfferOptions(RTCOfferOptionsPlatform::Create(
      options.hasOfferToReceiveVideo()
          ? std::max(options.offerToReceiveVideo(), 0)
          : -1,
      options.hasOfferToReceiveAudio()
          ? std::max(options.offerToReceiveAudio(), 0)
          : -1,
      options.hasVoiceActivityDetection() ? options.voiceActivityDetection()
                                          : true,
      options.hasIceRestart() ? options.iceRestart() : false));
}

WebRTCAnswerOptions ConvertToWebRTCAnswerOptions(
    const RTCAnswerOptions& options) {
  return WebRTCAnswerOptions(RTCAnswerOptionsPlatform::Create(
      options.hasVoiceActivityDetection() ? options.voiceActivityDetection()
                                          : true));
}

scoped_refptr<WebRTCICECandidate> ConvertToWebRTCIceCandidate(
    ExecutionContext* context,
    const RTCIceCandidateInitOrRTCIceCandidate& candidate) {
  DCHECK(!candidate.IsNull());
  if (candidate.IsRTCIceCandidateInit()) {
    const RTCIceCandidateInit& ice_candidate_init =
        candidate.GetAsRTCIceCandidateInit();
    // TODO(guidou): Change default value to -1. crbug.com/614958.
    unsigned short sdp_m_line_index = 0;
    if (ice_candidate_init.hasSdpMLineIndex()) {
      sdp_m_line_index = ice_candidate_init.sdpMLineIndex();
    } else {
      UseCounter::Count(context,
                        WebFeature::kRTCIceCandidateDefaultSdpMLineIndex);
    }
    return WebRTCICECandidate::Create(ice_candidate_init.candidate(),
                                      ice_candidate_init.sdpMid(),
                                      sdp_m_line_index);
  }

  DCHECK(candidate.IsRTCIceCandidate());
  return candidate.GetAsRTCIceCandidate()->WebCandidate();
}

enum SdpSemanticRequested {
  kSdpSemanticRequestedDefault,
  kSdpSemanticRequestedPlanB,
  kSdpSemanticRequestedUnifiedPlan,
  kSdpSemanticRequestedMax
};

SdpSemanticRequested GetSdpSemanticRequested(
    const blink::RTCConfiguration& configuration) {
  if (!configuration.hasSdpSemantics()) {
    return kSdpSemanticRequestedDefault;
  }
  if (configuration.sdpSemantics() == "plan-b") {
    return kSdpSemanticRequestedPlanB;
  }
  if (configuration.sdpSemantics() == "unified-plan") {
    return kSdpSemanticRequestedUnifiedPlan;
  }

  NOTREACHED();
  return kSdpSemanticRequestedDefault;
}

// Helper class for RTCPeerConnection::generateCertificate.
class WebRTCCertificateObserver : public WebRTCCertificateCallback {
 public:
  // Takes ownership of |resolver|.
  static WebRTCCertificateObserver* Create(ScriptPromiseResolver* resolver) {
    return new WebRTCCertificateObserver(resolver);
  }

  ~WebRTCCertificateObserver() override = default;

 private:
  explicit WebRTCCertificateObserver(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  void OnSuccess(rtc::scoped_refptr<rtc::RTCCertificate> certificate) override {
    resolver_->Resolve(new RTCCertificate(std::move(certificate)));
  }

  void OnError() override { resolver_->Reject(); }

  Persistent<ScriptPromiseResolver> resolver_;
};

webrtc::PeerConnectionInterface::IceTransportsType IceTransportPolicyFromString(
    const String& policy) {
  if (policy == "relay")
    return webrtc::PeerConnectionInterface::kRelay;
  DCHECK_EQ(policy, "all");
  return webrtc::PeerConnectionInterface::kAll;
}

webrtc::PeerConnectionInterface::RTCConfiguration ParseConfiguration(
    ExecutionContext* context,
    const RTCConfiguration& configuration,
    ExceptionState& exception_state) {
  DCHECK(context);

  webrtc::PeerConnectionInterface::RTCConfiguration web_configuration;

  if (configuration.hasIceTransportPolicy()) {
    UseCounter::Count(context, WebFeature::kRTCConfigurationIceTransportPolicy);
    web_configuration.type =
        IceTransportPolicyFromString(configuration.iceTransportPolicy());
  } else if (configuration.hasIceTransports()) {
    UseCounter::Count(context, WebFeature::kRTCConfigurationIceTransports);
    web_configuration.type =
        IceTransportPolicyFromString(configuration.iceTransports());
  }

  if (configuration.bundlePolicy() == "max-compat") {
    web_configuration.bundle_policy =
        webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
  } else if (configuration.bundlePolicy() == "max-bundle") {
    web_configuration.bundle_policy =
        webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
  } else {
    DCHECK_EQ(configuration.bundlePolicy(), "balanced");
  }

  if (configuration.rtcpMuxPolicy() == "negotiate") {
    web_configuration.rtcp_mux_policy =
        webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
    Deprecation::CountDeprecation(context, WebFeature::kRtcpMuxPolicyNegotiate);
  } else {
    DCHECK_EQ(configuration.rtcpMuxPolicy(), "require");
  }

  if (configuration.hasSdpSemantics()) {
    if (configuration.sdpSemantics() == "plan-b") {
      web_configuration.sdp_semantics = webrtc::SdpSemantics::kPlanB;
    } else {
      DCHECK_EQ(configuration.sdpSemantics(), "unified-plan");
      web_configuration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    }
  } else {
    if (!base::FeatureList::IsEnabled(features::kRTCUnifiedPlanByDefault) &&
        !RuntimeEnabledFeatures::RTCUnifiedPlanByDefaultEnabled()) {
      // By default: The default SDP semantics is: "kPlanB".
      web_configuration.sdp_semantics = webrtc::SdpSemantics::kPlanB;
    } else {
      // Override default SDP semantics to "kUnifiedPlan" with
      // --enable-features=RTCUnifiedPlanByDefault or with
      // --enable-blink-features=RTCUnifiedPlanByDefault.
      web_configuration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    }
  }

  if (configuration.hasIceServers()) {
    std::vector<webrtc::PeerConnectionInterface::IceServer> ice_servers;
    for (const RTCIceServer& ice_server : configuration.iceServers()) {
      Vector<String> url_strings;
      if (ice_server.hasURLs()) {
        UseCounter::Count(context, WebFeature::kRTCIceServerURLs);
        const StringOrStringSequence& urls = ice_server.urls();
        if (urls.IsString()) {
          url_strings.push_back(urls.GetAsString());
        } else {
          DCHECK(urls.IsStringSequence());
          url_strings = urls.GetAsStringSequence();
        }
      } else if (ice_server.hasURL()) {
        UseCounter::Count(context, WebFeature::kRTCIceServerURL);
        url_strings.push_back(ice_server.url());
      } else {
        exception_state.ThrowTypeError("Malformed RTCIceServer");
        return {};
      }

      String username = ice_server.username();
      String credential = ice_server.credential();

      for (const String& url_string : url_strings) {
        KURL url(NullURL(), url_string);
        if (!url.IsValid()) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kSyntaxError,
              "'" + url_string + "' is not a valid URL.");
          return {};
        }
        if (!(url.ProtocolIs("turn") || url.ProtocolIs("turns") ||
              url.ProtocolIs("stun"))) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kSyntaxError,
              "'" + url.Protocol() +
                  "' is not one of the supported URL schemes "
                  "'stun', 'turn' or 'turns'.");
          return {};
        }
        if ((url.ProtocolIs("turn") || url.ProtocolIs("turns")) &&
            (username.IsNull() || credential.IsNull())) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kInvalidAccessError,
              "Both username and credential are "
              "required when the URL scheme is "
              "\"turn\" or \"turns\".");
        }
        ice_servers.emplace_back();
        auto& converted_ice_server = ice_servers.back();
        converted_ice_server.urls.push_back(WebString(url).Utf8());
        converted_ice_server.username = WebString(username).Utf8();
        converted_ice_server.password = WebString(credential).Utf8();
      }
    }
    web_configuration.servers = ice_servers;
  }

  if (configuration.hasCertificates()) {
    const HeapVector<Member<RTCCertificate>>& certificates =
        configuration.certificates();
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates_copy(
        certificates.size());
    for (wtf_size_t i = 0; i < certificates.size(); ++i) {
      certificates_copy[i] = certificates[i]->Certificate();
    }
    web_configuration.certificates = std::move(certificates_copy);
  }

  web_configuration.ice_candidate_pool_size =
      configuration.iceCandidatePoolSize();
  return web_configuration;
}

RTCOfferOptionsPlatform* ParseOfferOptions(const Dictionary& options,
                                           ExceptionState& exception_state) {
  if (options.IsUndefinedOrNull())
    return nullptr;

  const Vector<String>& property_names =
      options.GetPropertyNames(exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Treat |options| as MediaConstraints if it is empty or has "optional" or
  // "mandatory" properties for compatibility.
  // TODO(jiayl): remove constraints when RTCOfferOptions reaches Stable and
  // client code is ready.
  if (property_names.IsEmpty() || property_names.Contains("optional") ||
      property_names.Contains("mandatory"))
    return nullptr;

  int32_t offer_to_receive_video = -1;
  int32_t offer_to_receive_audio = -1;
  bool voice_activity_detection = true;
  bool ice_restart = false;

  if (DictionaryHelper::Get(options, "offerToReceiveVideo",
                            offer_to_receive_video) &&
      offer_to_receive_video < 0)
    offer_to_receive_video = 0;
  if (DictionaryHelper::Get(options, "offerToReceiveAudio",
                            offer_to_receive_audio) &&
      offer_to_receive_audio < 0)
    offer_to_receive_audio = 0;
  DictionaryHelper::Get(options, "voiceActivityDetection",
                        voice_activity_detection);
  DictionaryHelper::Get(options, "iceRestart", ice_restart);

  RTCOfferOptionsPlatform* rtc_offer_options = RTCOfferOptionsPlatform::Create(
      offer_to_receive_video, offer_to_receive_audio, voice_activity_detection,
      ice_restart);
  return rtc_offer_options;
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
  if (old_fingerprint_end == WTF::kNotFound) {
    old_fingerprint_end = old_sdp.Find("\n", old_fingerprint_pos + 1);
  }
  wtf_size_t new_fingerprint_end =
      new_sdp.Find("\r\n", new_fingerprint_pos + 1);
  if (new_fingerprint_end == WTF::kNotFound) {
    new_fingerprint_end = new_sdp.Find("\n", new_fingerprint_pos + 1);
  }
  return old_sdp.Substring(old_fingerprint_pos,
                           old_fingerprint_end - old_fingerprint_pos) !=
         new_sdp.Substring(new_fingerprint_pos,
                           new_fingerprint_end - new_fingerprint_pos);
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

void RTCPeerConnection::EventWrapper::Trace(blink::Visitor* visitor) {
  visitor->Trace(event_);
}

RTCPeerConnection* RTCPeerConnection::Create(
    ExecutionContext* context,
    const RTCConfiguration& rtc_configuration,
    const Dictionary& media_constraints,
    ExceptionState& exception_state) {
  // Count number of PeerConnections that could potentially be impacted by CSP
  if (context) {
    auto& security_context = context->GetSecurityContext();
    auto* content_security_policy = security_context.GetContentSecurityPolicy();
    if (content_security_policy &&
        content_security_policy->IsActiveForConnections()) {
      UseCounter::Count(context, WebFeature::kRTCPeerConnectionWithActiveCsp);
    }
  }

  if (media_constraints.IsObject()) {
    UseCounter::Count(context,
                      WebFeature::kRTCPeerConnectionConstructorConstraints);
  } else {
    UseCounter::Count(context,
                      WebFeature::kRTCPeerConnectionConstructorCompliant);
  }

  webrtc::PeerConnectionInterface::RTCConfiguration configuration =
      ParseConfiguration(context, rtc_configuration, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Make sure no certificates have expired.
  if (configuration.certificates.size() > 0) {
    DOMTimeStamp now = ConvertSecondsToDOMTimeStamp(CurrentTime());
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

  MediaErrorState media_error_state;
  WebMediaConstraints constraints = media_constraints_impl::Create(
      context, media_constraints, media_error_state);
  if (media_error_state.HadException()) {
    media_error_state.RaiseException(exception_state);
    return nullptr;
  }

  RTCPeerConnection* peer_connection = new RTCPeerConnection(
      context, std::move(configuration), rtc_configuration.hasSdpSemantics(),
      constraints, exception_state);
  peer_connection->PauseIfNeeded();
  if (exception_state.HadException())
    return nullptr;

  UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.SdpSemanticRequested",
                            GetSdpSemanticRequested(rtc_configuration),
                            kSdpSemanticRequestedMax);

  return peer_connection;
}

RTCPeerConnection::RTCPeerConnection(
    ExecutionContext* context,
    webrtc::PeerConnectionInterface::RTCConfiguration configuration,
    bool sdp_semantics_specified,
    WebMediaConstraints constraints,
    ExceptionState& exception_state)
    : PausableObject(context),
      signaling_state_(
          webrtc::PeerConnectionInterface::SignalingState::kStable),
      ice_gathering_state_(webrtc::PeerConnectionInterface::kIceGatheringNew),
      ice_connection_state_(webrtc::PeerConnectionInterface::kIceConnectionNew),
      // WebRTC spec specifies kNetworking as task source.
      // https://www.w3.org/TR/webrtc/#operation
      dispatch_scheduled_event_runner_(
          AsyncMethodRunner<RTCPeerConnection>::Create(
              this,
              &RTCPeerConnection::DispatchScheduledEvent,
              context->GetTaskRunner(TaskType::kNetworking))),
      negotiation_needed_(false),
      stopped_(false),
      closed_(false),
      has_data_channels_(false),
      sdp_semantics_(configuration.sdp_semantics),
      sdp_semantics_specified_(sdp_semantics_specified) {
  Document* document = To<Document>(GetExecutionContext());

  InstanceCounters::IncrementCounter(
      InstanceCounters::kRTCPeerConnectionCounter);
  // If we fail, set |m_closed| and |m_stopped| to true, to avoid hitting the
  // assert in the destructor.
  if (InstanceCounters::CounterValue(
          InstanceCounters::kRTCPeerConnectionCounter) > kMaxPeerConnections) {
    closed_ = true;
    stopped_ = true;
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "Cannot create so many PeerConnections");
    return;
  }
  if (!document->GetFrame()) {
    closed_ = true;
    stopped_ = true;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "PeerConnections may not be created in detached documents.");
    return;
  }

  peer_handler_ = Platform::Current()->CreateRTCPeerConnectionHandler(
      this, document->GetTaskRunner(TaskType::kInternalMedia));
  if (!peer_handler_) {
    closed_ = true;
    stopped_ = true;
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "No PeerConnection handler can be "
                                      "created, perhaps WebRTC is disabled?");
    return;
  }

  document->GetFrame()->Client()->DispatchWillStartUsingPeerConnectionHandler(
      peer_handler_.get());

  if (!peer_handler_->Initialize(configuration, constraints)) {
    closed_ = true;
    stopped_ = true;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Failed to initialize native PeerConnection.");
    return;
  }

  connection_handle_for_scheduler_ =
      document->GetFrame()->GetFrameScheduler()->OnActiveConnectionCreated();
}

RTCPeerConnection::~RTCPeerConnection() {
  // This checks that close() or stop() is called before the destructor.
  // We are assuming that a wrapper is always created when RTCPeerConnection is
  // created.
  DCHECK(closed_ || stopped_);
  InstanceCounters::DecrementCounter(
      InstanceCounters::kRTCPeerConnectionCounter);
  DCHECK(InstanceCounters::CounterValue(
             InstanceCounters::kRTCPeerConnectionCounter) >= 0);
}

void RTCPeerConnection::Dispose() {
  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  peer_handler_.reset();
}

ScriptPromise RTCPeerConnection::createOffer(ScriptState* script_state,
                                             const RTCOfferOptions& options) {
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSignalingStateClosedMessage));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestPromiseImpl::Create(
          this, resolver, "RTCPeerConnection", "createOffer");
  if (options.hasOfferToReceiveAudio() || options.hasOfferToReceiveVideo()) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    UseCounter::Count(
        context,
        WebFeature::kRTCPeerConnectionCreateOfferOptionsOfferToReceive);
  }
  peer_handler_->CreateOffer(request, ConvertToWebRTCOfferOptions(options));
  return promise;
}

ScriptPromise RTCPeerConnection::createOffer(
    ScriptState* script_state,
    V8RTCSessionDescriptionCallback* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback,
    const Dictionary& rtc_offer_options,
    ExceptionState& exception_state) {
  DCHECK(success_callback);
  DCHECK(error_callback);
  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(
      context, WebFeature::kRTCPeerConnectionCreateOfferLegacyFailureCallback);
  if (CallErrorCallbackIfSignalingStateClosed(signaling_state_, error_callback))
    return ScriptPromise::CastUndefined(script_state);

  RTCOfferOptionsPlatform* offer_options =
      ParseOfferOptions(rtc_offer_options, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestImpl::Create(
          GetExecutionContext(), this, success_callback, error_callback);

  if (offer_options) {
    if (offer_options->OfferToReceiveAudio() != -1 ||
        offer_options->OfferToReceiveVideo() != -1) {
      UseCounter::Count(
          context, WebFeature::kRTCPeerConnectionCreateOfferLegacyOfferOptions);
    } else {
      UseCounter::Count(
          context, WebFeature::kRTCPeerConnectionCreateOfferLegacyCompliant);
    }

    peer_handler_->CreateOffer(request, WebRTCOfferOptions(offer_options));
  } else {
    MediaErrorState media_error_state;
    WebMediaConstraints constraints = media_constraints_impl::Create(
        context, rtc_offer_options, media_error_state);
    // Report constraints parsing errors via the callback, but ignore
    // unknown/unsupported constraints as they would be silently discarded by
    // WebIDL.
    if (media_error_state.CanGenerateException()) {
      String error_msg = media_error_state.GetErrorMessage();
      AsyncCallErrorCallback(
          error_callback,
          DOMException::Create(DOMExceptionCode::kOperationError, error_msg));
      return ScriptPromise::CastUndefined(script_state);
    }

    if (!constraints.IsEmpty()) {
      UseCounter::Count(
          context, WebFeature::kRTCPeerConnectionCreateOfferLegacyConstraints);
    } else {
      UseCounter::Count(
          context, WebFeature::kRTCPeerConnectionCreateOfferLegacyCompliant);
    }

    peer_handler_->CreateOffer(request, constraints);
  }

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCPeerConnection::createAnswer(ScriptState* script_state,
                                              const RTCAnswerOptions& options) {
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSignalingStateClosedMessage));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestPromiseImpl::Create(
          this, resolver, "RTCPeerConnection", "createAnswer");
  peer_handler_->CreateAnswer(request, ConvertToWebRTCAnswerOptions(options));
  return promise;
}

ScriptPromise RTCPeerConnection::createAnswer(
    ScriptState* script_state,
    V8RTCSessionDescriptionCallback* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback,
    const Dictionary& media_constraints) {
  DCHECK(success_callback);
  DCHECK(error_callback);
  ExecutionContext* context = ExecutionContext::From(script_state);
  UseCounter::Count(
      context, WebFeature::kRTCPeerConnectionCreateAnswerLegacyFailureCallback);
  if (media_constraints.IsObject()) {
    UseCounter::Count(
        context, WebFeature::kRTCPeerConnectionCreateAnswerLegacyConstraints);
  } else {
    UseCounter::Count(
        context, WebFeature::kRTCPeerConnectionCreateAnswerLegacyCompliant);
  }

  if (CallErrorCallbackIfSignalingStateClosed(signaling_state_, error_callback))
    return ScriptPromise::CastUndefined(script_state);

  MediaErrorState media_error_state;
  WebMediaConstraints constraints = media_constraints_impl::Create(
      context, media_constraints, media_error_state);
  // Report constraints parsing errors via the callback, but ignore
  // unknown/unsupported constraints as they would be silently discarded by
  // WebIDL.
  if (media_error_state.CanGenerateException()) {
    String error_msg = media_error_state.GetErrorMessage();
    AsyncCallErrorCallback(
        error_callback,
        DOMException::Create(DOMExceptionCode::kOperationError, error_msg));
    return ScriptPromise::CastUndefined(script_state);
  }

  RTCSessionDescriptionRequest* request =
      RTCSessionDescriptionRequestImpl::Create(
          GetExecutionContext(), this, success_callback, error_callback);
  peer_handler_->CreateAnswer(request, constraints);
  return ScriptPromise::CastUndefined(script_state);
}

DOMException* RTCPeerConnection::checkSdpForStateErrors(
    ExecutionContext* context,
    const RTCSessionDescriptionInit& session_description_init,
    String* sdp) {
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    return DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                kSignalingStateClosedMessage);
  }

  *sdp = session_description_init.sdp();
  if (session_description_init.type() == "offer") {
    if (sdp->IsNull() || sdp->IsEmpty()) {
      *sdp = last_offer_;
    } else if (session_description_init.sdp() != last_offer_) {
      if (FingerprintMismatch(last_offer_, *sdp)) {
        return DOMException::Create(DOMExceptionCode::kInvalidModificationError,
                                    kModifiedSdpMessage);
      } else {
        UseCounter::Count(context, WebFeature::kRTCLocalSdpModification);
        return nullptr;
        // TODO(https://crbug.com/823036): Return failure for all modification.
      }
    }
  } else if (session_description_init.type() == "answer" ||
             session_description_init.type() == "pranswer") {
    if (sdp->IsNull() || sdp->IsEmpty()) {
      *sdp = last_answer_;
    } else if (session_description_init.sdp() != last_answer_) {
      if (FingerprintMismatch(last_answer_, *sdp)) {
        return DOMException::Create(DOMExceptionCode::kInvalidModificationError,
                                    kModifiedSdpMessage);
      } else {
        UseCounter::Count(context, WebFeature::kRTCLocalSdpModification);
        return nullptr;
        // TODO(https://crbug.com/823036): Return failure for all modification.
      }
    }
  }
  return nullptr;
}

bool RTCPeerConnection::ShouldShowComplexPlanBSdpWarning(
    const RTCSessionDescriptionInit& session_description_init) const {
  if (sdp_semantics_specified_)
    return false;
  if (!session_description_init.hasType() || !session_description_init.hasSdp())
    return false;
  std::unique_ptr<webrtc::SessionDescriptionInterface> session_description(
      webrtc::CreateSessionDescription(
          session_description_init.type().Utf8().data(),
          session_description_init.sdp().Utf8().data(), nullptr));
  if (!session_description)
    return false;
  size_t num_audio_mlines = 0u;
  size_t num_video_mlines = 0u;
  size_t num_audio_tracks = 0u;
  size_t num_video_tracks = 0u;
  for (const cricket::ContentInfo& content :
       session_description->description()->contents()) {
    cricket::MediaType media_type = content.media_description()->type();
    size_t num_tracks = std::max(static_cast<size_t>(1u),
                                 content.media_description()->streams().size());
    if (media_type == cricket::MEDIA_TYPE_AUDIO) {
      ++num_audio_mlines;
      num_audio_tracks += num_tracks;
    } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
      ++num_video_mlines;
      num_video_tracks += num_tracks;
    }
  }
  return (num_audio_mlines == 1u && num_audio_tracks > 1u) ||
         (num_video_mlines == 1u && num_video_tracks > 1u);
}

ScriptPromise RTCPeerConnection::setLocalDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit& session_description_init) {
  if (ShouldShowComplexPlanBSdpWarning(session_description_init)) {
    Deprecation::CountDeprecation(
        GetExecutionContext(),
        WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics);
  }
  String sdp;
  DOMException* exception = checkSdpForStateErrors(
      ExecutionContext::From(script_state), session_description_init, &sdp);
  if (exception) {
    return ScriptPromise::RejectWithDOMException(script_state, exception);
  }
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  RTCVoidRequest* request = RTCVoidRequestPromiseImpl::Create(
      this, resolver, "RTCPeerConnection", "setLocalDescription");
  peer_handler_->SetLocalDescription(
      request, WebRTCSessionDescription(session_description_init.type(), sdp));
  return promise;
}

ScriptPromise RTCPeerConnection::setLocalDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit& session_description_init,
    V8VoidFunction* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback) {
  if (ShouldShowComplexPlanBSdpWarning(session_description_init)) {
    Deprecation::CountDeprecation(
        GetExecutionContext(),
        WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics);
  }
  ExecutionContext* context = ExecutionContext::From(script_state);
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

  String sdp;
  DOMException* exception =
      checkSdpForStateErrors(context, session_description_init, &sdp);
  if (exception) {
    if (error_callback)
      AsyncCallErrorCallback(error_callback, exception);
    return ScriptPromise::CastUndefined(script_state);
  }

  RTCVoidRequest* request = RTCVoidRequestImpl::Create(
      GetExecutionContext(), this, success_callback, error_callback);
  peer_handler_->SetLocalDescription(
      request, WebRTCSessionDescription(session_description_init.type(),
                                        session_description_init.sdp()));
  return ScriptPromise::CastUndefined(script_state);
}

RTCSessionDescription* RTCPeerConnection::localDescription() {
  WebRTCSessionDescription web_session_description =
      peer_handler_->LocalDescription();
  if (web_session_description.IsNull())
    return nullptr;

  return RTCSessionDescription::Create(web_session_description);
}

RTCSessionDescription* RTCPeerConnection::currentLocalDescription() {
  WebRTCSessionDescription web_session_description =
      peer_handler_->CurrentLocalDescription();
  if (web_session_description.IsNull())
    return nullptr;

  return RTCSessionDescription::Create(web_session_description);
}

RTCSessionDescription* RTCPeerConnection::pendingLocalDescription() {
  WebRTCSessionDescription web_session_description =
      peer_handler_->PendingLocalDescription();
  if (web_session_description.IsNull())
    return nullptr;

  return RTCSessionDescription::Create(web_session_description);
}

ScriptPromise RTCPeerConnection::setRemoteDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit& session_description_init) {
  if (ShouldShowComplexPlanBSdpWarning(session_description_init)) {
    Deprecation::CountDeprecation(
        GetExecutionContext(),
        WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics);
  }
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSignalingStateClosedMessage));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  RTCVoidRequest* request = RTCVoidRequestPromiseImpl::Create(
      this, resolver, "RTCPeerConnection", "setRemoteDescription");
  peer_handler_->SetRemoteDescription(
      request, WebRTCSessionDescription(session_description_init.type(),
                                        session_description_init.sdp()));
  return promise;
}

ScriptPromise RTCPeerConnection::setRemoteDescription(
    ScriptState* script_state,
    const RTCSessionDescriptionInit& session_description_init,
    V8VoidFunction* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback) {
  if (ShouldShowComplexPlanBSdpWarning(session_description_init)) {
    Deprecation::CountDeprecation(
        GetExecutionContext(),
        WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics);
  }
  ExecutionContext* context = ExecutionContext::From(script_state);
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

  if (CallErrorCallbackIfSignalingStateClosed(signaling_state_, error_callback))
    return ScriptPromise::CastUndefined(script_state);

  RTCVoidRequest* request = RTCVoidRequestImpl::Create(
      GetExecutionContext(), this, success_callback, error_callback);
  peer_handler_->SetRemoteDescription(
      request, WebRTCSessionDescription(session_description_init.type(),
                                        session_description_init.sdp()));
  return ScriptPromise::CastUndefined(script_state);
}

RTCSessionDescription* RTCPeerConnection::remoteDescription() {
  WebRTCSessionDescription web_session_description =
      peer_handler_->RemoteDescription();
  if (web_session_description.IsNull())
    return nullptr;

  return RTCSessionDescription::Create(web_session_description);
}

RTCSessionDescription* RTCPeerConnection::currentRemoteDescription() {
  WebRTCSessionDescription web_session_description =
      peer_handler_->CurrentRemoteDescription();
  if (web_session_description.IsNull())
    return nullptr;

  return RTCSessionDescription::Create(web_session_description);
}

RTCSessionDescription* RTCPeerConnection::pendingRemoteDescription() {
  WebRTCSessionDescription web_session_description =
      peer_handler_->PendingRemoteDescription();
  if (web_session_description.IsNull())
    return nullptr;

  return RTCSessionDescription::Create(web_session_description);
}

void RTCPeerConnection::getConfiguration(RTCConfiguration& result) {
  const auto& webrtc_configuration = peer_handler_->GetConfiguration();

  switch (webrtc_configuration.type) {
    case webrtc::PeerConnectionInterface::kRelay:
      result.setIceTransportPolicy("relay");
      break;
    case webrtc::PeerConnectionInterface::kAll:
      result.setIceTransportPolicy("all");
      break;
    default:
      NOTREACHED();
  }

  switch (webrtc_configuration.bundle_policy) {
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
      result.setBundlePolicy("max-compat");
      break;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
      result.setBundlePolicy("max-bundle");
      break;
    case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
      result.setBundlePolicy("balanced");
      break;
    default:
      NOTREACHED();
  }

  switch (webrtc_configuration.rtcp_mux_policy) {
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
      result.setRtcpMuxPolicy("negotiate");
      break;
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
      result.setRtcpMuxPolicy("require");
      break;
    default:
      NOTREACHED();
  }

  switch (webrtc_configuration.sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB:
      result.setSdpSemantics("plan-b");
      break;
    case webrtc::SdpSemantics::kUnifiedPlan:
      result.setSdpSemantics("unified-plan");
      break;
    default:
      NOTREACHED();
  }

  HeapVector<RTCIceServer> ice_servers;
  ice_servers.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_configuration.servers.size()));
  for (const auto& webrtc_server : webrtc_configuration.servers) {
    ice_servers.emplace_back();
    auto& ice_server = ice_servers.back();

    StringOrStringSequence urls;
    Vector<String> url_vector;
    url_vector.ReserveCapacity(SafeCast<wtf_size_t>(webrtc_server.urls.size()));
    for (const auto& url : webrtc_server.urls) {
      url_vector.emplace_back(url.c_str());
    }
    urls.SetStringSequence(std::move(url_vector));

    ice_server.setURLs(urls);
    ice_server.setUsername(webrtc_server.username.c_str());
    ice_server.setCredential(webrtc_server.password.c_str());
  }
  result.setIceServers(ice_servers);

  if (!webrtc_configuration.certificates.empty()) {
    HeapVector<blink::Member<RTCCertificate>> certificates;
    certificates.ReserveCapacity(
        SafeCast<wtf_size_t>(webrtc_configuration.certificates.size()));
    for (const auto& webrtc_certificate : webrtc_configuration.certificates) {
      certificates.emplace_back(new RTCCertificate(webrtc_certificate));
    }
    result.setCertificates(certificates);
  }

  result.setIceCandidatePoolSize(webrtc_configuration.ice_candidate_pool_size);
}

void RTCPeerConnection::setConfiguration(
    ScriptState* script_state,
    const RTCConfiguration& rtc_configuration,
    ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
    return;

  webrtc::PeerConnectionInterface::RTCConfiguration configuration =
      ParseConfiguration(ExecutionContext::From(script_state),
                         rtc_configuration, exception_state);

  if (exception_state.HadException())
    return;

  MediaErrorState media_error_state;
  if (media_error_state.HadException()) {
    media_error_state.RaiseException(exception_state);
    return;
  }

  webrtc::RTCErrorType error = peer_handler_->SetConfiguration(configuration);
  if (error != webrtc::RTCErrorType::NONE) {
    // All errors besides InvalidModification should have been detected above.
    if (error == webrtc::RTCErrorType::INVALID_MODIFICATION) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "Attempted to modify the PeerConnection's "
          "configuration in an unsupported way.");
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kOperationError,
          "Could not update the PeerConnection with the given configuration.");
    }
  }
}

ScriptPromise RTCPeerConnection::generateCertificate(
    ScriptState* script_state,
    const AlgorithmIdentifier& keygen_algorithm,
    ExceptionState& exception_state) {
  // Normalize |keygenAlgorithm| with WebCrypto, making sure it is a recognized
  // AlgorithmIdentifier.
  WebCryptoAlgorithm crypto_algorithm;
  AlgorithmError error;
  if (!NormalizeAlgorithm(keygen_algorithm, kWebCryptoOperationGenerateKey,
                          crypto_algorithm, &error)) {
    // Reject generateCertificate with the same error as was produced by
    // WebCrypto. |result| is garbage collected, no need to delete.
    CryptoResultImpl* result = CryptoResultImpl::Create(script_state);
    ScriptPromise promise = result->Promise();
    result->CompleteWithError(error.error_type, error.error_details);
    return promise;
  }

  // Check if |keygenAlgorithm| contains the optional DOMTimeStamp |expires|
  // attribute.
  base::Optional<DOMTimeStamp> expires;
  if (keygen_algorithm.IsDictionary()) {
    Dictionary keygen_algorithm_dict = keygen_algorithm.GetAsDictionary();
    if (keygen_algorithm_dict.HasProperty("expires", exception_state)) {
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
        }
      }
    }
  }
  if (exception_state.HadException()) {
    return ScriptPromise();
  }

  // Convert from WebCrypto representation to recognized WebRTCKeyParams. WebRTC
  // supports a small subset of what are valid AlgorithmIdentifiers.
  const char* unsupported_params_string =
      "The 1st argument provided is an AlgorithmIdentifier with a supported "
      "algorithm name, but the parameters are not supported.";
  base::Optional<WebRTCKeyParams> key_params;
  switch (crypto_algorithm.Id()) {
    case kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      // name: "RSASSA-PKCS1-v1_5"
      unsigned public_exponent;
      // "publicExponent" must fit in an unsigned int. The only recognized
      // "hash" is "SHA-256".
      if (crypto_algorithm.RsaHashedKeyGenParams()
              ->ConvertPublicExponentToUnsigned(public_exponent) &&
          crypto_algorithm.RsaHashedKeyGenParams()->GetHash().Id() ==
              kWebCryptoAlgorithmIdSha256) {
        unsigned modulus_length =
            crypto_algorithm.RsaHashedKeyGenParams()->ModulusLengthBits();
        key_params =
            WebRTCKeyParams::CreateRSA(modulus_length, public_exponent);
      } else {
        return ScriptPromise::RejectWithDOMException(
            script_state,
            DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                 unsupported_params_string));
      }
      break;
    case kWebCryptoAlgorithmIdEcdsa:
      // name: "ECDSA"
      // The only recognized "namedCurve" is "P-256".
      if (crypto_algorithm.EcKeyGenParams()->NamedCurve() ==
          kWebCryptoNamedCurveP256) {
        key_params = WebRTCKeyParams::CreateECDSA(kWebRTCECCurveNistP256);
      } else {
        return ScriptPromise::RejectWithDOMException(
            script_state,
            DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                 unsupported_params_string));
      }
      break;
    default:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kNotSupportedError,
                               "The 1st argument provided is an "
                               "AlgorithmIdentifier, but the "
                               "algorithm is not supported."));
      break;
  }
  DCHECK(key_params.has_value());

  std::unique_ptr<WebRTCCertificateGenerator> certificate_generator =
      Platform::Current()->CreateRTCCertificateGenerator();

  // |keyParams| was successfully constructed, but does the certificate
  // generator support these parameters?
  if (!certificate_generator->IsSupportedKeyParams(key_params.value())) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           unsupported_params_string));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  std::unique_ptr<WebRTCCertificateObserver> certificate_observer(
      WebRTCCertificateObserver::Create(resolver));

  // Generate certificate. The |certificateObserver| will resolve the promise
  // asynchronously upon completion. The observer will manage its own
  // destruction as well as the resolver's destruction.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalMedia);
  if (!expires) {
    certificate_generator->GenerateCertificate(
        key_params.value(), std::move(certificate_observer), task_runner);
  } else {
    certificate_generator->GenerateCertificateWithExpiration(
        key_params.value(), expires.value(), std::move(certificate_observer),
        task_runner);
  }

  return promise;
}

ScriptPromise RTCPeerConnection::addIceCandidate(
    ScriptState* script_state,
    const RTCIceCandidateInitOrRTCIceCandidate& candidate,
    ExceptionState& exception_state) {
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSignalingStateClosedMessage));
  }

  if (IsIceCandidateMissingSdp(candidate)) {
    exception_state.ThrowTypeError(
        "Candidate missing values for both sdpMid and sdpMLineIndex");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  RTCVoidRequest* request = RTCVoidRequestPromiseImpl::Create(
      this, resolver, "RTCPeerConnection", "addIceCandidate");
  scoped_refptr<WebRTCICECandidate> web_candidate = ConvertToWebRTCIceCandidate(
      ExecutionContext::From(script_state), candidate);
  bool implemented =
      peer_handler_->AddICECandidate(request, std::move(web_candidate));
  if (!implemented) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kOperationError,
                             "This operation could not be completed."));
  }

  return promise;
}

ScriptPromise RTCPeerConnection::addIceCandidate(
    ScriptState* script_state,
    const RTCIceCandidateInitOrRTCIceCandidate& candidate,
    V8VoidFunction* success_callback,
    V8RTCPeerConnectionErrorCallback* error_callback,
    ExceptionState& exception_state) {
  DCHECK(success_callback);
  DCHECK(error_callback);

  if (CallErrorCallbackIfSignalingStateClosed(signaling_state_, error_callback))
    return ScriptPromise::CastUndefined(script_state);

  if (IsIceCandidateMissingSdp(candidate)) {
    exception_state.ThrowTypeError(
        "Candidate missing values for both sdpMid and sdpMLineIndex");
    return ScriptPromise();
  }

  RTCVoidRequest* request = RTCVoidRequestImpl::Create(
      GetExecutionContext(), this, success_callback, error_callback);
  scoped_refptr<WebRTCICECandidate> web_candidate = ConvertToWebRTCIceCandidate(
      ExecutionContext::From(script_state), candidate);
  bool implemented =
      peer_handler_->AddICECandidate(request, std::move(web_candidate));
  if (!implemented)
    AsyncCallErrorCallback(
        error_callback,
        DOMException::Create(DOMExceptionCode::kOperationError,
                             "This operation could not be completed."));

  return ScriptPromise::CastUndefined(script_state);
}

String RTCPeerConnection::signalingState() const {
  switch (signaling_state_) {
    case webrtc::PeerConnectionInterface::SignalingState::kStable:
      return "stable";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalOffer:
      return "have-local-offer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer:
      return "have-remote-offer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalPrAnswer:
      return "have-local-pranswer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveRemotePrAnswer:
      return "have-remote-pranswer";
    case webrtc::PeerConnectionInterface::SignalingState::kClosed:
      return "closed";
  }

  NOTREACHED();
  return String();
}

String RTCPeerConnection::iceGatheringState() const {
  switch (ice_gathering_state_) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      return "new";
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      return "gathering";
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      return "complete";
    default:
      NOTREACHED();
      return "";
  }
}

String RTCPeerConnection::iceConnectionState() const {
  switch (ice_connection_state_) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      return "new";
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      return "checking";
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      return "connected";
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      return "completed";
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      return "failed";
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      return "disconnected";
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      return "closed";
    default:
      NOTREACHED();
      return "";
  }

}

void RTCPeerConnection::addStream(ScriptState* script_state,
                                  MediaStream* stream,
                                  const Dictionary& media_constraints,
                                  ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
    return;
  if (!media_constraints.IsUndefinedOrNull()) {
    MediaErrorState media_error_state;
    WebMediaConstraints constraints =
        media_constraints_impl::Create(ExecutionContext::From(script_state),
                                       media_constraints, media_error_state);
    if (media_error_state.HadException()) {
      media_error_state.RaiseException(exception_state);
      return;
    }
    LOG(WARNING)
        << "mediaConstraints is not a supported argument to addStream.";
    LOG(WARNING) << "mediaConstraints was " << constraints.ToString().Utf8();
  }

  MediaStreamVector streams;
  streams.push_back(stream);
  for (const auto& track : stream->getTracks()) {
    addTrack(track, streams, exception_state);
    exception_state.ClearException();
  }

  stream->RegisterObserver(this);
}

void RTCPeerConnection::removeStream(MediaStream* stream,
                                     ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
    return;
  for (const auto& track : stream->getTracks()) {
    auto* sender = FindSenderForTrackAndStream(track, stream);
    if (!sender)
      continue;
    removeTrack(sender, exception_state);
    exception_state.ClearException();
  }
  stream->UnregisterObserver(this);
}

String RTCPeerConnection::id(ScriptState* script_state) const {
  DCHECK(OriginTrials::RtcPeerConnectionIdEnabled(
      ExecutionContext::From(script_state)));
  return peer_handler_->Id();
}

MediaStreamVector RTCPeerConnection::getLocalStreams() const {
  MediaStreamVector local_streams;
  if (sdp_semantics_ == webrtc::SdpSemantics::kPlanB) {
    for (const auto& sender : rtp_senders_) {
      for (const auto& stream : sender->streams()) {
        if (!local_streams.Contains(stream))
          local_streams.push_back(stream);
      }
    }
  } else {
    for (const auto& transceiver : transceivers_) {
      if (!transceiver->DirectionHasSend())
        continue;
      for (const auto& stream : transceiver->sender()->streams()) {
        if (!local_streams.Contains(stream))
          local_streams.push_back(stream);
      }
    }
  }
  return local_streams;
}

MediaStreamVector RTCPeerConnection::getRemoteStreams() const {
  MediaStreamVector remote_streams;
  if (sdp_semantics_ == webrtc::SdpSemantics::kPlanB) {
    for (const auto& receiver : rtp_receivers_) {
      for (const auto& stream : receiver->streams()) {
        if (!remote_streams.Contains(stream))
          remote_streams.push_back(stream);
      }
    }
  } else {
    for (const auto& transceiver : transceivers_) {
      if (!transceiver->DirectionHasRecv())
        continue;
      for (const auto& stream : transceiver->receiver()->streams()) {
        if (!remote_streams.Contains(stream))
          remote_streams.push_back(stream);
      }
    }
  }
  return remote_streams;
}

MediaStream* RTCPeerConnection::getRemoteStreamById(const WebString& id) const {
  for (const auto& rtp_receiver : rtp_receivers_) {
    for (const auto& stream : rtp_receiver->streams()) {
      if (static_cast<WebString>(stream->id()) == id) {
        return stream;
      }
    }
  }
  return nullptr;
}

bool RTCPeerConnection::IsRemoteStream(MediaStream* stream) const {
  for (const auto& receiver : rtp_receivers_) {
    for (const auto& receiver_stream : receiver->streams()) {
      if (receiver_stream == stream) {
        return true;
      }
    }
  }
  return false;
}

ScriptPromise RTCPeerConnection::getStats(
    ScriptState* script_state,
    blink::ScriptValue callback_or_selector) {
  auto argument = callback_or_selector.V8Value();
  // Custom binding for legacy "getStats(RTCStatsCallback callback)".
  if (argument->IsFunction()) {
    V8RTCStatsCallback* success_callback =
        V8RTCStatsCallback::Create(argument.As<v8::Function>());
    return LegacyCallbackBasedGetStats(script_state, success_callback, nullptr);
  }
  // Custom binding for spec-compliant "getStats()" and "getStats(undefined)".
  if (argument->IsUndefined())
    return PromiseBasedGetStats(script_state, nullptr);
  auto* isolate = callback_or_selector.GetIsolate();
  // Custom binding for spec-compliant "getStats(MediaStreamTrack? selector)".
  // null is a valid selector value, but value of wrong type isn't. |selector|
  // set to no value means type error.
  base::Optional<MediaStreamTrack*> selector;
  if (argument->IsNull()) {
    selector = base::Optional<MediaStreamTrack*>(nullptr);
  } else {
    MediaStreamTrack* track =
        V8MediaStreamTrack::ToImplWithTypeCheck(isolate, argument);
    if (track)
      selector = base::Optional<MediaStreamTrack*>(track);
  }
  if (selector.has_value())
    return PromiseBasedGetStats(script_state, *selector);
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "RTCPeerConnection", "getStats");
  exception_state.ThrowTypeError(
      "The argument provided as parameter 1 is neither a callback (function) "
      "or selector (MediaStreamTrack or null).");
  return ScriptPromise::Reject(script_state, exception_state);
}

ScriptPromise RTCPeerConnection::getStats(ScriptState* script_state,
                                          V8RTCStatsCallback* success_callback,
                                          MediaStreamTrack* selector) {
  return LegacyCallbackBasedGetStats(script_state, success_callback, selector);
}

ScriptPromise RTCPeerConnection::getStats(ScriptState* script_state,
                                          MediaStreamTrack* selector) {
  return PromiseBasedGetStats(script_state, selector);
}

ScriptPromise RTCPeerConnection::LegacyCallbackBasedGetStats(
    ScriptState* script_state,
    V8RTCStatsCallback* success_callback,
    MediaStreamTrack* selector) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  UseCounter::Count(context,
                    WebFeature::kRTCPeerConnectionGetStatsLegacyNonCompliant);
  RTCStatsRequest* stats_request = RTCStatsRequestImpl::Create(
      GetExecutionContext(), this, success_callback, selector);
  // FIXME: Add passing selector as part of the statsRequest.
  peer_handler_->GetStats(stats_request);

  resolver->Resolve();
  return promise;
}

ScriptPromise RTCPeerConnection::PromiseBasedGetStats(
    ScriptState* script_state,
    MediaStreamTrack* selector) {
  if (!selector) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    UseCounter::Count(context, WebFeature::kRTCPeerConnectionGetStats);

    if (!peer_handler_) {
      LOG(ERROR) << "Internal error: peer_handler_ has been discarded";
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kOperationError,
                               "Internal error: release in progress"));
    }
    ScriptPromiseResolver* resolver =
        ScriptPromiseResolver::Create(script_state);
    ScriptPromise promise = resolver->Promise();
    peer_handler_->GetStats(
        WebRTCStatsReportCallbackResolver::Create(resolver));

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
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidAccessError,
                             "There is no sender or receiver for the track."));
  }
  if (track_uses > 1u) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kInvalidAccessError,
            "There are more than one sender or receiver for the track."));
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

RTCRtpTransceiver* RTCPeerConnection::addTransceiver(
    const MediaStreamTrackOrString& track_or_kind,
    const RTCRtpTransceiverInit& init,
    ExceptionState& exception_state) {
  if (sdp_semantics_ != webrtc::SdpSemantics::kUnifiedPlan) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOnlySupportedInUnifiedPlanMessage);
    return nullptr;
  }
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
    return nullptr;
  auto webrtc_init = ToRtpTransceiverInit(init);
  webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>> result =
      webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION);
  if (track_or_kind.IsMediaStreamTrack()) {
    MediaStreamTrack* track = track_or_kind.GetAsMediaStreamTrack();
    RegisterTrack(track);
    result = peer_handler_->AddTransceiverWithTrack(track->Component(),
                                                    std::move(webrtc_init));
  } else {
    const String& kind_string = track_or_kind.GetAsString();
    // TODO(hbos): Make cricket::MediaType an allowed identifier in
    // rtc_peer_connection.cc and use that instead of a boolean.
    std::string kind;
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
    result = peer_handler_->AddTransceiverWithKind(std::move(kind),
                                                   std::move(webrtc_init));
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
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
    return nullptr;
  if (sdp_semantics_ == webrtc::SdpSemantics::kPlanB && streams.size() >= 2) {
    // TODO(hbos): Update peer_handler_ to call the AddTrack() that returns the
    // appropriate errors, and let the lower layers handle it.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Adding a track to multiple streams is not supported.");
    return nullptr;
  }
  for (const auto& sender : rtp_senders_) {
    if (sender->track() == track) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "A sender already exists for the track.");
      return nullptr;
    }
  }

  WebVector<WebMediaStream> web_streams(streams.size());
  for (wtf_size_t i = 0; i < streams.size(); ++i) {
    web_streams[i] = streams[i]->Descriptor();
  }
  webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
      error_or_transceiver =
          peer_handler_->AddTrack(track->Component(), web_streams);
  if (!error_or_transceiver.ok()) {
    ThrowExceptionFromRTCError(error_or_transceiver.error(), exception_state);
    return nullptr;
  }

  auto web_transceiver = error_or_transceiver.MoveValue();

  // The track must be known to the peer connection when performing
  // CreateOrUpdateSender() below.
  RegisterTrack(track);

  auto stream_ids = web_transceiver->Sender()->StreamIds();
  RTCRtpSender* sender;
  if (sdp_semantics_ == webrtc::SdpSemantics::kPlanB) {
    DCHECK_EQ(web_transceiver->ImplementationType(),
              WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly);
    sender = CreateOrUpdateSender(web_transceiver->Sender(), track->kind());
  } else {
    DCHECK_EQ(sdp_semantics_, webrtc::SdpSemantics::kUnifiedPlan);
    DCHECK_EQ(web_transceiver->ImplementationType(),
              WebRTCRtpTransceiverImplementationType::kFullTransceiver);
    RTCRtpTransceiver* transceiver =
        CreateOrUpdateTransceiver(std::move(web_transceiver));
    sender = transceiver->sender();
  }
  // Newly created senders have no streams set, we have to set it ourselves.
  sender->set_streams(streams);

  // The stream IDs should match between layers, with one exception;
  // in Plan B if no stream was supplied, the lower layer still generates a
  // stream which has no blink layer correspondence.
  DCHECK(sdp_semantics_ != webrtc::SdpSemantics::kPlanB ||
         (streams.size() == 0u && stream_ids.size() == 1u) ||
         stream_ids.size() == streams.size());
  DCHECK(sdp_semantics_ != webrtc::SdpSemantics::kUnifiedPlan ||
         stream_ids.size() == streams.size());
  return sender;
}

void RTCPeerConnection::removeTrack(RTCRtpSender* sender,
                                    ExceptionState& exception_state) {
  DCHECK(sender);
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
    return;
  auto* it = FindSender(*sender->web_sender());
  if (it == rtp_senders_.end()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The sender was not created by this peer connection.");
    return;
  }

  auto error_or_transceiver = peer_handler_->RemoveTrack(sender->web_sender());
  if (sdp_semantics_ == webrtc::SdpSemantics::kPlanB) {
    // Plan B: Was the sender removed?
    if (!error_or_transceiver.ok()) {
      // Operation aborted. This indicates that the sender is no longer used by
      // the peer connection, i.e. that it was removed due to setting a remote
      // description of type "rollback".
      return;
    }
    // Successfully removing the track results in the sender's track property
    // being nulled.
    DCHECK(!sender->web_sender()->Track());
    sender->SetTrack(nullptr);
    rtp_senders_.erase(it);
  } else {
    // Unified Plan: Was the transceiver updated?
    DCHECK_EQ(sdp_semantics_, webrtc::SdpSemantics::kUnifiedPlan);
    if (!error_or_transceiver.ok()) {
      ThrowExceptionFromRTCError(error_or_transceiver.error(), exception_state);
      return;
    }
    CreateOrUpdateTransceiver(error_or_transceiver.MoveValue());
  }
}

RTCDataChannel* RTCPeerConnection::createDataChannel(
    ScriptState* script_state,
    String label,
    const RTCDataChannelInit& data_channel_dict,
    ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
    return nullptr;

  WebRTCDataChannelInit init;
  init.ordered = data_channel_dict.ordered();
  ExecutionContext* context = ExecutionContext::From(script_state);
  // maxPacketLifeTime and maxRetransmitTime are two names for the same thing,
  // but maxPacketLifeTime is the standardized name so it takes precedence.
  if (data_channel_dict.hasMaxPacketLifeTime()) {
    UseCounter::Count(
        context,
        WebFeature::kRTCPeerConnectionCreateDataChannelMaxPacketLifeTime);
    init.max_retransmit_time = data_channel_dict.maxPacketLifeTime();
  } else if (data_channel_dict.hasMaxRetransmitTime()) {
    Deprecation::CountDeprecation(
        context, WebFeature::kRTCDataChannelInitMaxRetransmitTime);
    UseCounter::Count(
        context,
        WebFeature::kRTCPeerConnectionCreateDataChannelMaxRetransmitTime);
    init.max_retransmit_time = data_channel_dict.maxRetransmitTime();
  }
  if (data_channel_dict.hasMaxRetransmits()) {
    UseCounter::Count(
        context, WebFeature::kRTCPeerConnectionCreateDataChannelMaxRetransmits);
    init.max_retransmits = data_channel_dict.maxRetransmits();
  }
  init.protocol = data_channel_dict.protocol();
  init.negotiated = data_channel_dict.negotiated();
  if (data_channel_dict.hasId())
    init.id = data_channel_dict.id();

  RTCDataChannel* channel = RTCDataChannel::Create(
      GetExecutionContext(), peer_handler_.get(), label, init, exception_state);
  if (exception_state.HadException())
    return nullptr;
  RTCDataChannel::ReadyState handler_state = channel->GetHandlerState();
  if (handler_state != RTCDataChannel::kReadyStateConnecting) {
    // There was an early state transition.  Don't miss it!
    channel->DidChangeReadyState(handler_state);
  }
  has_data_channels_ = true;

  return channel;
}

MediaStreamTrack* RTCPeerConnection::GetTrack(
    const WebMediaStreamTrack& web_track) const {
  return tracks_.at(static_cast<MediaStreamComponent*>(web_track));
}

RTCRtpSender* RTCPeerConnection::FindSenderForTrackAndStream(
    MediaStreamTrack* track,
    MediaStream* stream) {
  for (const auto& rtp_sender : rtp_senders_) {
    if (rtp_sender->track() == track) {
      auto streams = rtp_sender->streams();
      if (streams.size() == 1u && streams[0] == stream)
        return rtp_sender;
    }
  }
  return nullptr;
}

HeapVector<Member<RTCRtpSender>>::iterator RTCPeerConnection::FindSender(
    const WebRTCRtpSender& web_sender) {
  for (auto* it = rtp_senders_.begin(); it != rtp_senders_.end(); ++it) {
    if ((*it)->web_sender()->Id() == web_sender.Id())
      return it;
  }
  return rtp_senders_.end();
}

HeapVector<Member<RTCRtpReceiver>>::iterator RTCPeerConnection::FindReceiver(
    const WebRTCRtpReceiver& web_receiver) {
  for (auto* it = rtp_receivers_.begin(); it != rtp_receivers_.end(); ++it) {
    if ((*it)->web_receiver().Id() == web_receiver.Id())
      return it;
  }
  return rtp_receivers_.end();
}

HeapVector<Member<RTCRtpTransceiver>>::iterator
RTCPeerConnection::FindTransceiver(
    const WebRTCRtpTransceiver& web_transceiver) {
  for (auto* it = transceivers_.begin(); it != transceivers_.end(); ++it) {
    if ((*it)->web_transceiver()->Id() == web_transceiver.Id())
      return it;
  }
  return transceivers_.end();
}

RTCRtpSender* RTCPeerConnection::CreateOrUpdateSender(
    std::unique_ptr<WebRTCRtpSender> web_sender,
    String kind) {
  // The track corresponding to |web_track| must already be known to us by being
  // in |tracks_|, as is a prerequisite of CreateOrUpdateSender().
  WebMediaStreamTrack web_track = web_sender->Track();
  MediaStreamTrack* track;
  if (web_track.IsNull()) {
    track = nullptr;
  } else {
    track = tracks_.at(web_track);
    DCHECK(track);
  }

  // Create or update sender. If the web sender has stream IDs the sender's
  // streams need to be set separately outside of this method.
  auto* sender_it = FindSender(*web_sender);
  RTCRtpSender* sender;
  if (sender_it == rtp_senders_.end()) {
    // Create new sender (with empty stream set).
    sender = new RTCRtpSender(this, std::move(web_sender), kind, track, {});
    rtp_senders_.push_back(sender);
  } else {
    // Update existing sender (not touching the stream set).
    sender = *sender_it;
    DCHECK_EQ(sender->web_sender()->Id(), web_sender->Id());
    sender->SetTrack(track);
  }
  return sender;
}

RTCRtpReceiver* RTCPeerConnection::CreateOrUpdateReceiver(
    std::unique_ptr<WebRTCRtpReceiver> web_receiver) {
  auto* receiver_it = FindReceiver(*web_receiver);
  // Create track.
  MediaStreamTrack* track;
  if (receiver_it == rtp_receivers_.end()) {
    track =
        MediaStreamTrack::Create(GetExecutionContext(), web_receiver->Track());
    RegisterTrack(track);
  } else {
    track = (*receiver_it)->track();
  }

  // Create or update receiver. If the web receiver has stream IDs the
  // receiver's streams need to be set separately outside of this method.
  RTCRtpReceiver* receiver;
  if (receiver_it == rtp_receivers_.end()) {
    // Create new receiver.
    receiver = new RTCRtpReceiver(std::move(web_receiver), track, {});
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
    DCHECK_EQ(receiver->web_receiver().Id(), web_receiver->Id());
    DCHECK_EQ(receiver->track(), track);  // Its track should never change.
  }
  return receiver;
}

RTCRtpTransceiver* RTCPeerConnection::CreateOrUpdateTransceiver(
    std::unique_ptr<WebRTCRtpTransceiver> web_transceiver) {
  String kind = (web_transceiver->Receiver()->Track().Source().GetType() ==
                 WebMediaStreamSource::kTypeAudio)
                    ? "audio"
                    : "video";
  RTCRtpSender* sender = CreateOrUpdateSender(web_transceiver->Sender(), kind);
  RTCRtpReceiver* receiver =
      CreateOrUpdateReceiver(web_transceiver->Receiver());

  RTCRtpTransceiver* transceiver;
  auto* transceiver_it = FindTransceiver(*web_transceiver);
  if (transceiver_it == transceivers_.end()) {
    // Create new tranceiver.
    transceiver = new RTCRtpTransceiver(this, std::move(web_transceiver),
                                        sender, receiver);
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

RTCDTMFSender* RTCPeerConnection::createDTMFSender(
    MediaStreamTrack* track,
    ExceptionState& exception_state) {
  if (ThrowExceptionIfSignalingStateClosed(signaling_state_, exception_state))
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
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed)
    return;

  CloseInternal();
}

void RTCPeerConnection::RegisterTrack(MediaStreamTrack* track) {
  DCHECK(track);
  tracks_.insert(track->Component(), track);
}

void RTCPeerConnection::NoteSdpCreated(const RTCSessionDescription& desc) {
  if (desc.type() == "offer") {
    last_offer_ = desc.sdp();
  } else if (desc.type() == "answer") {
    last_answer_ = desc.sdp();
  }
}

void RTCPeerConnection::OnStreamAddTrack(MediaStream* stream,
                                         MediaStreamTrack* track) {
  ExceptionState exception_state(v8::Isolate::GetCurrent(),
                                 ExceptionState::kExecutionContext, nullptr,
                                 nullptr);
  MediaStreamVector streams;
  streams.push_back(stream);
  addTrack(track, streams, exception_state);
  // If addTrack() failed most likely the track already has a sender and this is
  // a NO-OP or the connection is closed. The exception can be suppressed, there
  // is nothing to do.
  exception_state.ClearException();
}

void RTCPeerConnection::OnStreamRemoveTrack(MediaStream* stream,
                                            MediaStreamTrack* track) {
  auto* sender = FindSenderForTrackAndStream(track, stream);
  if (sender) {
    ExceptionState exception_state(v8::Isolate::GetCurrent(),
                                   ExceptionState::kExecutionContext, nullptr,
                                   nullptr);
    removeTrack(sender, exception_state);
    // If removeTracl() failed most likely the connection is closed. The
    // exception can be suppressed, there is nothing to do.
    exception_state.ClearException();
  }
}

void RTCPeerConnection::NegotiationNeeded() {
  DCHECK(!closed_);
  negotiation_needed_ = true;
  Microtask::EnqueueMicrotask(
      WTF::Bind(&RTCPeerConnection::MaybeFireNegotiationNeeded,
                WrapWeakPersistent(this)));
}

void RTCPeerConnection::MaybeFireNegotiationNeeded() {
  if (!negotiation_needed_ || closed_)
    return;
  negotiation_needed_ = false;
  DispatchEvent(*Event::Create(EventTypeNames::negotiationneeded));
}

void RTCPeerConnection::DidGenerateICECandidate(
    scoped_refptr<WebRTCICECandidate> web_candidate) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  DCHECK(web_candidate);
  RTCIceCandidate* ice_candidate =
      RTCIceCandidate::Create(std::move(web_candidate));
  ScheduleDispatchEvent(RTCPeerConnectionIceEvent::Create(ice_candidate));
}

void RTCPeerConnection::DidChangeSignalingState(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  ChangeSignalingState(new_state, true);
}

void RTCPeerConnection::DidChangeIceGatheringState(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  ChangeIceGatheringState(new_state);
}

void RTCPeerConnection::DidChangeIceConnectionState(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  ChangeIceConnectionState(new_state);
}

void RTCPeerConnection::DidAddReceiverPlanB(
    std::unique_ptr<WebRTCRtpReceiver> web_receiver) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  DCHECK_EQ(sdp_semantics_, webrtc::SdpSemantics::kPlanB);
  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed)
    return;
  // Create track.
  MediaStreamTrack* track =
      MediaStreamTrack::Create(GetExecutionContext(), web_receiver->Track());
  tracks_.insert(track->Component(), track);
  // Create or update streams.
  HeapVector<Member<MediaStream>> streams;
  for (const auto& stream_id : web_receiver->StreamIds()) {
    MediaStream* stream = getRemoteStreamById(stream_id);
    if (!stream) {
      // The stream is new, create it containing this track.
      MediaStreamComponentVector audio_track_components;
      MediaStreamTrackVector audio_tracks;
      MediaStreamComponentVector video_track_components;
      MediaStreamTrackVector video_tracks;
      if (track->Component()->Source()->GetType() ==
          MediaStreamSource::kTypeAudio) {
        audio_track_components.push_back(track->Component());
        audio_tracks.push_back(track);
      } else {
        DCHECK(track->Component()->Source()->GetType() ==
               MediaStreamSource::kTypeVideo);
        video_track_components.push_back(track->Component());
        video_tracks.push_back(track);
      }
      MediaStreamDescriptor* descriptor = MediaStreamDescriptor::Create(
          stream_id, std::move(audio_track_components),
          std::move(video_track_components));
      stream =
          MediaStream::Create(GetExecutionContext(), descriptor,
                              std::move(audio_tracks), std::move(video_tracks));
      // Schedule to fire "pc.onaddstream".
      ScheduleDispatchEvent(
          MediaStreamEvent::Create(EventTypeNames::addstream, stream));
    } else {
      // The stream already exists, add the track to it.
      // This will cause to schedule to fire "stream.onaddtrack".
      stream->AddTrackAndFireEvents(track);
    }
    streams.push_back(stream);
  }
  DCHECK(FindReceiver(*web_receiver) == rtp_receivers_.end());
  RTCRtpReceiver* rtp_receiver =
      new RTCRtpReceiver(std::move(web_receiver), track, streams);
  rtp_receivers_.push_back(rtp_receiver);
  ScheduleDispatchEvent(
      new RTCTrackEvent(rtp_receiver, rtp_receiver->track(), streams, nullptr));
}

void RTCPeerConnection::DidRemoveReceiverPlanB(
    std::unique_ptr<WebRTCRtpReceiver> web_receiver) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());
  DCHECK_EQ(sdp_semantics_, webrtc::SdpSemantics::kPlanB);

  auto* it = FindReceiver(*web_receiver);
  DCHECK(it != rtp_receivers_.end());
  RTCRtpReceiver* rtp_receiver = *it;
  auto streams = rtp_receiver->streams();
  MediaStreamTrack* track = rtp_receiver->track();
  rtp_receivers_.erase(it);

  // End streams no longer in use and fire "removestream" events. This behavior
  // is no longer in the spec.
  for (const auto& stream : streams) {
    // Remove the track.
    // This will cause to schedule to fire "stream.onremovetrack".
    stream->RemoveTrackAndFireEvents(track);

    // Was this the last usage of the stream? Remove from remote streams.
    if (!IsRemoteStream(stream)) {
      // TODO(hbos): The stream should already have ended by being empty, no
      // need for |StreamEnded|.
      stream->StreamEnded();
      stream->UnregisterObserver(this);
      ScheduleDispatchEvent(
          MediaStreamEvent::Create(EventTypeNames::removestream, stream));
    }
  }

  // Mute track and fire "onmute" if not already muted.
  track->Component()->Source()->SetReadyState(
      MediaStreamSource::kReadyStateMuted);
}

void RTCPeerConnection::DidModifyTransceivers(
    std::vector<std::unique_ptr<WebRTCRtpTransceiver>> web_transceivers,
    bool is_remote_description) {
  HeapVector<Member<MediaStreamTrack>> mute_tracks;
  HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>
      remove_list;
  HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>> add_list;
  HeapVector<Member<RTCRtpTransceiver>> track_events;
  MediaStreamVector previous_streams = getRemoteStreams();
  for (auto& web_transceiver : web_transceivers) {
    auto* it = FindTransceiver(*web_transceiver);
    bool previously_had_recv =
        (it != transceivers_.end()) ? (*it)->FiredDirectionHasRecv() : false;
    RTCRtpTransceiver* transceiver =
        CreateOrUpdateTransceiver(std::move(web_transceiver));

    // The transceiver is now up-to-date. Compare before/after values of
    // FiredDirectionHasRecv() and process the remote track if it changed.
    if (is_remote_description && !previously_had_recv &&
        transceiver->FiredDirectionHasRecv()) {
      ProcessAdditionOfRemoteTrack(
          transceiver, transceiver->web_transceiver()->Receiver()->StreamIds(),
          &add_list, &track_events);
    }
    if (previously_had_recv && !transceiver->FiredDirectionHasRecv()) {
      ProcessRemovalOfRemoteTrack(transceiver, &remove_list, &mute_tracks);
    }
  }
  MediaStreamVector current_streams = getRemoteStreams();

  for (auto& track : mute_tracks) {
    // Mute the track. Fires "track.onmute" synchronously.
    track->Component()->Source()->SetReadyState(
        MediaStreamSource::kReadyStateMuted);
  }
  // Remove/add tracks to streams, this fires "stream.onremovetrack" and
  // "stream.onaddtrack" asynchronously (delayed with ScheduleDispatchEvent()).
  // This means that the streams will be updated immediately, but the
  // corresponding events will fire after "pc.ontrack".
  // TODO(https://crbug.com/788558): These should probably also fire
  // synchronously (before "pc.ontrack"). The webrtc-pc spec references the
  // mediacapture-streams spec for adding and removing tracks to streams, which
  // adds/removes and fires synchronously, but it says to do this in a queued
  // task, which would lead to unexpected behavior: the streams would be empty
  // at "pc.ontrack".
  for (auto& pair : remove_list) {
    auto& stream = pair.first;
    auto& track = pair.second;
    if (stream->getTracks().Contains(track)) {
      stream->RemoveTrackAndFireEvents(track);
    }
  }
  for (auto& pair : add_list) {
    auto& stream = pair.first;
    auto& track = pair.second;
    if (!stream->getTracks().Contains(track)) {
      stream->AddTrackAndFireEvents(track);
    }
  }

  // Legacy APIs: "pc.onaddstream" and "pc.onremovestream".
  for (const auto& current_stream : current_streams) {
    if (!previous_streams.Contains(current_stream)) {
      ScheduleDispatchEvent(
          MediaStreamEvent::Create(EventTypeNames::addstream, current_stream));
    }
  }
  for (const auto& previous_stream : previous_streams) {
    if (!current_streams.Contains(previous_stream)) {
      ScheduleDispatchEvent(MediaStreamEvent::Create(
          EventTypeNames::removestream, previous_stream));
    }
  }

  // Fire "pc.ontrack" synchronously.
  for (auto& transceiver : track_events) {
    auto* track_event = new RTCTrackEvent(
        transceiver->receiver(), transceiver->receiver()->track(),
        transceiver->receiver()->streams(), transceiver);
    DispatchEvent(*track_event);
  }

  // Unmute "pc.ontrack" tracks. Fires "track.onunmute" synchronously.
  // TODO(https://crbug.com/889487): The correct thing to do is to unmute in
  // response to receiving RTP packets.
  for (auto& transceiver : track_events) {
    transceiver->receiver()->track()->Component()->Source()->SetReadyState(
        MediaStreamSource::kReadyStateLive);
  }
}

void RTCPeerConnection::ProcessAdditionOfRemoteTrack(
    RTCRtpTransceiver* transceiver,
    const WebVector<WebString>& stream_ids,
    HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
        add_list,
    HeapVector<Member<RTCRtpTransceiver>>* track_events) {
  SetAssociatedMediaStreams(transceiver->receiver(), stream_ids, nullptr,
                            add_list);
  track_events->push_back(transceiver);
}

void RTCPeerConnection::ProcessRemovalOfRemoteTrack(
    RTCRtpTransceiver* transceiver,
    HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
        remove_list,
    HeapVector<Member<MediaStreamTrack>>* mute_tracks) {
  WebVector<WebString> stream_ids = {};
  SetAssociatedMediaStreams(transceiver->receiver(), stream_ids, remove_list,
                            nullptr);
  if (!transceiver->receiver()->track()->muted())
    mute_tracks->push_back(transceiver->receiver()->track());
}

void RTCPeerConnection::SetAssociatedMediaStreams(
    RTCRtpReceiver* receiver,
    const WebVector<WebString>& stream_ids,
    HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
        remove_list,
    HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
        add_list) {
  MediaStreamVector known_streams = getRemoteStreams();

  MediaStreamVector streams;
  for (const auto& stream_id : stream_ids) {
    MediaStream* curr_stream = nullptr;
    for (const auto& known_stream : known_streams) {
      if (static_cast<WebString>(known_stream->id()) == stream_id) {
        curr_stream = known_stream;
        break;
      }
    }
    if (!curr_stream) {
      curr_stream = MediaStream::Create(
          GetExecutionContext(), MediaStreamDescriptor::Create(
                                     static_cast<String>(stream_id), {}, {}));
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
    WebRTCDataChannelHandler* handler) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());

  if (signaling_state_ ==
      webrtc::PeerConnectionInterface::SignalingState::kClosed)
    return;

  RTCDataChannel* channel =
      RTCDataChannel::Create(GetExecutionContext(), base::WrapUnique(handler));
  ScheduleDispatchEvent(
      RTCDataChannelEvent::Create(EventTypeNames::datachannel, channel));
  has_data_channels_ = true;
}

void RTCPeerConnection::DidNoteInterestingUsage(int usage_pattern) {
  Document* document = To<Document>(GetExecutionContext());
  ukm::SourceId source_id = document->UkmSourceID();
  ukm::builders::WebRTC_AddressHarvesting(source_id)
      .SetUsagePattern(usage_pattern)
      .Record(document->UkmRecorder());
}

void RTCPeerConnection::ReleasePeerConnectionHandler() {
  if (stopped_)
    return;

  stopped_ = true;
  ice_connection_state_ = webrtc::PeerConnectionInterface::kIceConnectionClosed;
  signaling_state_ = webrtc::PeerConnectionInterface::SignalingState::kClosed;

  dispatch_scheduled_event_runner_->Stop();

  peer_handler_.reset();

  connection_handle_for_scheduler_.reset();
}

void RTCPeerConnection::ClosePeerConnection() {
  DCHECK(signaling_state_ !=
         webrtc::PeerConnectionInterface::SignalingState::kClosed);
  CloseInternal();
}

const AtomicString& RTCPeerConnection::InterfaceName() const {
  return EventTargetNames::RTCPeerConnection;
}

ExecutionContext* RTCPeerConnection::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

void RTCPeerConnection::Pause() {
  dispatch_scheduled_event_runner_->Pause();
}

void RTCPeerConnection::Unpause() {
  dispatch_scheduled_event_runner_->Unpause();
}

void RTCPeerConnection::ContextDestroyed(ExecutionContext*) {
  ReleasePeerConnectionHandler();
}

void RTCPeerConnection::ChangeSignalingState(
    webrtc::PeerConnectionInterface::SignalingState signaling_state,
    bool dispatch_event_immediately) {
  if (signaling_state_ == signaling_state)
    return;
  if (signaling_state_ !=
      webrtc::PeerConnectionInterface::SignalingState::kClosed) {
    signaling_state_ = signaling_state;
    Event* event = Event::Create(EventTypeNames::signalingstatechange);
    if (dispatch_event_immediately)
      DispatchEvent(*event);
    else
      ScheduleDispatchEvent(event);
  }
}

void RTCPeerConnection::ChangeIceGatheringState(
    webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state) {
  if (ice_connection_state_ !=
      webrtc::PeerConnectionInterface::kIceConnectionClosed) {
    ScheduleDispatchEvent(
        Event::Create(EventTypeNames::icegatheringstatechange),
        WTF::Bind(&RTCPeerConnection::SetIceGatheringState,
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
  if (ice_connection_state_ !=
      webrtc::PeerConnectionInterface::kIceConnectionClosed) {
    ScheduleDispatchEvent(
        Event::Create(EventTypeNames::iceconnectionstatechange),
        WTF::Bind(&RTCPeerConnection::SetIceConnectionState,
                  WrapPersistent(this), ice_connection_state));
  }
}

bool RTCPeerConnection::SetIceConnectionState(
    webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state) {
  if (ice_connection_state_ !=
          webrtc::PeerConnectionInterface::kIceConnectionClosed &&
      ice_connection_state_ != ice_connection_state) {
    ice_connection_state_ = ice_connection_state;
    if (ice_connection_state_ ==
        webrtc::PeerConnectionInterface::kIceConnectionConnected)
      RecordRapporMetrics();

    return true;
  }
  return false;
}

void RTCPeerConnection::CloseInternal() {
  DCHECK(signaling_state_ !=
         webrtc::PeerConnectionInterface::SignalingState::kClosed);
  peer_handler_->Stop();
  closed_ = true;

  ChangeIceConnectionState(
      webrtc::PeerConnectionInterface::kIceConnectionClosed);
  ChangeSignalingState(webrtc::PeerConnectionInterface::SignalingState::kClosed,
                       false);
  for (auto& transceiver : transceivers_) {
    transceiver->OnPeerConnectionClosed();
  }

  Document* document = To<Document>(GetExecutionContext());
  HostsUsingFeatures::CountAnyWorld(
      *document, HostsUsingFeatures::Feature::kRTCPeerConnectionUsed);

  connection_handle_for_scheduler_.reset();
}

void RTCPeerConnection::ScheduleDispatchEvent(Event* event) {
  ScheduleDispatchEvent(event, BoolFunction());
}

void RTCPeerConnection::ScheduleDispatchEvent(Event* event,
                                              BoolFunction setup_function) {
  scheduled_events_.push_back(
      new EventWrapper(event, std::move(setup_function)));

  dispatch_scheduled_event_runner_->RunAsync();
}

void RTCPeerConnection::DispatchScheduledEvent() {
  if (stopped_)
    return;

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

void RTCPeerConnection::RecordRapporMetrics() {
  Document* document = To<Document>(GetExecutionContext());
  for (const auto& component : tracks_.Keys()) {
    switch (component->Source()->GetType()) {
      case MediaStreamSource::kTypeAudio:
        HostsUsingFeatures::CountAnyWorld(
            *document, HostsUsingFeatures::Feature::kRTCPeerConnectionAudio);
        break;
      case MediaStreamSource::kTypeVideo:
        HostsUsingFeatures::CountAnyWorld(
            *document, HostsUsingFeatures::Feature::kRTCPeerConnectionVideo);
        break;
      default:
        NOTREACHED();
    }
  }

  if (has_data_channels_)
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::kRTCPeerConnectionDataChannel);
}

void RTCPeerConnection::Trace(blink::Visitor* visitor) {
  visitor->Trace(tracks_);
  visitor->Trace(rtp_senders_);
  visitor->Trace(rtp_receivers_);
  visitor->Trace(transceivers_);
  visitor->Trace(dispatch_scheduled_event_runner_);
  visitor->Trace(scheduled_events_);
  EventTargetWithInlineData::Trace(visitor);
  PausableObject::Trace(visitor);
  MediaStreamObserver::Trace(visitor);
}

int RTCPeerConnection::PeerConnectionCount() {
  return InstanceCounters::CounterValue(
      InstanceCounters::kRTCPeerConnectionCounter);
}

int RTCPeerConnection::PeerConnectionCountLimit() {
  return kMaxPeerConnections;
}

}  // namespace blink
