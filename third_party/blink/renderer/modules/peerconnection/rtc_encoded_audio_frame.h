// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_H_

#include <stdint.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace webrtc {
class TransformableAudioFrameInterface;
}  // namespace webrtc

namespace blink {

class DOMArrayBuffer;
class RTCEncodedAudioFrameDelegate;
class RTCEncodedAudioFrameMetadata;

class MODULES_EXPORT RTCEncodedAudioFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit RTCEncodedAudioFrame(
      std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame);
  explicit RTCEncodedAudioFrame(
      scoped_refptr<RTCEncodedAudioFrameDelegate> delegate);

  // rtc_encoded_audio_frame.idl implementation.
  // Returns the RTP Packet Timestamp for this frame.
  uint32_t timestamp() const;
  absl::optional<uint16_t> sequenceNumber() const;
  DOMArrayBuffer* data() const;
  RTCEncodedAudioFrameMetadata* getMetadata() const;
  void setMetadata(RTCEncodedAudioFrameMetadata* metadata,
                   ExceptionState& exception_state);
  void setData(DOMArrayBuffer*);
  void setTimestamp(uint32_t timestamp, ExceptionState& exception_state);
  String toString() const;

  scoped_refptr<RTCEncodedAudioFrameDelegate> Delegate() const;
  void SyncDelegate() const;

  // Returns and transfers ownership of the internal WebRTC frame
  // backing this RTCEncodedAudioFrame, neutering all RTCEncodedAudioFrames
  // backed by that internal WebRTC frame.
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> PassWebRtcFrame();

  // Check whether the current payload bytes is too large to send.
  // Always false if setData() hasn't been called.
  bool IsDataTooLarge();

  void Trace(Visitor*) const override;

 private:
  scoped_refptr<RTCEncodedAudioFrameDelegate> delegate_;
  Vector<uint32_t> contributing_sources_;
  mutable Member<DOMArrayBuffer> frame_data_;
  bool data_modified_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_H_
