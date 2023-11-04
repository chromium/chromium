// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

class DOMArrayBuffer;

// This class wraps a WebRTC audio frame and allows making shallow
// copies. Its purpose is to support making RTCEncodedVideoFrames
// serializable in the same process.
class RTCEncodedAudioFrameDelegate
    : public WTF::ThreadSafeRefCounted<RTCEncodedAudioFrameDelegate> {
 public:
  explicit RTCEncodedAudioFrameDelegate(
      std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame,
      rtc::ArrayView<const unsigned int> contributing_sources,
      absl::optional<uint16_t> sequence_number);

  uint32_t RtpTimestamp() const;
  DOMArrayBuffer* CreateDataBuffer() const;
  void SetData(const DOMArrayBuffer* data);
  void SetRtpTimestamp(uint32_t timestamp, ExceptionState& exception_state);
  absl::optional<uint32_t> Ssrc() const;
  absl::optional<uint8_t> PayloadType() const;
  absl::optional<std::string> MimeType() const;
  absl::optional<uint16_t> SequenceNumber() const;
  Vector<uint32_t> ContributingSources() const;
  absl::optional<uint64_t> AbsCaptureTime() const;
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> PassWebRtcFrame();
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> CloneWebRtcFrame();

 private:
  mutable base::Lock lock_;
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame_
      GUARDED_BY(lock_);
  Vector<uint32_t> contributing_sources_ GUARDED_BY(lock_);
  absl::optional<uint16_t> sequence_number_ GUARDED_BY(lock_);
};

class MODULES_EXPORT RTCEncodedAudioFramesAttachment
    : public SerializedScriptValue::Attachment {
 public:
  static const void* kAttachmentKey;
  RTCEncodedAudioFramesAttachment() = default;
  ~RTCEncodedAudioFramesAttachment() override = default;

  bool IsLockedToAgentCluster() const override {
    return !encoded_audio_frames_.empty();
  }

  Vector<scoped_refptr<RTCEncodedAudioFrameDelegate>>& EncodedAudioFrames() {
    return encoded_audio_frames_;
  }

  const Vector<scoped_refptr<RTCEncodedAudioFrameDelegate>>&
  EncodedAudioFrames() const {
    return encoded_audio_frames_;
  }

 private:
  Vector<scoped_refptr<RTCEncodedAudioFrameDelegate>> encoded_audio_frames_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_DELEGATE_H_
