// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_

#include "media/base/audio_buffer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class AudioDataInit;

class MODULES_EXPORT AudioData final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioData* Create(AudioDataInit*, ExceptionState&);

  // Internal constructor for creating from media::AudioDecoder output.
  explicit AudioData(scoped_refptr<media::AudioBuffer>);

  // audio_data.idl implementation.
  explicit AudioData(AudioDataInit*);

  // Creates a clone of |this|, taking on a new reference on |data_|. The cloned
  // frame will not be closed when |this| is, and its lifetime should be
  // independently managed.
  AudioData* clone(ExceptionState&);

  void close();
  int64_t timestamp() const;
  AudioBuffer* buffer();

  scoped_refptr<media::AudioBuffer> data() const { return data_; }

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 private:
  void CopyDataToBuffer();

  scoped_refptr<media::AudioBuffer> data_;

  int64_t timestamp_;
  Member<AudioBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_
