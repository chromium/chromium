// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ANSWER_OPTIONS_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ANSWER_OPTIONS_PLATFORM_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class RTCAnswerOptionsPlatform final
    : public GarbageCollected<RTCAnswerOptionsPlatform> {
 public:
  explicit RTCAnswerOptionsPlatform(bool voice_activity_detection)
      : voice_activity_detection_(voice_activity_detection) {}

  bool VoiceActivityDetection() const { return voice_activity_detection_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  bool voice_activity_detection_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ANSWER_OPTIONS_PLATFORM_H_
