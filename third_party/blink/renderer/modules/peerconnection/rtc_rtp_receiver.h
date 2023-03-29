// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_

#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_contributing_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_receive_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_synchronization_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"
#include "third_party/webrtc/api/media_types.h"

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
class RTCRtpReceiver final : public ScriptWrappable,
                             public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class MediaKind { kAudio, kVideo };

  // Takes ownership of the receiver.
  RTCRtpReceiver(RTCPeerConnection*,
                 std::unique_ptr<RTCRtpReceiverPlatform>,
                 MediaStreamTrack*,
                 MediaStreamVector,
                 bool encoded_insertable_streams);

  static RTCRtpCapabilities* getCapabilities(ScriptState* state,
                                             const String& kind);

  MediaStreamTrack* track() const;
  RTCDtlsTransport* transport();
  RTCDtlsTransport* rtcpTransport();
  absl::optional<double> playoutDelayHint() const;
  void setPlayoutDelayHint(absl::optional<double>, ExceptionState&);
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
  MediaKind kind() const;
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
  void OnAudioFrameFromDepacketizer(
      std::unique_ptr<webrtc::TransformableFrameInterface> encoded_audio_frame);
  void RegisterEncodedVideoStreamCallback();
  void UnregisterEncodedVideoStreamCallback();
  void InitializeEncodedVideoStreams(ScriptState*);
  void OnVideoFrameFromDepacketizer(
      std::unique_ptr<webrtc::TransformableVideoFrameInterface>
          encoded_video_frame);
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
  std::unique_ptr<RTCRtpReceiverPlatform> receiver_;
  Member<MediaStreamTrack> track_;
  Member<RTCDtlsTransport> transport_;
  MediaStreamVector streams_;

  // The current SSRCs and CSRCs. getSynchronizationSources() returns the SSRCs
  // and getContributingSources() returns the CSRCs.
  Vector<std::unique_ptr<RTCRtpSource>> web_sources_;
  Member<RTCRtpTransceiver> transceiver_;

  // Hint to the WebRTC Jitter Buffer about desired playout delay. Actual
  // observed delay may differ depending on the congestion control. |nullopt|
  // means default value must be used.
  absl::optional<double> playout_delay_hint_;

  // Insertable Streams flag, |True| if the receiver has been configured to
  // use Encoded Insertable Streams.
  bool encoded_insertable_streams_;

  THREAD_CHECKER(thread_checker_);

  // Insertable Streams support for audio.
  base::Lock audio_underlying_source_lock_;
  CrossThreadPersistent<RTCEncodedAudioUnderlyingSource>
      audio_from_depacketizer_underlying_source_
          GUARDED_BY(audio_underlying_source_lock_);
  base::Lock audio_underlying_sink_lock_;
  CrossThreadPersistent<RTCEncodedAudioUnderlyingSink>
      audio_to_decoder_underlying_sink_ GUARDED_BY(audio_underlying_sink_lock_);
  Member<RTCInsertableStreams> encoded_audio_streams_;
  const scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
      encoded_audio_transformer_;

  // Insertable Streams support for video.
  base::Lock video_underlying_source_lock_;
  CrossThreadPersistent<RTCEncodedVideoUnderlyingSource>
      video_from_depacketizer_underlying_source_
          GUARDED_BY(video_underlying_source_lock_);
  base::Lock video_underlying_sink_lock_;
  CrossThreadPersistent<RTCEncodedVideoUnderlyingSink>
      video_to_decoder_underlying_sink_ GUARDED_BY(video_underlying_sink_lock_);
  Member<RTCInsertableStreams> encoded_video_streams_;
  const scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
      encoded_video_transformer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_RECEIVER_H_
