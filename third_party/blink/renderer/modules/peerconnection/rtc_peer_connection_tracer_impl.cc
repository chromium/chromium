// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_tracer_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_answer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/data_channel_interface.h"
#include "third_party/webrtc/api/jsep.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/media_types.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/api/rtp_receiver_interface.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace blink {

namespace {

// PeerConnectionTracker forwards several of these strings straight into the
// non-nullable `string` fields of mojom::PeerConnectionTrackerHost, and
// String::FromUtf8() yields a *null* String for an empty span. Funnel every
// such value through here so serialization never sees a null String.
String ToNonNullString(std::string_view text) {
  String result = String::FromUtf8(text);
  return result.IsNull() ? g_empty_string : result;
}

// Every getter here is a BYPASS_PROXY_* method on the data channel proxy (see
// pc/sctp_data_channel.cc): a direct read, safe on the signaling thread. id()
// is not, which is why the tracer interface passes `id` alongside.
PeerConnectionTracker::DataChannelInfo SnapshotDataChannel(
    const webrtc::DataChannelInterface& channel,
    std::optional<int> id) {
  PeerConnectionTracker::DataChannelInfo info;
  info.label = ToNonNullString(channel.label());
  info.ordered = channel.ordered();
  info.max_packet_life_time = channel.maxPacketLifeTime();
  info.max_retransmits = channel.maxRetransmitsOpt();
  info.protocol = ToNonNullString(channel.protocol());
  info.negotiated = channel.negotiated();
  info.id = id;
  return info;
}

// Builds the JSON value webrtc-internals shows for a session description:
// { "type": ..., "sdp": ... }. Mirrors the formatting that
// RTCPeerConnectionHandler's CreateSessionDescriptionRequest uses.
String SerializeSessionDescription(
    const webrtc::SessionDescriptionInterface& description) {
  std::string sdp;
  description.ToString(&sdp);
  auto json = std::make_unique<JSONObject>();
  json->SetString("type", String::FromUtf8(description.type()));
  if (!sdp.empty()) {
    json->SetString("sdp", String::FromUtf8(sdp));
  }
  StringBuilder result;
  json->WriteJSON(&result);
  return result.ToString();
}

// Serializes the fields RTCIceCandidatePlatform needs, so that the garbage
// collected object itself can be allocated on the main thread.
struct IceCandidateFields {
  String sdp;
  String sdp_mid;
  std::optional<uint16_t> sdp_mline_index;
  String username_fragment;
  String url;
};

IceCandidateFields SerializeIceCandidate(
    const webrtc::IceCandidate& candidate) {
  IceCandidateFields fields;
  fields.sdp = String::FromUtf8(candidate.ToString());
  fields.sdp_mid = String::FromUtf8(candidate.sdp_mid());
  if (candidate.sdp_mline_index() >= 0) {
    fields.sdp_mline_index = static_cast<uint16_t>(candidate.sdp_mline_index());
  }
  fields.username_fragment = String::FromUtf8(candidate.candidate().username());
  fields.url = String::FromUtf8(candidate.server_url());
  return fields;
}

}  // namespace

RTCPeerConnectionTracerImpl::RTCPeerConnectionTracerImpl(
    CrossThreadWeakHandle<PeerConnectionTracker> tracker,
    base::WeakPtr<RTCPeerConnectionHandler> handler,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread)
    : tracker_(std::move(tracker)),
      handler_(std::move(handler)),
      main_thread_(std::move(main_thread)) {
  DETACH_FROM_SEQUENCE(signaling_sequence_checker_);
}

RTCPeerConnectionTracerImpl::~RTCPeerConnectionTracerImpl() = default;

// static
void RTCPeerConnectionTracerImpl::RunTrackerTask(
    PeerConnectionTracker* tracker,
    base::WeakPtr<RTCPeerConnectionHandler> handler,
    TrackerTask task) {
  if (!tracker || !handler) {
    return;
  }
  std::move(task).Run(tracker, handler.get());
}

void RTCPeerConnectionTracerImpl::PostToTracker(TrackerTask task) {
  PostCrossThreadTask(
      *main_thread_, FROM_HERE,
      CrossThreadBindOnce(&RTCPeerConnectionTracerImpl::RunTrackerTask,
                          MakeUnwrappingCrossThreadWeakHandle(tracker_),
                          handler_, std::move(task)));
}

void RTCPeerConnectionTracerImpl::OnCreate(
    const webrtc::PeerConnectionInterface::RTCConfiguration& configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  // Intentionally not forwarded.
  // PeerConnectionTracker::RegisterPeerConnection() already reports the
  // creation, with the same SerializeConfiguration() output plus the local id,
  // the frame URL and the renderer pid, none of which libWebRTC knows about.
  // Forwarding this would duplicate it.
}

void RTCPeerConnectionTracerImpl::OnCreateOffer(
    const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](int32_t offer_to_receive_video, int32_t offer_to_receive_audio,
         bool voice_activity_detection, bool ice_restart,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackCreateOffer(
            handler, MakeGarbageCollected<RTCOfferOptionsPlatform>(
                         offer_to_receive_video, offer_to_receive_audio,
                         voice_activity_detection, ice_restart));
      },
      options.offer_to_receive_video, options.offer_to_receive_audio,
      options.voice_activity_detection, options.ice_restart));
}

void RTCPeerConnectionTracerImpl::OnCreateOfferSuccess(
    const webrtc::SessionDescriptionInterface* description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  if (!description) {
    return;
  }
  PostToTracker(CrossThreadBindOnce(
      [](String value, String session_id, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler, PeerConnectionTracker::kActionCreateOffer, "OnSuccess",
            value);
        if (!session_id.empty()) {
          tracker->TrackSessionId(handler, session_id);
        }
      },
      SerializeSessionDescription(*description),
      String::FromUtf8(description->session_id())));
}

void RTCPeerConnectionTracerImpl::OnCreateOfferFailure(
    const webrtc::RTCError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](String message, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler, PeerConnectionTracker::kActionCreateOffer, "OnFailure",
            message);
      },
      ToNonNullString(error.message())));
}

void RTCPeerConnectionTracerImpl::OnCreateAnswer(
    const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](bool voice_activity_detection, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackCreateAnswer(
            handler, MakeGarbageCollected<RTCAnswerOptionsPlatform>(
                         voice_activity_detection));
      },
      options.voice_activity_detection));
}

void RTCPeerConnectionTracerImpl::OnCreateAnswerSuccess(
    const webrtc::SessionDescriptionInterface* description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  if (!description) {
    return;
  }
  PostToTracker(CrossThreadBindOnce(
      [](String value, String session_id, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler, PeerConnectionTracker::kActionCreateAnswer, "OnSuccess",
            value);
        if (!session_id.empty()) {
          tracker->TrackSessionId(handler, session_id);
        }
      },
      SerializeSessionDescription(*description),
      String::FromUtf8(description->session_id())));
}

void RTCPeerConnectionTracerImpl::OnCreateAnswerFailure(
    const webrtc::RTCError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](String message, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler, PeerConnectionTracker::kActionCreateAnswer, "OnFailure",
            message);
      },
      ToNonNullString(error.message())));
}

void RTCPeerConnectionTracerImpl::OnSetLocalDescription(
    const webrtc::SessionDescriptionInterface* description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  // A null description means the no-argument setLocalDescription() overload,
  // where the PeerConnection produces the SDP itself.
  set_local_description_is_implicit_ = !description;
  if (!description) {
    PostToTracker(CrossThreadBindOnce(
        [](PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
          tracker->TrackSetSessionDescriptionImplicit(handler);
        }));
    return;
  }
  std::string sdp;
  description->ToString(&sdp);
  PostToTracker(CrossThreadBindOnce(
      [](String sdp, String type, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSetSessionDescription(
            handler, sdp, type, PeerConnectionTracker::kSourceLocal);
      },
      String::FromUtf8(sdp), String::FromUtf8(description->type())));
}

void RTCPeerConnectionTracerImpl::OnSetLocalDescriptionSuccess(
    const webrtc::SessionDescriptionInterface* description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  // webrtc-internals reports the implicit form under its own action, and only
  // that form carries the SDP that was generated - for the explicit form the
  // page already logged the SDP the caller supplied.
  const bool is_implicit = set_local_description_is_implicit_;
  set_local_description_is_implicit_ = false;
  String value = g_empty_string;
  if (is_implicit && description) {
    value = SerializeSessionDescription(*description);
  }
  PostToTracker(CrossThreadBindOnce(
      [](bool is_implicit, String value, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler,
            is_implicit
                ? PeerConnectionTracker::kActionSetLocalDescriptionImplicit
                : PeerConnectionTracker::kActionSetLocalDescription,
            "OnSuccess", value);
      },
      is_implicit, std::move(value)));
}

void RTCPeerConnectionTracerImpl::OnSetLocalDescriptionFailure(
    const webrtc::RTCError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  const bool is_implicit = set_local_description_is_implicit_;
  set_local_description_is_implicit_ = false;
  PostToTracker(CrossThreadBindOnce(
      [](bool is_implicit, String message, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler,
            is_implicit
                ? PeerConnectionTracker::kActionSetLocalDescriptionImplicit
                : PeerConnectionTracker::kActionSetLocalDescription,
            "OnFailure", message);
      },
      is_implicit, ToNonNullString(error.message())));
}

void RTCPeerConnectionTracerImpl::OnSetRemoteDescription(
    const webrtc::SessionDescriptionInterface* description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  if (!description) {
    return;
  }
  std::string sdp;
  description->ToString(&sdp);
  PostToTracker(CrossThreadBindOnce(
      [](String sdp, String type, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSetSessionDescription(
            handler, sdp, type, PeerConnectionTracker::kSourceRemote);
      },
      String::FromUtf8(sdp), String::FromUtf8(description->type())));
}

void RTCPeerConnectionTracerImpl::OnSetRemoteDescriptionSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler, PeerConnectionTracker::kActionSetRemoteDescription,
            "OnSuccess", g_empty_string);
      }));
}

void RTCPeerConnectionTracerImpl::OnSetRemoteDescriptionFailure(
    const webrtc::RTCError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](String message, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackSessionDescriptionCallback(
            handler, PeerConnectionTracker::kActionSetRemoteDescription,
            "OnFailure", message);
      },
      ToNonNullString(error.message())));
}

void RTCPeerConnectionTracerImpl::OnSetConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration& configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration>
             configuration,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackSetConfiguration(handler, *configuration);
      },
      std::make_unique<webrtc::PeerConnectionInterface::RTCConfiguration>(
          configuration)));
}

void RTCPeerConnectionTracerImpl::OnRestartIce() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackRestartIce(handler);
      }));
}

void RTCPeerConnectionTracerImpl::OnClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  // Intentionally not forwarded. This fires on the signaling thread inside
  // webrtc::PeerConnection::Close(), so the task it would post lands after
  // RTCPeerConnectionHandler::Close() has already called
  // PeerConnectionTracker::UnregisterPeerConnection() and the local id is
  // gone. That handler reports the close directly instead.
}

void RTCPeerConnectionTracerImpl::OnIceCandidate(
    const webrtc::IceCandidate& candidate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  IceCandidateFields fields = SerializeIceCandidate(candidate);
  PostToTracker(CrossThreadBindOnce(
      [](String sdp, String sdp_mid, std::optional<uint16_t> sdp_mline_index,
         String username_fragment, String url, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackAddIceCandidate(
            handler,
            MakeGarbageCollected<RTCIceCandidatePlatform>(
                sdp, sdp_mid, sdp_mline_index, username_fragment, url),
            PeerConnectionTracker::kSourceLocal);
      },
      std::move(fields.sdp), std::move(fields.sdp_mid), fields.sdp_mline_index,
      std::move(fields.username_fragment), std::move(fields.url)));
}

void RTCPeerConnectionTracerImpl::OnAddIceCandidate(
    const webrtc::IceCandidate& candidate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  IceCandidateFields fields = SerializeIceCandidate(candidate);
  PostToTracker(CrossThreadBindOnce(
      [](String sdp, String sdp_mid, std::optional<uint16_t> sdp_mline_index,
         String username_fragment, String url, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackAddIceCandidate(
            handler,
            MakeGarbageCollected<RTCIceCandidatePlatform>(
                sdp, sdp_mid, sdp_mline_index, username_fragment, url),
            PeerConnectionTracker::kSourceRemote);
      },
      std::move(fields.sdp), std::move(fields.sdp_mid), fields.sdp_mline_index,
      std::move(fields.username_fragment), std::move(fields.url)));
}

void RTCPeerConnectionTracerImpl::OnAddIceCandidateSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  // Intentionally not forwarded. chrome://webrtc-internals has no success entry
  // for addIceCandidate; the addIceCandidate entry stands on its own and only
  // the failure is called out.
}

void RTCPeerConnectionTracerImpl::OnAddIceCandidateFailure(
    const webrtc::RTCError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](String message, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackAddIceCandidateFailed(handler, message);
      },
      ToNonNullString(error.message())));
}

void RTCPeerConnectionTracerImpl::OnIceCandidateError(
    absl::string_view address,
    int port,
    absl::string_view url,
    int error_code,
    absl::string_view error_text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  // Matches the framing of RTCPeerConnectionHandler::Observer: the address is
  // suppressed when there is no port, but the host candidate string keeps it.
  String address_string = port ? String::FromUtf8(address) : String();
  std::optional<uint16_t> port_value;
  if (port) {
    port_value = static_cast<uint16_t>(port);
  }
  String host_candidate =
      StrCat({String::FromUtf8(address), ":", String::Number(port)});
  PostToTracker(CrossThreadBindOnce(
      [](String address, std::optional<uint16_t> port, String host_candidate,
         String url, int error_code, String error_text,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackIceCandidateError(handler, address, port, host_candidate,
                                        url, error_code, error_text);
      },
      std::move(address_string), port_value, std::move(host_candidate),
      ToNonNullString(url), error_code, ToNonNullString(error_text)));
}

void RTCPeerConnectionTracerImpl::OnCreateDataChannel(
    const webrtc::DataChannelInterface& channel,
    std::optional<int> id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker::DataChannelInfo info,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackCreateDataChannel(handler, info,
                                        PeerConnectionTracker::kSourceLocal);
      },
      SnapshotDataChannel(channel, id)));
}

void RTCPeerConnectionTracerImpl::OnDataChannel(
    const webrtc::DataChannelInterface& channel,
    std::optional<int> id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker::DataChannelInfo info,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackCreateDataChannel(handler, info,
                                        PeerConnectionTracker::kSourceRemote);
      },
      SnapshotDataChannel(channel, id)));
}

void RTCPeerConnectionTracerImpl::OnAddTransceiver(
    webrtc::MediaType media_type,
    const webrtc::MediaStreamTrackInterface* track,
    const webrtc::RtpTransceiverInit& init) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PeerConnectionTracker::AddTransceiverInfo info;
  info.kind = media_type == webrtc::MediaType::AUDIO ? "audio" : "video";
  if (track) {
    info.track_id = ToNonNullString(track->id());
  }
  info.direction = init.direction;
  for (const auto& stream_id : init.stream_ids) {
    info.stream_ids.push_back(ToNonNullString(stream_id));
  }
  for (const auto& encoding : init.send_encodings) {
    info.send_encodings.push_back(encoding);
  }
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker::AddTransceiverInfo info,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackAddTransceiverCall(handler, info);
      },
      std::move(info)));
}

void RTCPeerConnectionTracerImpl::OnAddTrack(
    const webrtc::MediaStreamTrackInterface& track,
    const std::vector<std::string>& stream_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PeerConnectionTracker::TrackInfo info;
  info.kind = ToNonNullString(track.kind());
  info.id = ToNonNullString(track.id());
  for (const auto& stream_id : stream_ids) {
    info.stream_ids.push_back(ToNonNullString(stream_id));
  }
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker::TrackInfo info, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackAddTrack(handler, info);
      },
      std::move(info)));
}

void RTCPeerConnectionTracerImpl::OnTrack(
    const webrtc::RtpTransceiverInterface& transceiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver =
      transceiver.receiver();
  PeerConnectionTracker::TrackInfo info;
  info.kind = ToNonNullString(receiver->track()->kind());
  info.id = ToNonNullString(receiver->track()->id());
  for (const auto& stream_id : receiver->stream_ids()) {
    info.stream_ids.push_back(ToNonNullString(stream_id));
  }
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker::TrackInfo info, PeerConnectionTracker* tracker,
         RTCPeerConnectionHandler* handler) {
        tracker->TrackOnTrack(handler, info);
      },
      std::move(info)));
}

void RTCPeerConnectionTracerImpl::OnSignalingStateChanged(
    webrtc::PeerConnectionInterface::SignalingState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](webrtc::PeerConnectionInterface::SignalingState state,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackSignalingStateChange(handler, state);
      },
      state));
}

void RTCPeerConnectionTracerImpl::OnIceConnectionStateChanged(
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  // Intentionally not forwarded. chrome://webrtc-internals reports the
  // JavaScript visible iceConnectionState, which
  // RTCPeerConnection::ComputeIceConnectionState() derives from the
  // RTCIceTransports it surfaces rather than from `state`, and which
  // RTCPeerConnectionHandler::TrackIceConnectionStateChange() reports
  // directly.
}

void RTCPeerConnectionTracerImpl::OnConnectionStateChanged(
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](webrtc::PeerConnectionInterface::PeerConnectionState state,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackConnectionStateChange(handler, state);
      },
      state));
}

void RTCPeerConnectionTracerImpl::OnIceGatheringStateChanged(
    webrtc::PeerConnectionInterface::IceGatheringState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](webrtc::PeerConnectionInterface::IceGatheringState state,
         PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackIceGatheringStateChange(handler, state);
      },
      state));
}

void RTCPeerConnectionTracerImpl::OnNegotiationNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(signaling_sequence_checker_);
  PostToTracker(CrossThreadBindOnce(
      [](PeerConnectionTracker* tracker, RTCPeerConnectionHandler* handler) {
        tracker->TrackOnRenegotiationNeeded(handler);
      }));
}

}  // namespace blink
