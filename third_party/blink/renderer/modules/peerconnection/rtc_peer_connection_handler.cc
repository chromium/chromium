// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"

#include <string.h>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "build/chromecast_buildflags.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/speed_limit_uma_listener.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_set_description_observer.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_answer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink_proxy.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_transceiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_request.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/data_channel_interface.h"
#include "third_party/webrtc/api/rtc_event_log_output.h"
#include "third_party/webrtc/api/units/time_delta.h"
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
struct CrossThreadCopier<rtc::scoped_refptr<webrtc::StatsObserver>>
    : public CrossThreadCopierPassThrough<
          rtc::scoped_refptr<webrtc::StatsObserver>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {
namespace {

// Used to back histogram value of "WebRTC.PeerConnection.RtcpMux",
// so treat as append-only.
enum class RtcpMux { kDisabled, kEnabled, kNoMedia, kMax };

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

void RunSynchronousOnceClosure(base::OnceClosure closure,
                               const char* trace_event_name,
                               base::WaitableEvent* event) {
  {
    TRACE_EVENT0("webrtc", trace_event_name);
    std::move(closure).Run();
  }
  event->Signal();
}

// Converter functions from Blink types to WebRTC types.

// Class mapping responses from calls to libjingle CreateOffer/Answer and
// the blink::RTCSessionDescriptionRequest.
class CreateSessionDescriptionRequest
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateSessionDescriptionRequest(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      blink::RTCSessionDescriptionRequest* request,
      const base::WeakPtr<RTCPeerConnectionHandler>& handler,
      PeerConnectionTracker* tracker,
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

    auto tracker = tracker_.Lock();
    if (tracker && handler_) {
      std::string value;
      if (desc) {
        desc->ToString(&value);
        value = "type: " + desc->type() + ", sdp: " + value;
      }
      tracker->TrackSessionDescriptionCallback(
          handler_.get(), action_, "OnSuccess", String::FromUTF8(value));
      tracker->TrackSessionId(handler_.get(),
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

    auto tracker = tracker_.Lock();
    if (handler_ && tracker) {
      tracker->TrackSessionDescriptionCallback(
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
  const CrossThreadWeakPersistent<PeerConnectionTracker> tracker_;
  PeerConnectionTracker::Action action_;
};

using RTCStatsReportCallbackInternal =
    CrossThreadOnceFunction<void(std::unique_ptr<RTCStatsReportPlatform>)>;

void GetRTCStatsOnSignalingThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
    RTCStatsReportCallbackInternal callback) {
  TRACE_EVENT0("webrtc", "GetRTCStatsOnSignalingThread");
  native_peer_connection->GetStats(
      CreateRTCStatsCollectorCallback(
          main_thread, ConvertToBaseOnceCallback(std::move(callback)))
          .get());
}

std::set<RTCPeerConnectionHandler*>* GetPeerConnectionHandlers() {
  static std::set<RTCPeerConnectionHandler*>* handlers =
      new std::set<RTCPeerConnectionHandler*>();
  return handlers;
}

// Counts the number of senders that have |stream_id| as an associated stream.
size_t GetLocalStreamUsageCount(
    const Vector<std::unique_ptr<blink::RTCRtpSenderImpl>>& rtp_senders,
    const std::string& stream_id) {
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

MediaStreamTrackMetrics::Kind MediaStreamTrackMetricsKind(
    const MediaStreamComponent* component) {
  return component->GetSourceType() == MediaStreamSource::kTypeAudio
             ? MediaStreamTrackMetrics::Kind::kAudio
             : MediaStreamTrackMetrics::Kind::kVideo;
}

}  // namespace

// Implementation of ParsedSessionDescription
ParsedSessionDescription::ParsedSessionDescription(const String& sdp_type,
                                                   const String& sdp)
    : type_(sdp_type), sdp_(sdp) {}

// static
ParsedSessionDescription ParsedSessionDescription::Parse(
    const RTCSessionDescriptionInit* session_description_init) {
  ParsedSessionDescription temp(
      session_description_init->hasType()
          ? session_description_init->type().AsString()
          : String(),
      session_description_init->sdp());
  temp.DoParse();
  return temp;
}

// static
ParsedSessionDescription ParsedSessionDescription::Parse(
    const RTCSessionDescriptionPlatform* session_description_platform) {
  ParsedSessionDescription temp(session_description_platform->GetType(),
                                session_description_platform->Sdp());
  temp.DoParse();
  return temp;
}

// static
ParsedSessionDescription ParsedSessionDescription::Parse(const String& sdp_type,
                                                         const String& sdp) {
  ParsedSessionDescription temp(sdp_type, sdp);
  temp.DoParse();
  return temp;
}

void ParsedSessionDescription::DoParse() {
  std::optional<webrtc::SdpType> maybe_type =
      webrtc::SdpTypeFromString(type_.Utf8().c_str());
  if (!maybe_type.has_value()) {
    description_.reset();
    return;
  }
  description_ = webrtc::CreateSessionDescription(*maybe_type,
                                                  sdp_.Utf8().c_str(), &error_);
}

// Processes the resulting state changes of a SetLocalDescription() or
// SetRemoteDescription() call.
class RTCPeerConnectionHandler::WebRtcSetDescriptionObserverImpl
    : public WebRtcSetDescriptionObserver {
 public:
  WebRtcSetDescriptionObserverImpl(
      base::WeakPtr<RTCPeerConnectionHandler> handler,
      blink::RTCVoidRequest* web_request,
      PeerConnectionTracker* tracker,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      PeerConnectionTracker::Action action,
      bool is_rollback)
      : handler_(handler),
        main_thread_(task_runner),
        web_request_(web_request),
        tracker_(tracker),
        action_(action),
        is_rollback_(is_rollback) {}

  void OnSetDescriptionComplete(
      webrtc::RTCError error,
      WebRtcSetDescriptionObserver::States states) override {
    auto tracker = tracker_.Lock();
    if (!error.ok()) {
      if (tracker && handler_) {
        tracker->TrackSessionDescriptionCallback(
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
    if (tracker && handler_) {
      StringBuilder value;
      if (action_ ==
          PeerConnectionTracker::kActionSetLocalDescriptionImplicit) {
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
      tracker->TrackSessionDescriptionCallback(handler_.get(), action_,
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

    // This fires JS events and could cause |handler_| to become null.
    ProcessStateChanges(std::move(states));
    ResolvePromise();
  }

 private:
  ~WebRtcSetDescriptionObserverImpl() override {}

  void ResolvePromise() {
    web_request_->RequestSucceeded();
    web_request_ = nullptr;
  }

  void ProcessStateChanges(WebRtcSetDescriptionObserver::States states) {
    if (handler_) {
      handler_->OnModifySctpTransport(std::move(states.sctp_transport_state));
    }
    // Since OnSessionDescriptionsUpdated can fire events, it may cause
    // garbage collection. Ensure that handler_ is still valid.
    if (handler_ && !handler_->is_unregistered_) {
      handler_->OnModifyTransceivers(
          states.signaling_state, std::move(states.transceiver_states),
          action_ == PeerConnectionTracker::kActionSetRemoteDescription,
          is_rollback_);
    }
  }

  base::WeakPtr<RTCPeerConnectionHandler> handler_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_;
  Persistent<blink::RTCVoidRequest> web_request_;
  CrossThreadWeakPersistent<PeerConnectionTracker> tracker_;
  PeerConnectionTracker::Action action_;
  bool is_rollback_;
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
      : handler_(handler), main_thread_(task_runner) {}
  ~Observer() override {
    // `signaling_thread_` may be null in some testing-only environments.
    if (!signaling_thread_) {
      return;
    }
    // To avoid a PROXY block-invoke to ~webrtc::PeerConnection in the event
    // that `native_peer_connection_` was the last reference, we move it to the
    // signaling thread in a PostTask.
    signaling_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc) {
              // The binding releases `pc` on the signaling thread as
              // this method goes out of scope.
            },
            std::move(native_peer_connection_)));
  }

  void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> signaling_thread) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(!native_peer_connection_);
    DCHECK(handler_);
    native_peer_connection_ = handler_->native_peer_connection_;
    DCHECK(native_peer_connection_);
    signaling_thread_ = std::move(signaling_thread);
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
            WrapCrossThreadPersistent(this), data_channel));
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
      NOTREACHED_IN_MIGRATION() << "OnIceCandidate: Could not get SDP string.";
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
            String::FromUTF8(candidate->candidate().username()),
            String::FromUTF8(candidate->server_url()),
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

  void OnDataChannelImpl(rtc::scoped_refptr<DataChannelInterface> channel) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (handler_)
      handler_->OnDataChannel(channel);
  }

  void OnIceCandidateImpl(const String& sdp,
                          const String& sdp_mid,
                          int sdp_mline_index,
                          int component,
                          int address_family,
                          const String& username_fragment,
                          const String& url,
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
    }
    // Since OnSessionDescriptionsUpdated can fire events, it may cause
    // garbage collection. Ensure that handler_ is still valid.
    if (handler_) {
      handler_->OnIceCandidate(sdp, sdp_mid, sdp_mline_index, component,
                               address_family, username_fragment, url);
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
          port ? std::optional<uint16_t>(static_cast<uint16_t>(port))
               : std::nullopt,
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
  // The rest of the members are set at Initialize() but are otherwise constant
  // until destruction.
  scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;
  // A copy of |handler_->native_peer_connection_| for use on the WebRTC
  // signaling thread.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;
};

RTCPeerConnectionHandler::RTCPeerConnectionHandler(
    RTCPeerConnectionHandlerClient* client,
    blink::PeerConnectionDependencyFactory* dependency_factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool encoded_insertable_streams)
    : client_(client),
      dependency_factory_(dependency_factory),
      track_adapter_map_(
          base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
              dependency_factory_,
              task_runner)),
      encoded_insertable_streams_(encoded_insertable_streams),
      task_runner_(std::move(task_runner)) {
  CHECK(client_);

  GetPeerConnectionHandlers()->insert(this);
}

// Constructor to be used for creating mocks only.
RTCPeerConnectionHandler::RTCPeerConnectionHandler(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : is_unregistered_(true),  // Avoid CloseAndUnregister in destructor
      task_runner_(std::move(task_runner)) {}

RTCPeerConnectionHandler::~RTCPeerConnectionHandler() {
  if (!is_unregistered_) {
    CloseAndUnregister();
  }
  // Delete RTP Media API objects that may have references to the native peer
  // connection.
  rtp_senders_.clear();
  rtp_receivers_.clear();
  rtp_transceivers_.clear();
  // `signaling_thread_` may be null in some testing-only environments.
  if (!signaling_thread_) {
    return;
  }
  // To avoid a PROXY block-invoke to ~webrtc::PeerConnection in the event
  // that `native_peer_connection_` was the last reference, we move it to the
  // signaling thread in a PostTask.
  signaling_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc) {
            // The binding releases `pc` on the signaling thread as
            // this method goes out of scope.
          },
          std::move(native_peer_connection_)));
}

void RTCPeerConnectionHandler::CloseAndUnregister() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  Close();

  GetPeerConnectionHandlers()->erase(this);
  if (peer_connection_tracker_)
    peer_connection_tracker_->UnregisterPeerConnection(this);

  // Clear the pointer to client_ so that it does not interfere with
  // garbage collection.
  client_ = nullptr;
  is_unregistered_ = true;

  // Reset the `PeerConnectionDependencyFactory` so we don't prevent it from
  // being garbage-collected.
  dependency_factory_ = nullptr;
}

bool RTCPeerConnectionHandler::Initialize(
    ExecutionContext* context,
    const webrtc::PeerConnectionInterface::RTCConfiguration&
        server_configuration,
    WebLocalFrame* frame,
    ExceptionState& exception_state,
    RTCRtpTransport* rtp_transport) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(dependency_factory_);

  CHECK(!initialize_called_);
  initialize_called_ = true;

  // Prevent garbage collection of client_ during processing.
  auto* client_on_stack = client_.Get();
  if (!client_on_stack) {
    return false;
  }

  DCHECK(frame);
  frame_ = frame;
  peer_connection_tracker_ = PeerConnectionTracker::From(*frame);

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

  // Apply 40 ms worth of bursting. See webrtc::TaskQueuePacedSender.
  configuration_.pacer_burst_interval = webrtc::TimeDelta::Millis(40);

  peer_connection_observer_ =
      MakeGarbageCollected<Observer>(weak_factory_.GetWeakPtr(), task_runner_);
  native_peer_connection_ = dependency_factory_->CreatePeerConnection(
      configuration_, frame_, peer_connection_observer_, exception_state,
      rtp_transport);
  if (!native_peer_connection_.get()) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }
  // Now the signaling thread exists.
  signaling_thread_ = dependency_factory_->GetWebRtcSignalingTaskRunner();
  peer_connection_observer_->Initialize(signaling_thread_);

  if (peer_connection_tracker_) {
    peer_connection_tracker_->RegisterPeerConnection(this, configuration_,
                                                     frame_);
  }
  // Gratuitous usage of client_on_stack to prevent compiler errors.
  return !!client_on_stack;
}

bool RTCPeerConnectionHandler::InitializeForTest(
    const webrtc::PeerConnectionInterface::RTCConfiguration&
        server_configuration,
    PeerConnectionTracker* peer_connection_tracker,
    ExceptionState& exception_state,
    RTCRtpTransport* rtp_transport) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(dependency_factory_);

  CHECK(!initialize_called_);
  initialize_called_ = true;

  configuration_ = server_configuration;

  peer_connection_observer_ =
      MakeGarbageCollected<Observer>(weak_factory_.GetWeakPtr(), task_runner_);

  native_peer_connection_ = dependency_factory_->CreatePeerConnection(
      configuration_, nullptr, peer_connection_observer_, exception_state,
      rtp_transport);
  if (!native_peer_connection_.get()) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }
  // Now the signaling thread exists.
  signaling_thread_ = dependency_factory_->GetWebRtcSignalingTaskRunner();
  peer_connection_observer_->Initialize(signaling_thread_);
  peer_connection_tracker_ = peer_connection_tracker;
  return true;
}

Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::CreateOffer(RTCSessionDescriptionRequest* request,
                                      RTCOfferOptionsPlatform* options) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createOffer");

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateOffer(this, options);

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  if (options) {
    webrtc_options.offer_to_receive_audio = options->OfferToReceiveAudio();
    webrtc_options.offer_to_receive_video = options->OfferToReceiveVideo();
    webrtc_options.voice_activity_detection = options->VoiceActivityDetection();
    webrtc_options.ice_restart = options->IceRestart();
  }

  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          task_runner_, request, weak_factory_.GetWeakPtr(),
          peer_connection_tracker_, PeerConnectionTracker::kActionCreateOffer));

  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  RunSynchronousOnceClosureOnSignalingThread(
      base::BindOnce(&RTCPeerConnectionHandler::CreateOfferOnSignalingThread,
                     base::Unretained(this),
                     base::Unretained(description_request.get()),
                     std::move(webrtc_options),
                     base::Unretained(&transceiver_state_surfacer)),
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
      transceivers = native_peer_connection_->GetTransceivers();
  transceiver_state_surfacer->Initialize(
      native_peer_connection_, track_adapter_map_, std::move(transceivers));
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
          PeerConnectionTracker::kActionCreateAnswer));
  // TODO(tommi): Do this asynchronously via e.g. PostTaskAndReply.
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  if (options) {
    webrtc_options.voice_activity_detection = options->VoiceActivityDetection();
  }
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
          PeerConnectionTracker::kActionSetLocalDescriptionImplicit,
          /*is_rollback=*/true);

  rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>
      webrtc_observer(WebRtcSetLocalDescriptionObserverHandler::Create(
                          task_runner_, signaling_thread(),
                          native_peer_connection_, track_adapter_map_,
                          content_observer)
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
    ParsedSessionDescription parsed_sdp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setLocalDescription");

  String sdp = parsed_sdp.sdp();
  String type = parsed_sdp.type();

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackSetSessionDescription(
        this, sdp, type, PeerConnectionTracker::kSourceLocal);
  }

  const webrtc::SessionDescriptionInterface* native_desc =
      parsed_sdp.description();
  if (!native_desc) {
    webrtc::SdpParseError error(parsed_sdp.error());
    StringBuilder reason_str;
    reason_str.Append("Failed to parse SessionDescription. ");
    reason_str.Append(error.line.c_str());
    reason_str.Append(" ");
    reason_str.Append(error.description.c_str());
    LOG(ERROR) << reason_str.ToString();
    if (peer_connection_tracker_) {
      peer_connection_tracker_->TrackSessionDescriptionCallback(
          this, PeerConnectionTracker::kActionSetLocalDescription, "OnFailure",
          reason_str.ToString());
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

  if (!first_local_description_ && IsOfferOrAnswer(native_desc)) {
    first_local_description_ =
        std::make_unique<FirstSessionDescription>(native_desc);
    if (first_remote_description_) {
      ReportFirstSessionDescriptions(*first_local_description_,
                                     *first_remote_description_);
    }
  }

  scoped_refptr<WebRtcSetDescriptionObserverImpl> content_observer =
      base::MakeRefCounted<WebRtcSetDescriptionObserverImpl>(
          weak_factory_.GetWeakPtr(), request, peer_connection_tracker_,
          task_runner_, PeerConnectionTracker::kActionSetLocalDescription,
          type == "rollback");

  rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>
      webrtc_observer(WebRtcSetLocalDescriptionObserverHandler::Create(
                          task_runner_, signaling_thread(),
                          native_peer_connection_, track_adapter_map_,
                          content_observer)
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
              native_peer_connection_, parsed_sdp.release(), webrtc_observer),
          CrossThreadUnretained("SetLocalDescription")));
}

void RTCPeerConnectionHandler::SetRemoteDescription(
    blink::RTCVoidRequest* request,
    ParsedSessionDescription parsed_sdp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setRemoteDescription");

  String sdp = parsed_sdp.sdp();
  String type = parsed_sdp.type();

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackSetSessionDescription(
        this, sdp, type, PeerConnectionTracker::kSourceRemote);
  }

  webrtc::SdpParseError error(parsed_sdp.error());
  const webrtc::SessionDescriptionInterface* native_desc =
      parsed_sdp.description();
  if (!native_desc) {
    StringBuilder reason_str;
    reason_str.Append("Failed to parse SessionDescription. ");
    reason_str.Append(error.line.c_str());
    reason_str.Append(" ");
    reason_str.Append(error.description.c_str());
    LOG(ERROR) << reason_str.ToString();
    if (peer_connection_tracker_) {
      peer_connection_tracker_->TrackSessionDescriptionCallback(
          this, PeerConnectionTracker::kActionSetRemoteDescription, "OnFailure",
          reason_str.ToString());
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

  if (!first_remote_description_ && IsOfferOrAnswer(native_desc)) {
    first_remote_description_ =
        std::make_unique<FirstSessionDescription>(native_desc);
    if (first_local_description_) {
      ReportFirstSessionDescriptions(*first_local_description_,
                                     *first_remote_description_);
    }
  }

  scoped_refptr<WebRtcSetDescriptionObserverImpl> content_observer =
      base::MakeRefCounted<WebRtcSetDescriptionObserverImpl>(
          weak_factory_.GetWeakPtr(), request, peer_connection_tracker_,
          task_runner_, PeerConnectionTracker::kActionSetRemoteDescription,
          type == "rollback");

  rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>
      webrtc_observer(WebRtcSetRemoteDescriptionObserverHandler::Create(
                          task_runner_, signaling_thread(),
                          native_peer_connection_, track_adapter_map_,
                          content_observer)
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
              native_peer_connection_, parsed_sdp.release(), webrtc_observer),
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

void RTCPeerConnectionHandler::AddIceCandidate(
    RTCVoidRequest* request,
    RTCIceCandidatePlatform* candidate) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(dependency_factory_);
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::addIceCandidate");
  std::unique_ptr<webrtc::IceCandidateInterface> native_candidate(
      dependency_factory_->CreateIceCandidate(
          candidate->SdpMid(),
          candidate->SdpMLineIndex()
              ? static_cast<int>(*candidate->SdpMLineIndex())
              : -1,
          candidate->Candidate()));

  auto callback_on_task_runner =
      [](base::WeakPtr<RTCPeerConnectionHandler> handler_weak_ptr,
         CrossThreadPersistent<PeerConnectionTracker> tracker_ptr,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             pending_local_description,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             current_local_description,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             pending_remote_description,
         std::unique_ptr<webrtc::SessionDescriptionInterface>
             current_remote_description,
         CrossThreadPersistent<RTCIceCandidatePlatform> candidate,
         webrtc::RTCError result, RTCVoidRequest* request) {
        // Inform tracker (chrome://webrtc-internals).
        // Note that because the WTF::CrossThreadBindOnce() below uses a
        // CrossThreadWeakPersistent when binding |tracker_ptr| this lambda may
        // be invoked with a null |tracker_ptr| so we have to guard against it.
        if (handler_weak_ptr && tracker_ptr) {
          tracker_ptr->TrackAddIceCandidate(
              handler_weak_ptr.get(), candidate,
              PeerConnectionTracker::kSourceRemote, result.ok());
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
       tracker_weak_ptr =
           WrapCrossThreadWeakPersistent(peer_connection_tracker_.Get()),
       persistent_candidate = WrapCrossThreadPersistent(candidate),
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
                std::move(persistent_candidate), std::move(result),
                std::move(persistent_request)));
      });
}

void RTCPeerConnectionHandler::RestartIce() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // The proxy invokes RestartIce() on the signaling thread.
  native_peer_connection_->RestartIce();
}

void RTCPeerConnectionHandler::GetStandardStatsForTracker(
    rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> observer) {
  native_peer_connection_->GetStats(observer.get());
}

void RTCPeerConnectionHandler::EmitCurrentStateForTracker() {
  if (!peer_connection_tracker_) {
    return;
  }
  RTC_DCHECK(native_peer_connection_);
  const webrtc::SessionDescriptionInterface* local_desc =
      native_peer_connection_->local_description();
  // If the local desc is an answer, emit it after the offer.
  if (local_desc != nullptr &&
      local_desc->GetType() == webrtc::SdpType::kOffer) {
    std::string local_sdp;
    if (local_desc->ToString(&local_sdp)) {
      peer_connection_tracker_->TrackSetSessionDescription(
          this, String(local_sdp),
          String(SdpTypeToString(local_desc->GetType())),
          PeerConnectionTracker::kSourceLocal);
    }
  }
  const webrtc::SessionDescriptionInterface* remote_desc =
      native_peer_connection_->remote_description();
  if (remote_desc != nullptr) {
    std::string remote_sdp;
    if (remote_desc->ToString(&remote_sdp)) {
      peer_connection_tracker_->TrackSetSessionDescription(
          this, String(remote_sdp),
          String(SdpTypeToString(remote_desc->GetType())),
          PeerConnectionTracker::kSourceRemote);
    }
  }

  if (local_desc != nullptr &&
      local_desc->GetType() != webrtc::SdpType::kOffer) {
    std::string local_sdp;
    if (local_desc->ToString(&local_sdp)) {
      peer_connection_tracker_->TrackSetSessionDescription(
          this, String(local_sdp),
          String(SdpTypeToString(local_desc->GetType())),
          PeerConnectionTracker::kSourceLocal);
    }
  }
  peer_connection_tracker_->TrackSignalingStateChange(
      this, native_peer_connection_->signaling_state());
  peer_connection_tracker_->TrackIceConnectionStateChange(
      this, native_peer_connection_->standardized_ice_connection_state());
  peer_connection_tracker_->TrackConnectionStateChange(
      this, native_peer_connection_->peer_connection_state());
}

void RTCPeerConnectionHandler::GetStats(RTCStatsReportCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *signaling_thread().get(), FROM_HERE,
      CrossThreadBindOnce(&GetRTCStatsOnSignalingThread, task_runner_,
                          native_peer_connection_,
                          CrossThreadBindOnce(std::move(callback))));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::AddTransceiverWithTrack(
    MediaStreamComponent* component,
    const webrtc::RtpTransceiverInit& init) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
      track_ref = track_adapter_map_->GetOrCreateLocalTrackAdapter(component);
  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
      error_or_transceiver;
  RunSynchronousOnceClosureOnSignalingThread(
      base::BindOnce(
          &RTCPeerConnectionHandler::AddTransceiverWithTrackOnSignalingThread,
          base::Unretained(this),
          base::RetainedRef(track_ref->webrtc_track().get()), std::cref(init),
          base::Unretained(&transceiver_state_surfacer),
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
  if (peer_connection_tracker_) {
    size_t transceiver_index = GetTransceiverIndex(*platform_transceiver.get());
    peer_connection_tracker_->TrackAddTransceiver(
        this, PeerConnectionTracker::TransceiverUpdatedReason::kAddTransceiver,
        *platform_transceiver.get(), transceiver_index);
  }
  return platform_transceiver;
}

void RTCPeerConnectionHandler::AddTransceiverWithTrackOnSignalingThread(
    webrtc::MediaStreamTrackInterface* webrtc_track,
    webrtc::RtpTransceiverInit init,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer,
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>*
        error_or_transceiver) {
  *error_or_transceiver = native_peer_connection_->AddTransceiver(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>(webrtc_track),
      init);
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
  RunSynchronousOnceClosureOnSignalingThread(
      base::BindOnce(&RTCPeerConnectionHandler::
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
  if (peer_connection_tracker_) {
    size_t transceiver_index = GetTransceiverIndex(*platform_transceiver.get());
    peer_connection_tracker_->TrackAddTransceiver(
        this, PeerConnectionTracker::TransceiverUpdatedReason::kAddTransceiver,
        *platform_transceiver.get(), transceiver_index);
  }
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
  // transceiver.
  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
      error_or_sender;
  RunSynchronousOnceClosureOnSignalingThread(
      base::BindOnce(&RTCPeerConnectionHandler::AddTrackOnSignalingThread,
                     base::Unretained(this),
                     base::RetainedRef(track_ref->webrtc_track().get()),
                     std::move(stream_ids),
                     base::Unretained(&transceiver_state_surfacer),
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
  // Create or recycle a transceiver.
  auto transceiver = CreateOrUpdateTransceiver(
      std::move(transceiver_state), blink::TransceiverStateUpdateMode::kAll);
  platform_transceiver = std::move(transceiver);
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
    webrtc::MediaStreamTrackInterface* track,
    std::vector<std::string> stream_ids,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer,
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>*
        error_or_sender) {
  *error_or_sender = native_peer_connection_->AddTrack(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>(track), stream_ids);
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  if (error_or_sender->ok()) {
    auto sender = error_or_sender->value();
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver_for_sender;
    for (const auto& transceiver : native_peer_connection_->GetTransceivers()) {
      if (transceiver->sender() == sender) {
        transceiver_for_sender = transceiver;
        break;
      }
    }
    DCHECK(transceiver_for_sender);
    transceivers = {transceiver_for_sender};
  }
  transceiver_state_surfacer->Initialize(
      native_peer_connection_, track_adapter_map_, std::move(transceivers));
}

webrtc::RTCErrorOr<std::unique_ptr<RTCRtpTransceiverPlatform>>
RTCPeerConnectionHandler::RemoveTrack(blink::RTCRtpSenderPlatform* web_sender) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::RemoveTrack");
  auto it = FindSender(web_sender->Id());
  if (it == rtp_senders_.end())
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER);
  const auto& sender = *it;
  auto webrtc_sender = sender->state().webrtc_sender();

  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      task_runner_, signaling_thread());
  std::optional<webrtc::RTCError> result;
  RunSynchronousOnceClosureOnSignalingThread(
      base::BindOnce(&RTCPeerConnectionHandler::RemoveTrackOnSignalingThread,
                     base::Unretained(this),
                     base::RetainedRef(webrtc_sender.get()),
                     base::Unretained(&transceiver_state_surfacer),
                     base::Unretained(&result)),
      "RemoveTrackOnSignalingThread");
  DCHECK(transceiver_state_surfacer.is_initialized());
  if (!result || !result->ok()) {
    // Don't leave the surfacer in a pending state.
    transceiver_state_surfacer.ObtainStates();
    if (!result) {
      // Operation has been cancelled.
      return std::unique_ptr<RTCRtpTransceiverPlatform>(nullptr);
    }
    return std::move(*result);
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

void RTCPeerConnectionHandler::RemoveTrackOnSignalingThread(
    webrtc::RtpSenderInterface* sender,
    blink::TransceiverStateSurfacer* transceiver_state_surfacer,
    std::optional<webrtc::RTCError>* result) {
  *result = native_peer_connection_->RemoveTrackOrError(
      rtc::scoped_refptr<webrtc::RtpSenderInterface>(sender));
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  if ((*result)->ok()) {
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
      *result = std::nullopt;
    } else {
      transceivers = {transceiver_for_sender};
    }
  }
  transceiver_state_surfacer->Initialize(
      native_peer_connection_, track_adapter_map_, std::move(transceivers));
}

Vector<std::unique_ptr<blink::RTCRtpSenderPlatform>>
RTCPeerConnectionHandler::GetPlatformSenders() const {
  Vector<std::unique_ptr<blink::RTCRtpSenderPlatform>> senders;
  for (const auto& sender : rtp_senders_) {
    senders.push_back(sender->ShallowCopy());
  }
  return senders;
}

void RTCPeerConnectionHandler::CloseClientPeerConnection() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!is_closed_)
    client_->ClosePeerConnection();
}

void RTCPeerConnectionHandler::MaybeCreateThermalUmaListner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Instantiate the speed limit listener if we have one track.
  if (!speed_limit_uma_listener_) {
    for (const auto& sender : rtp_senders_) {
      if (sender->Track()) {
        speed_limit_uma_listener_ =
            std::make_unique<SpeedLimitUmaListener>(task_runner_);
        speed_limit_uma_listener_->OnSpeedLimitChange(last_speed_limit_);
        break;
      }
    }
  }
  if (!thermal_uma_listener_) {
    // Instantiate the thermal uma listener only if we are sending video.
    for (const auto& sender : rtp_senders_) {
      if (sender->Track() &&
          sender->Track()->GetSourceType() == MediaStreamSource::kTypeVideo) {
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

SpeedLimitUmaListener* RTCPeerConnectionHandler::speed_limit_uma_listener()
    const {
  return speed_limit_uma_listener_.get();
}

void RTCPeerConnectionHandler::OnThermalStateChange(
    mojom::blink::DeviceThermalState thermal_state) {
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

void RTCPeerConnectionHandler::OnSpeedLimitChange(int32_t speed_limit) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (is_closed_)
    return;
  last_speed_limit_ = speed_limit;
  if (speed_limit_uma_listener_)
    speed_limit_uma_listener_->OnSpeedLimitChange(speed_limit);
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

rtc::scoped_refptr<DataChannelInterface>
RTCPeerConnectionHandler::CreateDataChannel(
    const String& label,
    const webrtc::DataChannelInit& init) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createDataChannel");
  DVLOG(1) << "createDataChannel label " << label.Utf8();

  webrtc::RTCErrorOr<rtc::scoped_refptr<DataChannelInterface>> webrtc_channel =
      native_peer_connection_->CreateDataChannelOrError(label.Utf8(), &init);
  if (!webrtc_channel.ok()) {
    DLOG(ERROR) << "Could not create native data channel: "
                << webrtc_channel.error().message();
    return nullptr;
  }
  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackCreateDataChannel(
        this, webrtc_channel.value().get(),
        PeerConnectionTracker::kSourceLocal);
  }

  return webrtc_channel.value();
}

void RTCPeerConnectionHandler::Close() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << "RTCPeerConnectionHandler::stop";

  if (is_closed_ || !native_peer_connection_.get())
    return;  // Already stopped.

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackClose(this);

  native_peer_connection_->Close();

  // This object may no longer forward call backs to blink.
  is_closed_ = true;
}

webrtc::PeerConnectionInterface*
RTCPeerConnectionHandler::NativePeerConnection() {
  return native_peer_connection();
}

void RTCPeerConnectionHandler::RunSynchronousOnceClosureOnSignalingThread(
    base::OnceClosure closure,
    const char* trace_event_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<base::SingleThreadTaskRunner> thread(signaling_thread());
  if (!thread.get() || thread->BelongsToCurrentThread()) {
    TRACE_EVENT0("webrtc", trace_event_name);
    std::move(closure).Run();
  } else {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread->PostTask(
        FROM_HERE,
        base::BindOnce(&RunSynchronousOnceClosure, std::move(closure),
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
  // Prevent garbage collection of client_ during processing.
  auto* client_on_stack = client_.Get();
  if (!client_on_stack || is_closed_) {
    return;
  }
  client_on_stack->DidChangeSessionDescriptions(
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

// Called any time the lower layer IceConnectionState changes, which is NOT in
// sync with the iceConnectionState that is exposed to JavaScript (that one is
// computed by RTCPeerConnection::UpdateIceConnectionState)! This method is
// purely used for UMA reporting. We may want to consider wiring this up to
// UpdateIceConnectionState() instead...
void RTCPeerConnectionHandler::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceConnectionChange");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ReportICEState(new_state);
  track_metrics_.IceConnectionChange(new_state);
}

void RTCPeerConnectionHandler::TrackIceConnectionStateChange(
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!peer_connection_tracker_)
    return;
  peer_connection_tracker_->TrackIceConnectionStateChange(this, state);
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

void RTCPeerConnectionHandler::OnModifySctpTransport(
    blink::WebRTCSctpTransportSnapshot state) {
  if (client_)
    client_->DidModifySctpTransport(state);
}

void RTCPeerConnectionHandler::OnModifyTransceivers(
    webrtc::PeerConnectionInterface::SignalingState signaling_state,
    std::vector<blink::RtpTransceiverState> transceiver_states,
    bool is_remote_description,
    bool is_rollback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> platform_transceivers(
      base::checked_cast<WTF::wtf_size_t>(transceiver_states.size()));
  PeerConnectionTracker::TransceiverUpdatedReason update_reason =
      !is_remote_description ? PeerConnectionTracker::TransceiverUpdatedReason::
                                   kSetLocalDescription
                             : PeerConnectionTracker::TransceiverUpdatedReason::
                                   kSetRemoteDescription;
  Vector<uintptr_t> ids(
      base::checked_cast<wtf_size_t>(transceiver_states.size()));
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
          previous_state.direction() != transceiver_states[i].direction() ||
          previous_state.current_direction() !=
              transceiver_states[i].current_direction() ||
          previous_state.header_extensions_negotiated() !=
              transceiver_states[i].header_extensions_negotiated();
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
  // Search for removed transceivers by comparing to previous state. All of
  // these transceivers will have been stopped in the WebRTC layers, but we do
  // not have access to their states anymore. So it is up to `client_` to ensure
  // removed transceivers are reflected as "stopped" in JavaScript.
  Vector<uintptr_t> removed_transceivers;
  for (auto transceiver_id : previous_transceiver_ids_) {
    if (!base::Contains(ids, transceiver_id)) {
      removed_transceivers.emplace_back(transceiver_id);
      rtp_transceivers_.erase(FindTransceiver(transceiver_id));
    }
  }
  previous_transceiver_ids_ = ids;
  if (!is_closed_) {
    client_->DidModifyTransceivers(
        signaling_state, std::move(platform_transceivers), removed_transceivers,
        is_remote_description || is_rollback);
  }
}

void RTCPeerConnectionHandler::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> channel) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnDataChannelImpl");

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackCreateDataChannel(
        this, channel.get(), PeerConnectionTracker::kSourceRemote);
  }

  if (!is_closed_)
    client_->DidAddRemoteDataChannel(std::move(channel));
}

void RTCPeerConnectionHandler::OnIceCandidate(const String& sdp,
                                              const String& sdp_mid,
                                              int sdp_mline_index,
                                              int component,
                                              int address_family,
                                              const String& usernameFragment,
                                              const String& url) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // In order to ensure that the RTCPeerConnection is not garbage collected
  // from under the function, we keep a pointer to it on the stack.
  auto* client_on_stack = client_.Get();
  if (!client_on_stack) {
    return;
  }
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceCandidateImpl");
  std::optional<String> url_or_null;
  if (!url.empty()) {
    url_or_null = url;
  }
  // This line can cause garbage collection.
  auto* platform_candidate = MakeGarbageCollected<RTCIceCandidatePlatform>(
      sdp, sdp_mid, sdp_mline_index, usernameFragment, url_or_null);
  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackAddIceCandidate(
        this, platform_candidate, PeerConnectionTracker::kSourceLocal, true);
  }

  if (!is_closed_ && client_on_stack) {
    client_on_stack->DidGenerateICECandidate(platform_candidate);
  }
}

void RTCPeerConnectionHandler::OnIceCandidateError(const String& address,
                                                   std::optional<uint16_t> port,
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
  RtcpMux rtcp_mux = RtcpMux::kEnabled;
  if ((!local.audio && !local.video) || (!remote.audio && !remote.video)) {
    rtcp_mux = RtcpMux::kNoMedia;
  } else if (!local.rtcp_mux || !remote.rtcp_mux) {
    rtcp_mux = RtcpMux::kDisabled;
  }

  UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.RtcpMux", rtcp_mux,
                            RtcpMux::kMax);

  // TODO(pthatcher): Reports stats about whether we have audio and
  // video or not.
}

Vector<std::unique_ptr<blink::RTCRtpSenderImpl>>::iterator
RTCPeerConnectionHandler::FindSender(uintptr_t id) {
  return base::ranges::find_if(
      rtp_senders_, [id](const auto& sender) { return sender->Id() == id; });
}

Vector<std::unique_ptr<blink::RTCRtpReceiverImpl>>::iterator
RTCPeerConnectionHandler::FindReceiver(uintptr_t id) {
  return base::ranges::find_if(rtp_receivers_, [id](const auto& receiver) {
    return receiver->Id() == id;
  });
}

Vector<std::unique_ptr<blink::RTCRtpTransceiverImpl>>::iterator
RTCPeerConnectionHandler::FindTransceiver(uintptr_t id) {
  return base::ranges::find_if(
      rtp_transceivers_,
      [id](const auto& transceiver) { return transceiver->Id() == id; });
}

wtf_size_t RTCPeerConnectionHandler::GetTransceiverIndex(
    const RTCRtpTransceiverPlatform& platform_transceiver) {
  for (wtf_size_t i = 0; i < rtp_transceivers_.size(); ++i) {
    if (platform_transceiver.Id() == rtp_transceivers_[i]->Id())
      return i;
  }
  NOTREACHED_IN_MIGRATION();
  return 0u;
}

std::unique_ptr<blink::RTCRtpTransceiverImpl>
RTCPeerConnectionHandler::CreateOrUpdateTransceiver(
    blink::RtpTransceiverState transceiver_state,
    blink::TransceiverStateUpdateMode update_mode) {
  CHECK(dependency_factory_);
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
        std::move(transceiver_state), encoded_insertable_streams_,
        dependency_factory_->CreateDecodeMetronome());
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
  return signaling_thread_;
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

}  // namespace blink
