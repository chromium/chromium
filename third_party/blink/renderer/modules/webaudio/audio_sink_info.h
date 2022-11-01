// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_SINK_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_SINK_INFO_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AudioSinkInfo : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioSinkInfo* Create(const String&);

  explicit AudioSinkInfo(const String&);
  ~AudioSinkInfo() override;

  String type() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_SINK_INFO_H_
