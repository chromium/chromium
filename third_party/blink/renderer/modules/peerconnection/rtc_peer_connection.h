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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_H_

#include <memory>

#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/pausable_object.h"
#include "third_party/blink/renderer/modules/crypto/normalize_algorithm.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"
#include "third_party/blink/renderer/platform/async_method_runner.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {

class ExceptionState;
class MediaStreamTrack;
class MediaStreamTrackOrString;
class RTCAnswerOptions;
class RTCConfiguration;
class RTCDTMFSender;
class RTCDataChannel;
class RTCDataChannelInit;
class RTCIceCandidateInitOrRTCIceCandidate;
class RTCOfferOptions;
class RTCPeerConnectionTest;
class RTCRtpReceiver;
class RTCRtpSender;
class RTCRtpTransceiverInit;
class RTCSessionDescription;
class RTCSessionDescriptionInit;
class ScriptState;
class V8RTCPeerConnectionErrorCallback;
class V8RTCSessionDescriptionCallback;
class V8RTCStatsCallback;
class V8VoidFunction;

class MODULES_EXPORT RTCPeerConnection final
    : public EventTargetWithInlineData,
      public WebRTCPeerConnectionHandlerClient,
      public ActiveScriptWrappable<RTCPeerConnection>,
      public PausableObject,
      public MediaStreamObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RTCPeerConnection);
  USING_PRE_FINALIZER(RTCPeerConnection, Dispose);

 public:
  static RTCPeerConnection* Create(ExecutionContext*,
                                   const RTCConfiguration&,
                                   const Dictionary&,
                                   ExceptionState&);
  ~RTCPeerConnection() override;

  ScriptPromise createOffer(ScriptState*, const RTCOfferOptions&);
  ScriptPromise createOffer(ScriptState*,
                            V8RTCSessionDescriptionCallback*,
                            V8RTCPeerConnectionErrorCallback*,
                            const Dictionary&,
                            ExceptionState&);

  ScriptPromise createAnswer(ScriptState*, const RTCAnswerOptions&);
  ScriptPromise createAnswer(ScriptState*,
                             V8RTCSessionDescriptionCallback*,
                             V8RTCPeerConnectionErrorCallback*,
                             const Dictionary&);

  ScriptPromise setLocalDescription(ScriptState*,
                                    const RTCSessionDescriptionInit&);
  ScriptPromise setLocalDescription(
      ScriptState*,
      const RTCSessionDescriptionInit&,
      V8VoidFunction*,
      V8RTCPeerConnectionErrorCallback* = nullptr);
  RTCSessionDescription* localDescription();
  RTCSessionDescription* currentLocalDescription();
  RTCSessionDescription* pendingLocalDescription();

  ScriptPromise setRemoteDescription(ScriptState*,
                                     const RTCSessionDescriptionInit&);
  ScriptPromise setRemoteDescription(
      ScriptState*,
      const RTCSessionDescriptionInit&,
      V8VoidFunction*,
      V8RTCPeerConnectionErrorCallback* = nullptr);
  RTCSessionDescription* remoteDescription();
  RTCSessionDescription* currentRemoteDescription();
  RTCSessionDescription* pendingRemoteDescription();

  String signalingState() const;

  void getConfiguration(RTCConfiguration&);
  void setConfiguration(ScriptState*, const RTCConfiguration&, ExceptionState&);

  // Certificate management
  // http://w3c.github.io/webrtc-pc/#sec.cert-mgmt
  static ScriptPromise generateCertificate(
      ScriptState*,
      const AlgorithmIdentifier& keygen_algorithm,
      ExceptionState&);

  ScriptPromise addIceCandidate(ScriptState*,
                                const RTCIceCandidateInitOrRTCIceCandidate&,
                                ExceptionState&);
  ScriptPromise addIceCandidate(ScriptState*,
                                const RTCIceCandidateInitOrRTCIceCandidate&,
                                V8VoidFunction*,
                                V8RTCPeerConnectionErrorCallback*,
                                ExceptionState&);

  String iceGatheringState() const;

  String iceConnectionState() const;

  // A local stream is any stream associated with a sender.
  MediaStreamVector getLocalStreams() const;
  // A remote stream is any stream associated with a receiver.
  MediaStreamVector getRemoteStreams() const;
  MediaStream* getRemoteStreamById(const WebString&) const;
  bool IsRemoteStream(MediaStream* stream) const;

  void addStream(ScriptState*,
                 MediaStream*,
                 const Dictionary& media_constraints,
                 ExceptionState&);

  void removeStream(MediaStream*, ExceptionState&);

  String id(ScriptState*) const;

  // Calls one of the below versions (or rejects with an exception) depending on
  // type, see RTCPeerConnection.idl.
  ScriptPromise getStats(ScriptState*, blink::ScriptValue callback_or_selector);
  // Calls LegacyCallbackBasedGetStats().
  ScriptPromise getStats(ScriptState*,
                         V8RTCStatsCallback* success_callback,
                         MediaStreamTrack* selector = nullptr);
  // Calls PromiseBasedGetStats().
  ScriptPromise getStats(ScriptState*, MediaStreamTrack* selector = nullptr);
  ScriptPromise LegacyCallbackBasedGetStats(
      ScriptState*,
      V8RTCStatsCallback* success_callback,
      MediaStreamTrack* selector);
  ScriptPromise PromiseBasedGetStats(ScriptState*, MediaStreamTrack* selector);

  const HeapVector<Member<RTCRtpTransceiver>>& getTransceivers() const;
  const HeapVector<Member<RTCRtpSender>>& getSenders() const;
  const HeapVector<Member<RTCRtpReceiver>>& getReceivers() const;
  RTCRtpTransceiver* addTransceiver(const MediaStreamTrackOrString&,
                                    const RTCRtpTransceiverInit&,
                                    ExceptionState&);
  RTCRtpSender* addTrack(MediaStreamTrack*, MediaStreamVector, ExceptionState&);
  void removeTrack(RTCRtpSender*, ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(track);

  RTCDataChannel* createDataChannel(ScriptState*,
                                    String label,
                                    const RTCDataChannelInit&,
                                    ExceptionState&);

  RTCDTMFSender* createDTMFSender(MediaStreamTrack*, ExceptionState&);

  bool IsClosed() { return closed_; }
  void close();

  // Makes the peer connection aware of the track. This is used to map web
  // tracks to blink tracks, as is necessary for plumbing. There is no need to
  // unregister the track because Weak references are used.
  void RegisterTrack(MediaStreamTrack*);

  // We allow getStats after close, but not other calls or callbacks.
  bool ShouldFireDefaultCallbacks() { return !closed_ && !stopped_; }
  bool ShouldFireGetStatsCallback() { return !stopped_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(negotiationneeded);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(signalingstatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(iceconnectionstatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icegatheringstatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(datachannel);

  // Utility to note result of CreateOffer / CreateAnswer
  void NoteSdpCreated(const RTCSessionDescription&);

  // MediaStreamObserver
  void OnStreamAddTrack(MediaStream*, MediaStreamTrack*) override;
  void OnStreamRemoveTrack(MediaStream*, MediaStreamTrack*) override;

  // WebRTCPeerConnectionHandlerClient
  void NegotiationNeeded() override;
  void DidGenerateICECandidate(scoped_refptr<WebRTCICECandidate>) override;
  void DidChangeSignalingState(
      webrtc::PeerConnectionInterface::SignalingState) override;
  void DidChangeIceGatheringState(
      webrtc::PeerConnectionInterface::IceGatheringState) override;
  void DidChangeIceConnectionState(
      webrtc::PeerConnectionInterface::IceConnectionState) override;
  void DidAddReceiverPlanB(std::unique_ptr<WebRTCRtpReceiver>) override;
  void DidRemoveReceiverPlanB(std::unique_ptr<WebRTCRtpReceiver>) override;
  void DidModifyTransceivers(std::vector<std::unique_ptr<WebRTCRtpTransceiver>>,
                             bool is_remote_description) override;
  void DidAddRemoteDataChannel(WebRTCDataChannelHandler*) override;
  void DidNoteInterestingUsage(int usage_pattern) override;
  void ReleasePeerConnectionHandler() override;
  void ClosePeerConnection() override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // PausableObject
  void Pause() override;
  void Unpause() override;
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable
  // We keep the this object alive until either stopped or closed.
  bool HasPendingActivity() const final { return !closed_ && !stopped_; }

  // For testing; exported to testing/InternalWebRTCPeerConnection
  static int PeerConnectionCount();
  static int PeerConnectionCountLimit();

  // SLD/SRD helper method, public for testing.
  // "Complex" Plan B SDP is SDP that is not compatible with Unified Plan, i.e.
  // SDP that has multiple tracks listed under the same m= sections. We should
  // show a deprecation warning when setLocalDescription() or
  // setRemoteDescription() is called and:
  // - The SDP is complex Plan B SDP.
  // - sdpSemantics was not specified at RTCPeerConnection construction.
  // Such calls would normally succeed, but as soon as the default switches to
  // Unified Plan they would fail. This decides whether to show deprecation for
  // WebFeature::kRTCPeerConnectionComplexPlanBSdpUsingDefaultSdpSemantics.
  bool ShouldShowComplexPlanBSdpWarning(const RTCSessionDescriptionInit&) const;

  void Trace(blink::Visitor*) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetAudioTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetVideoTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetAudioAndVideoTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCAll);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest,
                           GetTrackRemoveStreamAndGCWithPersistentComponent);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest,
                           GetTrackRemoveStreamAndGCWithPersistentStream);

  typedef base::OnceCallback<bool()> BoolFunction;
  class EventWrapper : public GarbageCollectedFinalized<EventWrapper> {
   public:
    EventWrapper(Event*, BoolFunction);
    // Returns true if |m_setupFunction| returns true or it is null.
    // |m_event| will only be fired if setup() returns true;
    bool Setup();

    void Trace(blink::Visitor*);

    Member<Event> event_;

   private:
    BoolFunction setup_function_;
  };

  RTCPeerConnection(ExecutionContext*,
                    webrtc::PeerConnectionInterface::RTCConfiguration,
                    bool sdp_semantics_specified,
                    WebMediaConstraints,
                    ExceptionState&);
  void Dispose();

  void ScheduleDispatchEvent(Event*);
  void ScheduleDispatchEvent(Event*, BoolFunction);
  void DispatchScheduledEvent();
  void MaybeFireNegotiationNeeded();
  MediaStreamTrack* GetTrack(const WebMediaStreamTrack&) const;
  RTCRtpSender* FindSenderForTrackAndStream(MediaStreamTrack*, MediaStream*);
  HeapVector<Member<RTCRtpSender>>::iterator FindSender(
      const WebRTCRtpSender& web_sender);
  HeapVector<Member<RTCRtpReceiver>>::iterator FindReceiver(
      const WebRTCRtpReceiver& web_receiver);
  HeapVector<Member<RTCRtpTransceiver>>::iterator FindTransceiver(
      const WebRTCRtpTransceiver& web_transceiver);

  // Creates or updates the sender such that it is up-to-date with the
  // WebRTCRtpSender in all regards *except for streams*. The web sender only
  // knows of stream IDs; updating the stream objects requires additional logic
  // which is different depending on context, e.g:
  // - If created/updated with addTrack(), the streams were supplied as
  //   arguments.
  // The web sender's web track must already have a correspondent blink track in
  // |tracks_|. The caller is responsible for ensuring this with
  // RegisterTrack(), e.g:
  // - On addTrack(), the track is supplied as an argument.
  RTCRtpSender* CreateOrUpdateSender(std::unique_ptr<WebRTCRtpSender>,
                                     String kind);
  // Creates or updates the receiver such that it is up-to-date with the
  // WebRTCRtpReceiver in all regards *except for streams*. The web receiver
  // only knows of stream IDs; updating the stream objects requires additional
  // logic which is different depending on context, e.g:
  // - If created/updated with setRemoteDescription(), there is an algorithm for
  //   processing the addition/removal of remote tracks which includes how to
  //   create and update the associated streams set.
  RTCRtpReceiver* CreateOrUpdateReceiver(std::unique_ptr<WebRTCRtpReceiver>);
  // Creates or updates the transceiver such that it, including its sender and
  // receiver, are up-to-date with the WebRTCRtpTransceiver in all regerds
  // *except for sender and receiver streams*. The web sender and web receiver
  // only knows of stream IDs; updating the stream objects require additional
  // logic which is different depending on context. See above.
  RTCRtpTransceiver* CreateOrUpdateTransceiver(
      std::unique_ptr<WebRTCRtpTransceiver>);

  // https://w3c.github.io/webrtc-pc/#process-remote-track-addition
  void ProcessAdditionOfRemoteTrack(
      RTCRtpTransceiver* transceiver,
      const WebVector<WebString>& stream_ids,
      HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
          add_list,
      HeapVector<Member<RTCRtpTransceiver>>* track_events);
  // https://w3c.github.io/webrtc-pc/#process-remote-track-removal
  void ProcessRemovalOfRemoteTrack(
      RTCRtpTransceiver* transceiver,
      HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
          remove_list,
      HeapVector<Member<MediaStreamTrack>>* mute_tracks);
  // Update the |receiver->streams()| to the streams indicated by |stream_ids|,
  // adding to |remove_list| and |add_list| accordingly.
  // https://w3c.github.io/webrtc-pc/#set-associated-remote-streams
  void SetAssociatedMediaStreams(
      RTCRtpReceiver* receiver,
      const WebVector<WebString>& stream_ids,
      HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
          remove_list,
      HeapVector<std::pair<Member<MediaStream>, Member<MediaStreamTrack>>>*
          add_list);

  // Sets the signaling state synchronously, and dispatches a
  // signalingstatechange event synchronously or asynchronously depending on
  // |dispatch_event_immediately|.
  // TODO(hbos): The ability to not fire the event asynchronously is there
  // because CloseInternal() has historically fired asynchronously along with
  // other asynchronously fired events. If close() does not fire any events,
  // |dispatch_event_immediately| can be removed. https://crbug.com/849247
  void ChangeSignalingState(webrtc::PeerConnectionInterface::SignalingState,
                            bool dispatch_event_immediately);
  // The remaining "Change" methods set the state asynchronously and fire the
  // corresponding event immediately after changing the state (if it was really
  // changed).
  //
  // The "Set" methods are called asynchronously by the "Change" methods, and
  // set the corresponding state without firing an event, returning true if the
  // state was really changed.
  //
  // This is done because the standard guarantees that state changes and the
  // corresponding events will happen in the same task; it shouldn't be
  // possible to, for example, end up with two "icegatheringstatechange" events
  // that are delayed somehow and cause the application to read a "complete"
  // gathering state twice, missing the "gathering" state in the middle.
  void ChangeIceGatheringState(
      webrtc::PeerConnectionInterface::IceGatheringState);
  bool SetIceGatheringState(webrtc::PeerConnectionInterface::IceGatheringState);

  void ChangeIceConnectionState(
      webrtc::PeerConnectionInterface::IceConnectionState);
  bool SetIceConnectionState(
      webrtc::PeerConnectionInterface::IceConnectionState);

  void CloseInternal();

  void RecordRapporMetrics();

  DOMException* checkSdpForStateErrors(ExecutionContext*,
                                       const RTCSessionDescriptionInit&,
                                       String* sdp);

  webrtc::PeerConnectionInterface::SignalingState signaling_state_;
  webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state_;
  webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state_;

  // A map containing any track that is in use by the peer connection. This
  // includes tracks of |rtp_senders_| and |rtp_receivers_|.
  HeapHashMap<WeakMember<MediaStreamComponent>, WeakMember<MediaStreamTrack>>
      tracks_;
  // In Plan B, senders and receivers exist independently of one another.
  // In Unified Plan, all senders and receivers are the sender-receiver pairs of
  // transceivers.
  // TODO(hbos): When Plan B is removed, remove |rtp_senders_| and
  // |rtp_receivers_| since these are part of |transceivers_|.
  // https://crbug.com/857004
  HeapVector<Member<RTCRtpSender>> rtp_senders_;
  HeapVector<Member<RTCRtpReceiver>> rtp_receivers_;
  HeapVector<Member<RTCRtpTransceiver>> transceivers_;

  std::unique_ptr<WebRTCPeerConnectionHandler> peer_handler_;

  Member<AsyncMethodRunner<RTCPeerConnection>> dispatch_scheduled_event_runner_;
  HeapVector<Member<EventWrapper>> scheduled_events_;

  // This handle notifies scheduler about an active connection associated
  // with a frame. Handle should be destroyed when connection is closed.
  std::unique_ptr<FrameScheduler::ActiveConnectionHandle>
      connection_handle_for_scheduler_;

  bool negotiation_needed_;
  bool stopped_;
  bool closed_;

  // Internal state [[LastOffer]] and [[LastAnswer]]
  String last_offer_;
  String last_answer_;

  bool has_data_channels_;  // For RAPPOR metrics
  // In Plan B, senders and receivers are added or removed independently of one
  // another. In Unified Plan, senders and receivers are created in pairs as
  // transceivers. Transceivers may become inactive, but are never removed.
  // The value of this member affects the behavior of some methods and what
  // information is surfaced from webrtc. This has the value "kPlanB" or
  // "kUnifiedPlan", if constructed with "kDefault" it is translated to one or
  // the other.
  webrtc::SdpSemantics sdp_semantics_;
  // Whether sdpSemantics was specified at construction.
  bool sdp_semantics_specified_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_H_
