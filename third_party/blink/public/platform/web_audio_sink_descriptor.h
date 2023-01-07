// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_AUDIO_SINK_DESCRIPTOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_AUDIO_SINK_DESCRIPTOR_H_

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

// This descriptor object must be created after proper validation of |sink_id|
// in AudioContext. This object is created by AudioContext in blink, and
// consumed by RendererWebAudioDeviceImpl in media. Note that this class does
// NOT do the vailidation of an identifier.
class WebAudioSinkDescriptor {
 public:
  enum AudioSinkType {
    // A sink type that produces actual sound via a physical audio device.
    kAudible,
    // A sink type that is driven by a fake audio device. (e.g. worker thread)
    kSilent,
    kLastValue
  };

  // For an "audible" sink with a user-selected identifier. The empty string
  // on |sink_id| means the system's default audio device.
  explicit WebAudioSinkDescriptor(const WebString& sink_id)
      : type_(kAudible), sink_id_(sink_id) {}

  // For a "silent" sink.
  explicit WebAudioSinkDescriptor() : type_(kSilent) {}

  AudioSinkType Type() const { return type_; }
  WebString SinkId() const {
    DCHECK_EQ(type_, kAudible);
    return sink_id_;
  }

 private:
  AudioSinkType type_;
  WebString sink_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_AUDIO_SINK_DESCRIPTOR_H_
