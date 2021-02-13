// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_ENCODER_H_

#include <memory>

#include "media/base/audio_codecs.h"
#include "media/base/audio_encoder.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_codec_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/encoder_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class AudioEncoderConfig;
class AudioEncoderInit;

class MODULES_EXPORT AudioEncoderTraits {
 public:
  struct ParsedConfig final : public GarbageCollected<ParsedConfig> {
    media::AudioCodec codec = media::kUnknownAudioCodec;
    int channels = 0;
    uint64_t bitrate = 0;
    uint32_t sample_rate = 0;
    String codec_string;

    void Trace(Visitor*) const {}
  };

  struct AudioEncoderEncodeOptions
      : public GarbageCollected<AudioEncoderEncodeOptions> {
    void Trace(Visitor*) const {}
  };

  using Init = AudioEncoderInit;
  using Config = AudioEncoderConfig;
  using InternalConfig = ParsedConfig;
  using Frame = AudioFrame;
  using EncodeOptions = AudioEncoderEncodeOptions;
  using OutputChunk = EncodedAudioChunk;
  using OutputCallback = V8EncodedAudioChunkOutputCallback;
  using MediaEncoder = media::AudioEncoder;

  // Can't be a virtual method, because it's used from base ctor.
  static const char* GetNameForDevTools();
};

class MODULES_EXPORT AudioEncoder final
    : public EncoderBase<AudioEncoderTraits> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioEncoder* Create(ScriptState*,
                              const AudioEncoderInit*,
                              ExceptionState&);
  AudioEncoder(ScriptState*, const AudioEncoderInit*, ExceptionState&);
  ~AudioEncoder() override;

  void encode(AudioFrame* frame, ExceptionState& exception_state) {
    return Base::encode(frame, nullptr, exception_state);
  }

 private:
  using Base = EncoderBase<AudioEncoderTraits>;
  using ParsedConfig = AudioEncoderTraits::ParsedConfig;

  void ProcessEncode(Request* request) override;
  void ProcessConfigure(Request* request) override;
  void ProcessReconfigure(Request* request) override;
  void ProcessFlush(Request* request) override;

  ParsedConfig* ParseConfig(const AudioEncoderConfig* opts,
                            ExceptionState&) override;
  bool VerifyCodecSupport(ParsedConfig*, ExceptionState&) override;
  AudioFrame* CloneFrame(AudioFrame*, ExecutionContext*) override;

  bool CanReconfigure(ParsedConfig& original_config,
                      ParsedConfig& new_config) override;

  void CallOutputCallback(ParsedConfig* active_config,
                          uint32_t reset_count,
                          media::EncodedAudioBuffer encoded_buffer);

  bool produced_first_output_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_ENCODER_H_
