// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_FUZZER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_FUZZER_UTILS_H_

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <string>

namespace blink {

class FakeFunction : public ScriptFunction {
 public:
  static FakeFunction* Create(ScriptState* script_state, std::string name);

  explicit FakeFunction(ScriptState* script_state, std::string name);

  v8::Local<v8::Function> Bind();
  ScriptValue Call(ScriptValue) override;

 private:
  const std::string name_;
};

VideoDecoderConfig* MakeVideoDecoderConfig(
    const wc_fuzzer::ConfigureVideoDecoder& proto);

EncodedAudioConfig* MakeAudioDecoderConfig(
    const wc_fuzzer::ConfigureAudioDecoder& proto);

VideoEncoderConfig* MakeEncoderConfig(
    const wc_fuzzer::ConfigureVideoEncoder& proto);

EncodedVideoChunk* MakeEncodedVideoChunk(
    const wc_fuzzer::EncodedVideoChunk& proto);

EncodedAudioChunk* MakeEncodedAudioChunk(
    const wc_fuzzer::EncodedAudioChunk& proto);

VideoFrame* MakeVideoFrame(const wc_fuzzer::VideoFrameBitmapInit& proto);

VideoEncoderEncodeOptions* MakeEncodeOptions(
    const wc_fuzzer::EncodeVideo_EncodeOptions& proto);

String ToAccelerationType(
    wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference type);

String ToChunkType(wc_fuzzer::EncodedChunkType type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_FUZZER_UTILS_H_
