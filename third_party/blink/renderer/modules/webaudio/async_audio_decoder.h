/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ASYNC_AUDIO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ASYNC_AUDIO_DECODER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decode_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decode_success_callback.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class AudioBuffer;
class AudioBus;
class BaseAudioContext;
class DOMArrayBuffer;
class ScriptPromiseResolver;

// AsyncAudioDecoder asynchronously decodes audio file data from a
// DOMArrayBuffer in the background thread. Upon successful decoding, a
// completion callback will be invoked with the decoded PCM data in an
// AudioBuffer.

class AsyncAudioDecoder {
  DISALLOW_NEW();

 public:
  AsyncAudioDecoder() = default;
  ~AsyncAudioDecoder() = default;

  // Must be called on the main thread.  |decodeAsync| and callees must not
  // modify any of the parameters except |audioData|.  They are used to
  // associate this decoding instance with the caller to process the decoding
  // appropriately when finished.
  void DecodeAsync(DOMArrayBuffer* audio_data,
                   float sample_rate,
                   V8DecodeSuccessCallback*,
                   V8DecodeErrorCallback*,
                   ScriptPromiseResolver*,
                   BaseAudioContext*);

 private:
  AudioBuffer* CreateAudioBufferFromAudioBus(AudioBus*);
  static void DecodeOnBackgroundThread(
      DOMArrayBuffer* audio_data,
      float sample_rate,
      V8DecodeSuccessCallback*,
      V8DecodeErrorCallback*,
      ScriptPromiseResolver*,
      BaseAudioContext*,
      scoped_refptr<base::SingleThreadTaskRunner>);
  static void NotifyComplete(DOMArrayBuffer* audio_data,
                             V8DecodeSuccessCallback*,
                             V8DecodeErrorCallback*,
                             AudioBus*,
                             ScriptPromiseResolver*,
                             BaseAudioContext*);

  DISALLOW_COPY_AND_ASSIGN(AsyncAudioDecoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_ASYNC_AUDIO_DECODER_H_
