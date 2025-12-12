// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_answer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"

using webrtc::StatsReport;
using webrtc::StatsReports;

namespace blink {

class InternalStandardStatsObserver;

// TODO(hta): This module should be redesigned to reduce string copies.

namespace {

String SerializeGetUserMediaMediaConstraints(
    const MediaConstraints& constraints) {
  return constraints.ToString();
}

String SerializeOfferOptions(blink::RTCOfferOptionsPlatform* options) {
  if (!options) {
    return "null";
  }

  auto json = std::make_unique<JSONObject>();
  if (options->OfferToReceiveAudio()) {
    json->SetBoolean("offerToReceiveAudio", true);
  }
  if (options->OfferToReceiveVideo()) {
    json->SetBoolean("offerToReceiveVideo", true);
  }
  if (options->VoiceActivityDetection()) {
    json->SetBoolean("voiceActivityDetection", true);
  }
  if (options->IceRestart()) {
    json->SetBoolean("iceRestart", true);
  }
  StringBuilder value;
  json->WriteJSON(&value);
  return value.ToString();
}

String SerializeAnswerOptions(blink::RTCAnswerOptionsPlatform* options) {
  if (!options) {
    return "null";
  }

  auto json = std::make_unique<JSONObject>();
  if (options->VoiceActivityDetection()) {
    json->SetBoolean("voiceActivityDetection", true);
  }
  StringBuilder value;
  json->WriteJSON(&value);
  return value.ToString();
}

String SerializeDirection(webrtc::RtpTransceiverDirection direction) {
  switch (direction) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
      return "sendrecv";
    case webrtc::RtpTransceiverDirection::kSendOnly:
      return "sendonly";
    case webrtc::RtpTransceiverDirection::kRecvOnly:
      return "recvonly";
    case webrtc::RtpTransceiverDirection::kInactive:
      return "inactive";
    case webrtc::RtpTransceiverDirection::kStopped:
      return "stopped";
    default:
      NOTREACHED();
  }
}

String SerializeTransceiverKind(const RTCRtpTransceiverPlatform& transceiver) {
  DCHECK(transceiver.Receiver());
  DCHECK(transceiver.Receiver()->Track());

  switch (transceiver.Receiver()->Track()->GetSourceType()) {
    case MediaStreamSource::StreamType::kTypeAudio:
      return "audio";
    case MediaStreamSource::StreamType::kTypeVideo:
      return "video";
    default:
      NOTREACHED();
  }
}

std::unique_ptr<JSONArray> SerializeEncodingParameters(
    const std::vector<webrtc::RtpEncodingParameters>& send_encodings) {
  auto encodings = std::make_unique<JSONArray>();
  for (const auto& encoding : send_encodings) {
    auto obj = std::make_unique<JSONObject>();
    obj->SetBoolean("active", encoding.active);
    if (!encoding.rid.empty()) {
      obj->SetString("rid", String(encoding.rid));
    }
    if (encoding.max_framerate) {
      obj->SetDouble("maxFramerate", *encoding.max_framerate);
    }
    if (encoding.max_bitrate_bps) {
      obj->SetInteger("maxBitrate", *encoding.max_bitrate_bps);
    }
    if (encoding.scale_resolution_down_by) {
      obj->SetDouble("scaleResolutionDownBy",
                     *encoding.scale_resolution_down_by);
    }
    if (encoding.scale_resolution_down_to) {
      auto res = std::make_unique<JSONObject>();
      res->SetInteger("width", encoding.scale_resolution_down_to->width);
      res->SetInteger("height", encoding.scale_resolution_down_to->height);
      obj->SetObject("scaleResolutionDownTo", std::move(res));
    }
    if (encoding.scalability_mode) {
      obj->SetString("scalabilityMode", String(*encoding.scalability_mode));
    }
    if (encoding.adaptive_ptime) {
      obj->SetBoolean("adpativePtime", true);
    }
    encodings->PushObject(std::move(obj));
  }
  return encodings;
}

std::unique_ptr<JSONObject> SerializeSender(
    const blink::RTCRtpSenderPlatform& sender) {
  auto json = std::make_unique<JSONObject>();
  if (sender.Track()) {
    json->SetString("track", String(sender.Track()->Id()));
  } else {
    json->SetValue("track", JSONValue::Null());
  }

  auto stream_ids = std::make_unique<JSONArray>();
  for (const auto& stream_id : sender.StreamIds()) {
    stream_ids->PushString(String(stream_id));
  }
  json->SetArray("streams", std::move(stream_ids));

  json->SetArray("encodings", SerializeEncodingParameters(
                                  sender.GetParameters()->encodings));
  return json;
}

std::unique_ptr<JSONObject> SerializeReceiver(
    const RTCRtpReceiverPlatform& receiver) {
  auto json = std::make_unique<JSONObject>();
  DCHECK(receiver.Track());
  json->SetString("track", String(receiver.Track()->Id()));
  auto stream_ids = std::make_unique<JSONArray>();
  for (const auto& stream_id : receiver.StreamIds()) {
    stream_ids->PushString(String(stream_id));
  }
  json->SetArray("streams", std::move(stream_ids));
  return json;
}

std::unique_ptr<JSONObject> SerializeTransceiver(
    const RTCRtpTransceiverPlatform& transceiver) {
  auto json = std::make_unique<JSONObject>();
  if (transceiver.Mid().IsNull()) {
    json->SetValue("mid", JSONValue::Null());
  } else {
    json->SetString("mid", String(transceiver.Mid()));
  }
  json->SetString("kind", SerializeTransceiverKind(transceiver));
  json->SetValue("sender", SerializeSender(*transceiver.Sender()));
  json->SetValue("receiver", SerializeReceiver(*transceiver.Receiver()));
  json->SetString("direction", SerializeDirection(transceiver.Direction()));
  if (transceiver.CurrentDirection().has_value()) {
    json->SetString("currentDirection",
                    SerializeDirection(*transceiver.CurrentDirection()));
  } else {
    json->SetValue("currentDirection", JSONValue::Null());
  }
  return json;
}

// Serializes things that are of interest from the RTCConfiguration.
String SerializeConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    bool usesInsertableStreams) {
  auto json = std::make_unique<JSONObject>();
  // Serialize iceServers (without username and credential).
  if (!config.servers.empty()) {
    auto servers = std::make_unique<JSONArray>();
    for (const auto& ice_server : config.servers) {
      auto server = std::make_unique<JSONObject>();
      auto urls = std::make_unique<JSONArray>();
      for (const auto& url : ice_server.urls) {
        urls->PushString(String(url));
      }
      server->SetArray("urls", std::move(urls));
      servers->PushObject(std::move(server));
    }
    json->SetArray("iceServers", std::move(servers));
  }
  // Serialize iceTransportPolicy.
  switch (config.type) {
    case webrtc::PeerConnectionInterface::kRelay:
      json->SetString("iceTransportPolicy", "relay");
      break;
    default:
      // The other values are the default or not web-exposed.
      break;
  }
  // Serialize iceCandidatePoolSize.
  if (config.ice_candidate_pool_size > 0) {
    json->SetInteger("iceCandidatePoolSize", config.ice_candidate_pool_size);
  }
  // Serialize bundlePolicy.
  switch (config.bundle_policy) {
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
      json->SetString("bundlePolicy", "max-bundle");
      break;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
      json->SetString("bundlePolicy", "max-compat");
      break;
    default:
      // "balanced" is the default and not serialized.
      break;
  }
  // Serialize rtcpMuxPolicy.
  switch (config.rtcp_mux_policy) {
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
      // No longer standard.
      json->SetString("rtcpMuxPolicy", "negotiate");
      break;
    default:
      // "require" is the default and not serialized.
      break;
  }
  // Serialize (non-standard and obsolete) encodedInsertableStreams.
  if (usesInsertableStreams) {
    json->SetBoolean("encodedInsertableStreams", true);
  }
  // TODO(hbos): Add serialization of certificate.
  StringBuilder value;
  json->WriteJSON(&value);
  return value.ToString();
}

const char* GetTransceiverUpdatedReasonString(
    PeerConnectionTracker::TransceiverUpdatedReason reason) {
  switch (reason) {
    case PeerConnectionTracker::TransceiverUpdatedReason::kAddTransceiver:
      return "addTransceiver";
    case PeerConnectionTracker::TransceiverUpdatedReason::kAddTrack:
      return "addTrack";
    case PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack:
      return "removeTrack";
    case PeerConnectionTracker::TransceiverUpdatedReason::kSetLocalDescription:
      return "setLocalDescription";
    case PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription:
      return "setRemoteDescription";
  }
  NOTREACHED();
}

int GetNextProcessLocalID() {
  static int next_local_id = 1;
  return next_local_id++;
}

}  // namespace

// chrome://webrtc-internals displays stats and stats graphs. The call path
// involves thread and process hops (IPC). This is the stats observer that is
// used when webrtc-internals wants standard stats. It starts in
// webrtc_internals.js performing requestStandardStats and the result gets
// asynchronously delivered to webrtc_internals.js at addStandardStats.
class InternalStandardStatsObserver : public webrtc::RTCStatsCollectorCallback {
 public:
  InternalStandardStatsObserver(
      const base::WeakPtr<RTCPeerConnectionHandler> pc_handler,
      int lid,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      Vector<std::unique_ptr<blink::RTCRtpSenderPlatform>> senders,
      CrossThreadOnceFunction<void(int, base::Value::List)> completion_callback)
      : pc_handler_(pc_handler),
        lid_(lid),
        main_thread_(std::move(main_thread)),
        senders_(std::move(senders)),
        completion_callback_(std::move(completion_callback)) {}

  void OnStatsDelivered(
      const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
      override {
    // We're on the signaling thread.
    DCHECK(!main_thread_->BelongsToCurrentThread());
    PostCrossThreadTask(
        *main_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &InternalStandardStatsObserver::OnStatsDeliveredOnMainThread,
            scoped_refptr<InternalStandardStatsObserver>(this), report));
  }

 protected:
  ~InternalStandardStatsObserver() override {}

 private:
  void OnStatsDeliveredOnMainThread(
      webrtc::scoped_refptr<const webrtc::RTCStatsReport> report) {
    std::move(completion_callback_).Run(lid_, ReportToList(report));
  }

  base::Value::List ReportToList(
      const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
    std::map<std::string, MediaStreamTrackPlatform*> tracks_by_id;
    for (const auto& sender : senders_) {
      MediaStreamComponent* track_component = sender->Track();
      if (!track_component) {
        continue;
      }
      tracks_by_id.insert(std::make_pair(track_component->Id().Utf8(),
                                         track_component->GetPlatformTrack()));
    }

    base::Value::List result_list;

    if (!pc_handler_) {
      return result_list;
    }
    auto* local_frame = To<WebLocalFrameImpl>(*pc_handler_->frame()).GetFrame();
    DocumentLoadTiming& time_converter =
        local_frame->Loader().GetDocumentLoader()->GetTiming();
    // Used for string comparisons with const char* below.
    const std::string kTypeMediaSource = "media-source";
    for (const auto& stats : *report) {
      base::Value::Dict stats_dictionary;
      stats_dictionary.Set("id", stats.id());
      stats_dictionary.Set("type", stats.type());
      // The timestamp unit is milliseconds but we want decimal
      // precision so we convert ourselves.
      base::TimeDelta monotonic_time =
          time_converter.MonotonicTimeToPseudoWallTime(
              ConvertToBaseTimeTicks(stats.timestamp()));
      stats_dictionary.Set(
          "timestamp",
          monotonic_time.InMicrosecondsF() /
              static_cast<double>(base::Time::kMicrosecondsPerMillisecond));
      for (const auto& attribute : stats.Attributes()) {
        if (!attribute.has_value()) {
          continue;
        }
        stats_dictionary.Set(attribute.name(), AttributeToValue(attribute));
      }
      // Modify "media-source" to also contain the result of the
      // MediaStreamTrack Statistics API, if applicable.
      if (stats.type() == kTypeMediaSource) {
        const webrtc::RTCMediaSourceStats& media_source =
            static_cast<const webrtc::RTCMediaSourceStats&>(stats);
        if (media_source.kind.has_value() && *media_source.kind == "video" &&
            media_source.track_identifier.has_value()) {
          auto it = tracks_by_id.find(*media_source.track_identifier);
          if (it != tracks_by_id.end()) {
            MediaStreamTrackPlatform::VideoFrameStats video_frame_stats =
                it->second->GetVideoFrameStats();
            stats_dictionary.Set("track.deliveredFrames",
                                 base::Value(static_cast<int>(
                                     video_frame_stats.deliverable_frames)));
            stats_dictionary.Set("track.discardedFrames",
                                 base::Value(static_cast<int>(
                                     video_frame_stats.discarded_frames)));
            stats_dictionary.Set("track.totalFrames",
                                 base::Value(static_cast<int>(
                                     video_frame_stats.deliverable_frames +
                                     video_frame_stats.discarded_frames +
                                     video_frame_stats.dropped_frames)));
          }
        }
      }
      base::Value::List list;
      list.Append(stats.id());
      list.Append(std::move(stats_dictionary));
      result_list.Append(std::move(list));
    }
    return result_list;
  }

  base::Value AttributeToValue(const webrtc::Attribute& attribute) {
    // Types supported by `base::Value` are passed as the appropriate type.
    if (attribute.holds_alternative<bool>()) {
      return base::Value(attribute.get<bool>());
    }
    if (attribute.holds_alternative<int>()) {
      return base::Value(attribute.get<int>());
    }
    if (attribute.holds_alternative<int32_t>()) {
      return base::Value(attribute.get<int32_t>());
    }
    if (attribute.holds_alternative<uint32_t>()) {
      uint32_t value = attribute.get<uint32_t>();
      return base::Value(static_cast<double>(value));
    }
    if (attribute.holds_alternative<int64_t>()) {
      int64_t value = attribute.get<int64_t>();
      return base::Value(static_cast<double>(value));
    }
    if (attribute.holds_alternative<uint64_t>()) {
      uint64_t value = attribute.get<uint64_t>();
      return base::Value(static_cast<double>(value));
    }
    if (attribute.holds_alternative<double>()) {
      return base::Value(attribute.get<double>());
    }
    if (attribute.holds_alternative<std::string>()) {
      return base::Value(attribute.get<std::string>());
    }
    if (attribute.holds_alternative<std::map<std::string, double>>()) {
      base::Value::Dict dict;
      for (auto& value : attribute.get<std::map<std::string, double>>()) {
        dict.Set(value.first, value.second);
      }
      return base::Value(std::move(dict));
    }
    DCHECK(false) << "Unimplemented native stats type.";
    return base::Value(attribute.ToString());
  }

  const base::WeakPtr<RTCPeerConnectionHandler> pc_handler_;
  const int lid_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  const Vector<std::unique_ptr<blink::RTCRtpSenderPlatform>> senders_;
  CrossThreadOnceFunction<void(int, base::Value::List)> completion_callback_;
};

// static
const char PeerConnectionTracker::kSupplementName[] = "PeerConnectionTracker";

PeerConnectionTracker& PeerConnectionTracker::From(LocalDOMWindow& window) {
  PeerConnectionTracker* tracker =
      Supplement<LocalDOMWindow>::From<PeerConnectionTracker>(window);
  if (!tracker) {
    tracker = MakeGarbageCollected<PeerConnectionTracker>(
        window, window.GetTaskRunner(TaskType::kNetworking),
        base::PassKey<PeerConnectionTracker>());
    ProvideTo(window, tracker);
  }
  return *tracker;
}

PeerConnectionTracker* PeerConnectionTracker::From(LocalFrame& frame) {
  auto* window = frame.DomWindow();
  return window ? &From(*window) : nullptr;
}

PeerConnectionTracker* PeerConnectionTracker::From(WebLocalFrame& frame) {
  auto* local_frame = To<WebLocalFrameImpl>(frame).GetFrame();
  return local_frame ? From(*local_frame) : nullptr;
}

void PeerConnectionTracker::BindToFrame(
    LocalFrame* frame,
    mojo::PendingReceiver<blink::mojom::blink::PeerConnectionManager>
        receiver) {
  if (!frame)
    return;

  if (auto* tracker = From(*frame))
    tracker->Bind(std::move(receiver));
}

PeerConnectionTracker::PeerConnectionTracker(
    LocalDOMWindow& window,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    base::PassKey<PeerConnectionTracker>)
    : Supplement<LocalDOMWindow>(window),
      // Do not set a lifecycle notifier for `peer_connection_tracker_host_` to
      // ensure that its mojo pipe stays alive until the execution context is
      // destroyed. `RTCPeerConnection`, which owns a `RTCPeerConnectionHandler`
      // which keeps `this` alive, will to close and unregister the peer
      // connection when the execution context is destroyed. For this to happen,
      // the mojo pipe _must_ be alive to relay. See https://crbug.com/1426377
      // for details.
      peer_connection_tracker_host_(nullptr),
      receiver_(this, &window),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {
  window.GetBrowserInterfaceBroker().GetInterface(
      peer_connection_tracker_host_.BindNewPipeAndPassReceiver(
          main_thread_task_runner_));
}

// Constructor used for testing. Note that receiver_ doesn't have a context
// notifier in this case.
PeerConnectionTracker::PeerConnectionTracker(
    mojo::PendingRemote<blink::mojom::blink::PeerConnectionTrackerHost> host,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : Supplement(nullptr),
      peer_connection_tracker_host_(nullptr),
      receiver_(this, nullptr),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {
  peer_connection_tracker_host_.Bind(std::move(host), main_thread_task_runner_);
}

PeerConnectionTracker::~PeerConnectionTracker() {}

void PeerConnectionTracker::Bind(
    mojo::PendingReceiver<blink::mojom::blink::PeerConnectionManager>
        receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  receiver_.Bind(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kMiscPlatformAPI));
}

void PeerConnectionTracker::OnSuspend() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  // Closing peer connections fires events. If JavaScript triggers the creation
  // or garbage collection of more peer connections, this would invalidate the
  // |peer_connection_local_id_map_| iterator. Therefor we iterate on a copy.
  PeerConnectionLocalIdMap peer_connection_map_copy =
      peer_connection_local_id_map_;
  for (const auto& pair : peer_connection_map_copy) {
    RTCPeerConnectionHandler* peer_connection_handler = pair.key;
    if (!base::Contains(peer_connection_local_id_map_,
                        peer_connection_handler)) {
      // Skip peer connections that have been unregistered during this method
      // call. Avoids use-after-free.
      continue;
    }
    peer_connection_handler->CloseClientPeerConnection();
  }
}

void PeerConnectionTracker::OnThermalStateChange(
    mojom::blink::DeviceThermalState thermal_state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  current_thermal_state_ = thermal_state;
  for (auto& entry : peer_connection_local_id_map_) {
    entry.key->OnThermalStateChange(current_thermal_state_);
  }
}

void PeerConnectionTracker::StartEventLog(int peer_connection_local_id,
                                          int output_period_ms) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  for (auto& it : peer_connection_local_id_map_) {
    if (it.value == peer_connection_local_id) {
      it.key->StartEventLog(output_period_ms);
      return;
    }
  }
}

void PeerConnectionTracker::StopEventLog(int peer_connection_local_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  for (auto& it : peer_connection_local_id_map_) {
    if (it.value == peer_connection_local_id) {
      it.key->StopEventLog();
      return;
    }
  }
}

void PeerConnectionTracker::StartDataChannelLog(int peer_connection_local_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  for (auto& it : peer_connection_local_id_map_) {
    if (it.value == peer_connection_local_id) {
      it.key->StartDataChannelLog();
      return;
    }
  }
}

void PeerConnectionTracker::StopDataChannelLog(int peer_connection_local_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  for (auto& it : peer_connection_local_id_map_) {
    if (it.value == peer_connection_local_id) {
      it.key->StopDataChannelLog();
      return;
    }
  }
}

void PeerConnectionTracker::GetStandardStats() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  for (const auto& pair : peer_connection_local_id_map_) {
    Vector<std::unique_ptr<blink::RTCRtpSenderPlatform>> senders =
        pair.key->GetPlatformSenders();
    webrtc::scoped_refptr<InternalStandardStatsObserver> observer(
        new webrtc::RefCountedObject<InternalStandardStatsObserver>(
            pair.key->GetWeakPtr(), pair.value, main_thread_task_runner_,
            std::move(senders),
            CrossThreadBindOnce(&PeerConnectionTracker::AddStandardStats,
                                WrapCrossThreadWeakPersistent(this))));
    pair.key->GetStandardStatsForTracker(observer);
  }
}

void PeerConnectionTracker::GetCurrentState() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  for (const auto& pair : peer_connection_local_id_map_) {
    pair.key->EmitCurrentStateForTracker();
  }
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const blink::WebLocalFrame* frame) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DCHECK(pc_handler);
  DCHECK_EQ(GetLocalIDForHandler(pc_handler), -1);
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  auto info = blink::mojom::blink::PeerConnectionInfo::New();

  info->lid = GetNextLocalID();
  info->rtc_configuration =
      SerializeConfiguration(config, pc_handler->encoded_insertable_streams());

  if (frame)
    info->url = frame->GetDocument().Url().GetString();
  else
    info->url = "test:testing";

  int32_t lid = info->lid;
  peer_connection_tracker_host_->AddPeerConnection(std::move(info));

  peer_connection_local_id_map_.insert(pc_handler, lid);

  if (current_thermal_state_ != mojom::blink::DeviceThermalState::kUnknown) {
    pc_handler->OnThermalStateChange(current_thermal_state_);
  }
}

void PeerConnectionTracker::UnregisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DVLOG(1) << "PeerConnectionTracker::UnregisterPeerConnection()";

  auto it = peer_connection_local_id_map_.find(pc_handler);

  if (it == peer_connection_local_id_map_.end()) {
    // The PeerConnection might not have been registered if its initialization
    // failed.
    return;
  }

  peer_connection_tracker_host_->RemovePeerConnection(it->value);

  peer_connection_local_id_map_.erase(it);
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    RTCOfferOptionsPlatform* options) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "createOffer", SerializeOfferOptions(options));
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    RTCAnswerOptionsPlatform* options) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "createAnswer", SerializeAnswerOptions(options));
}

void PeerConnectionTracker::TrackSetSessionDescription(
    RTCPeerConnectionHandler* pc_handler,
    const String& sdp,
    const String& type,
    Source source) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  auto json = std::make_unique<JSONObject>();
  json->SetString("type", type);
  if (!sdp.empty()) {
    json->SetString("sdp", sdp);
  }
  StringBuilder value;
  json->WriteJSON(&value);
  SendPeerConnectionUpdate(
      id,
      source == kSourceLocal ? "setLocalDescription" : "setRemoteDescription",
      value.ToString());
}

void PeerConnectionTracker::TrackSetSessionDescriptionImplicit(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "setLocalDescription", "");
}

void PeerConnectionTracker::TrackSetConfiguration(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;

  SendPeerConnectionUpdate(
      id, "setConfiguration",
      SerializeConfiguration(config, pc_handler->encoded_insertable_streams()));
}

void PeerConnectionTracker::TrackAddIceCandidate(
    RTCPeerConnectionHandler* pc_handler,
    RTCIceCandidatePlatform* candidate,
    Source source,
    bool succeeded) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  String relay_protocol = candidate->RelayProtocol();
  String url = candidate->Url();

  auto json = std::make_unique<JSONObject>();
  json->SetString("sdpMid", candidate->SdpMid());
  if (candidate->SdpMLineIndex()) {
    json->SetInteger("sdpMLineIndex", *candidate->SdpMLineIndex());
  }
  json->SetString("candidate", candidate->Candidate());
  if (!url.empty()) {
    json->SetString("url", url);
  }
  if (!relay_protocol.empty()) {
    json->SetString("relayProtocol", relay_protocol);
  }

  // OnIceCandidate always succeeds as it's a callback from the browser.
  DCHECK(source != kSourceLocal || succeeded);

  StringBuilder value;
  json->WriteJSON(&value);
  const char* event =
      (source == kSourceLocal)
          ? "onicecandidate"
          : (succeeded ? "addIceCandidate" : "addIceCandidateFailed");

  SendPeerConnectionUpdate(id, event, value.ToString());
}

void PeerConnectionTracker::TrackIceCandidateError(
    RTCPeerConnectionHandler* pc_handler,
    const String& address,
    std::optional<uint16_t> port,
    const String& host_candidate,
    const String& url,
    int error_code,
    const String& error_text) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;

  auto json = std::make_unique<JSONObject>();
  json->SetString("url", url);
  if (address) {
    json->SetString("address", address);
  }
  if (port.has_value()) {
    json->SetInteger("port", *port);
  }
  json->SetString("host_candidate", host_candidate);
  json->SetString("error_text", error_text);
  json->SetInteger("error_code", error_code);
  StringBuilder value;
  json->WriteJSON(&value);
  SendPeerConnectionUpdate(id, "onicecandidateerror", value.ToString());
}

void PeerConnectionTracker::TrackAddTransceiver(
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const RTCRtpTransceiverPlatform& transceiver,
    size_t transceiver_index) {
  TrackTransceiver("Added", pc_handler, reason, transceiver, transceiver_index);
}

void PeerConnectionTracker::TrackModifyTransceiver(
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const RTCRtpTransceiverPlatform& transceiver,
    size_t transceiver_index) {
  TrackTransceiver("Modified", pc_handler, reason, transceiver,
                   transceiver_index);
}

void PeerConnectionTracker::TrackTransceiver(
    const char* callback_type_ending,
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const RTCRtpTransceiverPlatform& transceiver,
    size_t transceiver_index) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  String callback_type =
      StrCat({"transceiver", String::FromUTF8(callback_type_ending)});
  std::unique_ptr<JSONObject> json = SerializeTransceiver(transceiver);
  json->SetString("reason", GetTransceiverUpdatedReasonString(reason));
  json->SetInteger("transceiverIndex", transceiver_index);

  StringBuilder value;
  json->WriteJSON(&value);
  SendPeerConnectionUpdate(id, callback_type, value.ToString());
}

void PeerConnectionTracker::TrackCreateDataChannel(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::DataChannelInterface* data_channel,
    Source source) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  // See https://w3c.github.io/webrtc-pc/#dom-rtcdatachannelinit
  auto json = std::make_unique<JSONObject>();
  json->SetString("label", String::FromUTF8(data_channel->label()));
  json->SetBoolean("ordered", data_channel->ordered());
  std::optional<uint16_t> maxPacketLifeTime = data_channel->maxPacketLifeTime();
  if (maxPacketLifeTime.has_value()) {
    json->SetInteger("maxPacketLifeTime", *maxPacketLifeTime);
  }
  std::optional<uint16_t> maxRetransmits = data_channel->maxRetransmitsOpt();
  if (maxRetransmits.has_value()) {
    json->SetInteger("maxRetransmits", *maxRetransmits);
  }
  if (!data_channel->protocol().empty()) {
    json->SetString("protocol", String::FromUTF8(data_channel->protocol()));
  }
  bool negotiated = data_channel->negotiated();
  if (negotiated) {
    json->SetBoolean("negotiated", true);
    json->SetInteger("id", data_channel->id());
  }
  // TODO(crbug.com/1455847): add priority
  // https://w3c.github.io/webrtc-priority/#new-rtcdatachannelinit-member
  StringBuilder value;
  json->WriteJSON(&value);
  SendPeerConnectionUpdate(
      id, source == kSourceLocal ? "createDataChannel" : "ondatachannel",
      value.ToString());
}

void PeerConnectionTracker::TrackClose(RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "close", g_empty_string);
}

void PeerConnectionTracker::TrackSignalingStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::SignalingState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "onsignalingstatechange",
      StrCat({"\"", webrtc::PeerConnectionInterface::AsString(state).data(),
              "\""}));
}

void PeerConnectionTracker::TrackIceConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "oniceconnectionstatechange",
      StrCat({"\"", webrtc::PeerConnectionInterface::AsString(state).data(),
              "\""}));
}

void PeerConnectionTracker::TrackConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "onconnectionstatechange",
      StrCat({"\"", webrtc::PeerConnectionInterface::AsString(state).data(),
              "\""}));
}

void PeerConnectionTracker::TrackIceGatheringStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceGatheringState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "onicegatheringstatechange",
      StrCat({"\"", webrtc::PeerConnectionInterface::AsString(state).data(),
              "\""}));
}

void PeerConnectionTracker::TrackSessionDescriptionCallback(
    RTCPeerConnectionHandler* pc_handler,
    Action action,
    const String& callback_type,
    const String& value) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  String update_type;
  switch (action) {
    case kActionSetLocalDescription:
      update_type = "setLocalDescription";
      break;
    case kActionSetLocalDescriptionImplicit:
      update_type = "setLocalDescription";
      break;
    case kActionSetRemoteDescription:
      update_type = "setRemoteDescription";
      break;
    case kActionCreateOffer:
      update_type = "createOffer";
      break;
    case kActionCreateAnswer:
      update_type = "createAnswer";
      break;
    default:
      NOTREACHED();
  }
  update_type = StrCat({update_type, callback_type});

  SendPeerConnectionUpdate(id, update_type, value);
}

void PeerConnectionTracker::TrackSessionId(RTCPeerConnectionHandler* pc_handler,
                                           const String& session_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DCHECK(pc_handler);
  DCHECK(!session_id.empty());
  const int local_id = GetLocalIDForHandler(pc_handler);
  if (local_id == -1) {
    return;
  }

  String non_null_session_id =
      session_id.IsNull() ? g_empty_string : session_id;
  peer_connection_tracker_host_->OnPeerConnectionSessionIdSet(
      local_id, non_null_session_id);
}

void PeerConnectionTracker::TrackOnRenegotiationNeeded(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "onnegotiationneeded", g_empty_string);
}

void PeerConnectionTracker::TrackGetUserMedia(
    UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  peer_connection_tracker_host_->GetUserMedia(
      user_media_request->request_id(), user_media_request->Audio(),
      user_media_request->Video(),
      SerializeGetUserMediaMediaConstraints(
          user_media_request->AudioConstraints()),
      SerializeGetUserMediaMediaConstraints(
          user_media_request->VideoConstraints()));
}

void PeerConnectionTracker::TrackGetUserMediaSuccess(
    UserMediaRequest* user_media_request,
    const MediaStream* stream) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  // Serialize audio and video track information (id and label) or "null"
  // when there is no such track.
  String audio_track_info;
  if (!stream->getAudioTracks().empty()) {
    auto json = std::make_unique<JSONObject>();
    json->SetString("id", stream->getAudioTracks()[0]->id());
    json->SetString("label", stream->getAudioTracks()[0]->label());
    StringBuilder value;
    json->WriteJSON(&value);
    audio_track_info = value.ToString();
  } else {
    audio_track_info = "null";
  }
  String video_track_info;
  if (!stream->getVideoTracks().empty()) {
    auto json = std::make_unique<JSONObject>();
    json->SetString("id", stream->getVideoTracks()[0]->id());
    json->SetString("label", stream->getVideoTracks()[0]->label());
    StringBuilder value;
    json->WriteJSON(&value);
    video_track_info = value.ToString();
  } else {
    video_track_info = "null";
  }
  peer_connection_tracker_host_->GetUserMediaSuccess(
      user_media_request->request_id(), stream->id(), audio_track_info,
      video_track_info);
}

void PeerConnectionTracker::TrackGetUserMediaFailure(
    UserMediaRequest* user_media_request,
    const String& error,
    const String& error_message) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  peer_connection_tracker_host_->GetUserMediaFailure(
      user_media_request->request_id(), error, error_message);
}

void PeerConnectionTracker::TrackGetDisplayMedia(
    UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  peer_connection_tracker_host_->GetDisplayMedia(
      user_media_request->request_id(), user_media_request->Audio(),
      user_media_request->Video(),
      SerializeGetUserMediaMediaConstraints(
          user_media_request->AudioConstraints()),
      SerializeGetUserMediaMediaConstraints(
          user_media_request->VideoConstraints()));
}

void PeerConnectionTracker::TrackGetDisplayMediaSuccess(
    UserMediaRequest* user_media_request,
    MediaStream* stream) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  // Serialize audio and video track information (id and label) or "null"
  // when there is no such track.
  String audio_track_info;
  if (!stream->getAudioTracks().empty()) {
    auto json = std::make_unique<JSONObject>();
    json->SetString("id", stream->getAudioTracks()[0]->id());
    json->SetString("label", stream->getAudioTracks()[0]->label());
    StringBuilder value;
    json->WriteJSON(&value);
    audio_track_info = value.ToString();
  } else {
    audio_track_info = "null";
  }
  String video_track_info;
  if (!stream->getVideoTracks().empty()) {
    auto json = std::make_unique<JSONObject>();
    json->SetString("id", stream->getVideoTracks()[0]->id());
    json->SetString("label", stream->getVideoTracks()[0]->label());
    StringBuilder value;
    json->WriteJSON(&value);
    video_track_info = value.ToString();
  } else {
    video_track_info = "null";
  }
  peer_connection_tracker_host_->GetDisplayMediaSuccess(
      user_media_request->request_id(), stream->id(), audio_track_info,
      video_track_info);
}

void PeerConnectionTracker::TrackGetDisplayMediaFailure(
    UserMediaRequest* user_media_request,
    const String& error,
    const String& error_message) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  peer_connection_tracker_host_->GetDisplayMediaFailure(
      user_media_request->request_id(), error, error_message);
}

void PeerConnectionTracker::TrackRtcEventLogWrite(
    RTCPeerConnectionHandler* pc_handler,
    const Vector<uint8_t>& output) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;

  peer_connection_tracker_host_->WebRtcEventLogWrite(id, output);
}

void PeerConnectionTracker::TrackRtcDataChannelLogWrite(
    RTCPeerConnectionHandler* pc_handler,
    const Vector<uint8_t>& output) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1) {
    return;
  }

  peer_connection_tracker_host_->WebRtcDataChannelLogWrite(id, output);
}

int PeerConnectionTracker::GetNextLocalID() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  return GetNextProcessLocalID();
}

int PeerConnectionTracker::GetLocalIDForHandler(
    RTCPeerConnectionHandler* handler) const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  const auto found = peer_connection_local_id_map_.find(handler);
  if (found == peer_connection_local_id_map_.end()) {
    return -1;
  }
  DCHECK_NE(found->value, -1);
  return found->value;
}

void PeerConnectionTracker::SendPeerConnectionUpdate(
    int local_id,
    const String& callback_type,
    const String& value) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  peer_connection_tracker_host_->UpdatePeerConnection(local_id, callback_type,
                                                      value);
}

void PeerConnectionTracker::AddStandardStats(int lid, base::Value::List value) {
  peer_connection_tracker_host_->AddStandardStats(lid, std::move(value));
}

}  // namespace blink
