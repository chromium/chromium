// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_sink_info.h"

namespace blink {

AudioSinkInfo* AudioSinkInfo::Create(const String& type) {
  return MakeGarbageCollected<AudioSinkInfo>(type);
}

AudioSinkInfo::AudioSinkInfo(const String& type) {}

AudioSinkInfo::~AudioSinkInfo() = default;

String AudioSinkInfo::type() const {
  // Currently "none" is the only `type` available.
  return "none";
}

}  // namespace blink
