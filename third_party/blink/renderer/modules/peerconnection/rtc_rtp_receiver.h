// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_

#include "base/optional.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_contributing_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_receive_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_synchronization_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"

namespace blink {
class RTCDtlsTransport;
class RTCEncodedAudioUnderlyingSource;
class RTCEncodedAudioUnderlyingSink;
class RTCEncodedVideoUnderlyingSource;
class RTCEncodedVideoUnderlyingSink;
class RTCInsertableStreams;
class RTCPeerConnection;
class RTCRtpCapabilities;
class RTCRtpTransceiver;

// https://w3c.github.io/webrtc-pc/#rtcrtpreceiver-interface
class RTCRtpReceiver final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Takes ownership of the receiver.
  RTCRtpReceiver(RTCPeerConnection*,
                 std::unique_ptr<RTCRtpReceiverPlatform>,
                 MediaStreamTrack*,
                 MediaStreamVector,
                 bool force_encoded_audio_insertable_streams,
                 bool force_encoded_video_insertable_streams);

  static RTCRtpCapabilities* getCapabilities(ScriptState* state,
                                             const String& kind);

  MediaStreamTrack* track() const;
  RTCDtlsTransport* transport();
  RTCDtlsTransport* rtcpTransport();
  base::Optional<double> playoutDelayHint() const;
  void setPlayoutDelayHint(base::Optional<double>, ExceptionState&);
  RTCRtpReceiveParameters* getParameters();
  HeapVector<Member<RTCRtpSynchronizationSource>> getSynchronizationSources(
      ScriptState*,
      ExceptionState&);
  HeapVector<Member<RTCRtpContributingSource>> getContributingSources(
      ScriptState*,
      ExceptionState&);
  ScriptPromise getStats(ScriptState*);
  RTCInsertableStreams* createEncodedStreams(ScriptState*, ExceptionState&);
  // TODO(crbug.com/1069295): Make these methods private.
  RTCInsertableStreams* createEncodedAudioStreams(ScriptState*,
                                                  ExceptionState&);
  RTCInsertableStreams* createEncodedVideoStreams(ScriptState*,
                                                  ExceptionState&);

  RTCRtpReceiverPlatform* platform_receiver();
  MediaStreamVector streams() const;
  void set_streams(MediaStreamVector streams);
  void set_transceiver(RTCRtpTransceiver*);
  void set_transport(RTCDtlsTransport*);
  void UpdateSourcesIfNeeded();

  void Trace(Visitor*) const override;

 private:
  void SetContributingSourcesNeedsUpdating();
  void RegisterEncodedAudioStreamCallback();
  void UnregisterEncodedAudioStreamCallback();
  void InitializeEncodedAudioStreams(ScriptState*);
  void OnAudioFrameFromDepacketizer(
      std::unique_ptr<webrtc::TransformableFrameInterface> encoded_audio_frame);
  void RegisterEncodedVideoStreamCallback();
  void UnregisterEncodedVideoStreamCallback();
  void InitializeEncodedVideoStreams(ScriptState*);
  void OnVideoFrameFromDepacketizer(
      std::unique_ptr<webrtc::TransformableVideoFrameInterface>
          encoded_video_frame);

  Member<RTCPeerConnection> pc_;
  std::unique_ptr<RTCRtpReceiverPlatform> receiver_;
  Member<MediaStreamTrack> track_;
  Member<RTCDtlsTransport> transport_;
  MediaStreamVector streams_;

  // The current SSRCs and CSRCs. getSynchronizationSources() returns the SSRCs
  // and getContributingSources() returns the CSRCs.
  Vector<std::unique_ptr<RTCRtpSource>> web_sources_;
  bool web_sources_needs_updating_ = true;
  Member<RTCRtpTransceiver> transceiver_;

  // Hint to the WebRTC Jitter Buffer about desired playout delay. Actual
  // observed delay may differ depending on the congestion control. |nullopt|
  // means default value must be used.
  base::Optional<double> playout_delay_hint_;

  // Insertable Streams support for audio
  bool force_encoded_audio_insertable_streams_;
  Member<RTCEncodedAudioUnderlyingSource>
      audio_from_depacketizer_underlying_source_;
  Member<RTCEncodedAudioUnderlyingSink> audio_to_decoder_underlying_sink_;
  Member<RTCInsertableStreams> encoded_audio_streams_;

  // Insertable Streams support for video
  bool force_encoded_video_insertable_streams_;
  Member<RTCEncodedVideoUnderlyingSource>
      video_from_depacketizer_underlying_source_;
  Member<RTCEncodedVideoUnderlyingSink> video_to_decoder_underlying_sink_;
  Member<RTCInsertableStreams> encoded_video_streams_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_
