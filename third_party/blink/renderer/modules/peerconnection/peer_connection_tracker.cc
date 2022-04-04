// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_answer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using webrtc::StatsReport;
using webrtc::StatsReports;

namespace blink {
class InternalStandardStatsObserver;
}

namespace WTF {

template <>
struct CrossThreadCopier<scoped_refptr<blink::InternalStandardStatsObserver>>
    : public CrossThreadCopierPassThrough<
          scoped_refptr<blink::InternalStandardStatsObserver>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <typename T>
struct CrossThreadCopier<rtc::scoped_refptr<T>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = rtc::scoped_refptr<T>;
  static Type Copy(Type pointer) { return pointer; }
};

template <>
struct CrossThreadCopier<base::Value::List>
    : public CrossThreadCopierByValuePassThrough<base::Value::List> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

// TODO(hta): This module should be redesigned to reduce string copies.

namespace {

String SerializeBoolean(bool value) {
  return value ? "true" : "false";
}

String SerializeServers(
    const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers) {
  StringBuilder result;
  result.Append("[");

  bool following = false;
  for (const auto& server : servers) {
    for (const auto& url : server.urls) {
      if (following)
        result.Append(", ");
      else
        following = true;

      result.Append(String::FromUTF8(url));
    }
  }
  result.Append("]");
  return result.ToString();
}

String SerializeMediaConstraints(const MediaConstraints& constraints) {
  return String(constraints.ToString());
}

String SerializeOfferOptions(blink::RTCOfferOptionsPlatform* options) {
  if (!options)
    return "null";

  StringBuilder result;
  result.Append("offerToReceiveVideo: ");
  result.AppendNumber(options->OfferToReceiveVideo());
  result.Append(", offerToReceiveAudio: ");
  result.AppendNumber(options->OfferToReceiveAudio());
  result.Append(", voiceActivityDetection: ");
  result.Append(SerializeBoolean(options->VoiceActivityDetection()));
  result.Append(", iceRestart: ");
  result.Append(SerializeBoolean(options->IceRestart()));
  return result.ToString();
}

String SerializeAnswerOptions(blink::RTCAnswerOptionsPlatform* options) {
  if (!options)
    return "null";

  StringBuilder result;
  result.Append(", voiceActivityDetection: ");
  result.Append(SerializeBoolean(options->VoiceActivityDetection()));
  return result.ToString();
}

String SerializeMediaStreamIds(const Vector<String>& stream_ids) {
  if (!stream_ids.size())
    return "[]";
  StringBuilder result;
  result.Append("[");
  for (const auto& stream_id : stream_ids) {
    if (result.length() > 2u)
      result.Append(",");
    result.Append("'");
    result.Append(stream_id);
    result.Append("'");
  }
  result.Append("]");
  return result.ToString();
}

String SerializeDirection(webrtc::RtpTransceiverDirection direction) {
  switch (direction) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
      return "'sendrecv'";
    case webrtc::RtpTransceiverDirection::kSendOnly:
      return "'sendonly'";
    case webrtc::RtpTransceiverDirection::kRecvOnly:
      return "'recvonly'";
    case webrtc::RtpTransceiverDirection::kInactive:
      return "'inactive'";
    case webrtc::RtpTransceiverDirection::kStopped:
      return "'stopped'";
    default:
      NOTREACHED();
      return String();
  }
}

String SerializeOptionalDirection(
    const absl::optional<webrtc::RtpTransceiverDirection>& direction) {
  return direction ? SerializeDirection(*direction) : "null";
}

String SerializeSender(const String& indent,
                       const blink::RTCRtpSenderPlatform& sender) {
  StringBuilder result;
  result.Append("{\n");
  // track:'id',
  result.Append(indent);
  result.Append("  track:");
  if (!sender.Track()) {
    result.Append("null");
  } else {
    result.Append("'");
    result.Append(sender.Track()->Id());
    result.Append("'");
  }
  result.Append(",\n");
  // streams:['id,'id'],
  result.Append(indent);
  result.Append("  streams:");
  result.Append(SerializeMediaStreamIds(sender.StreamIds()));
  result.Append(",\n");
  result.Append(indent);
  result.Append("}");
  return result.ToString();
}

String SerializeReceiver(const String& indent,
                         const RTCRtpReceiverPlatform& receiver) {
  StringBuilder result;
  result.Append("{\n");
  // track:'id',
  DCHECK(receiver.Track());
  result.Append(indent);
  result.Append("  track:'");
  result.Append(receiver.Track()->Id());
  result.Append("',\n");
  // streams:['id,'id'],
  result.Append(indent);
  result.Append("  streams:");
  result.Append(SerializeMediaStreamIds(receiver.StreamIds()));
  result.Append(",\n");
  result.Append(indent);
  result.Append("}");
  return result.ToString();
}

String SerializeTransceiver(const RTCRtpTransceiverPlatform& transceiver) {
  if (transceiver.ImplementationType() ==
      RTCRtpTransceiverPlatformImplementationType::kFullTransceiver) {
    StringBuilder result;
    result.Append("{\n");
    // mid:'foo',
    if (transceiver.Mid().IsNull()) {
      result.Append("  mid:null,\n");
    } else {
      result.Append("  mid:'");
      result.Append(String(transceiver.Mid()));
      result.Append("',\n");
    }
    // sender:{...},
    result.Append("  sender:");
    result.Append(SerializeSender("  ", *transceiver.Sender()));
    result.Append(",\n");
    // receiver:{...},
    result.Append("  receiver:");
    result.Append(SerializeReceiver("  ", *transceiver.Receiver()));
    result.Append(",\n");
    // stopped:false,
    result.Append("  stopped:");
    result.Append(SerializeBoolean(transceiver.Stopped()));
    result.Append(",\n");
    // direction:'sendrecv',
    result.Append("  direction:");
    result.Append(SerializeDirection(transceiver.Direction()));
    result.Append(",\n");
    // currentDirection:null,
    result.Append("  currentDirection:");
    result.Append(SerializeOptionalDirection(transceiver.CurrentDirection()));
    result.Append(",\n");
    result.Append("}");
    return result.ToString();
  }
  if (transceiver.ImplementationType() ==
      RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly) {
    return SerializeSender("", *transceiver.Sender());
  }
  DCHECK(transceiver.ImplementationType() ==
         RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
  return SerializeReceiver("", *transceiver.Receiver());
}

String SerializeIceTransportType(
    webrtc::PeerConnectionInterface::IceTransportsType type) {
  String transport_type("");
  switch (type) {
    case webrtc::PeerConnectionInterface::kNone:
      transport_type = "none";
      break;
    case webrtc::PeerConnectionInterface::kRelay:
      transport_type = "relay";
      break;
    case webrtc::PeerConnectionInterface::kAll:
      transport_type = "all";
      break;
    case webrtc::PeerConnectionInterface::kNoHost:
      transport_type = "noHost";
      break;
    default:
      NOTREACHED();
  }
  return transport_type;
}

String SerializeBundlePolicy(
    webrtc::PeerConnectionInterface::BundlePolicy policy) {
  String policy_str("");
  switch (policy) {
    case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
      policy_str = "balanced";
      break;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
      policy_str = "max-bundle";
      break;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
      policy_str = "max-compat";
      break;
    default:
      NOTREACHED();
  }
  return policy_str;
}

String SerializeRtcpMuxPolicy(
    webrtc::PeerConnectionInterface::RtcpMuxPolicy policy) {
  String policy_str("");
  switch (policy) {
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
      policy_str = "negotiate";
      break;
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
      policy_str = "require";
      break;
    default:
      NOTREACHED();
  }
  return policy_str;
}

String SerializeSdpSemantics(webrtc::SdpSemantics sdp_semantics) {
  String sdp_semantics_str("");
  switch (sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB:
      sdp_semantics_str = "plan-b";
      break;
    case webrtc::SdpSemantics::kUnifiedPlan:
      sdp_semantics_str = "unified-plan";
      break;
    default:
      NOTREACHED();
  }
  return "\"" + sdp_semantics_str + "\"";
}

String SerializeConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    bool usesInsertableStreams) {
  StringBuilder result;
  // TODO(hbos): Add serialization of certificate.
  result.Append("{ iceServers: ");
  result.Append(SerializeServers(config.servers));
  result.Append(", iceTransportPolicy: ");
  result.Append(SerializeIceTransportType(config.type));
  result.Append(", bundlePolicy: ");
  result.Append(SerializeBundlePolicy(config.bundle_policy));
  result.Append(", rtcpMuxPolicy: ");
  result.Append(SerializeRtcpMuxPolicy(config.rtcp_mux_policy));
  result.Append(", iceCandidatePoolSize: ");
  result.AppendNumber(config.ice_candidate_pool_size);
  result.Append(", sdpSemantics: ");
  result.Append(SerializeSdpSemantics(config.sdp_semantics));
  if (usesInsertableStreams) {
    result.Append(", encodedInsertableStreams: true");
  }
  result.Append(", extmapAllowMixed: ");
  result.Append(SerializeBoolean(config.offer_extmap_allow_mixed));
  result.Append(" }");
  return result.ToString();
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
  return nullptr;
}

// Builds a dictionary Value from the StatsReport.
// Note:
// The format must be consistent with what webrtc_internals.js expects.
// If you change it here, you must change webrtc_internals.js as well.
absl::optional<base::Value::Dict> GetDictValueStats(const StatsReport& report) {
  if (report.values().empty())
    return absl::nullopt;

  base::Value::List values;

  for (const auto& v : report.values()) {
    const StatsReport::ValuePtr& value = v.second;
    values.Append(value->display_name());
    switch (value->type()) {
      case StatsReport::Value::kInt:
        values.Append(value->int_val());
        break;
      case StatsReport::Value::kFloat:
        values.Append(value->float_val());
        break;
      case StatsReport::Value::kString:
        values.Append(value->string_val());
        break;
      case StatsReport::Value::kStaticString:
        values.Append(value->static_string_val());
        break;
      case StatsReport::Value::kBool:
        values.Append(value->bool_val());
        break;
      case StatsReport::Value::kInt64:  // int64_t isn't supported, so use
                                        // string.
      case StatsReport::Value::kId:
      default:
        values.Append(value->ToString());
        break;
    }
  }

  base::Value::Dict dict;
  dict.Set("timestamp", report.timestamp());
  dict.Set("values", std::move(values));

  return dict;
}

// Builds a dictionary Value from the StatsReport.
absl::optional<base::Value::Dict> GetDictValue(const StatsReport& report) {
  absl::optional<base::Value::Dict> stats = GetDictValueStats(report);
  if (!stats)
    return absl::nullopt;

  // Note:
  // The format must be consistent with what webrtc_internals.js expects.
  // If you change it here, you must change webrtc_internals.js as well.
  base::Value::Dict result;
  result.Set("stats", std::move(stats).value());
  result.Set("id", report.id()->ToString());
  result.Set("type", report.TypeToString());

  return result;
}

int GetNextProcessLocalID() {
  static int next_local_id = 1;
  return next_local_id++;
}

}  // namespace

// chrome://webrtc-internals displays stats and stats graphs. The call path
// involves thread and process hops (IPC). This is the webrtc::StatsObserver
// that is used when webrtc-internals wants legacy stats. It starts in
// webrtc_internals.js performing requestLegacyStats and the result gets
// asynchronously delivered to webrtc_internals.js at addLegacyStats.
class InternalLegacyStatsObserver : public webrtc::StatsObserver {
 public:
  InternalLegacyStatsObserver(
      int lid,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      CrossThreadOnceFunction<void(int, base::Value::List)> completion_callback)
      : lid_(lid),
        main_thread_(std::move(main_thread)),
        completion_callback_(std::move(completion_callback)) {}

  void OnComplete(const StatsReports& reports) override {
    base::Value::List list;
    for (const auto* r : reports) {
      absl::optional<base::Value::Dict> report = GetDictValue(*r);
      if (report)
        list.Append(std::move(*report));
    }

    if (!list.empty()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(&InternalLegacyStatsObserver::OnCompleteImpl,
                              std::move(list), lid_,
                              std::move(completion_callback_)));
    }
  }

 protected:
  ~InternalLegacyStatsObserver() override {
    // Will be destructed on libjingle's signaling thread.
    // The signaling thread is where libjingle's objects live and from where
    // libjingle makes callbacks.  This may or may not be the same thread as
    // the main thread.
  }

 private:
  // Static since |this| will most likely have been deleted by the time we
  // get here.
  static void OnCompleteImpl(
      base::Value::List list,
      int lid,
      CrossThreadOnceFunction<void(int, base::Value::List)>
          completion_callback) {
    DCHECK(!list.empty());
    std::move(completion_callback).Run(lid, std::move(list));
  }

  const int lid_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  CrossThreadOnceFunction<void(int, base::Value::List)> completion_callback_;
};

// chrome://webrtc-internals displays stats and stats graphs. The call path
// involves thread and process hops (IPC). This is the ----webrtc::StatsObserver
// that is used when webrtc-internals wants standard stats. It starts in
// webrtc_internals.js performing requestStandardStats and the result gets
// asynchronously delivered to webrtc_internals.js at addStandardStats.
class InternalStandardStatsObserver : public webrtc::RTCStatsCollectorCallback {
 public:
  InternalStandardStatsObserver(
      int lid,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      CrossThreadOnceFunction<void(int, base::Value::List)> completion_callback)
      : lid_(lid),
        main_thread_(std::move(main_thread)),
        completion_callback_(std::move(completion_callback)) {}

  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override {
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
      rtc::scoped_refptr<const webrtc::RTCStatsReport> report) {
    std::move(completion_callback_).Run(lid_, ReportToList(report));
  }

  base::Value::List ReportToList(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
    base::Value::List result_list;
    for (const auto& stats : *report) {
      // The format of "stats_subdictionary" is:
      // {timestamp:<milliseconds>, values: [<key-value pairs>]}
      base::Value::Dict stats_subdictionary;
      // Timestamp is reported in milliseconds.
      stats_subdictionary.Set("timestamp", stats.timestamp_us() / 1000.0);
      // Values are reported as
      // "values": ["member1", value, "member2", value...]
      base::Value::List name_value_pairs;
      for (const auto* member : stats.Members()) {
        if (!member->is_defined())
          continue;
        // Non-standardized / provisional stats which are not exposed
        // to Javascript are postfixed with an asterisk.
        std::string postfix = member->is_standardized() ? "" : "*";
        name_value_pairs.Append(member->name() + postfix);
        name_value_pairs.Append(MemberToValue(*member));
      }
      stats_subdictionary.Set("values", std::move(name_value_pairs));

      // The format of "stats_dictionary" is:
      // {id:<string>, stats:<stats_subdictionary>, type:<string>}
      base::Value::Dict stats_dictionary;
      stats_dictionary.Set("stats", std::move(stats_subdictionary));
      stats_dictionary.Set("id", stats.id());
      stats_dictionary.Set("type", stats.type());
      result_list.Append(std::move(stats_dictionary));
    }
    return result_list;
  }

  base::Value MemberToValue(const webrtc::RTCStatsMemberInterface& member) {
    switch (member.type()) {
      // Types supported by base::Value are passed as the appropriate type.
      case webrtc::RTCStatsMemberInterface::Type::kBool:
        return base::Value(*member.cast_to<webrtc::RTCStatsMember<bool>>());
      case webrtc::RTCStatsMemberInterface::Type::kInt32:
        return base::Value(*member.cast_to<webrtc::RTCStatsMember<int32_t>>());
      case webrtc::RTCStatsMemberInterface::Type::kString:
        return base::Value(
            *member.cast_to<webrtc::RTCStatsMember<std::string>>());
      case webrtc::RTCStatsMemberInterface::Type::kDouble:
        return base::Value(*member.cast_to<webrtc::RTCStatsMember<double>>());
      // Types not supported by base::Value are converted to string.
      case webrtc::RTCStatsMemberInterface::Type::kUint32:
      case webrtc::RTCStatsMemberInterface::Type::kInt64:
      case webrtc::RTCStatsMemberInterface::Type::kUint64:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceBool:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceInt32:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceUint32:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceInt64:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceUint64:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceDouble:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceString:
      case webrtc::RTCStatsMemberInterface::Type::kMapStringUint64:
      case webrtc::RTCStatsMemberInterface::Type::kMapStringDouble:
      default:
        return base::Value(member.ValueToString());
    }
  }

  const int lid_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  CrossThreadOnceFunction<void(int, base::Value::List)> completion_callback_;
};

// static
const char PeerConnectionTracker::kSupplementName[] = "PeerConnectionTracker";

PeerConnectionTracker& PeerConnectionTracker::From(LocalDOMWindow& window) {
  PeerConnectionTracker* tracker =
      Supplement<LocalDOMWindow>::From<PeerConnectionTracker>(window);
  if (!tracker) {
    tracker = MakeGarbageCollected<PeerConnectionTracker>(
        window, Thread::MainThread()->GetTaskRunner(),
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
      main_thread_task_runner_(std::move(main_thread_task_runner)) {
  window.GetBrowserInterfaceBroker().GetInterface(
      peer_connection_tracker_host_.BindNewPipeAndPassReceiver());
}

PeerConnectionTracker::PeerConnectionTracker(
    mojo::Remote<blink::mojom::blink::PeerConnectionTrackerHost> host,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : Supplement(nullptr),
      peer_connection_tracker_host_(std::move(host)),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {}

PeerConnectionTracker::~PeerConnectionTracker() {}

void PeerConnectionTracker::Bind(
    mojo::PendingReceiver<blink::mojom::blink::PeerConnectionManager>
        receiver) {
  DCHECK(!receiver_.is_bound());
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

void PeerConnectionTracker::OnSpeedLimitChange(int32_t speed_limit) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  current_speed_limit_ = speed_limit;
  for (auto& entry : peer_connection_local_id_map_) {
    entry.key->OnSpeedLimitChange(speed_limit);
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

void PeerConnectionTracker::GetStandardStats() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  for (const auto& pair : peer_connection_local_id_map_) {
    scoped_refptr<InternalStandardStatsObserver> observer(
        new rtc::RefCountedObject<InternalStandardStatsObserver>(
            pair.value, main_thread_task_runner_,
            CrossThreadBindOnce(&PeerConnectionTracker::AddStandardStats,
                                WrapCrossThreadWeakPersistent(this))));
    pair.key->GetStandardStatsForTracker(observer);
  }
}

void PeerConnectionTracker::GetLegacyStats() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  for (const auto& pair : peer_connection_local_id_map_) {
    rtc::scoped_refptr<InternalLegacyStatsObserver> observer(
        new rtc::RefCountedObject<InternalLegacyStatsObserver>(
            pair.value, main_thread_task_runner_,
            CrossThreadBindOnce(&PeerConnectionTracker::AddLegacyStats,
                                WrapCrossThreadWeakPersistent(this))));
    pair.key->GetStats(observer,
                       webrtc::PeerConnectionInterface::kStatsOutputLevelDebug,
                       nullptr);
  }
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const MediaConstraints& constraints,
    const blink::WebLocalFrame* frame) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DCHECK(pc_handler);
  DCHECK_EQ(GetLocalIDForHandler(pc_handler), -1);
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  auto info = blink::mojom::blink::PeerConnectionInfo::New();

  info->lid = GetNextLocalID();
  bool usesInsertableStreams =
      pc_handler->force_encoded_audio_insertable_streams() &&
      pc_handler->force_encoded_video_insertable_streams();
  info->rtc_configuration =
      SerializeConfiguration(config, usesInsertableStreams);

  info->constraints = SerializeMediaConstraints(constraints);
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
    // The PeerConnection might not have been registered if its initilization
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
  SendPeerConnectionUpdate(id, "createOffer",
                           "options: {" + SerializeOfferOptions(options) + "}");
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    const MediaConstraints& constraints) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createOffer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    blink::RTCAnswerOptionsPlatform* options) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createAnswer", "options: {" + SerializeAnswerOptions(options) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    const MediaConstraints& constraints) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createAnswer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
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
  String value = "type: " + type + ", sdp: " + sdp;
  SendPeerConnectionUpdate(
      id,
      source == kSourceLocal ? "setLocalDescription" : "setRemoteDescription",
      value);
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

  bool usesInsertableStreams =
      pc_handler->force_encoded_audio_insertable_streams() &&
      pc_handler->force_encoded_video_insertable_streams();
  SendPeerConnectionUpdate(
      id, "setConfiguration",
      SerializeConfiguration(config, usesInsertableStreams));
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
  String value =
      "sdpMid: " + String(candidate->SdpMid()) + ", " + "sdpMLineIndex: " +
      (candidate->SdpMLineIndex() ? String::Number(*candidate->SdpMLineIndex())
                                  : "null") +
      ", " + "candidate: " + String(candidate->Candidate());

  // OnIceCandidate always succeeds as it's a callback from the browser.
  DCHECK(source != kSourceLocal || succeeded);

  const char* event =
      (source == kSourceLocal)
          ? "icecandidate"
          : (succeeded ? "addIceCandidate" : "addIceCandidateFailed");

  SendPeerConnectionUpdate(id, event, value);
}

void PeerConnectionTracker::TrackIceCandidateError(
    RTCPeerConnectionHandler* pc_handler,
    const String& address,
    absl::optional<uint16_t> port,
    const String& host_candidate,
    const String& url,
    int error_code,
    const String& error_text) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  String address_string = address ? "address: " + address + "\n" : String();
  String port_string =
      port.has_value() ? String::Format("port: %d\n", port.value()) : "";
  String value = "url: " + url + "\n" + address_string + port_string +
                 "host_candidate: " + host_candidate + "\n" +
                 "error_text: " + error_text + "\n" +
                 "error_code: " + String::Number(error_code);
  SendPeerConnectionUpdate(id, "icecandidateerror", value);
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

void PeerConnectionTracker::TrackRemoveTransceiver(
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const RTCRtpTransceiverPlatform& transceiver,
    size_t transceiver_index) {
  TrackTransceiver("Removed", pc_handler, reason, transceiver,
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
  String callback_type;
  if (transceiver.ImplementationType() ==
      RTCRtpTransceiverPlatformImplementationType::kFullTransceiver) {
    callback_type = "transceiver";
  } else if (transceiver.ImplementationType() ==
             RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly) {
    callback_type = "sender";
  } else {
    callback_type = "receiver";
  }
  callback_type = callback_type + callback_type_ending;

  StringBuilder result;
  result.Append("Caused by: ");
  result.Append(GetTransceiverUpdatedReasonString(reason));
  result.Append("\n\n");
  if (transceiver.ImplementationType() ==
      RTCRtpTransceiverPlatformImplementationType::kFullTransceiver) {
    result.Append("getTransceivers()");
  } else if (transceiver.ImplementationType() ==
             RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly) {
    result.Append("getSenders()");
  } else {
    DCHECK_EQ(transceiver.ImplementationType(),
              RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
    result.Append("getReceivers()");
  }
  result.Append(String("[" + String::Number(transceiver_index) + "]:"));
  result.Append(SerializeTransceiver(transceiver));
  SendPeerConnectionUpdate(id, callback_type, result.ToString());
}

void PeerConnectionTracker::TrackCreateDataChannel(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::DataChannelInterface* data_channel,
    Source source) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  String value = "label: " + String::FromUTF8(data_channel->label()) +
                 ", reliable: " + SerializeBoolean(data_channel->reliable());
  SendPeerConnectionUpdate(
      id, source == kSourceLocal ? "createDataChannel" : "datachannel", value);
}

void PeerConnectionTracker::TrackClose(RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "close", String(""));
}

void PeerConnectionTracker::TrackSignalingStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::SignalingState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "signalingstatechange",
      webrtc::PeerConnectionInterface::AsString(state).data());
}

void PeerConnectionTracker::TrackLegacyIceConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "iceconnectionstatechange (legacy)",
      webrtc::PeerConnectionInterface::AsString(state).data());
}

void PeerConnectionTracker::TrackIceConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "iceconnectionstatechange",
      webrtc::PeerConnectionInterface::AsString(state).data());
}

void PeerConnectionTracker::TrackConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "connectionstatechange",
      webrtc::PeerConnectionInterface::AsString(state).data());
}

void PeerConnectionTracker::TrackIceGatheringStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceGatheringState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "icegatheringstatechange",
      webrtc::PeerConnectionInterface::AsString(state).data());
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
      break;
  }
  update_type = update_type + callback_type;

  SendPeerConnectionUpdate(id, update_type, value);
}

void PeerConnectionTracker::TrackSessionId(RTCPeerConnectionHandler* pc_handler,
                                           const String& session_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DCHECK(pc_handler);
  DCHECK(!session_id.IsEmpty());
  const int local_id = GetLocalIDForHandler(pc_handler);
  if (local_id == -1) {
    return;
  }

  String non_null_session_id =
      session_id.IsNull() ? WTF::g_empty_string : session_id;
  peer_connection_tracker_host_->OnPeerConnectionSessionIdSet(
      local_id, non_null_session_id);
}

void PeerConnectionTracker::TrackOnRenegotiationNeeded(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "negotiationneeded", String(""));
}

void PeerConnectionTracker::TrackGetUserMedia(
    UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  peer_connection_tracker_host_->GetUserMedia(
      user_media_request->request_id(), user_media_request->Audio(),
      user_media_request->Video(),
      SerializeMediaConstraints(user_media_request->AudioConstraints()),
      SerializeMediaConstraints(user_media_request->VideoConstraints()));
}

void PeerConnectionTracker::TrackGetUserMediaSuccess(
    UserMediaRequest* user_media_request,
    MediaStream* stream) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  // Serialize audio and video track information (id and label) or an
  // empty string when there is no such track.
  String audio_track_info =
      stream->getAudioTracks().IsEmpty()
          ? String("")
          : String("id:") + stream->getAudioTracks()[0]->id() +
                String(" label:") + stream->getAudioTracks()[0]->label();
  String video_track_info =
      stream->getVideoTracks().IsEmpty()
          ? String("")
          : String("id:") + stream->getVideoTracks()[0]->id() +
                String(" label:") + stream->getVideoTracks()[0]->label();

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

void PeerConnectionTracker::TrackRtcEventLogWrite(
    RTCPeerConnectionHandler* pc_handler,
    const WTF::Vector<uint8_t>& output) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;

  peer_connection_tracker_host_->WebRtcEventLogWrite(id, output);
}

int PeerConnectionTracker::GetNextLocalID() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  return GetNextProcessLocalID();
}

int PeerConnectionTracker::GetLocalIDForHandler(
    RTCPeerConnectionHandler* handler) const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  const auto found = peer_connection_local_id_map_.find(handler);
  if (found == peer_connection_local_id_map_.end())
    return -1;
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

void PeerConnectionTracker::AddLegacyStats(int lid, base::Value::List value) {
  peer_connection_tracker_host_->AddLegacyStats(lid, std::move(value));
}

}  // namespace blink
