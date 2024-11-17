// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_capability.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

V8RTCRtpTransceiverDirection::Enum TransceiverDirectionToEnum(
    const webrtc::RtpTransceiverDirection& direction) {
  switch (direction) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
      return V8RTCRtpTransceiverDirection::Enum::kSendrecv;
    case webrtc::RtpTransceiverDirection::kSendOnly:
      return V8RTCRtpTransceiverDirection::Enum::kSendonly;
    case webrtc::RtpTransceiverDirection::kRecvOnly:
      return V8RTCRtpTransceiverDirection::Enum::kRecvonly;
    case webrtc::RtpTransceiverDirection::kInactive:
      return V8RTCRtpTransceiverDirection::Enum::kInactive;
    case webrtc::RtpTransceiverDirection::kStopped:
      return V8RTCRtpTransceiverDirection::Enum::kStopped;
  }
  NOTREACHED();
}

std::optional<V8RTCRtpTransceiverDirection::Enum>
OptionalTransceiverDirectionToEnum(
    const std::optional<webrtc::RtpTransceiverDirection>& direction) {
  if (!direction) {
    return std::nullopt;
  }
  return TransceiverDirectionToEnum(*direction);
}

bool TransceiverDirectionFromEnum(
    V8RTCRtpTransceiverDirection::Enum direction,
    std::optional<webrtc::RtpTransceiverDirection>* direction_out) {
  switch (direction) {
    case V8RTCRtpTransceiverDirection::Enum::kSendrecv:
      *direction_out = webrtc::RtpTransceiverDirection::kSendRecv;
      return true;
    case V8RTCRtpTransceiverDirection::Enum::kSendonly:
      *direction_out = webrtc::RtpTransceiverDirection::kSendOnly;
      return true;
    case V8RTCRtpTransceiverDirection::Enum::kRecvonly:
      *direction_out = webrtc::RtpTransceiverDirection::kRecvOnly;
      return true;
    case V8RTCRtpTransceiverDirection::Enum::kInactive:
      *direction_out = webrtc::RtpTransceiverDirection::kInactive;
      return true;
    case V8RTCRtpTransceiverDirection::Enum::kStopped:
      return false;
  }
  NOTREACHED();
}

bool OptionalTransceiverDirectionFromEnumWithStopped(
    V8RTCRtpTransceiverDirection::Enum direction,
    std::optional<webrtc::RtpTransceiverDirection>* direction_out) {
  if (direction == V8RTCRtpTransceiverDirection::Enum::kStopped) {
    *direction_out = webrtc::RtpTransceiverDirection::kStopped;
    return true;
  }
  return TransceiverDirectionFromEnum(direction, direction_out);
}

}  // namespace

webrtc::RtpTransceiverInit ToRtpTransceiverInit(
    ExecutionContext* context,
    const RTCRtpTransceiverInit* init,
    const String& kind) {
  webrtc::RtpTransceiverInit webrtc_init;
  std::optional<webrtc::RtpTransceiverDirection> direction;
  if (init->hasDirection() &&
      TransceiverDirectionFromEnum(init->direction().AsEnum(), &direction) &&
      direction) {
    webrtc_init.direction = *direction;
  }
  DCHECK(init->hasStreams());
  for (const auto& stream : init->streams()) {
    webrtc_init.stream_ids.push_back(stream->id().Utf8());
  }
  DCHECK(init->hasSendEncodings());
  for (const auto& encoding : init->sendEncodings()) {
    webrtc_init.send_encodings.push_back(
        ToRtpEncodingParameters(context, encoding, kind));
  }
  return webrtc_init;
}

RTCRtpTransceiver::RTCRtpTransceiver(
    RTCPeerConnection* pc,
    std::unique_ptr<RTCRtpTransceiverPlatform> platform_transceiver,
    RTCRtpSender* sender,
    RTCRtpReceiver* receiver)
    : pc_(pc),
      platform_transceiver_(std::move(platform_transceiver)),
      sender_(sender),
      receiver_(receiver),
      fired_direction_(std::nullopt) {
  DCHECK(pc_);
  DCHECK(platform_transceiver_);
  DCHECK(sender_);
  DCHECK(receiver_);
  UpdateMembers();
  sender_->set_transceiver(this);
  receiver_->set_transceiver(this);
}

String RTCRtpTransceiver::mid() const {
  return mid_;
}

RTCRtpSender* RTCRtpTransceiver::sender() const {
  return sender_.Get();
}

RTCRtpReceiver* RTCRtpTransceiver::receiver() const {
  return receiver_.Get();
}

bool RTCRtpTransceiver::stopped() const {
  // Non-standard attribute reflecting being "stopping", whether or not we are
  // "stopped" per current_direction_.
  return direction_ == V8RTCRtpTransceiverDirection::Enum::kStopped;
}

V8RTCRtpTransceiverDirection RTCRtpTransceiver::direction() const {
  return V8RTCRtpTransceiverDirection(direction_);
}

void RTCRtpTransceiver::setDirection(
    const V8RTCRtpTransceiverDirection& direction,
    ExceptionState& exception_state) {
  std::optional<webrtc::RtpTransceiverDirection> webrtc_direction;
  if (!TransceiverDirectionFromEnum(direction.AsEnum(), &webrtc_direction) ||
      !webrtc_direction) {
    exception_state.ThrowTypeError("Invalid RTCRtpTransceiverDirection.");
    return;
  }
  if (pc_->IsClosed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The peer connection is closed.");
    return;
  }
  if (current_direction_ == V8RTCRtpTransceiverDirection::Enum::kStopped) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The transceiver is stopped.");
    return;
  }
  if (direction_ == V8RTCRtpTransceiverDirection::Enum::kStopped) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The transceiver is stopping.");
    return;
  }
  webrtc::RTCError error =
      platform_transceiver_->SetDirection(*webrtc_direction);
  if (!error.ok()) {
    ThrowExceptionFromRTCError(error, exception_state);
    return;
  }
  UpdateMembers();
}

std::optional<V8RTCRtpTransceiverDirection>
RTCRtpTransceiver::currentDirection() const {
  if (!current_direction_) {
    return std::nullopt;
  }
  return V8RTCRtpTransceiverDirection(current_direction_.value());
}

void RTCRtpTransceiver::UpdateMembers() {
  if (current_direction_ == V8RTCRtpTransceiverDirection::Enum::kStopped) {
    // No need to update, stopped is a permanent state. Also: on removal, the
    // state of `platform_transceiver_` becomes obsolete and may not reflect
    // being stopped, so let's not update the members anymore.
    return;
  }
  mid_ = platform_transceiver_->Mid();
  direction_ = TransceiverDirectionToEnum(platform_transceiver_->Direction());
  current_direction_ = OptionalTransceiverDirectionToEnum(
      platform_transceiver_->CurrentDirection());
  fired_direction_ = platform_transceiver_->FiredDirection();
}

void RTCRtpTransceiver::OnTransceiverStopped() {
  receiver_->set_streams(MediaStreamVector());
  mid_ = String();
  direction_ =
      TransceiverDirectionToEnum(webrtc::RtpTransceiverDirection::kStopped);
  current_direction_ =
      TransceiverDirectionToEnum(webrtc::RtpTransceiverDirection::kStopped);
  fired_direction_ = webrtc::RtpTransceiverDirection::kStopped;
}

RTCRtpTransceiverPlatform* RTCRtpTransceiver::platform_transceiver() const {
  return platform_transceiver_.get();
}

std::optional<webrtc::RtpTransceiverDirection>
RTCRtpTransceiver::fired_direction() const {
  return fired_direction_;
}

bool RTCRtpTransceiver::DirectionHasSend() const {
  auto direction = platform_transceiver_->Direction();
  return direction == webrtc::RtpTransceiverDirection::kSendRecv ||
         direction == webrtc::RtpTransceiverDirection::kSendOnly;
}

bool RTCRtpTransceiver::DirectionHasRecv() const {
  auto direction = platform_transceiver_->Direction();
  return direction == webrtc::RtpTransceiverDirection::kSendRecv ||
         direction == webrtc::RtpTransceiverDirection::kRecvOnly;
}

bool RTCRtpTransceiver::FiredDirectionHasRecv() const {
  return fired_direction_ &&
         (*fired_direction_ == webrtc::RtpTransceiverDirection::kSendRecv ||
          *fired_direction_ == webrtc::RtpTransceiverDirection::kRecvOnly);
}

void RTCRtpTransceiver::stop(ExceptionState& exception_state) {
  if (pc_->IsClosed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The peer connection is closed.");
    return;
  }
  webrtc::RTCError error = platform_transceiver_->Stop();
  if (!error.ok()) {
    ThrowExceptionFromRTCError(error, exception_state);
    return;
  }
  // We should become stopping, but negotiation is needed to become stopped.
  UpdateMembers();
}

void RTCRtpTransceiver::setCodecPreferences(
    const HeapVector<Member<RTCRtpCodecCapability>>& codecs,
    ExceptionState& exception_state) {
  Vector<webrtc::RtpCodecCapability> codec_preferences;
  codec_preferences.reserve(codecs.size());
  for (const auto& codec : codecs) {
    codec_preferences.emplace_back();
    auto& webrtc_codec = codec_preferences.back();
    auto slash_position = codec->mimeType().find('/');
    if (slash_position == WTF::kNotFound) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError, "Invalid codec");
      return;
    }
    auto type = codec->mimeType().Left(slash_position);
    if (type == "video") {
      webrtc_codec.kind = cricket::MEDIA_TYPE_VIDEO;
    } else if (type == "audio") {
      webrtc_codec.kind = cricket::MEDIA_TYPE_AUDIO;
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError, "Invalid codec");
      return;
    }
    webrtc_codec.name = codec->mimeType().Substring(slash_position + 1).Ascii();
    webrtc_codec.clock_rate = codec->clockRate();
    if (codec->hasChannels()) {
      webrtc_codec.num_channels = codec->channels();
    }
    if (codec->hasSdpFmtpLine()) {
      auto sdpFmtpLine = codec->sdpFmtpLine();
      if (sdpFmtpLine.find('=') == WTF::kNotFound) {
        // Some parameters don't follow the key=value form.
        webrtc_codec.parameters.emplace("", sdpFmtpLine.Ascii());
      } else {
        WTF::Vector<WTF::String> parameters;
        sdpFmtpLine.Split(';', parameters);
        for (const auto& parameter : parameters) {
          auto equal_position = parameter.find('=');
          if (equal_position == WTF::kNotFound) {
            exception_state.ThrowDOMException(
                DOMExceptionCode::kInvalidModificationError, "Invalid codec");
            return;
          }
          auto parameter_name = parameter.Left(equal_position);
          auto parameter_value = parameter.Substring(equal_position + 1);
          webrtc_codec.parameters.emplace(parameter_name.Ascii(),
                                          parameter_value.Ascii());
        }
      }
    }
  }
  auto result = platform_transceiver_->SetCodecPreferences(codec_preferences);
  if (!result.ok()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError, result.message());
  }
}

void RTCRtpTransceiver::setHeaderExtensionsToNegotiate(
    const HeapVector<Member<RTCRtpHeaderExtensionCapability>>& extensions,
    ExceptionState& exception_state) {
  Vector<webrtc::RtpHeaderExtensionCapability> webrtc_hdr_exts;
  auto webrtc_offered_exts =
      platform_transceiver_->GetHeaderExtensionsToNegotiate();
  int id = 1;
  for (const auto& hdr_ext : extensions) {
    // Handle invalid requests for mandatory extensions as per
    // https://w3c.github.io/webrtc-extensions/#rtcrtptransceiver-interface
    // Step 2.1 (not handled on the WebRTC level).
    if (hdr_ext->uri().empty()) {
      exception_state.ThrowTypeError("The extension URL cannot be empty.");
      return;
    }

    std::optional<webrtc::RtpTransceiverDirection> direction;
    if (!OptionalTransceiverDirectionFromEnumWithStopped(
            hdr_ext->direction().AsEnum(), &direction) ||
        !direction) {
      exception_state.ThrowTypeError("Invalid RTCRtpTransceiverDirection.");
      return;
    }
    const int id_to_store = direction ? id++ : 0;
    webrtc_hdr_exts.emplace_back(hdr_ext->uri().Ascii(), id_to_store,
                                 *direction);
  }
  webrtc::RTCError status =
      platform_transceiver_->SetHeaderExtensionsToNegotiate(
          std::move(webrtc_hdr_exts));
  if (status.type() == webrtc::RTCErrorType::UNSUPPORTED_PARAMETER) {
    // TODO(crbug.com/1051821): support DOMExceptionCode::kNotSupportedError in
    // rtc_error_util.h/cc and get rid of this manually handled case.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      status.message());
    return;
  } else if (status.type() != webrtc::RTCErrorType::NONE) {
    ThrowExceptionFromRTCError(status, exception_state);
    return;
  }
}

HeapVector<Member<RTCRtpHeaderExtensionCapability>>
RTCRtpTransceiver::getHeaderExtensionsToNegotiate() const {
  auto webrtc_exts = platform_transceiver_->GetHeaderExtensionsToNegotiate();
  HeapVector<Member<RTCRtpHeaderExtensionCapability>> exts;
  for (const auto& webrtc_ext : webrtc_exts) {
    auto* ext = MakeGarbageCollected<RTCRtpHeaderExtensionCapability>();
    ext->setDirection(TransceiverDirectionToEnum(webrtc_ext.direction));
    ext->setUri(webrtc_ext.uri.c_str());
    exts.push_back(ext);
  }
  return exts;
}

HeapVector<Member<RTCRtpHeaderExtensionCapability>>
RTCRtpTransceiver::getNegotiatedHeaderExtensions() const {
  auto webrtc_exts = platform_transceiver_->GetNegotiatedHeaderExtensions();
  HeapVector<Member<RTCRtpHeaderExtensionCapability>> exts;
  for (const auto& webrtc_ext : webrtc_exts) {
    auto* ext = MakeGarbageCollected<RTCRtpHeaderExtensionCapability>();
    ext->setDirection(TransceiverDirectionToEnum(webrtc_ext.direction));
    ext->setUri(webrtc_ext.uri.c_str());
    exts.push_back(ext);
  }
  return exts;
}

void RTCRtpTransceiver::Trace(Visitor* visitor) const {
  visitor->Trace(pc_);
  visitor->Trace(sender_);
  visitor->Trace(receiver_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
