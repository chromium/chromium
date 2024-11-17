// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"

#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_insertable_streams.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtcp_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_codec_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_decoding_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_capability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_parameters.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_receiver_sink_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_receiver_source_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_receiver_sink_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_receiver_source_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transform.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/rtp_parameters.h"

namespace blink {

RTCRtpReceiver::RTCRtpReceiver(RTCPeerConnection* pc,
                               std::unique_ptr<RTCRtpReceiverPlatform> receiver,
                               MediaStreamTrack* track,
                               MediaStreamVector streams,
                               bool require_encoded_insertable_streams,
                               scoped_refptr<base::SequencedTaskRunner>
                                   encoded_transform_shortcircuit_runner)
    : ExecutionContextLifecycleObserver(pc->GetExecutionContext()),
      pc_(pc),
      receiver_(std::move(receiver)),
      track_(track),
      streams_(std::move(streams)),
      encoded_audio_transformer_(
          track_->kind() == "audio"
              ? receiver_->GetEncodedAudioStreamTransformer()->GetBroker()
              : nullptr),
      encoded_video_transformer_(
          track_->kind() == "video"
              ? receiver_->GetEncodedVideoStreamTransformer()->GetBroker()
              : nullptr) {
  DCHECK(pc_);
  DCHECK(receiver_);
  DCHECK(track_);
  if (!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
    if (encoded_audio_transformer_) {
      RegisterEncodedAudioStreamCallback();
    } else if (encoded_video_transformer_) {
      CHECK(encoded_video_transformer_);
      RegisterEncodedVideoStreamCallback();
    }
  }

  if (!require_encoded_insertable_streams) {
    // We're not requiring JS to create encoded streams itself, so schedule a
    // task to shortcircuit the encoded transform if JS doesn't synchronously
    // create them - implementing
    // https://www.w3.org/TR/2023/WD-webrtc-encoded-transform-20231012/#stream-creation
    // step 12.

    encoded_transform_shortcircuit_runner->PostTask(
        FROM_HERE,
        WTF::BindOnce(&RTCRtpReceiver::MaybeShortCircuitEncodedStreams,
                      WrapPersistent(this)));
  }
}

MediaStreamTrack* RTCRtpReceiver::track() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return track_.Get();
}

RTCDtlsTransport* RTCRtpReceiver::transport() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return transport_.Get();
}

RTCDtlsTransport* RTCRtpReceiver::rtcpTransport() {
  // Chrome does not support turning off RTCP-mux.
  return nullptr;
}

std::optional<double> RTCRtpReceiver::playoutDelayHint() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return playout_delay_hint_;
}

void RTCRtpReceiver::setPlayoutDelayHint(std::optional<double> hint,
                                         ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (hint.has_value() && hint.value() < 0.0) {
    exception_state.ThrowTypeError("playoutDelayHint can't be negative");
    return;
  }

  playout_delay_hint_ = hint;
  receiver_->SetJitterBufferMinimumDelay(playout_delay_hint_);
}

std::optional<double> RTCRtpReceiver::jitterBufferTarget() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return jitter_buffer_target_;
}

void RTCRtpReceiver::setJitterBufferTarget(std::optional<double> target,
                                           ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (target.has_value() && (target.value() < 0.0 || target.value() > 4000.0)) {
    exception_state.ThrowRangeError(
        "jitterBufferTarget is out of expected range 0 to 4000 ms");
    return;
  }

  jitter_buffer_target_ = target;
  if (jitter_buffer_target_.has_value()) {
    receiver_->SetJitterBufferMinimumDelay(jitter_buffer_target_.value() /
                                           1000.0);
  } else {
    receiver_->SetJitterBufferMinimumDelay(std::nullopt);
  }
}

HeapVector<Member<RTCRtpSynchronizationSource>>
RTCRtpReceiver::getSynchronizationSources(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return pc_->GetRtpContributingSourceCache().getSynchronizationSources(
      script_state, exception_state, this);
}

HeapVector<Member<RTCRtpContributingSource>>
RTCRtpReceiver::getContributingSources(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return pc_->GetRtpContributingSourceCache().getContributingSources(
      script_state, exception_state, this);
}

ScriptPromise<RTCStatsReport> RTCRtpReceiver::getStats(
    ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<RTCStatsReport>>(script_state);
  auto promise = resolver->Promise();
  receiver_->GetStats(WTF::BindOnce(WebRTCStatsReportCallbackResolver,
                                    WrapPersistent(resolver)));
  return promise;
}

RTCInsertableStreams* RTCRtpReceiver::createEncodedStreams(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LogMessage(base::StringPrintf("%s({transform_shortcircuited_=%s})", __func__,
                                transform_shortcircuited_ ? "true" : "false"));
  if (transform_shortcircuited_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Too late to create encoded streams");
    return nullptr;
  }
  if (encoded_streams_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Encoded streams already created");
    return nullptr;
  }

  if (kind() == MediaKind::kAudio) {
    return CreateEncodedAudioStreams(script_state);
  }
  CHECK_EQ(kind(), MediaKind::kVideo);
  return CreateEncodedVideoStreams(script_state);
}

RTCRtpReceiverPlatform* RTCRtpReceiver::platform_receiver() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return receiver_.get();
}

RTCRtpReceiver::MediaKind RTCRtpReceiver::kind() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (track_->kind() == "audio")
    return MediaKind::kAudio;
  DCHECK_EQ(track_->kind(), "video");
  return MediaKind::kVideo;
}

MediaStreamVector RTCRtpReceiver::streams() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return streams_;
}

void RTCRtpReceiver::set_streams(MediaStreamVector streams) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  streams_ = std::move(streams);
}

void RTCRtpReceiver::set_transceiver(RTCRtpTransceiver* transceiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transceiver_ = transceiver;
}

void RTCRtpReceiver::set_transport(RTCDtlsTransport* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_ = transport;
}

V8RTCRtpTransceiverDirection RTCRtpReceiver::TransceiverDirection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // `transceiver_` is always initialized to a valid value.
  return transceiver_->direction();
}

std::optional<V8RTCRtpTransceiverDirection>
RTCRtpReceiver::TransceiverCurrentDirection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // `transceiver_` is always initialized to a valid value.
  return transceiver_->currentDirection();
}

void RTCRtpReceiver::ContextDestroyed() {
  {
    base::AutoLock locker(audio_underlying_source_lock_);
    audio_from_depacketizer_underlying_source_.Clear();
  }
  {
    base::AutoLock locker(audio_underlying_sink_lock_);
    audio_to_decoder_underlying_sink_.Clear();
  }
  {
    base::AutoLock locker(video_underlying_source_lock_);
    video_from_depacketizer_underlying_source_.Clear();
  }
  {
    base::AutoLock locker(video_underlying_sink_lock_);
    video_to_decoder_underlying_sink_.Clear();
  }
}

void RTCRtpReceiver::Trace(Visitor* visitor) const {
  visitor->Trace(pc_);
  visitor->Trace(track_);
  visitor->Trace(transport_);
  visitor->Trace(streams_);
  visitor->Trace(transceiver_);
  visitor->Trace(encoded_streams_);
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

RTCRtpCapabilities* RTCRtpReceiver::getCapabilities(ScriptState* state,
                                                    const String& kind) {
  if (kind != "audio" && kind != "video")
    return nullptr;

  RTCRtpCapabilities* capabilities = RTCRtpCapabilities::Create();
  capabilities->setCodecs(HeapVector<Member<RTCRtpCodecCapability>>());
  capabilities->setHeaderExtensions(
      HeapVector<Member<RTCRtpHeaderExtensionCapability>>());

  std::unique_ptr<webrtc::RtpCapabilities> rtc_capabilities =
      PeerConnectionDependencyFactory::From(*ExecutionContext::From(state))
          .GetReceiverCapabilities(kind);

  HeapVector<Member<RTCRtpCodecCapability>> codecs;
  codecs.ReserveInitialCapacity(
      base::checked_cast<wtf_size_t>(rtc_capabilities->codecs.size()));
  for (const auto& rtc_codec : rtc_capabilities->codecs) {
    auto* codec = RTCRtpCodecCapability::Create();
    codec->setMimeType(WTF::String::FromUTF8(rtc_codec.mime_type()));
    if (rtc_codec.clock_rate)
      codec->setClockRate(rtc_codec.clock_rate.value());
    if (rtc_codec.num_channels)
      codec->setChannels(rtc_codec.num_channels.value());
    if (!rtc_codec.parameters.empty()) {
      std::string sdp_fmtp_line;
      for (const auto& parameter : rtc_codec.parameters) {
        if (!sdp_fmtp_line.empty())
          sdp_fmtp_line += ";";
        if (parameter.first.empty()) {
          sdp_fmtp_line += parameter.second;
        } else {
          sdp_fmtp_line += parameter.first + "=" + parameter.second;
        }
      }
      codec->setSdpFmtpLine(sdp_fmtp_line.c_str());
    }
    codecs.push_back(codec);
  }
  capabilities->setCodecs(codecs);

  HeapVector<Member<RTCRtpHeaderExtensionCapability>> header_extensions;
  header_extensions.ReserveInitialCapacity(base::checked_cast<wtf_size_t>(
      rtc_capabilities->header_extensions.size()));
  for (const auto& rtc_header_extension : rtc_capabilities->header_extensions) {
    auto* header_extension = RTCRtpHeaderExtensionCapability::Create();
    header_extension->setUri(WTF::String::FromUTF8(rtc_header_extension.uri));
    header_extensions.push_back(header_extension);
  }
  capabilities->setHeaderExtensions(header_extensions);

  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kRtcRtpReceiverGetCapabilities)) {
    IdentifiableTokenBuilder builder;
    IdentifiabilityAddRTCRtpCapabilitiesToBuilder(builder, *capabilities);
    IdentifiabilityMetricBuilder(ExecutionContext::From(state)->UkmSourceID())
        .Add(IdentifiableSurface::FromTypeAndToken(
                 IdentifiableSurface::Type::kRtcRtpReceiverGetCapabilities,
                 IdentifiabilityBenignStringToken(kind)),
             builder.GetToken())
        .Record(ExecutionContext::From(state)->UkmRecorder());
  }
  return capabilities;
}

RTCRtpReceiveParameters* RTCRtpReceiver::getParameters() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RTCRtpReceiveParameters* parameters = RTCRtpReceiveParameters::Create();
  std::unique_ptr<webrtc::RtpParameters> webrtc_parameters =
      receiver_->GetParameters();

  RTCRtcpParameters* rtcp = RTCRtcpParameters::Create();
  rtcp->setReducedSize(webrtc_parameters->rtcp.reduced_size);
  parameters->setRtcp(rtcp);

  HeapVector<Member<RTCRtpDecodingParameters>> encodings;
  encodings.reserve(
      base::checked_cast<wtf_size_t>(webrtc_parameters->encodings.size()));
  for (const auto& webrtc_encoding : webrtc_parameters->encodings) {
    RTCRtpDecodingParameters* encoding = RTCRtpDecodingParameters::Create();
    if (!webrtc_encoding.rid.empty()) {
      // TODO(orphis): Add rid when supported by WebRTC
    }
    encodings.push_back(encoding);
  }
  parameters->setEncodings(encodings);

  HeapVector<Member<RTCRtpHeaderExtensionParameters>> headers;
  headers.reserve(base::checked_cast<wtf_size_t>(
      webrtc_parameters->header_extensions.size()));
  for (const auto& webrtc_header : webrtc_parameters->header_extensions) {
    headers.push_back(ToRtpHeaderExtensionParameters(webrtc_header));
  }
  parameters->setHeaderExtensions(headers);

  HeapVector<Member<RTCRtpCodecParameters>> codecs;
  codecs.reserve(
      base::checked_cast<wtf_size_t>(webrtc_parameters->codecs.size()));
  for (const auto& webrtc_codec : webrtc_parameters->codecs) {
    codecs.push_back(ToRtpCodecParameters(webrtc_codec));
  }
  parameters->setCodecs(codecs);

  return parameters;
}

void RTCRtpReceiver::RegisterEncodedAudioStreamCallback() {
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.
  encoded_audio_transformer_->SetTransformerCallback(
      WTF::CrossThreadBindRepeating(
          &RTCRtpReceiver::OnAudioFrameFromDepacketizer,
          WrapCrossThreadWeakPersistent(this)));
}

void RTCRtpReceiver::UnregisterEncodedAudioStreamCallback() {
  // Threadsafe as this might be called from the realm to which a stream has
  // been transferred.
  encoded_audio_transformer_->ResetTransformerCallback();
}

void RTCRtpReceiver::SetAudioUnderlyingSource(
    RTCEncodedAudioUnderlyingSource* new_underlying_source,
    scoped_refptr<base::SingleThreadTaskRunner> new_source_task_runner) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpReceiver, underlying
    // source(s), and transformer are about to be garbage collected, so there's
    // no reason to continue.
    return;
  }
  {
    base::AutoLock locker(audio_underlying_source_lock_);
    audio_from_depacketizer_underlying_source_->OnSourceTransferStarted();
    audio_from_depacketizer_underlying_source_ = new_underlying_source;
    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_audio_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedAudioUnderlyingSource::OnFrameFromSource,
              audio_from_depacketizer_underlying_source_));
    }
  }

  encoded_audio_transformer_->SetSourceTaskRunner(
      std::move(new_source_task_runner));
}

void RTCRtpReceiver::SetAudioUnderlyingSink(
    RTCEncodedAudioUnderlyingSink* new_underlying_sink) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpReceiver and underlying
    // sink(s) are about to be garbage collected, so there's no reason to
    // continue.
    return;
  }
  base::AutoLock locker(audio_underlying_sink_lock_);
  audio_to_decoder_underlying_sink_ = new_underlying_sink;
}

RTCInsertableStreams* RTCRtpReceiver::CreateEncodedAudioStreams(
    ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!encoded_streams_);

  encoded_streams_ = RTCInsertableStreams::Create();

  {
    base::AutoLock locker(audio_underlying_source_lock_);
    DCHECK(!audio_from_depacketizer_underlying_source_);

    // Set up readable.
    audio_from_depacketizer_underlying_source_ =
        MakeGarbageCollected<RTCEncodedAudioUnderlyingSource>(
            script_state,
            WTF::CrossThreadBindOnce(
                &RTCRtpReceiver::UnregisterEncodedAudioStreamCallback,
                WrapCrossThreadWeakPersistent(this)));

    auto set_underlying_source =
        WTF::CrossThreadBindRepeating(&RTCRtpReceiver::SetAudioUnderlyingSource,
                                      WrapCrossThreadWeakPersistent(this));
    auto disconnect_callback = WTF::CrossThreadBindOnce(
        &RTCRtpReceiver::UnregisterEncodedAudioStreamCallback,
        WrapCrossThreadWeakPersistent(this));
    // The high water mark for the readable stream is set to 0 so that frames
    // are removed from the queue right away, without introducing a new buffer.
    ReadableStream* readable_stream =
        ReadableStream::CreateWithCountQueueingStrategy(
            script_state, audio_from_depacketizer_underlying_source_,
            /*high_water_mark=*/0, AllowPerChunkTransferring(false),
            std::make_unique<RtcEncodedAudioReceiverSourceOptimizer>(
                std::move(set_underlying_source),
                std::move(disconnect_callback)));
    encoded_streams_->setReadable(readable_stream);

    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_audio_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedAudioUnderlyingSource::OnFrameFromSource,
              audio_from_depacketizer_underlying_source_));
    }
  }

  WritableStream* writable_stream;
  {
    base::AutoLock locker(audio_underlying_sink_lock_);
    DCHECK(!audio_to_decoder_underlying_sink_);

    // Set up writable.
    audio_to_decoder_underlying_sink_ =
        MakeGarbageCollected<RTCEncodedAudioUnderlyingSink>(
            script_state, encoded_audio_transformer_,
            /*detach_frame_data_on_write=*/false);

    auto set_underlying_sink =
        WTF::CrossThreadBindOnce(&RTCRtpReceiver::SetAudioUnderlyingSink,
                                 WrapCrossThreadWeakPersistent(this));

    // The high water mark for the stream is set to 1 so that the stream seems
    // ready to write, but without queuing frames.
    writable_stream = WritableStream::CreateWithCountQueueingStrategy(
        script_state, audio_to_decoder_underlying_sink_,
        /*high_water_mark=*/1,
        std::make_unique<RtcEncodedAudioReceiverSinkOptimizer>(
            std::move(set_underlying_sink), encoded_audio_transformer_));
  }

  encoded_streams_->setWritable(writable_stream);
  return encoded_streams_;
}

void RTCRtpReceiver::setTransform(RTCRtpScriptTransform* transform,
                                  ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (transform_ == transform) {
    return;
  }
  if (!transform) {
    transform_->Detach();
    transform_ = nullptr;
    return;
  }
  if (transform->IsAttached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Transform is already in use");
    return;
  }
  if (transform_) {
    transform_->Detach();
  }
  transform_ = transform;
  transform_->AttachToReceiver(this);

  if (kind() == MediaKind::kAudio) {
    transform_->CreateAudioUnderlyingSourceAndSink(
        WTF::CrossThreadBindOnce(
            &RTCRtpReceiver::UnregisterEncodedAudioStreamCallback,
            WrapCrossThreadWeakPersistent(this)),
        encoded_audio_transformer_);
    return;
  }
  CHECK(kind() == MediaKind::kVideo);
  transform_->CreateVideoUnderlyingSourceAndSink(
      WTF::CrossThreadBindOnce(
          &RTCRtpReceiver::UnregisterEncodedVideoStreamCallback,
          WrapCrossThreadWeakPersistent(this)),
      encoded_video_transformer_);
}

void RTCRtpReceiver::OnAudioFrameFromDepacketizer(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface>
        encoded_audio_frame) {
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));

  base::AutoLock locker(audio_underlying_source_lock_);
  if (audio_from_depacketizer_underlying_source_) {
    audio_from_depacketizer_underlying_source_->OnFrameFromSource(
        std::move(encoded_audio_frame));
  }
}

void RTCRtpReceiver::RegisterEncodedVideoStreamCallback() {
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.
  encoded_video_transformer_->SetTransformerCallback(
      WTF::CrossThreadBindRepeating(
          &RTCRtpReceiver::OnVideoFrameFromDepacketizer,
          WrapCrossThreadWeakPersistent(this)));
}

void RTCRtpReceiver::UnregisterEncodedVideoStreamCallback() {
  // Threadsafe as this might be called from the realm to which a stream has
  // been transferred.
  encoded_video_transformer_->ResetTransformerCallback();
}

void RTCRtpReceiver::SetVideoUnderlyingSource(
    RTCEncodedVideoUnderlyingSource* new_underlying_source,
    scoped_refptr<base::SingleThreadTaskRunner> new_source_task_runner) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpReceiver, underlying
    // source(s), and transformer are about to be garbage collected, so there's
    // no reason to continue.
    return;
  }
  {
    base::AutoLock locker(video_underlying_source_lock_);
    video_from_depacketizer_underlying_source_->OnSourceTransferStarted();
    video_from_depacketizer_underlying_source_ = new_underlying_source;
    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_video_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedVideoUnderlyingSource::OnFrameFromSource,
              video_from_depacketizer_underlying_source_));
    }
  }

  encoded_video_transformer_->SetSourceTaskRunner(
      std::move(new_source_task_runner));
}

void RTCRtpReceiver::SetVideoUnderlyingSink(
    RTCEncodedVideoUnderlyingSink* new_underlying_sink) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpReceiver and underlying
    // sink(s) are about to be garbage collected, so there's no reason to
    // continue.
    return;
  }
  base::AutoLock locker(video_underlying_sink_lock_);
  video_to_decoder_underlying_sink_ = new_underlying_sink;
}

void RTCRtpReceiver::MaybeShortCircuitEncodedStreams() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!encoded_streams_ && !transform_) {
    transform_shortcircuited_ = true;
    LogMessage("Starting short circuiting of transform");
    if (kind() == MediaKind::kVideo) {
      encoded_video_transformer_->StartShortCircuiting();
    } else {
      CHECK_EQ(kind(), MediaKind::kAudio);
      encoded_audio_transformer_->StartShortCircuiting();
    }
  }
}

RTCInsertableStreams* RTCRtpReceiver::CreateEncodedVideoStreams(
    ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!encoded_streams_);

  encoded_streams_ = RTCInsertableStreams::Create();

  {
    base::AutoLock locker(video_underlying_source_lock_);
    DCHECK(!video_from_depacketizer_underlying_source_);

    // Set up readable.
    video_from_depacketizer_underlying_source_ =
        MakeGarbageCollected<RTCEncodedVideoUnderlyingSource>(
            script_state,
            WTF::CrossThreadBindOnce(
                &RTCRtpReceiver::UnregisterEncodedVideoStreamCallback,
                WrapCrossThreadWeakPersistent(this)));

    auto set_underlying_source =
        WTF::CrossThreadBindRepeating(&RTCRtpReceiver::SetVideoUnderlyingSource,
                                      WrapCrossThreadWeakPersistent(this));
    auto disconnect_callback = WTF::CrossThreadBindOnce(
        &RTCRtpReceiver::UnregisterEncodedVideoStreamCallback,
        WrapCrossThreadWeakPersistent(this));
    // The high water mark for the readable stream is set to 0 so that frames
    // are removed from the queue right away, without introducing a new buffer.
    ReadableStream* readable_stream =
        ReadableStream::CreateWithCountQueueingStrategy(
            script_state, video_from_depacketizer_underlying_source_,
            /*high_water_mark=*/0, AllowPerChunkTransferring(false),
            std::make_unique<RtcEncodedVideoReceiverSourceOptimizer>(
                std::move(set_underlying_source),
                std::move(disconnect_callback)));
    encoded_streams_->setReadable(readable_stream);

    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback)) {
      encoded_video_transformer_->SetTransformerCallback(
          WTF::CrossThreadBindRepeating(
              &RTCEncodedVideoUnderlyingSource::OnFrameFromSource,
              video_from_depacketizer_underlying_source_));
    }
  }

  WritableStream* writable_stream;
  {
    base::AutoLock locker(video_underlying_sink_lock_);
    DCHECK(!video_to_decoder_underlying_sink_);

    // Set up writable.
    video_to_decoder_underlying_sink_ =
        MakeGarbageCollected<RTCEncodedVideoUnderlyingSink>(
            script_state, encoded_video_transformer_,
            /*detach_frame_data_on_write=*/false);

    auto set_underlying_sink =
        WTF::CrossThreadBindOnce(&RTCRtpReceiver::SetVideoUnderlyingSink,
                                 WrapCrossThreadWeakPersistent(this));

    // The high water mark for the stream is set to 1 so that the stream seems
    // ready to write, but without queuing frames.
    writable_stream = WritableStream::CreateWithCountQueueingStrategy(
        script_state, video_to_decoder_underlying_sink_,
        /*high_water_mark=*/1,
        std::make_unique<RtcEncodedVideoReceiverSinkOptimizer>(
            std::move(set_underlying_sink), encoded_video_transformer_));
  }

  encoded_streams_->setWritable(writable_stream);
  return encoded_streams_;
}

void RTCRtpReceiver::OnVideoFrameFromDepacketizer(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface>
        encoded_video_frame) {
  // TODO(crbug.com/347915599): Delete this method once
  // kWebRtcEncodedTransformDirectCallback is fully launched.
  CHECK(!base::FeatureList::IsEnabled(kWebRtcEncodedTransformDirectCallback));

  base::AutoLock locker(video_underlying_source_lock_);
  if (video_from_depacketizer_underlying_source_) {
    video_from_depacketizer_underlying_source_->OnFrameFromSource(
        std::move(encoded_video_frame));
  }
}

void RTCRtpReceiver::LogMessage(const std::string& message) {
  blink::WebRtcLogMessage(
      base::StringPrintf("RtpRcvr::%s [this=0x%" PRIXPTR "]", message.c_str(),
                         reinterpret_cast<uintptr_t>(this)));
}

}  // namespace blink
