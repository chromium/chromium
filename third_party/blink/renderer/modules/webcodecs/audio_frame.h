// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_H_

#include "media/base/audio_buffer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class AudioFrameInit;

class MODULES_EXPORT AudioFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioFrame* Create(AudioFrameInit*, ExceptionState&);

  // Internal constructor for creating from media::AudioDecoder output.
  explicit AudioFrame(scoped_refptr<media::AudioBuffer>);

  // audio_frame.idl implementation.
  explicit AudioFrame(AudioFrameInit*);
  void close();
  uint64_t timestamp() const;
  AudioBuffer* buffer() const;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 private:
  uint64_t timestamp_;
  Member<AudioBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_H_
