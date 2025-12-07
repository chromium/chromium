// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/units/timestamp.h"

namespace blink {

class DOMArrayBuffer;

// This class wraps a WebRTC audio frame and allows making shallow
// copies. Its purpose is to support making RTCEncodedVideoFrames
// serializable in the same process.
class RTCEncodedAudioFrameDelegate
    : public ThreadSafeRefCounted<RTCEncodedAudioFrameDelegate> {
 public:
  explicit RTCEncodedAudioFrameDelegate(
      std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame,
      webrtc::ArrayView<const unsigned int> contributing_sources,
      std::optional<uint16_t> sequence_number);

  uint32_t RtpTimestamp() const;
  DOMArrayBuffer* CreateDataBuffer(v8::Isolate* isolate) const;
  void SetData(const DOMArrayBuffer* data);

  // This method can only be called from the main thread.
  base::expected<void, String> SetWebRtcFrameMetadata(
      ExecutionContext*,
      const RTCEncodedAudioFrameMetadata*);

  std::optional<uint32_t> Ssrc() const;
  std::optional<uint8_t> PayloadType() const;
  std::optional<std::string> MimeType() const;
  std::optional<uint16_t> SequenceNumber() const;
  Vector<uint32_t> ContributingSources() const;
  std::optional<base::TimeTicks> ReceiveTime() const;
  std::optional<CaptureTimeInfo> CaptureTime() const;
  std::optional<base::TimeDelta> SenderCaptureTimeOffset() const;
  std::optional<double> AudioLevel() const;
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> PassWebRtcFrame();
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> CloneWebRtcFrame();

 private:
  std::optional<base::TimeTicks> ComputeReceiveTime() const
      EXCLUSIVE_LOCKS_REQUIRED(&lock_);
  std::optional<CaptureTimeInfo> ComputeCaptureTime() const
      EXCLUSIVE_LOCKS_REQUIRED(&lock_);
  std::optional<base::TimeDelta> ComputeSenderCaptureTimeOffset() const
      EXCLUSIVE_LOCKS_REQUIRED(&lock_);
  std::optional<double> ComputeAudioLevel() const
      EXCLUSIVE_LOCKS_REQUIRED(&lock_);

  mutable base::Lock lock_;
  std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame_
      GUARDED_BY(lock_);
  const Vector<uint32_t> contributing_sources_;
  const std::optional<uint16_t> sequence_number_;

  struct Metadata {
    std::optional<uint32_t> ssrc;
    std::optional<uint8_t> payload_type;
    std::optional<std::string> mime_type;
    std::optional<base::TimeTicks> receive_time;
    std::optional<CaptureTimeInfo> capture_time_info;
    std::optional<base::TimeDelta> sender_capture_time_offset;
    std::optional<double> audio_level;
    uint32_t rtp_timestamp = 0;
  };
  // This field is set after the frame is neutered (e.g., written to a stream or
  // transferred).
  Metadata post_neuter_metadata_ GUARDED_BY(lock_);
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
