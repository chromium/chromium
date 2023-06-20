// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_encoding_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_send_parameters.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
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
    const RTCRtpEncodingParameters*,
    const String& kind);
RTCRtpHeaderExtensionParameters* ToRtpHeaderExtensionParameters(
    const webrtc::RtpExtension& headers);
RTCRtpCodecParameters* ToRtpCodecParameters(
    const webrtc::RtpCodecParameters& codecs);

// https://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
class RTCRtpSender final : public ScriptWrappable,
                           public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // TODO(hbos): Get rid of sender's reference to RTCPeerConnection?
  // https://github.com/w3c/webrtc-pc/issues/1712
  RTCRtpSender(RTCPeerConnection*,
               std::unique_ptr<RTCRtpSenderPlatform>,
               String kind,
               MediaStreamTrack*,
               MediaStreamVector streams,
               bool encoded_insertable_streams);

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

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  void RegisterEncodedAudioStreamCallback();
  void UnregisterEncodedAudioStreamCallback();
  void InitializeEncodedAudioStreams(ScriptState*);
  void OnAudioFrameFromEncoder(
      std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame);

  void RegisterEncodedVideoStreamCallback();
  void UnregisterEncodedVideoStreamCallback();
  void InitializeEncodedVideoStreams(ScriptState*);
  void OnVideoFrameFromEncoder(
      std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame);
  void SetAudioUnderlyingSource(
      RTCEncodedAudioUnderlyingSource* new_underlying_source,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  void SetAudioUnderlyingSink(
      RTCEncodedAudioUnderlyingSink* new_underlying_sink);
  void SetVideoUnderlyingSource(
      RTCEncodedVideoUnderlyingSource* new_underlying_source,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  void SetVideoUnderlyingSink(
      RTCEncodedVideoUnderlyingSink* new_underlying_sink);

  Member<RTCPeerConnection> pc_;
  std::unique_ptr<RTCRtpSenderPlatform> sender_;
  String kind_;
  Member<MediaStreamTrack> track_;
  Member<RTCDtlsTransport> transport_;
  Member<RTCDTMFSender> dtmf_;
  MediaStreamVector streams_;
  Member<RTCRtpSendParameters> last_returned_parameters_;
  Member<RTCRtpTransceiver> transceiver_;

  // Insertable Streams flag, |True| if the sender has been configured to
  // use Encoded Insertable Streams.
  bool encoded_insertable_streams_;

  // Insertable Streams audio support
  base::Lock audio_underlying_source_lock_;
  CrossThreadPersistent<RTCEncodedAudioUnderlyingSource>
      audio_from_encoder_underlying_source_
          GUARDED_BY(audio_underlying_source_lock_);
  base::Lock audio_underlying_sink_lock_;
  CrossThreadPersistent<RTCEncodedAudioUnderlyingSink>
      audio_to_packetizer_underlying_sink_
          GUARDED_BY(audio_underlying_sink_lock_);
  Member<RTCInsertableStreams> encoded_audio_streams_;
  const scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
      encoded_audio_transformer_;

  // Insertable Streams video support
  base::Lock video_underlying_source_lock_;
  CrossThreadPersistent<RTCEncodedVideoUnderlyingSource>
      video_from_encoder_underlying_source_
          GUARDED_BY(video_underlying_source_lock_);
  base::Lock video_underlying_sink_lock_;
  CrossThreadPersistent<RTCEncodedVideoUnderlyingSink>
      video_to_packetizer_underlying_sink_
          GUARDED_BY(video_underlying_sink_lock_);
  Member<RTCInsertableStreams> encoded_video_streams_;
  const scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
      encoded_video_transformer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_H_
