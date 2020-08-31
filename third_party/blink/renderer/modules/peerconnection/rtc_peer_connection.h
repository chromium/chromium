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
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/crypto/normalize_algorithm.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/call_setup_state_tracker.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_controller.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_enums.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_request.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExceptionState;
class MediaStreamTrack;
class MediaStreamTrackOrString;
class RTCAnswerOptions;
class RTCConfiguration;
class RTCDtlsTransport;
class RTCDTMFSender;
class RTCDataChannel;
class RTCDataChannelInit;
class RTCIceCandidateInitOrRTCIceCandidate;
class RTCIceTransport;
class RTCOfferOptions;
class RTCPeerConnectionTest;
class RTCRtpReceiver;
class RTCRtpSender;
class RTCRtpTransceiverInit;
class RTCSctpTransport;
class RTCSessionDescription;
class RTCSessionDescriptionInit;
class ScriptState;
class V8RTCPeerConnectionErrorCallback;
class V8RTCSessionDescriptionCallback;
class V8RTCStatsCallback;
class V8VoidFunction;

extern const char kOnlySupportedInUnifiedPlanMessage[];

// This enum is used to track usage of SDP during the transition of the default
// "sdpSemantics" value from "Plan B" to "Unified Plan". Usage refers to
// operations such as createOffer(), createAnswer(), setLocalDescription() and
// setRemoteDescription(). "Complex" SDP refers to SDP that is not compatible
// between SDP formats. Usage of SDP falls into two categories: "safe" and
// "unsafe". Applications with unsafe usage are predicted to break when the
// default changes. This includes complex SDP usage and relying on the default
// sdpSemantics. kUnknown is used if the SDP format could not be deduced, such
// as if SDP could not be parsed.
enum class SdpUsageCategory {
  kSafe = 0,
  kUnsafe = 1,
  kUnknown = 2,
  kMaxValue = kUnknown,
};

MODULES_EXPORT SdpUsageCategory
DeduceSdpUsageCategory(const String& sdp_type,
                       const String& sdp,
                       bool sdp_semantics_specified,
                       webrtc::SdpSemantics sdp_semantics);

class MODULES_EXPORT RTCPeerConnection final
    : public EventTargetWithInlineData,
      public RTCPeerConnectionHandlerClient,
      public ActiveScriptWrappable<RTCPeerConnection>,
      public ExecutionContextLifecycleObserver,
      public MediaStreamObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(RTCPeerConnection, Dispose);

 public:
  static RTCPeerConnection* Create(ExecutionContext*,
                                   const RTCConfiguration*,
                                   const Dictionary&,
                                   ExceptionState&);
  static RTCPeerConnection* Create(ExecutionContext*,
                                   const RTCConfiguration*,
                                   const ScriptValue&,
                                   ExceptionState&);
  static RTCPeerConnection* Create(ExecutionContext*,
                                   const RTCConfiguration*,
                                   ExceptionState&);

  RTCPeerConnection(ExecutionContext*,
                    webrtc::PeerConnectionInterface::RTCConfiguration,
                    bool sdp_semantics_specified,
                    bool force_encoded_audio_insertable_streams,
                    bool force_encoded_video_insertable_streams,
                    MediaConstraints,
                    ExceptionState&);
  ~RTCPeerConnection() override;

  ScriptPromise createOffer(ScriptState*,
                            const RTCOfferOptions*,
                            ExceptionState&);
  ScriptPromise createOffer(ScriptState*,
                            V8RTCSessionDescriptionCallback*,
                            V8RTCPeerConnectionErrorCallback*,
                            const ScriptValue&,
                            ExceptionState&);
  ScriptPromise createOffer(ScriptState*,
                            V8RTCSessionDescriptionCallback*,
                            V8RTCPeerConnectionErrorCallback*,
                            ExceptionState&);
  ScriptPromise CreateOffer(ScriptState*,
                            V8RTCSessionDescriptionCallback*,
                            V8RTCPeerConnectionErrorCallback*,
                            const Dictionary&,
                            ExceptionState&);

  ScriptPromise createAnswer(ScriptState*,
                             const RTCAnswerOptions*,
                             ExceptionState&);
  ScriptPromise createAnswer(ScriptState*,
                             V8RTCSessionDescriptionCallback*,
                             V8RTCPeerConnectionErrorCallback*,
                             const ScriptValue&,
                             ExceptionState&);
  ScriptPromise createAnswer(ScriptState*,
                             V8RTCSessionDescriptionCallback*,
                             V8RTCPeerConnectionErrorCallback*,
                             ExceptionState&);
  ScriptPromise CreateAnswer(ScriptState*,
                             V8RTCSessionDescriptionCallback*,
                             V8RTCPeerConnectionErrorCallback*,
                             const Dictionary&);

  ScriptPromise setLocalDescription(ScriptState*);
  ScriptPromise setLocalDescription(ScriptState*,
                                    const RTCSessionDescriptionInit*,
                                    ExceptionState&);
  ScriptPromise setLocalDescription(
      ScriptState*,
      const RTCSessionDescriptionInit*,
      V8VoidFunction*,
      V8RTCPeerConnectionErrorCallback* = nullptr);
  RTCSessionDescription* localDescription() const;
  RTCSessionDescription* currentLocalDescription() const;
  RTCSessionDescription* pendingLocalDescription() const;

  ScriptPromise setRemoteDescription(ScriptState*,
                                     const RTCSessionDescriptionInit*,
                                     ExceptionState&);
  ScriptPromise setRemoteDescription(
      ScriptState*,
      const RTCSessionDescriptionInit*,
      V8VoidFunction*,
      V8RTCPeerConnectionErrorCallback* = nullptr);
  RTCSessionDescription* remoteDescription() const;
  RTCSessionDescription* currentRemoteDescription() const;
  RTCSessionDescription* pendingRemoteDescription() const;

  String signalingState() const;

  RTCConfiguration* getConfiguration(ScriptState*) const;
  void setConfiguration(ScriptState*, const RTCConfiguration*, ExceptionState&);

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

  String connectionState() const;

  base::Optional<bool> canTrickleIceCandidates() const;

  void restartIce();

  // A local stream is any stream associated with a sender.
  MediaStreamVector getLocalStreams() const;
  // A remote stream is any stream associated with a receiver.
  MediaStreamVector getRemoteStreams() const;
  MediaStream* getRemoteStreamById(const String&) const;
  bool IsRemoteStream(MediaStream* stream) const;

  void addStream(ScriptState*,
                 MediaStream*,
                 const ScriptValue& media_constraints,
                 ExceptionState&);
  void addStream(ScriptState*, MediaStream*, ExceptionState&);
  void AddStream(ScriptState*,
                 MediaStream*,
                 const Dictionary& media_constraints,
                 ExceptionState&);

  void removeStream(MediaStream*, ExceptionState&);

  // Calls LegacyCallbackBasedGetStats() or PromiseBasedGetStats() (or rejects
  // with an exception) depending on type, see rtc_peer_connection.idl.
  ScriptPromise getStats(ScriptState* script_state, ExceptionState&);
  ScriptPromise getStats(ScriptState* script_state,
                         ScriptValue callback_or_selector,
                         ExceptionState&);
  ScriptPromise getStats(ScriptState* script_state,
                         ScriptValue callback_or_selector,
                         ScriptValue legacy_selector,
                         ExceptionState&);
  ScriptPromise LegacyCallbackBasedGetStats(
      ScriptState*,
      V8RTCStatsCallback* success_callback,
      MediaStreamTrack* selector);
  ScriptPromise PromiseBasedGetStats(ScriptState*,
                                     MediaStreamTrack* selector,
                                     ExceptionState&);

  const HeapVector<Member<RTCRtpTransceiver>>& getTransceivers() const;
  const HeapVector<Member<RTCRtpSender>>& getSenders() const;
  const HeapVector<Member<RTCRtpReceiver>>& getReceivers() const;
  RTCRtpTransceiver* addTransceiver(const MediaStreamTrackOrString&,
                                    const RTCRtpTransceiverInit*,
                                    ExceptionState&);
  RTCRtpSender* addTrack(MediaStreamTrack*, MediaStreamVector, ExceptionState&);
  void removeTrack(RTCRtpSender*, ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(track, kTrack)

  RTCSctpTransport* sctp() const;
  RTCDataChannel* createDataChannel(ScriptState*,
                                    String label,
                                    const RTCDataChannelInit*,
                                    ExceptionState&);

  RTCDTMFSender* createDTMFSender(MediaStreamTrack*, ExceptionState&);

  bool IsClosed() { return closed_; }
  void close();

  // Makes the peer connection aware of the track. This is used to map web
  // tracks to blink tracks, as is necessary for plumbing. There is no need to
  // unregister the track because Weak references are used.
  void RegisterTrack(MediaStreamTrack*);

  // We allow getStats after close, but not other calls or callbacks.
  bool ShouldFireDefaultCallbacks() {
    return !closed_ && !peer_handler_unregistered_;
  }
  bool ShouldFireGetStatsCallback() { return !peer_handler_unregistered_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(negotiationneeded, kNegotiationneeded)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate, kIcecandidate)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(signalingstatechange, kSignalingstatechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream, kAddstream)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream, kRemovestream)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(iceconnectionstatechange,
                                  kIceconnectionstatechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connectionstatechange, kConnectionstatechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icegatheringstatechange,
                                  kIcegatheringstatechange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(datachannel, kDatachannel)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidateerror, kIcecandidateerror)

  // Utility to note result of CreateOffer / CreateAnswer
  void NoteSdpCreated(const RTCSessionDescription&);
  // Utility to report SDP usage of setLocalDescription / setRemoteDescription.
  enum class SetSdpOperationType {
    kSetLocalDescription,
    kSetRemoteDescription,
  };
  void ReportSetSdpUsage(
      SetSdpOperationType operation_type,
      const RTCSessionDescriptionInit* session_description_init) const;

  // MediaStreamObserver
  void OnStreamAddTrack(MediaStream*, MediaStreamTrack*) override;
  void OnStreamRemoveTrack(MediaStream*, MediaStreamTrack*) override;

  // RTCPeerConnectionHandlerClient
  void NegotiationNeeded() override;

  void DidGenerateICECandidate(RTCIceCandidatePlatform*) override;
  void DidFailICECandidate(const String& address,
                           base::Optional<uint16_t> port,
                           const String& host_candidate,
                           const String& url,
                           int error_code,
                           const String& error_text) override;
  void DidChangeSessionDescriptions(
      RTCSessionDescriptionPlatform* pending_local_description,
      RTCSessionDescriptionPlatform* current_local_description,
      RTCSessionDescriptionPlatform* pending_remote_description,
      RTCSessionDescriptionPlatform* current_remote_description) override;
  void DidChangeIceGatheringState(
      webrtc::PeerConnectionInterface::IceGatheringState) override;
  void DidChangeIceConnectionState(
      webrtc::PeerConnectionInterface::IceConnectionState) override;
  void DidChangePeerConnectionState(
      webrtc::PeerConnectionInterface::PeerConnectionState) override;
  void DidModifyReceiversPlanB(
      webrtc::PeerConnectionInterface::SignalingState,
      Vector<std::unique_ptr<RTCRtpReceiverPlatform>> platform_receivers_added,
      Vector<std::unique_ptr<RTCRtpReceiverPlatform>>
          platform_receivers_removed) override;
  void DidModifySctpTransport(WebRTCSctpTransportSnapshot) override;
  void DidModifyTransceivers(webrtc::PeerConnectionInterface::SignalingState,
                             Vector<std::unique_ptr<RTCRtpTransceiverPlatform>>,
                             Vector<uintptr_t>,
                             bool is_remote_description) override;
  void DidAddRemoteDataChannel(
      scoped_refptr<webrtc::DataChannelInterface> channel) override;
  void DidNoteInterestingUsage(int usage_pattern) override;
  void UnregisterPeerConnectionHandler() override;
  void ClosePeerConnection() override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  // ScriptWrappable
  // We keep the this object alive until either stopped or closed.
  bool HasPendingActivity() const final {
    return !closed_ && !peer_handler_unregistered_;
  }

  // For testing; exported to testing/InternalWebRTCPeerConnection
  static int PeerConnectionCount();
  static int PeerConnectionCountLimit();

  // SLD/SRD Helper method, public for testing.
  // This function returns a value that indicates if complex SDP is being used
  // and whether a format is explicitly specified. If the SDP is not complex or
  // it could not be parsed, base::nullopt is returned.
  // When "Complex" SDP (i.e., SDP that has multiple tracks) is used without
  // explicitly specifying the SDP format, there may be errors if the
  // application assumes a format that differs from the actual default format.
  base::Optional<ComplexSdpCategory> CheckForComplexSdp(
      const RTCSessionDescriptionInit* session_description_init) const;

  const CallSetupStateTracker& call_setup_state_tracker() const;
  void NoteCallSetupStateEventPending(
      RTCPeerConnection::SetSdpOperationType operation,
      const RTCSessionDescriptionInit& description);
  void NoteSessionDescriptionRequestCompleted(
      RTCCreateSessionDescriptionOperation operation,
      bool success);
  void NoteVoidRequestCompleted(RTCSetSessionDescriptionOperation operation,
                                bool success);
  static void GenerateCertificateCompleted(
      ScriptPromiseResolver* resolver,
      rtc::scoped_refptr<rtc::RTCCertificate> certificate);
  // Checks if the document that the peer connection lives in has ever executed
  // getUserMedia().
  bool HasDocumentMedia() const;

  // Called by RTCIceTransport::OnStateChange to update the ice connection
  // state.
  void UpdateIceConnectionState();

  webrtc::SdpSemantics sdp_semantics() { return sdp_semantics_; }

  bool force_encoded_audio_insertable_streams() {
    return force_encoded_audio_insertable_streams_;
  }

  bool force_encoded_video_insertable_streams() {
    return force_encoded_video_insertable_streams_;
  }

  void Trace(Visitor*) const override;

  base::TimeTicks WebRtcTimestampToBlinkTimestamp(
      base::TimeTicks webrtc_monotonic_time) const;

  using RtcPeerConnectionHandlerFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<RTCPeerConnectionHandler>()>;
  static void SetRtcPeerConnectionHandlerFactoryForTesting(
      RtcPeerConnectionHandlerFactoryCallback);

 private:
  friend class InternalsRTCPeerConnection;
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetAudioTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetVideoTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetAudioAndVideoTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCAll);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest,
                           GetTrackRemoveStreamAndGCWithPersistentComponent);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest,
                           GetTrackRemoveStreamAndGCWithPersistentStream);

  typedef base::OnceCallback<bool()> BoolFunction;
  class EventWrapper final : public GarbageCollected<EventWrapper> {
   public:
    EventWrapper(Event*, BoolFunction);
    // Returns true if |m_setupFunction| returns true or it is null.
    // |m_event| will only be fired if setup() returns true;
    bool Setup();

    void Trace(Visitor*) const;

    Member<Event> event_;

   private:
    BoolFunction setup_function_;
  };
  void Dispose();

  void MaybeDispatchEvent(Event*);
  // TODO(hbos): Remove any remaining uses of ScheduleDispatchEvent.
  void ScheduleDispatchEvent(Event*);
  void ScheduleDispatchEvent(Event*, BoolFunction);
  void DispatchScheduledEvents();
  MediaStreamTrack* GetTrack(MediaStreamComponent*) const;
  RTCRtpSender* FindSenderForTrackAndStream(MediaStreamTrack*, MediaStream*);
  HeapVector<Member<RTCRtpSender>>::iterator FindSender(
      const RTCRtpSenderPlatform& web_sender);
  HeapVector<Member<RTCRtpReceiver>>::iterator FindReceiver(
      const RTCRtpReceiverPlatform& platform_receiver);
  HeapVector<Member<RTCRtpTransceiver>>::iterator FindTransceiver(
      const RTCRtpTransceiverPlatform& platform_transceiver);

  // Creates or updates the sender such that it is up-to-date with the
  // RTCRtpSenderPlatform in all regards *except for streams*. The web sender
  // only knows of stream IDs; updating the stream objects requires additional
  // logic which is different depending on context, e.g:
  // - If created/updated with addTrack(), the streams were supplied as
  //   arguments.
  // The web sender's web track must already have a correspondent blink track in
  // |tracks_|. The caller is responsible for ensuring this with
  // RegisterTrack(), e.g:
  // - On addTrack(), the track is supplied as an argument.
  RTCRtpSender* CreateOrUpdateSender(std::unique_ptr<RTCRtpSenderPlatform>,
                                     String kind);
  // Creates or updates the receiver such that it is up-to-date with the
  // RTCRtpReceiverPlatform in all regards *except for streams*. The web
  // receiver only knows of stream IDs; updating the stream objects requires
  // additional logic which is different depending on context, e.g:
  // - If created/updated with setRemoteDescription(), there is an algorithm for
  //   processing the addition/removal of remote tracks which includes how to
  //   create and update the associated streams set.
  RTCRtpReceiver* CreateOrUpdateReceiver(
      std::unique_ptr<RTCRtpReceiverPlatform>);
  // Creates or updates the transceiver such that it, including its sender and
  // receiver, are up-to-date with the RTCRtpTransceiverPlatform in all regerds
  // *except for sender and receiver streams*. The web sender and web receiver
  // only knows of stream IDs; updating the stream objects require additional
  // logic which is different depending on context. See above.
  RTCRtpTransceiver* CreateOrUpdateTransceiver(
      std::unique_ptr<RTCRtpTransceiverPlatform>);

  // Creates or updates the RTCDtlsTransport object corresponding to the
  // given webrtc::DtlsTransportInterface object.
  RTCDtlsTransport* CreateOrUpdateDtlsTransport(
      rtc::scoped_refptr<webrtc::DtlsTransportInterface>,
      const webrtc::DtlsTransportInformation& info);

  // Creates or updates the RTCIceTransport object corresponding to the given
  // webrtc::IceTransportInterface object.
  RTCIceTransport* CreateOrUpdateIceTransport(
      rtc::scoped_refptr<webrtc::IceTransportInterface>);

  // Update the |receiver->streams()| to the streams indicated by |stream_ids|,
  // adding to |remove_list| and |add_list| accordingly.
  // https://w3c.github.io/webrtc-pc/#set-associated-remote-streams
  void SetAssociatedMediaStreams(
      RTCRtpReceiver* receiver,
      const Vector<String>& stream_ids,
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
  webrtc::PeerConnectionInterface::IceConnectionState
  ComputeIceConnectionState();
  bool HasAnyFailedIceTransport() const;
  bool HasAnyDisconnectedIceTransport() const;
  bool HasAllNewOrClosedIceTransports() const;
  bool HasAnyNewOrCheckingIceTransport() const;
  bool HasAllCompletedOrClosedIceTransports() const;
  bool HasAllConnectedCompletedOrClosedIceTransports() const;

  void ChangePeerConnectionState(
      webrtc::PeerConnectionInterface::PeerConnectionState);
  bool SetPeerConnectionState(
      webrtc::PeerConnectionInterface::PeerConnectionState);

  void CloseInternal();

  void RecordRapporMetrics();

  DOMException* checkSdpForStateErrors(ExecutionContext*,
                                       const RTCSessionDescriptionInit*,
                                       String* sdp);
  void MaybeWarnAboutUnsafeSdp(
      const RTCSessionDescriptionInit* session_description_init) const;

  HeapHashSet<Member<RTCIceTransport>> ActiveIceTransports() const;

  Member<RTCSessionDescription> pending_local_description_;
  Member<RTCSessionDescription> current_local_description_;
  Member<RTCSessionDescription> pending_remote_description_;
  Member<RTCSessionDescription> current_remote_description_;
  webrtc::PeerConnectionInterface::SignalingState signaling_state_;
  webrtc::PeerConnectionInterface::IceGatheringState ice_gathering_state_;
  webrtc::PeerConnectionInterface::IceConnectionState ice_connection_state_;
  webrtc::PeerConnectionInterface::PeerConnectionState peer_connection_state_;
  // TODO(https://crbug.com/857004): The trackers' metrics are currently not
  // uploaded; either use the metrics it produces (i.e. revert
  // https://chromium-review.googlesource.com/c/chromium/src/+/1991421) or
  // delete all CallSetupStateTracker code for good.
  CallSetupStateTracker call_setup_state_tracker_;

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

  // A map of all webrtc::DtlsTransports that have a corresponding
  // RTCDtlsTransport object. Garbage collection will remove map entries
  // when they are no longer in use.
  // Note: Transports may exist in the map even if they are not currently
  // in use, since garbage collection only happens when needed.
  HeapHashMap<webrtc::DtlsTransportInterface*, WeakMember<RTCDtlsTransport>>
      dtls_transports_by_native_transport_;
  // The same kind of map for webrtc::IceTransports.
  HeapHashMap<webrtc::IceTransportInterface*, WeakMember<RTCIceTransport>>
      ice_transports_by_native_transport_;

  // TODO(crbug.com/787254): Use RTCPeerConnectionHandler.
  std::unique_ptr<RTCPeerConnectionHandler> peer_handler_;

  base::OnceClosure dispatch_events_task_created_callback_for_testing_;
  TaskHandle dispatch_scheduled_events_task_handle_;
  HeapVector<Member<EventWrapper>> scheduled_events_;

  // This handle notifies scheduler about an active connection associated
  // with a frame. Handle should be destroyed when connection is closed.
  FrameScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  // When the |peer_handler_| is unregistered, the native peer connection is
  // closed and disappears from the chrome://webrtc-internals page. This happens
  // when page context is destroyed.
  //
  // Note that the peer connection can be |closed_| without being unregistered
  // (in which case it is still visible in chrome://webrtc-internals). If
  // context is destroyed before the peer connection is closed, the native peer
  // connection will be closed and stop surfacing states to blink but the blink
  // peer connection will be unaware of the native layer being closed.
  bool peer_handler_unregistered_;
  // Reflects the RTCPeerConnection's [[IsClosed]] internal slot.
  // https://w3c.github.io/webrtc-pc/#dfn-isclosed
  // TODO(https://crbug.com/1083204): According to spec, the peer connection can
  // only be closed through the close() API. However, our implementation can
  // also be closed asynchronously by the |peer_handler_|, such as in response
  // to laptop lid close on some system (depending on OS and settings).
  bool closed_;
  // When true, events on the RTCPeerConnection will not be dispatched to
  // JavaScript. This happens when close() is called but not if the peer
  // connection was closed asynchronously. This also happens if the context is
  // destroyed.
  // TODO(https://crbug.com/1083204): When we are spec compliant and don't close
  // the peer connection asynchronously, this can be removed in favor of
  // |closed_|.
  bool suppress_events_;

  // Internal state [[LastOffer]] and [[LastAnswer]]
  String last_offer_;
  String last_answer_;

  Member<RTCSctpTransport> sctp_transport_;
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

  // Blink and WebRTC timestamp diff.
  const base::TimeDelta blink_webrtc_time_diff_;

  // Insertable streams.
  bool force_encoded_audio_insertable_streams_;
  bool force_encoded_video_insertable_streams_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_H_
