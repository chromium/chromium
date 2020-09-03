// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_set_description_observer.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_answer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink_proxy.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_legacy_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_transceiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_request.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/data_channel_interface.h"
#include "third_party/webrtc/api/rtc_event_log_output.h"
#include "third_party/webrtc/pc/media_session.h"
#include "third_party/webrtc/pc/session_description.h"

using webrtc::DataChannelInterface;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::StatsReport;
using webrtc::StatsReports;

namespace WTF {

template <>
struct CrossThreadCopier<scoped_refptr<DataChannelInterface>>
    : public CrossThreadCopierPassThrough<scoped_refptr<DataChannelInterface>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<scoped_refptr<PeerConnectionInterface>>
    : public CrossThreadCopierPassThrough<
          scoped_refptr<PeerConnectionInterface>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<scoped_refptr<webrtc::StatsObserver>>
    : public CrossThreadCopierPassThrough<
          scoped_refptr<webrtc::StatsObserver>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {
namespace {

// Used to back histogram value of "WebRTC.PeerConnection.RtcpMux",
// so treat as append-only.
enum RtcpMux {
  RTCP_MUX_DISABLED,
  RTCP_MUX_ENABLED,
  RTCP_MUX_NO_MEDIA,
  RTCP_MUX_MAX
};

RTCSessionDescriptionPlatform* CreateWebKitSessionDescription(
    const std::string& sdp,
    const std::string& type) {
  return MakeGarbageCollected<RTCSessionDescriptionPlatform>(
      String::FromUTF8(type), String::FromUTF8(sdp));
}

RTCSessionDescriptionPlatform* CreateWebKitSessionDescription(
    const webrtc::SessionDescriptionInterface* native_desc) {
  if (!native_desc) {
    LOG(ERROR) << "Native session description is null.";
    return nullptr;
  }

  std::string sdp;
  if (!native_desc->ToString(&sdp)) {
    LOG(ERROR) << "Failed to get SDP string of native session description.";
    return nullptr;
  }

  return CreateWebKitSessionDescription(sdp, native_desc->type());
}

void RunClosureWithTrace(CrossThreadOnceClosure closure,
                         const char* trace_event_name) {
  TRACE_EVENT0("webrtc", trace_event_name);
  std::move(closure).Run();
}

void RunSynchronousOnceClosure(CrossThreadOnceClosure closure,
                               const char* trace_event_name,
                               base::WaitableEvent* event) {
  {
    TRACE_EVENT0("webrtc", trace_event_name);
    std::move(closure).Run();
  }
  event->Signal();
}

void RunSynchronousRepeatingClosure(const base::RepeatingClosure& closure,
                                    const char* trace_event_name,
                                    base::WaitableEvent* event) {
  {
    TRACE_EVENT0("webrtc", trace_event_name);
    closure.Run();
  }
  event->Signal();
}

// Converter functions from Blink types to WebRTC types.

absl::optional<bool> ConstraintToOptional(
    const MediaConstraints& constraints,
    const blink::BooleanConstraint MediaTrackConstraintSetPlatform::*picker) {
  bool value;
  if (GetConstraintValueAsBoolean(constraints, picker, &value)) {
    return absl::optional<bool>(value);
  }
  return absl::nullopt;
}

void CopyConstraintsIntoRtcConfiguration(
    const MediaConstraints constraints,
    webrtc::PeerConnectionInterface::RTCConfiguration* configuration) {
  // Copy info from constraints into configuration, if present.
  if (constraints.IsEmpty()) {
    return;
  }

  bool the_value;
  if (GetConstraintValueAsBoolean(
          constraints, &MediaTrackConstraintSetPlatform::enable_i_pv6,
          &the_value)) {
    configuration->disable_ipv6 = !the_value;
  } else {
    // Note: IPv6 WebRTC value is "disable" while Blink is "enable".
    configuration->disable_ipv6 = false;
  }

  if (GetConstraintValueAsBoolean(constraints,
                                  &MediaTrackConstraintSetPlatform::enable_dscp,
                                  &the_value)) {
    configuration->set_dscp(the_value);
  }

  if (GetConstraintValueAsBoolean(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_cpu_overuse_detection,
          &the_value)) {
    configuration->set_cpu_adaptation(the_value);
  }

  if (GetConstraintValueAsBoolean(
          constraints,
          &MediaTrackConstraintSetPlatform::
              goog_enable_video_suspend_below_min_bitrate,
          &the_value)) {
    configuration->set_suspend_below_min_bitrate(the_value);
  }

  if (!GetConstraintValueAsBoolean(
          constraints,
          &MediaTrackConstraintSetPlatform::enable_rtp_data_channels,
          &configuration->enable_rtp_data_channel)) {
    configuration->enable_rtp_data_channel = false;
  }
  int rate;
  if (GetConstraintValueAsInteger(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_screencast_min_bitrate,
          &rate)) {
    configuration->screencast_min_bitrate = rate;
  }
  configuration->combined_audio_video_bwe = ConstraintToOptional(
      constraints,
      &MediaTrackConstraintSetPlatform::goog_combined_audio_video_bwe);
  configuration->enable_dtls_srtp = ConstraintToOptional(
      constraints, &MediaTrackConstraintSetPlatform::enable_dtls_srtp);
}

// Class mapping responses from calls to libjingle CreateOffer/Answer and
// the blink::RTCSessionDescriptionRequest.
class CreateSessionDescriptionRequest
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateSessionDescriptionRequest(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      blink::RTCSessionDescriptionRequest* request,
      const base::WeakPtr<RTCPeerConnectionHandler>& handler,
      const base::WeakPtr<PeerConnectionTracker>& tracker,
      PeerConnectionTracker::Action action)
      : main_thread_(main_thread),
        webkit_request_(request),
        handler_(handler),
        tracker_(tracker),
        action_(action) {}

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(
              &CreateSessionDescriptionRequest::OnSuccess,
              rtc::scoped_refptr<CreateSessionDescriptionRequest>(this),
              CrossThreadUnretained(desc)));
      return;
    }

    if (tracker_ && handler_) {
      std::string value;
      if (desc) {
        desc->ToString(&value);
        value = "type: " + desc->type() + ", sdp: " + value;
      }
      tracker_->TrackSessionDescriptionCallback(
          handler_.get(), action_, "OnSuccess", String::FromUTF8(value));
      tracker_->TrackSessionId(handler_.get(),
                               String::FromUTF8(desc->session_id()));
    }
    webkit_request_->RequestSucceeded(CreateWebKitSessionDescription(desc));
    webkit_request_ = nullptr;
    delete desc;
  }
  void OnFailure(webrtc::RTCError error) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(
              &CreateSessionDescriptionRequest::OnFailure,
              rtc::scoped_refptr<CreateSessionDescriptionRequest>(this),
              std::move(error)));
      return;
    }

    if (handler_ && tracker_) {
      tracker_->TrackSessionDescriptionCallback(
          handler_.get(), action_, "OnFailure",
          String::FromUTF8(error.message()));
    }
    // TODO(hta): Convert CreateSessionDescriptionRequest.OnFailure
    webkit_request_->RequestFailed(error);
    webkit_request_ = nullptr;
  }

 protected:
  ~CreateSessionDescriptionRequest() override {
    // This object is reference counted and its callback methods |OnSuccess| and
    // |OnFailure| will be invoked on libjingle's signaling thread and posted to
    // the main thread. Since the main thread may complete before the signaling
    // thread has deferenced this object there is no guarantee that this object
    // is destructed on the main thread.
    DLOG_IF(ERROR, webkit_request_)
        << "CreateSessionDescriptionRequest not completed. Shutting down?";
  }

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  Persistent<RTCSessionDescriptionRequest> webkit_request_;
  const base::WeakPtr<RTCPeerConnectionHandler> handler_;
  const base::WeakPtr<PeerConnectionTracker> tracker_;
  PeerConnectionTracker::Action action_;
};

// Class mapping responses from calls to libjingle
// GetStats into a blink::WebRTCStatsCallback.
class StatsResponse : public webrtc::StatsObserver {
 public:
  StatsResponse(const scoped_refptr<LocalRTCStatsRequest>& request,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : request_(request.get()), main_thread_(task_runner) {
    // Measure the overall time it takes to satisfy a getStats request.
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("webrtc", "getStats_Native",
                                      TRACE_ID_LOCAL(this));
    DETACH_FROM_THREAD(signaling_thread_checker_);
  }

  void OnComplete(const StatsReports& reports) override {
    DCHECK_CALLED_ON_VALID_THREAD(signaling_thread_checker_);
    TRACE_EVENT0("webrtc", "StatsResponse::OnComplete");
    // We can't use webkit objects directly since they use a single threaded
    // heap allocator.
    std::vector<Report*>* report_copies = new std::vector<Report*>();
    report_copies->reserve(reports.size());
    for (auto* r : reports)
      report_copies->push_back(new Report(r));

    main_thread_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&StatsResponse::DeliverCallback,
                       rtc::scoped_refptr<StatsResponse>(this),
                       base::Unretained(report_copies)),
        base::BindOnce(&StatsResponse::DeleteReports,
                       base::Unretained(report_copies)));
  }

 private:
  class Report : public RTCLegacyStats {
   public:
    class MemberIterator : public RTCLegacyStatsMemberIterator {
     public:
      MemberIterator(const StatsReport::Values::const_iterator& it,
                     const StatsReport::Values::const_iterator& end)
          : it_(it), end_(end) {}

      // RTCLegacyStatsMemberIterator
      bool IsEnd() const override { return it_ == end_; }
      void Next() override { ++it_; }
      String GetName() const override {
        return String::FromUTF8(it_->second->display_name());
      }
      webrtc::StatsReport::Value::Type GetType() const override {
        return it_->second->type();
      }
      int ValueInt() const override { return it_->second->int_val(); }
      int64_t ValueInt64() const override { return it_->second->int64_val(); }
      float ValueFloat() const override { return it_->second->float_val(); }
      String ValueString() const override {
        const StatsReport::ValuePtr& value = it_->second;
        if (value->type() == StatsReport::Value::kString)
          return String::FromUTF8(value->string_val());
        DCHECK_EQ(value->type(), StatsReport::Value::kStaticString);
        return String::FromUTF8(value->static_string_val());
      }
      bool ValueBool() const override { return it_->second->bool_val(); }
      String ValueToString() const override {
        const StatsReport::ValuePtr& value = it_->second;
        if (value->type() == StatsReport::Value::kString)
          return String::FromUTF8(value->string_val());
        if (value->type() == StatsReport::Value::kStaticString)
          return String::FromUTF8(value->static_string_val());
        return String::FromUTF8(value->ToString());
      }

     private:
      StatsReport::Values::const_iterator it_;
      StatsReport::Values::const_iterator end_;
    };

    explicit Report(const StatsReport* report)
        : id_(report->id()->ToString()),
          type_(report->type()),
          type_name_(report->TypeToString()),
          timestamp_(report->timestamp()),
          values_(report->values()) {}

    ~Report() override {
      // Since the values vector holds pointers to const objects that are bound
      // to the signaling thread, they must be released on the same thread.
      DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    }

    // RTCLegacyStats
    String Id() const override { return String::FromUTF8(id_); }
    String GetType() const override { return String::FromUTF8(type_name_); }
    double Timestamp() const override { return timestamp_; }
    RTCLegacyStatsMemberIterator* Iterator() const override {
      return new MemberIterator(values_.cbegin(), values_.cend());
    }

    bool HasValues() const { return values_.size() > 0; }

   private:
    THREAD_CHECKER(thread_checker_);
    const std::string id_;
    const StatsReport::StatsType type_;
    const std::string type_name_;
    const double timestamp_;
    const StatsReport::Values values_;
  };

  static void DeleteReports(std::vector<Report*>* reports) {
    TRACE_EVENT0("webrtc", "StatsResponse::DeleteReports");
    for (auto* p : *reports)
      delete p;
    delete reports;
  }

  void DeliverCallback(const std::vector<Report*>* reports) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    TRACE_EVENT0("webrtc", "StatsResponse::DeliverCallback");

    rtc::scoped_refptr<LocalRTCStatsResponse> response(
        request_->createResponse().get());
    for (const auto* report : *reports) {
      if (report->HasValues())
        AddReport(response.get(), *report);
    }

    // Record the getStats operation as done before calling into Blink so that
    // we don't skew the perf measurements of the native code with whatever the
    // callback might be doing.
    TRACE_EVENT_NESTABLE_ASYNC_END0("webrtc", "getStats_Native",
                                    TRACE_ID_LOCAL(this));
    request_->requestSucceeded(response);
    request_ = nullptr;  // must be freed on the main thread.
  }

  void AddReport(LocalRTCStatsResponse* response, const Report& report) {
    response->addStats(report);
  }

  rtc::scoped_refptr<LocalRTCStatsRequest> request_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  THREAD_CHECKER(signaling_thread_checker_);
};

void GetStatsOnSignalingThread(
    const scoped_refptr<webrtc::PeerConnectionInterface>& pc,
    webrtc::PeerConnectionInterface::StatsOutputLevel level,
    const scoped_refptr<webrtc::StatsObserver>& observer,
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> selector) {
  TRACE_EVENT0("webrtc", "GetStatsOnSignalingThread");

  if (selector) {
    bool belongs_to_pc = false;
    for (const auto& sender : pc->GetSenders()) {
      if (sender->track() == selector) {
        belongs_to_pc = true;
        break;
      }
    }
    if (!belongs_to_pc) {
      for (const auto& receiver : pc->GetReceivers()) {
        if (receiver->track() == selector) {
          belongs_to_pc = true;
          break;
        }
      }
    }
    if (!belongs_to_pc) {
      DVLOG(1) << "GetStats: Track not found.";
      observer->OnComplete(StatsReports());
      return;
    }
  }

  if (!pc->GetStats(observer.get(), selector.get(), level)) {
    DVLOG(1) << "GetStats failed.";
    observer->OnComplete(StatsReports());
  }
}

using RTCStatsReportCallbackInternal =
    CrossThreadOnceFunction<void(std::unique_ptr<RTCStatsReportPlatform>)>;

void GetRTCStatsOnSignalingThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
    RTCStatsReportCallbackInternal callback,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids) {
  TRACE_EVENT0("webrtc", "GetRTCStatsOnSignalingThread");
  native_peer_connection->GetStats(CreateRTCStatsCollectorCallback(
      main_thread, ConvertToBaseOnceCallback(std::move(callback)),
      exposed_group_ids));
}

void ConvertOfferOptionsToWebrtcOfferOptions(
    const RTCOfferOptionsPlatform* options,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions* output) {
  DCHECK(options);
  output->offer_to_receive_audio = options->OfferToReceiveAudio();
  output->offer_to_receive_video = options->OfferToReceiveVideo();
  output->voice_activity_detection = options->VoiceActivityDetection();
  output->ice_restart = options->IceRestart();
}

void ConvertAnswerOptionsToWebrtcAnswerOptions(
    blink::RTCAnswerOptionsPlatform* options,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions* output) {
  output->voice_activity_detection = options->VoiceActivityDetection();
}

void ConvertConstraintsToWebrtcOfferOptions(
    const MediaConstraints& constraints,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions* output) {
  if (constraints.IsEmpty()) {
    return;
  }
  String failing_name;
  if (constraints.Basic().HasMandatoryOutsideSet(
          {constraints.Basic().offer_to_receive_audio.GetName(),
           constraints.Basic().offer_to_receive_video.GetName(),
           constraints.Basic().voice_activity_detection.GetName(),
           constraints.Basic().ice_restart.GetName()},
          failing_name)) {
    // TODO(hta): Reject the calling operation with "constraint error"
    // https://crbug.com/594894
    DLOG(ERROR) << "Invalid mandatory constraint to CreateOffer/Answer: "
                << failing_name;
  }
  GetConstraintValueAsInteger(
      constraints, &MediaTrackConstraintSetPlatform::offer_to_receive_audio,
      &output->offer_to_receive_audio);
  GetConstraintValueAsInteger(
      constraints, &MediaTrackConstraintSetPlatform::offer_to_receive_video,
      &output->offer_to_receive_video);
  GetConstraintValueAsBoolean(
      constraints, &MediaTrackConstraintSetPlatform::voice_activity_detection,
      &output->voice_activity_detection);
  GetConstraintValueAsBoolean(constraints,
                              &MediaTrackConstraintSetPlatform::ice_restart,
                              &output->ice_restart);
}

std::set<RTCPeerConnectionHandler*>* GetPeerConnectionHandlers() {
  static std::set<RTCPeerConnectionHandler*>* handlers =
      new std::set<RTCPeerConnectionHandler*>();
  return handlers;
}

// Counts the number of senders that have |stream_id| as an associated stream.
size_t GetLocalStreamUsageCount(
    const std::vector<std::unique_ptr<blink::RTCRtpSenderImpl>>& rtp_senders,
    const std::string stream_id) {
  size_t usage_count = 0;
  for (const auto& sender : rtp_senders) {
    for (const auto& sender_stream_id : sender->state().stream_ids()) {
      if (sender_stream_id == stream_id) {
        ++usage_count;
        break;
      }
    }
  }
  return usage_count;
}

bool IsRemoteStream(
    const std::vector<std::unique_ptr<blink::RTCRtpReceiverImpl>>&
        rtp_receivers,
    const std::string& stream_id) {
  for (const auto& receiver : rtp_receivers) {
    for (const auto& receiver_stream_id : receiver->state().stream_ids()) {
      if (stream_id == receiver_stream_id)
        return true;
    }
  }
  return false;
}

MediaStreamTrackMetrics::Kind MediaStreamTrackMetricsKind(
    const MediaStreamComponent* component) {
  return component->Source()->GetType() == MediaStreamSource::kTypeAudio
             ? MediaStreamTrackMetrics::Kind::kAudio
             : MediaStreamTrackMetrics::Kind::kVideo;
}

bool IsHostnameCandidate(const RTCIceCandidatePlatform& candidate) {
  // Currently the legitimate hostname candidates have only the .local
  // top-level domain, which are gathered when the mDNS concealment of local
  // IPs is enabled.
  const char kLocalTld[] = ".local";
  if (!candidate.Address().ContainsOnlyASCIIOrEmpty())
    return false;
  return candidate.Address().EndsWithIgnoringASCIICase(kLocalTld);
}

}  // namespace

// Implementation of LocalRTCStatsRequest.
LocalRTCStatsRequest::LocalRTCStatsRequest(RTCStatsRequest* impl)
    : impl_(impl) {}

LocalRTCStatsRequest::LocalRTCStatsRequest() {}
LocalRTCStatsRequest::~LocalRTCStatsRequest() {}

bool LocalRTCStatsRequest::hasSelector() const {
  return impl_->HasSelector();
}

MediaStreamComponent* LocalRTCStatsRequest::component() const {
  return impl_->Component();
}

scoped_refptr<LocalRTCStatsResponse> LocalRTCStatsRequest::createResponse() {
  return scoped_refptr<LocalRTCStatsResponse>(
      new rtc::RefCountedObject<LocalRTCStatsResponse>(
          impl_->CreateResponse()));
}

void LocalRTCStatsRequest::requestSucceeded(
    const LocalRTCStatsResponse* response) {
  impl_->RequestSucceeded(response->webKitStatsResponse());
}

// Implementation of LocalRTCStatsResponse.
RTCStatsResponseBase* LocalRTCStatsResponse::webKitStatsResponse() const {
  return impl_;
}

void LocalRTCStatsResponse::addStats(const RTCLegacyStats& stats) {
  impl_->AddStats(stats);
}

// Processes the resulting state changes of a SetLocalDescription() or
// SetRemoteDescription() call.
class RTCPeerConnectionHandler::WebRtcSetDescriptionObserverImpl
    : public WebRtcSetDescriptionObserver {
 public:
  WebRtcSetDescriptionObserverImpl(
      base::WeakPtr<RTCPeerConnectionHandler> handler,
      blink::RTCVoidRequest* web_request,
      base::WeakPtr<PeerConnectionTracker> tracker,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      PeerConnectionTracker::Action action,
      webrtc::SdpSemantics sdp_semantics)
      : handler_(handler),
        main_thread_(task_runner),
        web_request_(web_request),
        tracker_(tracker),
        action_(action),
        sdp_semantics_(sdp_semantics) {}

  void OnSetDescriptionComplete(
      webrtc::RTCError error,
      WebRtcSetDescriptionObserver::States states) override {
    if (!error.ok()) {
      if (tracker_ && handler_) {
        tracker_->TrackSessionDescriptionCallback(
            handler_.get(), action_, "OnFailure",
            String::FromUTF8(error.message()));
      }
      web_request_->RequestFailed(error);
      web_request_ = nullptr;
      return;
    }

    // Copy/move some of the states to be able to use them after moving
    // |state| below.
    webrtc::PeerConnectionInterface::SignalingState signaling_state =
        states.signaling_state;
    auto pending_local_description =
        std::move(states.pending_local_description);
    auto current_local_description =
        std::move(states.current_local_description);
    auto pending_remote_description =
        std::move(states.pending_remote_description);
    auto current_remote_description =
        std::move(states.current_remote_description);

    // Track result in chrome://webrtc-internals/.
    if (tracker_ && handler_) {
      StringBuilder value;
      if (action_ ==
          PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION_IMPLICIT) {
        webrtc::SessionDescriptionInterface* created_session_description =
            nullptr;
        // Deduce which SDP was created based on signaling state.
        if (signaling_state ==
                webrtc::PeerConnectionInterface::kHaveLocalOffer &&
            pending_local_description) {
          created_session_description = pending_local_description.get();
        } else if (signaling_state ==
                       webrtc::PeerConnectionInterface::kStable &&
                   current_local_description) {
          created_session_description = current_local_description.get();
        }
        RTC_DCHECK(created_session_description);
        std::string sdp;
        created_session_description->ToString(&sdp);
        value.Append("type: ");
        value.Append(
            webrtc::SdpTypeToString(created_session_description->GetType()));
        value.Append(", sdp: ");
        value.Append(sdp.c_str());
      }
      tracker_->TrackSessionDescriptionCallback(handler_.get(), action_,
                                                "OnSuccess", value.ToString());
      handler_->TrackSignalingChange(signaling_state);
    }

    if (handler_) {
      handler_->OnSessionDescriptionsUpdated(
          std::move(pending_local_description),
          std::move(current_local_description),
          std::move(pending_remote_description),
          std::move(current_remote_description));
    }

    // Process the rest of the state changes differently depending on SDP
    // semantics. This fires JS events could cause |handler_| to become null.
    if (sdp_semantics_ == webrtc::SdpSemantics::kPlanB) {
      ProcessStateChangesPlanB(std::move(states));
    } else {
      DCHECK_EQ(sdp_semantics_, webrtc::SdpSemantics::kUnifiedPlan);
      ProcessStateChangesUnifiedPlan(std::move(states));
    }

    ResolvePromise();
  }

 private:
  ~WebRtcSetDescriptionObserverImpl() override {}

  void ResolvePromise() {
    web_request_->RequestSucceeded();
    web_request_ = nullptr;
  }

  void ProcessStateChangesPlanB(WebRtcSetDescriptionObserver::States states) {
    DCHECK_EQ(sdp_semantics_, webrtc::SdpSemantics::kPlanB);
    if (!handler_)
      return;

    // Determine which receivers have been removed before processing the
    // removal as to not invalidate the iterator.
    Vector<blink::RTCRtpReceiverImpl*> removed_receivers;
    for (const auto& receiver : handler_->rtp_receivers_) {
      if (ReceiverWasRemoved(*receiver, states.transceiver_states))
        removed_receivers.push_back(receiver.get());
    }

    // Process the addition and removal of remote receivers/tracks.
    Vector<blink::RtpReceiverState> added_receiver_states;
    for (auto& transceiver_state : states.transceiver_states) {
      if (ReceiverWasAdded(transceiver_state)) {
        added_receiver_states.push_back(transceiver_state.MoveReceiverState());
      }
    }
    Vector<uintptr_t> removed_receiver_ids;
    for (const auto* removed_receiver : removed_receivers) {
      removed_receiver_ids.push_back(blink::RTCRtpReceiverImpl::getId(
          removed_receiver->state().webrtc_receiver().get()));
    }
    // |handler_| can become null after this call.
    handler_->OnReceiversModifiedPlanB(states.signaling_state,
                                       std::move(added_receiver_states),
                                       std::move(removed_receiver_ids));
  }

  bool ReceiverWasAdded(const blink::RtpTransceiverState& transceiver_state) {
    DCHECK(handler_);
    uintptr_t receiver_id = blink::RTCRtpReceiverImpl::getId(
        transceiver_state.receiver_state()->webrtc_receiver().get());
    for (const auto& receiver : handler_->rtp_receivers_) {
      if (receiver->Id() == receiver_id)
        return false;
    }
    return true;
  }

  bool ReceiverWasRemoved(
      const blink::RTCRtpReceiverImpl& receiver,
      const std::vector<blink::RtpTransceiverState>& transceiver_states) {
    for (const auto& transceiver_state : transceiver_states) {
      if (transceiver_state.receiver_state()->webrtc_receiver() ==
          receiver.state().webrtc_receiver()) {
        return false;
      }
    }
    return true;
  }

  void ProcessStateChangesUnifiedPlan(
      WebRtcSetDescriptionObserver::States states) {
    DCHECK_EQ(sdp_semantics_, webrtc::SdpSemantics::kUnifiedPlan);
    if (handler_) {
      handler_->OnModifySctpTransport(std::move(states.sctp_transport_state));
    }
    if (handler_) {
      handler_->OnModifyTransceivers(
          states.signaling_state, std::move(states.transceiver_states),
          action_ == PeerConnectionTracker::ACTION_SET_REMOTE_DESCRIPTION);
    }
  }

  base::WeakPtr<RTCPeerConnectionHandler> handler_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_;
  Persistent<blink::RTCVoidRequest> web_request_;
  base::WeakPtr<PeerConnectionTracker> tracker_;
  PeerConnectionTracker::Action action_;
  webrtc::SdpSemantics sdp_semantics_;
};

// Receives notifications from a PeerConnection object about state changes. The
// callbacks we receive here come on the webrtc signaling thread, so this class
// takes care of delivering them to an RTCPeerConnectionHandler instance on the
// main thread. In order to do safe PostTask-ing, the class is reference counted
// and checks for the existence of the RTCPeerConnectionHandler instance before
// delivering callbacks on the main thread.
class RTCPeerConnectionHandler::Observer
    : public GarbageCollected<RTCPeerConnectionHandler::Observer>,
      public PeerConnectionObserver,
      public RtcEventLogOutputSink {
 public:
  Observer(const base::WeakPtr<RTCPeerConnectionHandler>& handler,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : handler_(handler),
        main_thread_(task_runner),
        native_peer_connection_(nullptr) {}
  ~Observer() override = default;

  void Initialize() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(!native_peer_connection_);
    DCHECK(handler_);
    native_peer_connection_ = handler_->native_peer_connection_;
    DCHECK(native_peer_connection_);
  }

  // When an RTC event log is sent back from PeerConnection, it arrives here.
  void OnWebRtcEventLogWrite(const WTF::Vector<uint8_t>& output) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(
              &RTCPeerConnectionHandler::Observer::OnWebRtcEventLogWrite,
              WrapCrossThreadPersistent(this), output));
    } else if (handler_) {
      handler_->OnWebRtcEventLogWrite(output);
    }
  }

  void Trace(Visitor* visitor) const override {}

 protected:
  // TODO(hbos): Remove once no longer mandatory to implement.
  void OnSignalingChange(PeerConnectionInterface::SignalingState) override {}
  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface>) override {}
  void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface>) override {}

  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    PostCrossThreadTask(
        *main_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &RTCPeerConnectionHandler::Observer::OnDataChannelImpl,
            WrapCrossThreadPersistent(this),
            base::WrapRefCounted<DataChannelInterface>(data_channel.get())));
  }

  void OnNegotiationNeededEvent(uint32_t event_id) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(
              &RTCPeerConnectionHandler::Observer::OnNegotiationNeededEvent,
              WrapCrossThreadPersistent(this), event_id));
    } else if (handler_) {
      handler_->OnNegotiationNeededEvent(event_id);
    }
  }

  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {}
  void OnStandardizedIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(&RTCPeerConnectionHandler::Observer::
                                  OnStandardizedIceConnectionChange,
                              WrapCrossThreadPersistent(this), new_state));
    } else if (handler_) {
      handler_->OnIceConnectionChange(new_state);
    }
  }

  void OnConnectionChange(
      PeerConnectionInterface::PeerConnectionState new_state) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(
              &RTCPeerConnectionHandler::Observer::OnConnectionChange,
              WrapCrossThreadPersistent(this), new_state));
    } else if (handler_) {
      handler_->OnConnectionChange(new_state);
    }
  }

  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      PostCrossThreadTask(
          *main_thread_.get(), FROM_HERE,
          CrossThreadBindOnce(
              &RTCPeerConnectionHandler::Observer::OnIceGatheringChange,
              WrapCrossThreadPersistent(this), new_state));
    } else if (handler_) {
      handler_->OnIceGatheringChange(new_state);
    }
  }

  void OnIceCandidate(const IceCandidateInterface* candidate) override {
    DCHECK(native_peer_connection_);
    std::string sdp;
    if (!candidate->ToString(&sdp)) {
      NOTREACHED() << "OnIceCandidate: Could not get SDP string.";
      return;
    }
    // The generated candidate may have been added to the pending or current
    // local description, take a snapshot and surface them to the main thread.
    // Remote descriptions are also surfaced because
    // OnSessionDescriptionsUpdated() requires all four as arguments.
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        pending_local_description = CopySessionDescription(
            native_peer_connection_->pending_local_description());
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        current_local_description = CopySessionDescription(
            native_peer_connection_->current_local_description());
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        pending_remote_description = CopySessionDescription(
            native_peer_connection_->pending_remote_description());
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        current_remote_description = CopySessionDescription(
            native_peer_connection_->current_remote_description());

    PostCrossThreadTask(
        *main_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &RTCPeerConnectionHandler::Observer::OnIceCandidateImpl,
            WrapCrossThreadPersistent(this), String::FromUTF8(sdp),
            String::FromUTF8(candidate->sdp_mid()),
            candidate->sdp_mline_index(), candidate->candidate().component(),
            candidate->candidate().address().family(),
            std::move(pending_local_description),
            std::move(current_local_description),
            std::move(pending_remote_description),
            std::move(current_remote_description)));
  }

  void OnIceCandidateError(const std::string& address,
                           int port,
                           const std::string& url,
                           int error_code,
                           const std::string& error_text) override {
    PostCrossThreadTask(
        *main_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &RTCPeerConnectionHandler::Observer::OnIceCandidateErrorImpl,
            WrapCrossThreadPersistent(this),
            port ? String::FromUTF8(address) : String(),
            static_cast<uint16_t>(port),
            String::Format("%s:%d", address.c_str(), port),
            String::FromUTF8(url), error_code, String::FromUTF8(error_text)));
  }

  void OnDataChannelImpl(scoped_refptr<DataChannelInterface> channel) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (handler_)
      handler_->OnDataChannel(std::move(channel));
  }

  void OnIceCandidateImpl(const String& sdp,
                          const String& sdp_mid,
                          int sdp_mline_index,
                          int component,
                          int address_family,
                          std::unique_ptr<webrtc::SessionDescriptionInterface>
                              pending_local_description,
                          std::unique_ptr<webrtc::SessionDescriptionInterface>
                              current_local_description,
                          std::unique_ptr<webrtc::SessionDescriptionInterface>
                              pending_remote_description,
                          std::unique_ptr<webrtc::SessionDescriptionInterface>
                              current_remote_description) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (handler_) {
      handler_->OnSessionDescriptionsUpdated(
          std::move(pending_local_description),
          std::move(current_local_description),
          std::move(pending_remote_description),
          std::move(current_remote_description));
      handler_->OnIceCandidate(sdp, sdp_mid, sdp_mline_index, component,
                               address_family);
    }
  }

  void OnIceCandidateErrorImpl(const String& address,
                               int port,
                               const String& host_candidate,
                               const String& url,
                               int error_code,
                               const String& error_text) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (handler_) {
      handler_->OnIceCandidateError(
          address,
          port ? base::Optional<uint16_t>(static_cast<uint16_t>(port))
               : base::nullopt,
          host_candidate, url, error_code, error_text);
    }
  }

  void OnInterestingUsage(int usage_pattern) override {
    PostCrossThreadTask(
        *main_thread_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &RTCPeerConnectionHandler::Observer::OnInterestingUsageImpl,
            WrapCrossThreadPersistent(this), usage_pattern));
  }

  void OnInterestingUsageImpl(int usage_pattern) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (handler_) {
      handler_->OnInterestingUsage(usage_pattern);
    }
  }

 private:
  const base::WeakPtr<RTCPeerConnectionHandler> handler_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // A copy of |handler_->native_peer_connection_| for use on the WebRTC
  // signaling thread.
  scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;
};

RTCPeerConnectionHandler::RTCPeerConnectionHandler(
    RTCPeerConnectionHandlerClient* client,
    blink::PeerConnectionDependencyFactory* dependency_factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool force_encoded_audio_insertable_streams,
    bool force_encoded_video_insertable_streams)
    : client_(client),
      dependency_factory_(dependency_factory),
      track_adapter_map_(
          base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
              dependency_factory_,
              task_runner)),
      force_encoded_audio_insertable_streams_(
          force_encoded_audio_insertable_streams),
      force_encoded_video_insertable_streams_(
          force_encoded_video_insertable_streams),
      task_runner_(std::move(task_runner)) {
  CHECK(client_);

  GetPeerConnectionHandlers()->insert(this);
}

// Constructor to be used for creating mocks only.
RTCPeerConnectionHandler::RTCPeerConnectionHandler(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : is_unregistered_(true),  // Avoid StopAndUnregister in destructor
      task_runner_(std::move(task_runner)) {}

RTCPeerConnectionHandler::~RTCPeerConnectionHandler() {
  if (!is_unregistered_) {
    StopAndUnregister();
  }
}

void RTCPeerConnectionHandler::StopAndUnregister() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  Stop();

  GetPeerConnectionHandlers()->erase(this);
  if (peer_connection_tracker_)
    peer_connection_tracker_->UnregisterPeerConnection(this);

  UMA_HISTOGRAM_COUNTS_10000("WebRTC.NumDataChannelsPerPeerConnection",
                             num_data_channels_created_);
  // Clear the pointer to client_ so that it does not interfere with
  // garbage collection.
  client_ = nullptr;
  is_unregistered_ = true;
}

bool RTCPeerConnectionHandler::Initialize(
    const webrtc::PeerConnectionInterface::RTCConfiguration&
        server_configuration,
    const MediaConstraints& options,
    WebLocalFrame* frame) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(frame);
  frame_ = frame;

  CHECK(!initialize_called_);
  initialize_called_ = true;

  // TODO(crbug.com/787254): Evaluate the need for passing weak ptr since
  // PeerConnectionTracker is now leaky with base::LazyInstance.
  peer_connection_tracker_ = PeerConnectionTracker::GetInstance()->AsWeakPtr();

  configuration_ = server_configuration;

  // Choose between RTC smoothness algorithm and prerenderer smoothing.
  // Prerenderer smoothing is turned on if RTC smoothness is turned off.
  configuration_.set_prerenderer_smoothing(
      !blink::Platform::Current()->RTCSmoothnessAlgorithmEnabled());

  configuration_.set_experiment_cpu_load_estimator(true);

  // Configure optional SRTP configurations enabled via the command line.
  configuration_.crypto_options = webrtc::CryptoOptions{};
  configuration_.crypto_options->srtp.enable_gcm_crypto_suites = true;
  configuration_.crypto_options->srtp.enable_encrypted_rtp_header_extensions =
      blink::Platform::Current()->IsWebRtcSrtpEncryptedHeadersEnabled();
  configuration_.enable_implicit_rollback = true;

  // Copy all the relevant constraints into |config|.
  CopyConstraintsIntoRtcConfiguration(options, &configuration_);

  peer_connection_observer_ =
      MakeGarbageCollected<Observer>(weak_factory_.GetWeakPtr(), task_runner_);
  native_peer_connection_ = dependency_factory_->CreatePeerConnection(
      configuration_, frame_, peer_connection_observer_);
  if (!native_peer_connection_.get()) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }
  peer_connection_observer_->Initialize();

  if (peer_connection_tracker_) {
    peer_connection_tracker_->RegisterPeerConnection(this, configuration_,
                                                     options, frame_);
  }

  return true;
}

bool RTCPeerConnectionHandler::InitializeForTest(
    const webrtc::PeerConnectionInterface::RTCConfiguration&
        server_configuration,
    const MediaConstraints& options,
    const base::WeakPtr<PeerConnectionTracker>& peer_connection_tracker) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  CHECK(!initialize_called_);
  initialize_called_ = true;

  configuration_ = server_configuration;

  peer_connection_observer_ =
      MakeGarbageCollected<Observer>(weak_factory_.GetWeakPtr(), task_runner_);
  CopyConstraintsIntoRtcConfiguration(options, &configuration_);

  native_peer_connection_ = dependency_factory_->CreatePeerConnection(
      configuration_, nullptr, peer_connection_observer_);
  if (!native_peer_connection_.get()) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }
  peer_connection_observer_->Initialize();
  peer_connection_tracker_ = peer_connection_tracker;
  return true;
}

Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::CreateOffer(RTCSessionDescriptionRequest* request,
                                      const MediaConstraints& options) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createOffer");

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateOffer(this, options);

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertConstraintsToWebrtcOfferOptions(options, &webrtc_options);
  return CreateOfferInternal(request, std::move(webrtc_options));
}

Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::CreateOffer(RTCSessionDescriptionRequest* request,
                                      RTCOfferOptionsPlatform* options) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createOffer");

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateOffer(this, options);

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertOfferOptionsToWebrtcOfferOptions(options, &webrtc_options);
  return CreateOfferInternal(request, std::move(webrtc_options));
}

Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::CreateOfferInternal(
    blink::RTCSessionDescriptionRequest* request,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          task_runner_, request, weak_factory_.GetWeakPtr(),
          peer_connection_tracker_,
          PeerConnectionTracker::ACTION_CREATE_OFFER));

  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  RunSynchronousRepeatingClosureOnSignalingThread(
      base::BindRepeating(
          &RTCPeerConnectionHandler::CreateOfferOnSignalingThread,
          base::Unretained(this), base::Unretained(description_request.get()),
          std::move(options), base::Unretained(&transceiver_state_surfacer)),
      "CreateOfferOnSignalingThread");
  DCHECK(transceiver_state_surfacer.is_initialized());

  auto transceiver_states = transceiver_state_surfacer.ObtainStates();
  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> transceivers;
  for (auto& transceiver_state : transceiver_states) {
    auto transceiver = CreateOrUpdateTransceiver(
        std::move(transceiver_state), blink::TransceiverStateUpdateMode::kAll);
    transceivers.push_back(std::move(transceiver));
  }
  return transceivers;
}

void RTCPeerConnectionHandler::CreateOfferOnSignalingThread(
    webrtc::CreateSessionDescriptionObserver* observer,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions offer_options,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer) {
  native_peer_connection_->CreateOffer(observer, offer_options);
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
      transceivers =
          configuration_.sdp_semantics == webrtc::SdpSemantics::kUnifiedPlan
              ? native_peer_connection_->GetTransceivers()
              : std::vector<
                    rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>();
  transceiver_state_surfacer->Initialize(
      native_peer_connection_, track_adapter_map_, std::move(transceivers));
}

void RTCPeerConnectionHandler::CreateAnswer(
    blink::RTCSessionDescriptionRequest* request,
    const MediaConstraints& options) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createAnswer");
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          task_runner_, request, weak_factory_.GetWeakPtr(),
          peer_connection_tracker_,
          PeerConnectionTracker::ACTION_CREATE_ANSWER));
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertConstraintsToWebrtcOfferOptions(options, &webrtc_options);
  // TODO(tommi): Do this asynchronously via e.g. PostTaskAndReply.
  native_peer_connection_->CreateAnswer(description_request.get(),
                                        webrtc_options);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateAnswer(this, options);
}

void RTCPeerConnectionHandler::CreateAnswer(
    blink::RTCSessionDescriptionRequest* request,
    blink::RTCAnswerOptionsPlatform* options) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createAnswer");
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          task_runner_, request, weak_factory_.GetWeakPtr(),
          peer_connection_tracker_,
          PeerConnectionTracker::ACTION_CREATE_ANSWER));
  // TODO(tommi): Do this asynchronously via e.g. PostTaskAndReply.
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertAnswerOptionsToWebrtcAnswerOptions(options, &webrtc_options);
  native_peer_connection_->CreateAnswer(description_request.get(),
                                        webrtc_options);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateAnswer(this, options);
}

bool IsOfferOrAnswer(const webrtc::SessionDescriptionInterface* native_desc) {
  DCHECK(native_desc);
  return native_desc->type() == "offer" || native_desc->type() == "answer";
}

void RTCPeerConnectionHandler::SetLocalDescription(
    blink::RTCVoidRequest* request) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setLocalDescription");

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackSetSessionDescriptionImplicit(this);

  scoped_refptr<WebRtcSetDescriptionObserverImpl> content_observer =
      base::MakeRefCounted<WebRtcSetDescriptionObserverImpl>(
          weak_factory_.GetWeakPtr(), request, peer_connection_tracker_,
          task_runner_,
          PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION_IMPLICIT,
          configuration_.sdp_semantics);

  // Surfacing transceivers is not applicable in Plan B.
  bool surface_receivers_only =
      (configuration_.sdp_semantics == webrtc::SdpSemantics::kPlanB);
  rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>
      webrtc_observer(WebRtcSetLocalDescriptionObserverHandler::Create(
                          task_runner_, signaling_thread(),
                          native_peer_connection_, track_adapter_map_,
                          content_observer, surface_receivers_only)
                          .get());

  PostCrossThreadTask(
      *signaling_thread().get(), FROM_HERE,
      CrossThreadBindOnce(
          &RunClosureWithTrace,
          CrossThreadBindOnce(
              static_cast<void (webrtc::PeerConnectionInterface::*)(
                  rtc::scoped_refptr<
                      webrtc::SetLocalDescriptionObserverInterface>)>(
                  &webrtc::PeerConnectionInterface::SetLocalDescription),
              native_peer_connection_, webrtc_observer),
          CrossThreadUnretained("SetLocalDescription")));
}

void RTCPeerConnectionHandler::SetLocalDescription(
    blink::RTCVoidRequest* request,
    RTCSessionDescriptionPlatform* description) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setLocalDescription");

  String sdp = description->Sdp();
  String type = description->GetType();

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackSetSessionDescription(
        this, sdp, type, PeerConnectionTracker::SOURCE_LOCAL);
  }

  webrtc::SdpParseError error;
  // Since CreateNativeSessionDescription uses the dependency factory, we need
  // to make this call on the current thread to be safe.
  std::unique_ptr<webrtc::SessionDescriptionInterface> native_desc(
      CreateNativeSessionDescription(sdp, type, &error));
  if (!native_desc) {
    StringBuilder reason_str;
    reason_str.Append("Failed to parse SessionDescription. ");
    reason_str.Append(error.line.c_str());
    reason_str.Append(" ");
    reason_str.Append(error.description.c_str());
    LOG(ERROR) << reason_str.ToString();
    if (peer_connection_tracker_) {
      peer_connection_tracker_->TrackSessionDescriptionCallback(
          this, PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION,
          "OnFailure", reason_str.ToString());
    }
    // Warning: this line triggers the error callback to be executed, causing
    // arbitrary JavaScript to be executed synchronously. As a result, it is
    // possible for |this| to be deleted after this line. See
    // https://crbug.com/1005251.
    if (request) {
      request->RequestFailed(webrtc::RTCError(
          webrtc::RTCErrorType::INTERNAL_ERROR, reason_str.ToString().Utf8()));
    }
    return;
  }

  if (!first_local_description_ && IsOfferOrAnswer(native_desc.get())) {
    first_local_description_ =
        std::make_unique<FirstSessionDescription>(native_desc.get());
    if (first_remote_description_) {
      ReportFirstSessionDescriptions(*first_local_description_,
                                     *first_remote_description_);
    }
  }

  scoped_refptr<WebRtcSetDescriptionObserverImpl> content_observer =
      base::MakeRefCounted<WebRtcSetDescriptionObserverImpl>(
          weak_factory_.GetWeakPtr(), request, peer_connection_tracker_,
          task_runner_, PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION,
          configuration_.sdp_semantics);

  bool surface_receivers_only =
      (configuration_.sdp_semantics == webrtc::SdpSemantics::kPlanB);
  rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>
      webrtc_observer(WebRtcSetLocalDescriptionObserverHandler::Create(
                          task_runner_, signaling_thread(),
                          native_peer_connection_, track_adapter_map_,
                          content_observer, surface_receivers_only)
                          .get());

  PostCrossThreadTask(
      *signaling_thread().get(), FROM_HERE,
      CrossThreadBindOnce(
          &RunClosureWithTrace,
          CrossThreadBindOnce(
              static_cast<void (webrtc::PeerConnectionInterface::*)(
                  std::unique_ptr<webrtc::SessionDescriptionInterface>,
                  rtc::scoped_refptr<
                      webrtc::SetLocalDescriptionObserverInterface>)>(
                  &webrtc::PeerConnectionInterface::SetLocalDescription),
              native_peer_connection_, WTF::Passed(std::move(native_desc)),
              webrtc_observer),
          CrossThreadUnretained("SetLocalDescription")));
}

void RTCPeerConnectionHandler::SetRemoteDescription(
    blink::RTCVoidRequest* request,
    RTCSessionDescriptionPlatform* description) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setRemoteDescription");

  String sdp = description->Sdp();
  String type = description->GetType();

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackSetSessionDescription(
        this, sdp, type, PeerConnectionTracker::SOURCE_REMOTE);
  }

  webrtc::SdpParseError error;
  // Since CreateNativeSessionDescription uses the dependency factory, we need
  // to make this call on the current thread to be safe.
  std::unique_ptr<webrtc::SessionDescriptionInterface> native_desc(
      CreateNativeSessionDescription(sdp, type, &error));
  if (!native_desc) {
    StringBuilder reason_str;
    reason_str.Append("Failed to parse SessionDescription. ");
    reason_str.Append(error.line.c_str());
    reason_str.Append(" ");
    reason_str.Append(error.description.c_str());
    LOG(ERROR) << reason_str.ToString();
    if (peer_connection_tracker_) {
      peer_connection_tracker_->TrackSessionDescriptionCallback(
          this, PeerConnectionTracker::ACTION_SET_REMOTE_DESCRIPTION,
          "OnFailure", reason_str.ToString());
    }
    // Warning: this line triggers the error callback to be executed, causing
    // arbitrary JavaScript to be executed synchronously. As a result, it is
    // possible for |this| to be deleted after this line. See
    // https://crbug.com/1005251.
    if (request) {
      request->RequestFailed(
          webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION,
                           reason_str.ToString().Utf8()));
    }
    return;
  }

  if (!first_remote_description_ && IsOfferOrAnswer(native_desc.get())) {
    first_remote_description_.reset(
        new FirstSessionDescription(native_desc.get()));
    if (first_local_description_) {
      ReportFirstSessionDescriptions(*first_local_description_,
                                     *first_remote_description_);
    }
  }

  scoped_refptr<WebRtcSetDescriptionObserverImpl> content_observer =
      base::MakeRefCounted<WebRtcSetDescriptionObserverImpl>(
          weak_factory_.GetWeakPtr(), request, peer_connection_tracker_,
          task_runner_, PeerConnectionTracker::ACTION_SET_REMOTE_DESCRIPTION,
          configuration_.sdp_semantics);

  bool surface_receivers_only =
      (configuration_.sdp_semantics == webrtc::SdpSemantics::kPlanB);
  rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>
      webrtc_observer(WebRtcSetRemoteDescriptionObserverHandler::Create(
                          task_runner_, signaling_thread(),
                          native_peer_connection_, track_adapter_map_,
                          content_observer, surface_receivers_only)
                          .get());

  PostCrossThreadTask(
      *signaling_thread().get(), FROM_HERE,
      CrossThreadBindOnce(
          &RunClosureWithTrace,
          CrossThreadBindOnce(
              static_cast<void (webrtc::PeerConnectionInterface::*)(
                  std::unique_ptr<webrtc::SessionDescriptionInterface>,
                  rtc::scoped_refptr<
                      webrtc::SetRemoteDescriptionObserverInterface>)>(
                  &webrtc::PeerConnectionInterface::SetRemoteDescription),
              native_peer_connection_, WTF::Passed(std::move(native_desc)),
              webrtc_observer),
          CrossThreadUnretained("SetRemoteDescription")));
}

const webrtc::PeerConnectionInterface::RTCConfiguration&
RTCPeerConnectionHandler::GetConfiguration() const {
  return configuration_;
}

webrtc::RTCErrorType RTCPeerConnectionHandler::SetConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration& blink_config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setConfiguration");

  // Update the configuration with the potentially modified fields
  webrtc::PeerConnectionInterface::RTCConfiguration new_configuration =
      configuration_;
  new_configuration.servers = blink_config.servers;
  new_configuration.type = blink_config.type;
  new_configuration.bundle_policy = blink_config.bundle_policy;
  new_configuration.rtcp_mux_policy = blink_config.rtcp_mux_policy;
  new_configuration.sdp_semantics = blink_config.sdp_semantics;
  new_configuration.certificates = blink_config.certificates;
  new_configuration.ice_candidate_pool_size =
      blink_config.ice_candidate_pool_size;

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackSetConfiguration(this, new_configuration);

  webrtc::RTCError webrtc_error =
      native_peer_connection_->SetConfiguration(new_configuration);
  if (webrtc_error.ok()) {
    configuration_ = new_configuration;
  }

  return webrtc_error.type();
}

void RTCPeerConnectionHandler::AddICECandidate(
    RTCVoidRequest* request,
    RTCIceCandidatePlatform* candidate) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::addICECandidate");
  std::unique_ptr<webrtc::IceCandidateInterface> native_candidate(
      dependency_factory_->CreateIceCandidate(
          candidate->SdpMid(),
          candidate->SdpMLineIndex()
              ? static_cast<int>(*candidate->SdpMLineIndex())
              : -1,
          candidate->Candidate()));

  auto callback_on_task_runner =
      [](base::WeakPtr<RTCPeerConnectionHandler> handler_weak_ptr,
         base::WeakPtr<PeerConnectionTracker> tracker_weak_ptr,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             pending_local_description,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             current_local_description,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             pending_remote_description,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             current_remote_description,
         RTCIceCandidatePlatform* candidate, webrtc::RTCError result,
         RTCVoidRequest* request) {
        // Inform tracker (chrome://webrtc-internals).
        if (handler_weak_ptr && tracker_weak_ptr) {
          tracker_weak_ptr->TrackAddIceCandidate(
              handler_weak_ptr.get(), candidate,
              PeerConnectionTracker::SOURCE_REMOTE, result.ok());
        }
        // Update session descriptions.
        if (handler_weak_ptr) {
          handler_weak_ptr->OnSessionDescriptionsUpdated(
              std::move(pending_local_description),
              std::move(current_local_description),
              std::move(pending_remote_description),
              std::move(current_remote_description));
        }
        // Resolve promise.
        if (result.ok())
          request->RequestSucceeded();
        else
          request->RequestFailed(result);
      };

  native_peer_connection_->AddIceCandidate(
      std::move(native_candidate),
      [pc = native_peer_connection_, task_runner = task_runner_,
       handler_weak_ptr = weak_factory_.GetWeakPtr(),
       tracker_weak_ptr = peer_connection_tracker_, candidate,
       persistent_request = WrapCrossThreadPersistent(request),
       callback_on_task_runner =
           std::move(callback_on_task_runner)](webrtc::RTCError result) {
        // Grab a snapshot of all the session descriptions. AddIceCandidate may
        // have modified the remote description.
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            pending_local_description =
                CopySessionDescription(pc->pending_local_description());
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            current_local_description =
                CopySessionDescription(pc->current_local_description());
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            pending_remote_description =
                CopySessionDescription(pc->pending_remote_description());
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            current_remote_description =
                CopySessionDescription(pc->current_remote_description());
        // This callback is invoked on the webrtc signaling thread (this is true
        // in production, not in rtc_peer_connection_handler_test.cc which uses
        // a fake |native_peer_connection_|). Jump back to the renderer thread.
        PostCrossThreadTask(
            *task_runner, FROM_HERE,
            WTF::CrossThreadBindOnce(
                std::move(callback_on_task_runner), handler_weak_ptr,
                tracker_weak_ptr, std::move(pending_local_description),
                std::move(current_local_description),
                std::move(pending_remote_description),
                std::move(current_remote_description),
                WrapCrossThreadPersistent(candidate), std::move(result),
                std::move(persistent_request)));
      });
}

void RTCPeerConnectionHandler::RestartIce() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // The proxy invokes RestartIce() on the signaling thread.
  native_peer_connection_->RestartIce();
}

void RTCPeerConnectionHandler::GetStandardStatsForTracker(
    scoped_refptr<webrtc::RTCStatsCollectorCallback> observer) {
  native_peer_connection_->GetStats(observer.get());
}

void RTCPeerConnectionHandler::GetStats(RTCStatsRequest* request) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<LocalRTCStatsRequest> inner_request(
      new rtc::RefCountedObject<LocalRTCStatsRequest>(request));
  getStats(inner_request);
}

void RTCPeerConnectionHandler::getStats(
    const scoped_refptr<LocalRTCStatsRequest>& request) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::getStats");

  rtc::scoped_refptr<webrtc::StatsObserver> observer(
      new rtc::RefCountedObject<StatsResponse>(request, task_runner_));

  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> selector;
  if (request->hasSelector()) {
    auto track_adapter_ref =
        track_adapter_map_->GetLocalTrackAdapter(request->component());
    if (!track_adapter_ref) {
      track_adapter_ref =
          track_adapter_map_->GetRemoteTrackAdapter(request->component());
    }
    if (track_adapter_ref)
      selector = track_adapter_ref->webrtc_track();
  }

  GetStats(observer, webrtc::PeerConnectionInterface::kStatsOutputLevelStandard,
           std::move(selector));
}

// TODO(tommi,hbos): It's weird to have three {g|G}etStats methods for the
// legacy stats collector API and even more for the new stats API. Clean it up.
// TODO(hbos): Rename old |getStats| and related functions to "getLegacyStats",
// rename new |getStats|'s helper functions from "GetRTCStats*" to "GetStats*".
void RTCPeerConnectionHandler::GetStats(
    webrtc::StatsObserver* observer,
    webrtc::PeerConnectionInterface::StatsOutputLevel level,
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> selector) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *signaling_thread().get(), FROM_HERE,
      CrossThreadBindOnce(&GetStatsOnSignalingThread, native_peer_connection_,
                          level, base::WrapRefCounted(observer),
                          std::move(selector)));
}

void RTCPeerConnectionHandler::GetStats(
    RTCStatsReportCallback callback,
    const Vector<webrtc::NonStandardGroupId>& exposed_group_ids) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *signaling_thread().get(), FROM_HERE,
      CrossThreadBindOnce(
          &GetRTCStatsOnSignalingThread, task_runner_, native_peer_connection_,
          CrossThreadBindOnce(std::move(callback)), exposed_group_ids));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::AddTransceiverWithTrack(
    MediaStreamComponent* component,
    const webrtc::RtpTransceiverInit& init) {
  DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kUnifiedPlan);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
      track_ref = track_adapter_map_->GetOrCreateLocalTrackAdapter(component);
  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
      error_or_transceiver;
  RunSynchronousRepeatingClosureOnSignalingThread(
      base::BindRepeating(
          &RTCPeerConnectionHandler::AddTransceiverWithTrackOnSignalingThread,
          base::Unretained(this), base::RetainedRef(track_ref->webrtc_track()),
          std::cref(init), base::Unretained(&transceiver_state_surfacer),
          base::Unretained(&error_or_transceiver)),
      "AddTransceiverWithTrackOnSignalingThread");
  if (!error_or_transceiver.ok()) {
    // Don't leave the surfacer in a pending state.
    transceiver_state_surfacer.ObtainStates();
    return error_or_transceiver.MoveError();
  }

  auto transceiver_states = transceiver_state_surfacer.ObtainStates();
  auto transceiver =
      CreateOrUpdateTransceiver(std::move(transceiver_states[0]),
                                blink::TransceiverStateUpdateMode::kAll);
  std::unique_ptr<RTCRtpTransceiverPlatform> platform_transceiver =
      std::move(transceiver);
  return platform_transceiver;
}

void RTCPeerConnectionHandler::AddTransceiverWithTrackOnSignalingThread(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track,
    webrtc::RtpTransceiverInit init,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer,
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>*
        error_or_transceiver) {
  *error_or_transceiver =
      native_peer_connection_->AddTransceiver(webrtc_track, init);
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  if (error_or_transceiver->ok())
    transceivers.push_back(error_or_transceiver->value());
  transceiver_state_surfacer->Initialize(native_peer_connection_,
                                         track_adapter_map_, transceivers);
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::AddTransceiverWithKind(
    const String& kind,
    const webrtc::RtpTransceiverInit& init) {
  DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kUnifiedPlan);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  cricket::MediaType media_type;
  if (kind == webrtc::MediaStreamTrackInterface::kAudioKind) {
    media_type = cricket::MEDIA_TYPE_AUDIO;
  } else {
    DCHECK_EQ(kind, webrtc::MediaStreamTrackInterface::kVideoKind);
    media_type = cricket::MEDIA_TYPE_VIDEO;
  }
  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
      error_or_transceiver;
  RunSynchronousRepeatingClosureOnSignalingThread(
      base::BindRepeating(&RTCPeerConnectionHandler::
                              AddTransceiverWithMediaTypeOnSignalingThread,
                          base::Unretained(this), std::cref(media_type),
                          std::cref(init),
                          base::Unretained(&transceiver_state_surfacer),
                          base::Unretained(&error_or_transceiver)),
      "AddTransceiverWithMediaTypeOnSignalingThread");
  if (!error_or_transceiver.ok()) {
    // Don't leave the surfacer in a pending state.
    transceiver_state_surfacer.ObtainStates();
    return error_or_transceiver.MoveError();
  }

  auto transceiver_states = transceiver_state_surfacer.ObtainStates();
  auto transceiver =
      CreateOrUpdateTransceiver(std::move(transceiver_states[0]),
                                blink::TransceiverStateUpdateMode::kAll);
  std::unique_ptr<RTCRtpTransceiverPlatform> platform_transceiver =
      std::move(transceiver);
  return std::move(platform_transceiver);
}

void RTCPeerConnectionHandler::AddTransceiverWithMediaTypeOnSignalingThread(
    cricket::MediaType media_type,
    webrtc::RtpTransceiverInit init,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer,
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>*
        error_or_transceiver) {
  *error_or_transceiver =
      native_peer_connection_->AddTransceiver(media_type, init);
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  if (error_or_transceiver->ok())
    transceivers.push_back(error_or_transceiver->value());
  transceiver_state_surfacer->Initialize(native_peer_connection_,
                                         track_adapter_map_, transceivers);
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::AddTrack(
    MediaStreamComponent* component,
    const MediaStreamDescriptorVector& descriptors) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::AddTrack");

  std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
      track_ref = track_adapter_map_->GetOrCreateLocalTrackAdapter(component);
  std::vector<std::string> stream_ids(descriptors.size());
  for (WTF::wtf_size_t i = 0; i < descriptors.size(); ++i)
    stream_ids[i] = descriptors[i]->Id().Utf8();

  // Invoke native AddTrack() on the signaling thread and surface the resulting
  // transceiver (Plan B: sender only).
  // TODO(hbos): Implement and surface full transceiver support under Unified
  // Plan. https://crbug.com/777617
  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
      error_or_sender;
  RunSynchronousRepeatingClosureOnSignalingThread(
      base::BindRepeating(
          &RTCPeerConnectionHandler::AddTrackOnSignalingThread,
          base::Unretained(this), base::RetainedRef(track_ref->webrtc_track()),
          std::move(stream_ids), base::Unretained(&transceiver_state_surfacer),
          base::Unretained(&error_or_sender)),
      "AddTrackOnSignalingThread");
  DCHECK(transceiver_state_surfacer.is_initialized());
  if (!error_or_sender.ok()) {
    // Don't leave the surfacer in a pending state.
    transceiver_state_surfacer.ObtainStates();
    return error_or_sender.MoveError();
  }
  track_metrics_.AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                          MediaStreamTrackMetricsKind(component),
                          component->Id().Utf8());

  auto transceiver_states = transceiver_state_surfacer.ObtainStates();
  DCHECK_EQ(transceiver_states.size(), 1u);
  auto transceiver_state = std::move(transceiver_states[0]);

  std::unique_ptr<RTCRtpTransceiverPlatform> platform_transceiver;
  if (configuration_.sdp_semantics == webrtc::SdpSemantics::kPlanB) {
    // Plan B: Create sender only.
    DCHECK(transceiver_state.sender_state());
    auto webrtc_sender = transceiver_state.sender_state()->webrtc_sender();
    DCHECK(FindSender(blink::RTCRtpSenderImpl::getId(webrtc_sender.get())) ==
           rtp_senders_.end());
    blink::RtpSenderState sender_state = transceiver_state.MoveSenderState();
    DCHECK(sender_state.is_initialized());
    rtp_senders_.push_back(std::make_unique<blink::RTCRtpSenderImpl>(
        native_peer_connection_, track_adapter_map_, std::move(sender_state),
        force_encoded_audio_insertable_streams_,
        force_encoded_video_insertable_streams_));
    MaybeCreateThermalUmaListner();
    platform_transceiver = std::make_unique<blink::RTCRtpSenderOnlyTransceiver>(
        std::make_unique<blink::RTCRtpSenderImpl>(*rtp_senders_.back().get()));
  } else {
    DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kUnifiedPlan);
    // Unified Plan: Create or recycle a transceiver.
    auto transceiver = CreateOrUpdateTransceiver(
        std::move(transceiver_state), blink::TransceiverStateUpdateMode::kAll);
    platform_transceiver = std::move(transceiver);
  }
  if (peer_connection_tracker_) {
    size_t transceiver_index = GetTransceiverIndex(*platform_transceiver.get());
    peer_connection_tracker_->TrackAddTransceiver(
        this, PeerConnectionTracker::TransceiverUpdatedReason::kAddTrack,
        *platform_transceiver.get(), transceiver_index);
  }
  for (const auto& stream_id : rtp_senders_.back()->state().stream_ids()) {
    if (GetLocalStreamUsageCount(rtp_senders_, stream_id) == 1u) {
      // This is the first occurrence of this stream.
      blink::PerSessionWebRTCAPIMetrics::GetInstance()
          ->IncrementStreamCounter();
    }
  }
  return platform_transceiver;
}

void RTCPeerConnectionHandler::AddTrackOnSignalingThread(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    std::vector<std::string> stream_ids,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer,
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>*
        error_or_sender) {
  *error_or_sender = native_peer_connection_->AddTrack(track, stream_ids);
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  if (error_or_sender->ok()) {
    auto sender = error_or_sender->value();
    if (configuration_.sdp_semantics == webrtc::SdpSemantics::kPlanB) {
      transceivers = {new blink::SurfaceSenderStateOnly(sender)};
    } else {
      DCHECK_EQ(configuration_.sdp_semantics,
                webrtc::SdpSemantics::kUnifiedPlan);
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface>
          transceiver_for_sender = nullptr;
      for (const auto& transceiver :
           native_peer_connection_->GetTransceivers()) {
        if (transceiver->sender() == sender) {
          transceiver_for_sender = transceiver;
          break;
        }
      }
      DCHECK(transceiver_for_sender);
      transceivers = {transceiver_for_sender};
    }
  }
  transceiver_state_surfacer->Initialize(
      native_peer_connection_, track_adapter_map_, std::move(transceivers));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::RemoveTrack(blink::RTCRtpSenderPlatform* web_sender) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::RemoveTrack");
  if (configuration_.sdp_semantics == webrtc::SdpSemantics::kPlanB) {
    if (RemoveTrackPlanB(web_sender)) {
      // In Plan B, null indicates success.
      std::unique_ptr<RTCRtpTransceiverPlatform> platform_transceiver = nullptr;
      return std::move(platform_transceiver);
    }
    // TODO(hbos): Surface RTCError from third_party/webrtc when
    // peerconnectioninterface.h is updated. https://crbug.com/webrtc/9534
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE);
  }
  DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kUnifiedPlan);
  return RemoveTrackUnifiedPlan(web_sender);
}

bool RTCPeerConnectionHandler::RemoveTrackPlanB(
    blink::RTCRtpSenderPlatform* web_sender) {
  DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kPlanB);
  auto* track = web_sender->Track();
  auto it = FindSender(web_sender->Id());
  if (it == rtp_senders_.end())
    return false;
  if (!(*it)->RemoveFromPeerConnection(native_peer_connection_.get()))
    return false;
  if (track) {
    track_metrics_.RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                               MediaStreamTrackMetricsKind(track),
                               track->Id().Utf8());
  }
  if (peer_connection_tracker_) {
    auto sender_only_transceiver =
        std::make_unique<blink::RTCRtpSenderOnlyTransceiver>(
            std::make_unique<blink::RTCRtpSenderImpl>(*it->get()));
    size_t sender_index = GetTransceiverIndex(*sender_only_transceiver);
    peer_connection_tracker_->TrackRemoveTransceiver(
        this, PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack,
        *sender_only_transceiver.get(), sender_index);
  }
  std::vector<std::string> stream_ids = (*it)->state().stream_ids();
  rtp_senders_.erase(it);
  for (const auto& stream_id : stream_ids) {
    if (GetLocalStreamUsageCount(rtp_senders_, stream_id) == 0u) {
      // This was the last occurrence of this stream.
      blink::PerSessionWebRTCAPIMetrics::GetInstance()
          ->DecrementStreamCounter();
    }
  }
  return true;
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::RemoveTrackUnifiedPlan(
    blink::RTCRtpSenderPlatform* web_sender) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kUnifiedPlan);
  auto it = FindSender(web_sender->Id());
  if (it == rtp_senders_.end())
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER);
  const auto& sender = *it;
  auto webrtc_sender = sender->state().webrtc_sender();

  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  CancellableBooleanOperationResult result;
  RunSynchronousRepeatingClosureOnSignalingThread(
      base::BindRepeating(
          &RTCPeerConnectionHandler::RemoveTrackUnifiedPlanOnSignalingThread,
          base::Unretained(this), base::RetainedRef(webrtc_sender),
          base::Unretained(&transceiver_state_surfacer),
          base::Unretained(&result)),
      "RemoveTrackUnifiedPlanOnSignalingThread");
  DCHECK(transceiver_state_surfacer.is_initialized());
  if (result != CancellableBooleanOperationResult::kSuccess) {
    // Don't leave the surfacer in a pending state.
    transceiver_state_surfacer.ObtainStates();
    if (result == CancellableBooleanOperationResult::kCancelled) {
      return std::unique_ptr<RTCRtpTransceiverPlatform>(nullptr);
    }
    // TODO(hbos): Surface RTCError from third_party/webrtc when
    // peerconnectioninterface.h is updated. https://crbug.com/webrtc/9534
    return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
  }

  auto transceiver_states = transceiver_state_surfacer.ObtainStates();
  DCHECK_EQ(transceiver_states.size(), 1u);
  auto transceiver_state = std::move(transceiver_states[0]);

  // Update the transceiver.
  auto transceiver = CreateOrUpdateTransceiver(
      std::move(transceiver_state), blink::TransceiverStateUpdateMode::kAll);
  if (peer_connection_tracker_) {
    size_t transceiver_index = GetTransceiverIndex(*transceiver);
    peer_connection_tracker_->TrackModifyTransceiver(
        this, PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack,
        *transceiver.get(), transceiver_index);
  }
  std::unique_ptr<RTCRtpTransceiverPlatform> platform_transceiver =
      std::move(transceiver);
  return platform_transceiver;
}

void RTCPeerConnectionHandler::RemoveTrackUnifiedPlanOnSignalingThread(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer,
    CancellableBooleanOperationResult* result) {
  bool is_successful = native_peer_connection_->RemoveTrack(sender);
  *result = is_successful ? CancellableBooleanOperationResult::kSuccess
                          : CancellableBooleanOperationResult::kFailure;
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  if (is_successful) {
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver_for_sender =
        nullptr;
    for (const auto& transceiver : native_peer_connection_->GetTransceivers()) {
      if (transceiver->sender() == sender) {
        transceiver_for_sender = transceiver;
        break;
      }
    }
    if (!transceiver_for_sender) {
      // If the transceiver doesn't exist, it must have been rolled back while
      // we were performing removeTrack(). Abort this operation.
      *result = CancellableBooleanOperationResult::kCancelled;
    } else {
      transceivers = {transceiver_for_sender};
    }
  }
  transceiver_state_surfacer->Initialize(
      native_peer_connection_, track_adapter_map_, std::move(transceivers));
}

void RTCPeerConnectionHandler::CloseClientPeerConnection() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!is_closed_)
    client_->ClosePeerConnection();
}

void RTCPeerConnectionHandler::MaybeCreateThermalUmaListner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!thermal_uma_listener_) {
    // Instantiate the thermal uma listener only if we are sending video.
    for (const auto& sender : rtp_senders_) {
      if (sender->Track() && sender->Track()->Source()->GetType() ==
                                 MediaStreamSource::kTypeVideo) {
        thermal_uma_listener_ = ThermalUmaListener::Create(task_runner_);
        thermal_uma_listener_->OnThermalMeasurement(last_thermal_state_);
        return;
      }
    }
  }
}

ThermalUmaListener* RTCPeerConnectionHandler::thermal_uma_listener() const {
  return thermal_uma_listener_.get();
}

void RTCPeerConnectionHandler::OnThermalStateChange(
    base::PowerObserver::DeviceThermalState thermal_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (is_closed_)
    return;
  last_thermal_state_ = thermal_state;
  if (thermal_uma_listener_) {
    thermal_uma_listener_->OnThermalMeasurement(thermal_state);
  }
  if (!base::FeatureList::IsEnabled(kWebRtcThermalResource))
    return;
  if (!thermal_resource_) {
    thermal_resource_ = ThermalResource::Create(task_runner_);
    native_peer_connection_->AddAdaptationResource(
        rtc::scoped_refptr<ThermalResource>(thermal_resource_.get()));
  }
  thermal_resource_->OnThermalMeasurement(thermal_state);
}

void RTCPeerConnectionHandler::StartEventLog(int output_period_ms) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // TODO(eladalon): StartRtcEventLog() return value is not useful; remove it
  // or find a way to be able to use it.
  // https://crbug.com/775415
  native_peer_connection_->StartRtcEventLog(
      std::make_unique<RtcEventLogOutputSinkProxy>(peer_connection_observer_),
      output_period_ms);
}

void RTCPeerConnectionHandler::StopEventLog() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  native_peer_connection_->StopRtcEventLog();
}

void RTCPeerConnectionHandler::OnWebRtcEventLogWrite(
    const WTF::Vector<uint8_t>& output) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackRtcEventLogWrite(this, output);
  }
}

scoped_refptr<DataChannelInterface> RTCPeerConnectionHandler::CreateDataChannel(
    const String& label,
    const webrtc::DataChannelInit& init) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createDataChannel");
  DVLOG(1) << "createDataChannel label " << label.Utf8();

  rtc::scoped_refptr<DataChannelInterface> webrtc_channel(
      native_peer_connection_->CreateDataChannel(label.Utf8(), &init));
  if (!webrtc_channel) {
    DLOG(ERROR) << "Could not create native data channel.";
    return nullptr;
  }
  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackCreateDataChannel(
        this, webrtc_channel.get(), PeerConnectionTracker::SOURCE_LOCAL);
  }

  ++num_data_channels_created_;

  return base::WrapRefCounted<DataChannelInterface>(webrtc_channel.get());
}

void RTCPeerConnectionHandler::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << "RTCPeerConnectionHandler::stop";

  if (is_closed_ || !native_peer_connection_.get())
    return;  // Already stopped.

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackStop(this);

  native_peer_connection_->Close();

  // This object may no longer forward call backs to blink.
  is_closed_ = true;
}

webrtc::PeerConnectionInterface*
RTCPeerConnectionHandler::NativePeerConnection() {
  return native_peer_connection();
}

void RTCPeerConnectionHandler::RunSynchronousOnceClosureOnSignalingThread(
    CrossThreadOnceClosure closure,
    const char* trace_event_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<base::SingleThreadTaskRunner> thread(signaling_thread());
  if (!thread.get() || thread->BelongsToCurrentThread()) {
    TRACE_EVENT0("webrtc", trace_event_name);
    std::move(closure).Run();
  } else {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    PostCrossThreadTask(
        *thread.get(), FROM_HERE,
        CrossThreadBindOnce(&RunSynchronousOnceClosure, std::move(closure),
                            CrossThreadUnretained(trace_event_name),
                            CrossThreadUnretained(&event)));
    event.Wait();
  }
}

void RTCPeerConnectionHandler::RunSynchronousRepeatingClosureOnSignalingThread(
    const base::RepeatingClosure& closure,
    const char* trace_event_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<base::SingleThreadTaskRunner> thread(signaling_thread());
  if (!thread.get() || thread->BelongsToCurrentThread()) {
    TRACE_EVENT0("webrtc", trace_event_name);
    closure.Run();
  } else {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread->PostTask(FROM_HERE,
                     base::BindOnce(&RunSynchronousRepeatingClosure, closure,
                                    base::Unretained(trace_event_name),
                                    base::Unretained(&event)));
    event.Wait();
  }
}

void RTCPeerConnectionHandler::OnSessionDescriptionsUpdated(
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        pending_local_description,
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        current_local_description,
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        pending_remote_description,
    std::unique_ptr<webrtc::SessionDescriptionInterface>
        current_remote_description) {
  if (!client_ || is_closed_)
    return;
  client_->DidChangeSessionDescriptions(
      pending_local_description
          ? CreateWebKitSessionDescription(pending_local_description.get())
          : nullptr,
      current_local_description
          ? CreateWebKitSessionDescription(current_local_description.get())
          : nullptr,
      pending_remote_description
          ? CreateWebKitSessionDescription(pending_remote_description.get())
          : nullptr,
      current_remote_description
          ? CreateWebKitSessionDescription(current_remote_description.get())
          : nullptr);
}

// Note: This function is purely for chrome://webrtc-internals/ tracking
// purposes. The JavaScript visible event and attribute is processed together
// with transceiver or receiver changes.
void RTCPeerConnectionHandler::TrackSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::TrackSignalingChange");
  if (previous_signaling_state_ ==
          webrtc::PeerConnectionInterface::kHaveLocalOffer &&
      new_state == webrtc::PeerConnectionInterface::kHaveRemoteOffer) {
    // Inject missing kStable in case of implicit rollback.
    auto stable_state = webrtc::PeerConnectionInterface::kStable;
    if (peer_connection_tracker_)
      peer_connection_tracker_->TrackSignalingStateChange(this, stable_state);
  }
  previous_signaling_state_ = new_state;
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackSignalingStateChange(this, new_state);
}

// Called any time the IceConnectionState changes
void RTCPeerConnectionHandler::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceConnectionChange");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ReportICEState(new_state);
  if (new_state == webrtc::PeerConnectionInterface::kIceConnectionChecking) {
    ice_connection_checking_start_ = base::TimeTicks::Now();
  } else if (new_state ==
             webrtc::PeerConnectionInterface::kIceConnectionConnected) {
    // If the state becomes connected, send the time needed for PC to become
    // connected from checking to UMA. UMA data will help to know how much
    // time needed for PC to connect with remote peer.
    if (ice_connection_checking_start_.is_null()) {
      // From UMA, we have observed a large number of calls falling into the
      // overflow buckets. One possibility is that the Checking is not signaled
      // before Connected. This is to guard against that situation to make the
      // metric more robust.
      UMA_HISTOGRAM_MEDIUM_TIMES("WebRTC.PeerConnection.TimeToConnect",
                                 base::TimeDelta());
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "WebRTC.PeerConnection.TimeToConnect",
          base::TimeTicks::Now() - ice_connection_checking_start_);
    }
  }

  track_metrics_.IceConnectionChange(new_state);
  if (!is_closed_)
    client_->DidChangeIceConnectionState(new_state);
}

void RTCPeerConnectionHandler::TrackIceConnectionStateChange(
    RTCPeerConnectionHandler::IceConnectionStateVersion version,
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  if (!peer_connection_tracker_)
    return;
  switch (version) {
    case RTCPeerConnectionHandler::IceConnectionStateVersion::kLegacy:
      peer_connection_tracker_->TrackLegacyIceConnectionStateChange(this,
                                                                    state);
      break;
    case RTCPeerConnectionHandler::IceConnectionStateVersion::kDefault:
      peer_connection_tracker_->TrackIceConnectionStateChange(this, state);
      break;
  }
}

// Called any time the combined peerconnection state changes
void RTCPeerConnectionHandler::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackConnectionStateChange(this, new_state);
  if (!is_closed_)
    client_->DidChangePeerConnectionState(new_state);
}

// Called any time the IceGatheringState changes
void RTCPeerConnectionHandler::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceGatheringChange");

  if (new_state == webrtc::PeerConnectionInterface::kIceGatheringComplete) {
    UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv4LocalCandidates",
                             num_local_candidates_ipv4_);

    UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv6LocalCandidates",
                             num_local_candidates_ipv6_);
  } else if (new_state ==
             webrtc::PeerConnectionInterface::kIceGatheringGathering) {
    // ICE restarts will change gathering state back to "gathering",
    // reset the counter.
    ResetUMAStats();
  }

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackIceGatheringStateChange(this, new_state);
  if (!is_closed_)
    client_->DidChangeIceGatheringState(new_state);
}

void RTCPeerConnectionHandler::OnNegotiationNeededEvent(uint32_t event_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnNegotiationNeededEvent");
  if (is_closed_)
    return;
  if (!native_peer_connection_->ShouldFireNegotiationNeededEvent(event_id)) {
    return;
  }
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackOnRenegotiationNeeded(this);
  client_->NegotiationNeeded();
}

void RTCPeerConnectionHandler::OnReceiversModifiedPlanB(
    webrtc::PeerConnectionInterface::SignalingState signaling_state,
    Vector<blink::RtpReceiverState> added_receiver_states,
    Vector<uintptr_t> removed_receiver_ids) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnReceiversModifiedPlanB");

  // Process the addition of receivers.
  Vector<std::unique_ptr<RTCRtpReceiverPlatform>> platform_receivers_added;
  for (blink::RtpReceiverState& receiver_state : added_receiver_states) {
    DCHECK(receiver_state.is_initialized());
    auto* track = receiver_state.track_ref()->track();
    // Update metrics.
    track_metrics_.AddTrack(MediaStreamTrackMetrics::Direction::kReceive,
                            MediaStreamTrackMetricsKind(track),
                            track->Id().Utf8());
    for (const auto& stream_id : receiver_state.stream_ids()) {
      // New remote stream?
      if (!IsRemoteStream(rtp_receivers_, stream_id)) {
        blink::PerSessionWebRTCAPIMetrics::GetInstance()
            ->IncrementStreamCounter();
      }
    }
    uintptr_t receiver_id = blink::RTCRtpReceiverImpl::getId(
        receiver_state.webrtc_receiver().get());
    DCHECK(FindReceiver(receiver_id) == rtp_receivers_.end());
    auto rtp_receiver = std::make_unique<blink::RTCRtpReceiverImpl>(
        native_peer_connection_, std::move(receiver_state),
        force_encoded_audio_insertable_streams_,
        force_encoded_video_insertable_streams_);
    rtp_receivers_.push_back(
        std::make_unique<blink::RTCRtpReceiverImpl>(*rtp_receiver));
    if (peer_connection_tracker_) {
      auto receiver_only_transceiver =
          std::make_unique<blink::RTCRtpReceiverOnlyTransceiver>(
              std::make_unique<blink::RTCRtpReceiverImpl>(*rtp_receiver));
      size_t receiver_index = GetTransceiverIndex(*receiver_only_transceiver);
      peer_connection_tracker_->TrackAddTransceiver(
          this,
          PeerConnectionTracker::TransceiverUpdatedReason::
              kSetRemoteDescription,
          *receiver_only_transceiver.get(), receiver_index);
    }

    platform_receivers_added.push_back(rtp_receiver->ShallowCopy());
  }

  // Process the removal of receivers.
  Vector<std::unique_ptr<RTCRtpReceiverPlatform>> platform_receivers_removed;
  for (uintptr_t receiver_id : removed_receiver_ids) {
    auto it = FindReceiver(receiver_id);
    DCHECK(it != rtp_receivers_.end());
    auto receiver = std::make_unique<blink::RTCRtpReceiverImpl>(*(*it));
    // Update metrics.
    track_metrics_.RemoveTrack(MediaStreamTrackMetrics::Direction::kReceive,
                               MediaStreamTrackMetricsKind(receiver->Track()),
                               receiver->Track()->Id().Utf8());
    if (peer_connection_tracker_) {
      auto receiver_only_transceiver =
          std::make_unique<blink::RTCRtpReceiverOnlyTransceiver>(
              std::make_unique<blink::RTCRtpReceiverImpl>(*receiver));
      size_t receiver_index = GetTransceiverIndex(*receiver_only_transceiver);
      peer_connection_tracker_->TrackRemoveTransceiver(
          this,
          PeerConnectionTracker::TransceiverUpdatedReason::
              kSetRemoteDescription,
          *receiver_only_transceiver.get(), receiver_index);
    }
    rtp_receivers_.erase(it);
    for (const auto& stream_id : receiver->state().stream_ids()) {
      // This was the last occurence of the stream?
      if (!IsRemoteStream(rtp_receivers_, stream_id)) {
        blink::PerSessionWebRTCAPIMetrics::GetInstance()
            ->IncrementStreamCounter();
      }
    }

    platform_receivers_removed.push_back(std::move(receiver));
  }

  // Surface changes to RTCPeerConnection.
  if (!is_closed_) {
    client_->DidModifyReceiversPlanB(signaling_state,
                                     std::move(platform_receivers_added),
                                     std::move(platform_receivers_removed));
  }
}

void RTCPeerConnectionHandler::OnModifySctpTransport(
    blink::WebRTCSctpTransportSnapshot state) {
  if (client_)
    client_->DidModifySctpTransport(state);
}

void RTCPeerConnectionHandler::OnModifyTransceivers(
    webrtc::PeerConnectionInterface::SignalingState signaling_state,
    std::vector<blink::RtpTransceiverState> transceiver_states,
    bool is_remote_description) {
  DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kUnifiedPlan);
  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> platform_transceivers(
      SafeCast<WTF::wtf_size_t>(transceiver_states.size()));
  PeerConnectionTracker::TransceiverUpdatedReason update_reason =
      !is_remote_description ? PeerConnectionTracker::TransceiverUpdatedReason::
                                   kSetLocalDescription
                             : PeerConnectionTracker::TransceiverUpdatedReason::
                                   kSetRemoteDescription;
  Vector<uintptr_t> ids(SafeCast<wtf_size_t>(transceiver_states.size()));
  for (WTF::wtf_size_t i = 0; i < transceiver_states.size(); ++i) {
    // Figure out if this transceiver is new or if setting the state modified
    // the transceiver such that it should be logged by the
    // |peer_connection_tracker_|.
    uintptr_t transceiver_id = blink::RTCRtpTransceiverImpl::GetId(
        transceiver_states[i].webrtc_transceiver().get());
    ids[i] = transceiver_id;
    auto it = FindTransceiver(transceiver_id);
    bool transceiver_is_new = (it == rtp_transceivers_.end());
    bool transceiver_was_modified = false;
    if (!transceiver_is_new) {
      const auto& previous_state = (*it)->state();
      transceiver_was_modified =
          previous_state.mid() != transceiver_states[i].mid() ||
          previous_state.stopped() != transceiver_states[i].stopped() ||
          previous_state.direction() != transceiver_states[i].direction() ||
          previous_state.current_direction() !=
              transceiver_states[i].current_direction();
    }

    // Update the transceiver.
    platform_transceivers[i] = CreateOrUpdateTransceiver(
        std::move(transceiver_states[i]),
        blink::TransceiverStateUpdateMode::kSetDescription);

    // Log a "transceiverAdded" or "transceiverModified" event in
    // chrome://webrtc-internals if new or modified.
    if (peer_connection_tracker_ &&
        (transceiver_is_new || transceiver_was_modified)) {
      size_t transceiver_index = GetTransceiverIndex(*platform_transceivers[i]);
      if (transceiver_is_new) {
        peer_connection_tracker_->TrackAddTransceiver(
            this, update_reason, *platform_transceivers[i].get(),
            transceiver_index);
      } else if (transceiver_was_modified) {
        peer_connection_tracker_->TrackModifyTransceiver(
            this, update_reason, *platform_transceivers[i].get(),
            transceiver_index);
      }
    }
  }
  // Search for removed transceivers by comparing to previous state.
  Vector<uintptr_t> removed_transceivers;
  for (auto transceiver_id : previous_transceiver_ids_) {
    if (std::find(ids.begin(), ids.end(), transceiver_id) == ids.end()) {
      removed_transceivers.emplace_back(transceiver_id);
      rtp_transceivers_.erase(FindTransceiver(transceiver_id));
    }
  }
  previous_transceiver_ids_ = ids;
  if (!is_closed_) {
    client_->DidModifyTransceivers(signaling_state,
                                   std::move(platform_transceivers),
                                   removed_transceivers, is_remote_description);
  }
}

void RTCPeerConnectionHandler::OnDataChannel(
    scoped_refptr<DataChannelInterface> channel) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnDataChannelImpl");

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackCreateDataChannel(
        this, channel.get(), PeerConnectionTracker::SOURCE_REMOTE);
  }

  if (!is_closed_)
    client_->DidAddRemoteDataChannel(std::move(channel));
}

void RTCPeerConnectionHandler::OnIceCandidate(const String& sdp,
                                              const String& sdp_mid,
                                              int sdp_mline_index,
                                              int component,
                                              int address_family) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceCandidateImpl");
  auto* platform_candidate = MakeGarbageCollected<RTCIceCandidatePlatform>(
      sdp, sdp_mid, sdp_mline_index);
  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackAddIceCandidate(
        this, platform_candidate, PeerConnectionTracker::SOURCE_LOCAL, true);
  }

  // Only the first m line's first component is tracked to avoid
  // miscounting when doing BUNDLE or rtcp mux.
  if (sdp_mline_index == 0 && component == 1) {
    if (address_family == AF_INET) {
      ++num_local_candidates_ipv4_;
    } else if (address_family == AF_INET6) {
      ++num_local_candidates_ipv6_;
    } else if (!IsHostnameCandidate(*platform_candidate)) {
      NOTREACHED();
    }
  }
  if (!is_closed_)
    client_->DidGenerateICECandidate(platform_candidate);
}

void RTCPeerConnectionHandler::OnIceCandidateError(
    const String& address,
    base::Optional<uint16_t> port,
    const String& host_candidate,
    const String& url,
    int error_code,
    const String& error_text) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceCandidateError");

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackIceCandidateError(
        this, address, port, host_candidate, url, error_code, error_text);
  }
  if (!is_closed_) {
    client_->DidFailICECandidate(address, port, host_candidate, url, error_code,
                                 error_text);
  }
}

void RTCPeerConnectionHandler::OnInterestingUsage(int usage_pattern) {
  if (client_)
    client_->DidNoteInterestingUsage(usage_pattern);
}

webrtc::SessionDescriptionInterface*
RTCPeerConnectionHandler::CreateNativeSessionDescription(
    const String& sdp,
    const String& type,
    webrtc::SdpParseError* error) {
  webrtc::SessionDescriptionInterface* native_desc =
      dependency_factory_->CreateSessionDescription(type, sdp, error);

  LOG_IF(ERROR, !native_desc) << "Failed to create native session description."
                              << " Type: " << type << " SDP: " << sdp;

  return native_desc;
}

RTCPeerConnectionHandler::FirstSessionDescription::FirstSessionDescription(
    const webrtc::SessionDescriptionInterface* sdesc) {
  DCHECK(sdesc);

  for (const auto& content : sdesc->description()->contents()) {
    if (content.type == cricket::MediaProtocolType::kRtp) {
      const auto* mdesc = content.media_description();
      audio = audio || (mdesc->type() == cricket::MEDIA_TYPE_AUDIO);
      video = video || (mdesc->type() == cricket::MEDIA_TYPE_VIDEO);
      rtcp_mux = rtcp_mux || mdesc->rtcp_mux();
    }
  }
}

void RTCPeerConnectionHandler::ReportFirstSessionDescriptions(
    const FirstSessionDescription& local,
    const FirstSessionDescription& remote) {
  RtcpMux rtcp_mux = RTCP_MUX_ENABLED;
  if ((!local.audio && !local.video) || (!remote.audio && !remote.video)) {
    rtcp_mux = RTCP_MUX_NO_MEDIA;
  } else if (!local.rtcp_mux || !remote.rtcp_mux) {
    rtcp_mux = RTCP_MUX_DISABLED;
  }

  UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.RtcpMux", rtcp_mux,
                            RTCP_MUX_MAX);

  // TODO(pthatcher): Reports stats about whether we have audio and
  // video or not.
}

std::vector<std::unique_ptr<blink::RTCRtpSenderImpl>>::iterator
RTCPeerConnectionHandler::FindSender(uintptr_t id) {
  for (auto it = rtp_senders_.begin(); it != rtp_senders_.end(); ++it) {
    if ((*it)->Id() == id)
      return it;
  }
  return rtp_senders_.end();
}

std::vector<std::unique_ptr<blink::RTCRtpReceiverImpl>>::iterator
RTCPeerConnectionHandler::FindReceiver(uintptr_t id) {
  for (auto it = rtp_receivers_.begin(); it != rtp_receivers_.end(); ++it) {
    if ((*it)->Id() == id)
      return it;
  }
  return rtp_receivers_.end();
}

std::vector<std::unique_ptr<blink::RTCRtpTransceiverImpl>>::iterator
RTCPeerConnectionHandler::FindTransceiver(uintptr_t id) {
  for (auto it = rtp_transceivers_.begin(); it != rtp_transceivers_.end();
       ++it) {
    if ((*it)->Id() == id)
      return it;
  }
  return rtp_transceivers_.end();
}

size_t RTCPeerConnectionHandler::GetTransceiverIndex(
    const RTCRtpTransceiverPlatform& platform_transceiver) {
  if (platform_transceiver.ImplementationType() ==
      RTCRtpTransceiverPlatformImplementationType::kFullTransceiver) {
    for (size_t i = 0; i < rtp_transceivers_.size(); ++i) {
      if (platform_transceiver.Id() == rtp_transceivers_[i]->Id())
        return i;
    }
  } else if (platform_transceiver.ImplementationType() ==
             RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly) {
    const auto web_sender = platform_transceiver.Sender();
    for (size_t i = 0; i < rtp_senders_.size(); ++i) {
      if (web_sender->Id() == rtp_senders_[i]->Id())
        return i;
    }
  } else {
    RTC_DCHECK(platform_transceiver.ImplementationType() ==
               RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
    const auto platform_receiver = platform_transceiver.Receiver();
    for (size_t i = 0; i < rtp_receivers_.size(); ++i) {
      if (platform_receiver->Id() == rtp_receivers_[i]->Id())
        return i;
    }
  }
  NOTREACHED();
  return 0u;
}

std::unique_ptr<blink::RTCRtpTransceiverImpl>
RTCPeerConnectionHandler::CreateOrUpdateTransceiver(
    blink::RtpTransceiverState transceiver_state,
    blink::TransceiverStateUpdateMode update_mode) {
  DCHECK_EQ(configuration_.sdp_semantics, webrtc::SdpSemantics::kUnifiedPlan);
  DCHECK(transceiver_state.is_initialized());
  DCHECK(transceiver_state.sender_state());
  DCHECK(transceiver_state.receiver_state());
  auto webrtc_transceiver = transceiver_state.webrtc_transceiver();
  auto webrtc_sender = transceiver_state.sender_state()->webrtc_sender();
  auto webrtc_receiver = transceiver_state.receiver_state()->webrtc_receiver();

  std::unique_ptr<blink::RTCRtpTransceiverImpl> transceiver;
  auto it = FindTransceiver(
      blink::RTCRtpTransceiverImpl::GetId(webrtc_transceiver.get()));
  if (it == rtp_transceivers_.end()) {
    // Create a new transceiver, including a sender and a receiver.
    transceiver = std::make_unique<blink::RTCRtpTransceiverImpl>(
        native_peer_connection_, track_adapter_map_,
        std::move(transceiver_state), force_encoded_audio_insertable_streams_,
        force_encoded_video_insertable_streams_);
    rtp_transceivers_.push_back(transceiver->ShallowCopy());
    DCHECK(FindSender(blink::RTCRtpSenderImpl::getId(webrtc_sender.get())) ==
           rtp_senders_.end());
    rtp_senders_.push_back(std::make_unique<blink::RTCRtpSenderImpl>(
        *transceiver->content_sender()));
    MaybeCreateThermalUmaListner();
    DCHECK(FindReceiver(blink::RTCRtpReceiverImpl::getId(
               webrtc_receiver.get())) == rtp_receivers_.end());
    rtp_receivers_.push_back(std::make_unique<blink::RTCRtpReceiverImpl>(
        *transceiver->content_receiver()));
  } else {
    // Update the transceiver. This also updates the sender and receiver.
    transceiver = (*it)->ShallowCopy();
    transceiver->set_state(std::move(transceiver_state), update_mode);
    DCHECK(FindSender(blink::RTCRtpSenderImpl::getId(webrtc_sender.get())) !=
           rtp_senders_.end());
    DCHECK(FindReceiver(blink::RTCRtpReceiverImpl::getId(
               webrtc_receiver.get())) != rtp_receivers_.end());
  }
  return transceiver;
}

scoped_refptr<base::SingleThreadTaskRunner>
RTCPeerConnectionHandler::signaling_thread() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return dependency_factory_->GetWebRtcSignalingTaskRunner();
}

void RTCPeerConnectionHandler::ReportICEState(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (ice_state_seen_[new_state])
    return;
  ice_state_seen_[new_state] = true;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.ConnectionState", new_state,
                            webrtc::PeerConnectionInterface::kIceConnectionMax);
}

void RTCPeerConnectionHandler::ResetUMAStats() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  num_local_candidates_ipv6_ = 0;
  num_local_candidates_ipv4_ = 0;
  ice_connection_checking_start_ = base::TimeTicks();
  memset(ice_state_seen_, 0, sizeof(ice_state_seen_));
}
}  // namespace blink
