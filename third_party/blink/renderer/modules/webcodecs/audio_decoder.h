// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_H_

#include <stdint.h>
#include <memory>

#include "media/base/audio_decoder.h"
#include "media/base/status.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/modules/webcodecs/decoder_template.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace media {

class AudioBuffer;
class DecoderBuffer;
class MediaLog;

}  // namespace media

namespace blink {

class AudioDecoderConfig;
class AudioFrame;
class EncodedAudioChunk;
class ExceptionState;
class AudioDecoderInit;
class ScriptPromise;
class V8AudioFrameOutputCallback;

class MODULES_EXPORT AudioDecoderTraits {
 public:
  using InitType = AudioDecoderInit;
  using OutputType = AudioFrame;
  using MediaOutputType = media::AudioBuffer;
  using MediaDecoderType = media::AudioDecoder;
  using OutputCallbackType = V8AudioFrameOutputCallback;
  using ConfigType = AudioDecoderConfig;
  using MediaConfigType = media::AudioDecoderConfig;
  using InputType = EncodedAudioChunk;

  static constexpr bool kNeedsGpuFactories = false;

  static std::unique_ptr<MediaDecoderType> CreateDecoder(
      ExecutionContext& execution_context,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      media::MediaLog* media_log);
  static void InitializeDecoder(MediaDecoderType& decoder,
                                const MediaConfigType& media_config,
                                MediaDecoderType::InitCB init_cb,
                                MediaDecoderType::OutputCB output_cb);
  static int GetMaxDecodeRequests(const MediaDecoderType& decoder);
  static void UpdateDecoderLog(const MediaDecoderType& decoder,
                               const MediaConfigType& media_config,
                               media::MediaLog* media_log);
  static OutputType* MakeOutput(scoped_refptr<MediaOutputType>,
                                ExecutionContext*);
};

class MODULES_EXPORT AudioDecoder : public DecoderTemplate<AudioDecoderTraits> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioDecoder* Create(ScriptState*,
                              const AudioDecoderInit*,
                              ExceptionState&);

  static ScriptPromise isConfigSupported(ScriptState*,
                                         const AudioDecoderConfig*,
                                         ExceptionState&);

  // For use by MediaSource and by ::MakeMediaConfig.
  static CodecConfigEval MakeMediaAudioDecoderConfig(
      const ConfigType& config,
      MediaConfigType& out_media_config,
      String& out_console_message);

  AudioDecoder(ScriptState*, const AudioDecoderInit*, ExceptionState&);
  ~AudioDecoder() override = default;

 protected:
  CodecConfigEval MakeMediaConfig(const ConfigType& config,
                                  MediaConfigType* out_media_config,
                                  String* out_console_message) override;
  media::StatusOr<scoped_refptr<media::DecoderBuffer>> MakeDecoderBuffer(
      const InputType& chunk) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DECODER_H_
