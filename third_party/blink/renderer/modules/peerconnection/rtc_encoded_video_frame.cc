// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_specifics_vp_8.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_decode_target_indication.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_metadata.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

namespace {

V8RTCDecodeTargetIndication
V8RTCDecodeTargetIndicationFromDecodeTargetIndication(
    webrtc::DecodeTargetIndication decode_target_indication) {
  switch (decode_target_indication) {
    case webrtc::DecodeTargetIndication::kNotPresent:
      return V8RTCDecodeTargetIndication(
          V8RTCDecodeTargetIndication::Enum::kNotPresent);
    case webrtc::DecodeTargetIndication::kDiscardable:
      return V8RTCDecodeTargetIndication(
          V8RTCDecodeTargetIndication::Enum::kDiscardable);
    case webrtc::DecodeTargetIndication::kSwitch:
      return V8RTCDecodeTargetIndication(
          V8RTCDecodeTargetIndication::Enum::kSwitch);
    case webrtc::DecodeTargetIndication::kRequired:
      return V8RTCDecodeTargetIndication(
          V8RTCDecodeTargetIndication::Enum::kRequired);
    default:
      NOTREACHED();
  }
}

webrtc::DecodeTargetIndication
DecodeTargetIndicationFromV8RTCDecodeTargetIndication(
    V8RTCDecodeTargetIndication decode_target_indication) {
  switch (decode_target_indication.AsEnum()) {
    case V8RTCDecodeTargetIndication::Enum::kNotPresent:
      return webrtc::DecodeTargetIndication::kNotPresent;
    case V8RTCDecodeTargetIndication::Enum::kDiscardable:
      return webrtc::DecodeTargetIndication::kDiscardable;
    case V8RTCDecodeTargetIndication::Enum::kSwitch:
      return webrtc::DecodeTargetIndication::kSwitch;
    case V8RTCDecodeTargetIndication::Enum::kRequired:
      return webrtc::DecodeTargetIndication::kRequired;
    default:
      NOTREACHED();
  }
}

String RTCVideoCodecTypeFromVideoCodecType(
    webrtc::VideoCodecType video_codec_type) {
  switch (video_codec_type) {
    case webrtc::VideoCodecType::kVideoCodecVP8:
      return "vp8";
    case webrtc::VideoCodecType::kVideoCodecVP9:
      return "vp9";
    case webrtc::VideoCodecType::kVideoCodecH264:
      return "h264";
    default:
      return "";
  }
}

webrtc::VideoCodecType VideoCodecTypeFromRTCVideoCodecType(
    const String& video_codec_type) {
  if (video_codec_type == "vp8") {
    return webrtc::VideoCodecType::kVideoCodecVP8;
  } else if (video_codec_type == "vp9") {
    return webrtc::VideoCodecType::kVideoCodecVP9;
  } else if (video_codec_type == "h264") {
    return webrtc::VideoCodecType::kVideoCodecH264;
  } else {
    NOTREACHED();
    return webrtc::VideoCodecType::kVideoCodecGeneric;
  }
}

String RTCEncodedVideoFrameTypeFromVideoFrameType(
    webrtc::VideoFrameType video_frame_type) {
  switch (video_frame_type) {
    case webrtc::VideoFrameType::kEmptyFrame:
      return "empty";
    case webrtc::VideoFrameType::kVideoFrameKey:
      return "key";
    case webrtc::VideoFrameType::kVideoFrameDelta:
      return "delta";
    default:
      NOTREACHED();
      return "";
  }
}

webrtc::VideoFrameType VideoFrameTypeFromRTCEncodedVideoFrameType(
    const String& video_frame_type) {
  if (video_frame_type == "empty") {
    return webrtc::VideoFrameType::kEmptyFrame;
  } else if (video_frame_type == "key") {
    return webrtc::VideoFrameType::kVideoFrameKey;
  } else if (video_frame_type == "delta") {
    return webrtc::VideoFrameType::kVideoFrameDelta;
  } else {
    NOTREACHED();
    return webrtc::VideoFrameType::kEmptyFrame;
  }
}

}  // namespace

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame)
    : delegate_(base::MakeRefCounted<RTCEncodedVideoFrameDelegate>(
          std::move(webrtc_frame))) {}

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    scoped_refptr<RTCEncodedVideoFrameDelegate> delegate)
    : delegate_(std::move(delegate)) {}

String RTCEncodedVideoFrame::type() const {
  return delegate_->Type();
}

uint32_t RTCEncodedVideoFrame::timestamp() const {
  return delegate_->Timestamp();
}

DOMArrayBuffer* RTCEncodedVideoFrame::data() const {
  if (!frame_data_)
    frame_data_ = delegate_->CreateDataBuffer();
  return frame_data_;
}

RTCEncodedVideoFrameMetadata* RTCEncodedVideoFrame::getMetadata() const {
  RTCEncodedVideoFrameMetadata* metadata =
      RTCEncodedVideoFrameMetadata::Create();
  if (delegate_->Ssrc()) {
    metadata->setSynchronizationSource(*delegate_->Ssrc());
  }
  if (delegate_->PayloadType()) {
    metadata->setPayloadType(*delegate_->PayloadType());
  }
  const auto* webrtc_metadata = delegate_->GetMetadata();
  if (!webrtc_metadata)
    return metadata;

  if (webrtc_metadata->GetFrameId())
    metadata->setFrameId(*webrtc_metadata->GetFrameId());

  Vector<int64_t> dependencies;
  for (const auto& dependency : webrtc_metadata->GetFrameDependencies())
    dependencies.push_back(dependency);
  metadata->setDependencies(dependencies);
  metadata->setWidth(webrtc_metadata->GetWidth());
  metadata->setHeight(webrtc_metadata->GetHeight());
  metadata->setSpatialIndex(webrtc_metadata->GetSpatialIndex());
  metadata->setTemporalIndex(webrtc_metadata->GetTemporalIndex());
  if (RuntimeEnabledFeatures::RTCEncodedVideoFrameAdditionalMetadataEnabled()) {
    Vector<V8RTCDecodeTargetIndication> decode_target_indications;
    for (const auto& decode_target_indication :
         webrtc_metadata->GetDecodeTargetIndications()) {
      decode_target_indications.push_back(
          V8RTCDecodeTargetIndicationFromDecodeTargetIndication(
              decode_target_indication));
    }
    metadata->setDecodeTargetIndications(decode_target_indications);
    metadata->setIsLastFrameInPicture(
        webrtc_metadata->GetIsLastFrameInPicture());
    metadata->setSimulcastIdx(webrtc_metadata->GetSimulcastIdx());
    String codec =
        RTCVideoCodecTypeFromVideoCodecType(webrtc_metadata->GetCodec());
    if (!codec.empty()) {
      metadata->setCodec(codec);
    } else {
      LOG(ERROR) << "Unrecognized RTCVideoCodecType.";
    }
    switch (webrtc_metadata->GetCodec()) {
      case webrtc::VideoCodecType::kVideoCodecVP8: {
        const webrtc::RTPVideoHeaderVP8& webrtc_vp8_specifics =
            absl::get<webrtc::RTPVideoHeaderVP8>(
                webrtc_metadata->GetRTPVideoHeaderCodecSpecifics());
        RTCCodecSpecificsVP8* vp8_specifics = RTCCodecSpecificsVP8::Create();
        vp8_specifics->setNonReference(webrtc_vp8_specifics.nonReference);
        vp8_specifics->setPictureId(webrtc_vp8_specifics.pictureId);
        vp8_specifics->setTl0PicIdx(webrtc_vp8_specifics.tl0PicIdx);
        vp8_specifics->setTemporalIdx(webrtc_vp8_specifics.temporalIdx);
        vp8_specifics->setLayerSync(webrtc_vp8_specifics.layerSync);
        vp8_specifics->setKeyIdx(webrtc_vp8_specifics.keyIdx);
        vp8_specifics->setPartitionId(webrtc_vp8_specifics.partitionId);
        vp8_specifics->setBeginningOfPartition(
            webrtc_vp8_specifics.beginningOfPartition);
        metadata->setCodecSpecifics(vp8_specifics);
        break;
      }
      default:
        // TODO(https://crbug.com/webrtc/14709): Support more codecs.
        LOG(ERROR) << "Unsupported RTCCodecSpecifics.";
        break;
    }

    metadata->setFrameType(RTCEncodedVideoFrameTypeFromVideoFrameType(
        webrtc_metadata->GetFrameType()));
  }
  return metadata;
}

void RTCEncodedVideoFrame::setMetadata(RTCEncodedVideoFrameMetadata* metadata,
                                       ExceptionState& exception_state) {
  if (!metadata->hasFrameId() || !metadata->hasDependencies() ||
      !metadata->hasWidth() || !metadata->hasHeight() ||
      !metadata->hasSpatialIndex() || !metadata->hasTemporalIndex() ||
      !metadata->hasDecodeTargetIndications() ||
      !metadata->hasIsLastFrameInPicture() || !metadata->hasSimulcastIdx() ||
      !metadata->hasCodec() || !metadata->hasCodecSpecifics()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Member(s) missing in RTCEncodedVideoFrameMetadata.");
    return;
  }
  // TODO(https://crbug.com/webrtc/14709): Also set RTP related metadata.
  webrtc::VideoFrameMetadata webrtc_metadata;
  webrtc_metadata.SetFrameId(metadata->frameId());
  webrtc_metadata.SetFrameDependencies(metadata->dependencies());
  webrtc_metadata.SetWidth(metadata->width());
  webrtc_metadata.SetHeight(metadata->height());
  webrtc_metadata.SetSpatialIndex(metadata->spatialIndex());
  webrtc_metadata.SetTemporalIndex(metadata->temporalIndex());
  std::vector<webrtc::DecodeTargetIndication> decode_target_indications;
  for (const auto& decode_target_indication :
       metadata->decodeTargetIndications()) {
    decode_target_indications.push_back(
        DecodeTargetIndicationFromV8RTCDecodeTargetIndication(
            decode_target_indication));
  }
  webrtc_metadata.SetDecodeTargetIndications(decode_target_indications);
  webrtc_metadata.SetIsLastFrameInPicture(metadata->isLastFrameInPicture());
  webrtc_metadata.SetSimulcastIdx(metadata->simulcastIdx());
  webrtc::VideoCodecType codec =
      VideoCodecTypeFromRTCVideoCodecType(metadata->codec());
  webrtc_metadata.SetFrameType(
      VideoFrameTypeFromRTCEncodedVideoFrameType(metadata->frameType()));

  webrtc_metadata.SetCodec(codec);
  switch (codec) {
    case webrtc::VideoCodecType::kVideoCodecVP8: {
      RTCCodecSpecificsVP8* vp8_specifics = metadata->codecSpecifics();
      if (!vp8_specifics->hasNonReference() || !vp8_specifics->hasPictureId() ||
          !vp8_specifics->hasTl0PicIdx() || !vp8_specifics->hasTemporalIdx() ||
          !vp8_specifics->hasLayerSync() || !vp8_specifics->hasKeyIdx() ||
          !vp8_specifics->hasPartitionId() ||
          !vp8_specifics->hasBeginningOfPartition()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidModificationError,
            "Member(s) missing in RTCCodecSpecificsVP8.");
      }
      webrtc::RTPVideoHeaderVP8 webrtc_vp8_specifics;
      webrtc_vp8_specifics.nonReference = vp8_specifics->nonReference();
      webrtc_vp8_specifics.pictureId = vp8_specifics->pictureId();
      webrtc_vp8_specifics.tl0PicIdx = vp8_specifics->tl0PicIdx();
      webrtc_vp8_specifics.temporalIdx = vp8_specifics->temporalIdx();
      webrtc_vp8_specifics.layerSync = vp8_specifics->layerSync();
      webrtc_vp8_specifics.keyIdx = vp8_specifics->keyIdx();
      webrtc_vp8_specifics.partitionId = vp8_specifics->partitionId();
      webrtc_vp8_specifics.beginningOfPartition =
          vp8_specifics->beginningOfPartition();
      webrtc_metadata.SetRTPVideoHeaderCodecSpecifics(webrtc_vp8_specifics);
      break;
    }
    default:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "setMetadata() does not support this codec.");
      return;
  }
  delegate_->SetMetadata(webrtc_metadata);
}

void RTCEncodedVideoFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedVideoFrame::toString() const {
  if (!delegate_)
    return "empty";

  StringBuilder sb;
  sb.Append("RTCEncodedVideoFrame{rtpTimestamp: ");
  sb.AppendNumber(timestamp());
  sb.Append(", size: ");
  sb.AppendNumber(data()->ByteLength());
  sb.Append(" bytes, type: ");
  sb.Append(type());
  sb.Append("}");
  return sb.ToString();
}

RTCEncodedVideoFrame* RTCEncodedVideoFrame::clone() const {
  std::unique_ptr<webrtc::TransformableVideoFrameInterface> new_webrtc_frame =
      delegate_->CloneWebRtcFrame();
  return MakeGarbageCollected<RTCEncodedVideoFrame>(
      std::move(new_webrtc_frame));
}

void RTCEncodedVideoFrame::SyncDelegate() const {
  delegate_->SetData(frame_data_);
}

scoped_refptr<RTCEncodedVideoFrameDelegate> RTCEncodedVideoFrame::Delegate()
    const {
  SyncDelegate();
  return delegate_;
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrame::PassWebRtcFrame() {
  SyncDelegate();
  return delegate_->PassWebRtcFrame();
}

void RTCEncodedVideoFrame::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
