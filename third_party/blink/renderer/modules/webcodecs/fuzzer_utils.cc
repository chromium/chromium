// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <string>

namespace blink {

// static
FakeFunction* FakeFunction::Create(ScriptState* script_state,
                                   std::string name) {
  return MakeGarbageCollected<FakeFunction>(script_state, name);
}

FakeFunction::FakeFunction(ScriptState* script_state, std::string name)
    : ScriptFunction(script_state), name_(name) {}

v8::Local<v8::Function> FakeFunction::Bind() {
  return BindToV8Function();
}

ScriptValue FakeFunction::Call(ScriptValue) {
  return ScriptValue();
}

EncodedVideoConfig* MakeVideoDecoderConfig(
    const wc_fuzzer::ConfigureVideoDecoder& proto) {
  auto* config = EncodedVideoConfig::Create();
  config->setCodec(proto.codec().c_str());
  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(
      proto.description().data(), proto.description().size());
  config->setDescription(
      ArrayBufferOrArrayBufferView::FromArrayBuffer(data_copy));
  return config;
}

EncodedAudioConfig* MakeAudioDecoderConfig(
    const wc_fuzzer::ConfigureAudioDecoder& proto) {
  auto* config = EncodedAudioConfig::Create();
  config->setCodec(proto.codec().c_str());
  config->setSampleRate(proto.sample_rate());
  config->setNumberOfChannels(proto.number_of_channels());

  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(
      proto.description().data(), proto.description().size());
  config->setDescription(
      ArrayBufferOrArrayBufferView::FromArrayBuffer(data_copy));

  return config;
}

String ToChunkType(wc_fuzzer::EncodedChunkType type) {
  switch (type) {
    case wc_fuzzer::EncodedChunkType::KEY:
      return "key";
    case wc_fuzzer::EncodedChunkType::DELTA:
      return "delta";
  }
}

EncodedVideoChunk* MakeEncodedVideoChunk(
    const wc_fuzzer::EncodedVideoChunk& proto) {
  DOMArrayBuffer* data_copy =
      DOMArrayBuffer::Create(proto.data().data(), proto.data().size());

  return EncodedVideoChunk::Create(ToChunkType(proto.type()), proto.timestamp(),
                                   proto.duration(), data_copy);
}

EncodedAudioChunk* MakeEncodedAudioChunk(
    const wc_fuzzer::EncodedAudioChunk& proto) {
  DOMArrayBuffer* data_copy =
      DOMArrayBuffer::Create(proto.data().data(), proto.data().size());

  return EncodedAudioChunk::Create(ToChunkType(proto.type()), proto.timestamp(),
                                   data_copy);
}

}  // namespace blink
