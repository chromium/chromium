// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_sink_info.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_sink_type.h"

namespace blink {

AudioSinkInfo* AudioSinkInfo::Create(const String& type) {
  return MakeGarbageCollected<AudioSinkInfo>(type);
}

AudioSinkInfo::AudioSinkInfo(const String& type) {}

AudioSinkInfo::~AudioSinkInfo() = default;

V8AudioSinkType AudioSinkInfo::type() const {
  // Currently "none" is the only `type` available.
  return V8AudioSinkType(V8AudioSinkType::Enum::kNone);
}

}  // namespace blink
