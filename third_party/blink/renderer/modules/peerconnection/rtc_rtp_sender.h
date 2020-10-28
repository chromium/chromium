// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_encoding_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_send_parameters.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"

namespace blink {

class ExceptionState;
class MediaStreamTrack;
class RTCDtlsTransport;
class RTCDTMFSender;
class RTCEncodedAudioUnderlyingSink;
class RTCEncodedAudioUnderlyingSource;
class RTCEncodedVideoUnderlyingSink;
class RTCEncodedVideoUnderlyingSource;
class RTCInsertableStreams;
class RTCPeerConnection;
class RTCRtpCapabilities;
class RTCRtpTransceiver;
class RTCInsertableStreams;

webrtc::RtpEncodingParameters ToRtpEncodingParameters(
    ExecutionContext* context,
    const RTCRtpEncodingParameters*);
RTCRtpHeaderExtensionParameters* ToRtpHeaderExtensionParameters(
    const webrtc::RtpExtension& headers);
RTCRtpCodecParameters* ToRtpCodecParameters(
    const webrtc::RtpCodecParameters& codecs);

// https://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
class RTCRtpSender final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // TODO(hbos): Get rid of sender's reference to RTCPeerConnection?
  // https://github.com/w3c/webrtc-pc/issues/1712
  RTCRtpSender(RTCPeerConnection*,
               std::unique_ptr<RTCRtpSenderPlatform>,
               String kind,
               MediaStreamTrack*,
               MediaStreamVector streams,
               bool force_encoded_audio_insertable_streams,
               bool force_encoded_video_insertable_streams);

  MediaStreamTrack* track();
  RTCDtlsTransport* transport();
  RTCDtlsTransport* rtcpTransport();
  ScriptPromise replaceTrack(ScriptState*, MediaStreamTrack*);
  RTCDTMFSender* dtmf();
  static RTCRtpCapabilities* getCapabilities(ScriptState* state,
                                             const String& kind);
  RTCRtpSendParameters* getParameters();
  ScriptPromise setParameters(ScriptState*, const RTCRtpSendParameters*);
  ScriptPromise getStats(ScriptState*);
  void setStreams(HeapVector<Member<MediaStream>> streams, ExceptionState&);
  RTCInsertableStreams* createEncodedStreams(ScriptState*, ExceptionState&);
  // TODO(crbug.com/1069295): Make these methods private.
  RTCInsertableStreams* createEncodedAudioStreams(ScriptState*,
                                                  ExceptionState&);
  RTCInsertableStreams* createEncodedVideoStreams(ScriptState*,
                                                  ExceptionState&);

  RTCRtpSenderPlatform* web_sender();
  // Sets the track. This must be called when the |RTCRtpSenderPlatform| has its
  // track updated, and the |track| must match the
  // |RTCRtpSenderPlatform::Track|.
  void SetTrack(MediaStreamTrack*);
  void ClearLastReturnedParameters();
  MediaStreamVector streams() const;
  void set_streams(MediaStreamVector streams);
  void set_transceiver(RTCRtpTransceiver*);
  void set_transport(RTCDtlsTransport*);

  void Trace(Visitor*) const override;

 private:
  void RegisterEncodedAudioStreamCallback();
  void UnregisterEncodedAudioStreamCallback();
  void InitializeEncodedAudioStreams(ScriptState*);
  void OnAudioFrameFromEncoder(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame);

  void RegisterEncodedVideoStreamCallback();
  void UnregisterEncodedVideoStreamCallback();
  void InitializeEncodedVideoStreams(ScriptState*);
  void OnVideoFrameFromEncoder(
      std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame);

  Member<RTCPeerConnection> pc_;
  std::unique_ptr<RTCRtpSenderPlatform> sender_;
  // The spec says that "kind" should be looked up in transceiver, but keeping
  // a copy here as long as we support Plan B.
  String kind_;
  Member<MediaStreamTrack> track_;
  Member<RTCDtlsTransport> transport_;
  Member<RTCDTMFSender> dtmf_;
  MediaStreamVector streams_;
  Member<RTCRtpSendParameters> last_returned_parameters_;
  Member<RTCRtpTransceiver> transceiver_;

  // Insertable Streams audio support
  bool force_encoded_audio_insertable_streams_;
  Member<RTCEncodedAudioUnderlyingSource> audio_from_encoder_underlying_source_;
  Member<RTCEncodedAudioUnderlyingSink> audio_to_packetizer_underlying_sink_;
  Member<RTCInsertableStreams> encoded_audio_streams_;

  // Insertable Streams video support
  bool force_encoded_video_insertable_streams_;
  Member<RTCEncodedVideoUnderlyingSource> video_from_encoder_underlying_source_;
  Member<RTCEncodedVideoUnderlyingSink> video_to_packetizer_underlying_sink_;
  Member<RTCInsertableStreams> encoded_video_streams_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_H_
