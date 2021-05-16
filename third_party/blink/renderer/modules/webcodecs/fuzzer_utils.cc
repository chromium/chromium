// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"

#include <string>

#include "media/base/limits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

VideoDecoderConfig* MakeVideoDecoderConfig(
    const wc_fuzzer::ConfigureVideoDecoder& proto) {
  auto* config = VideoDecoderConfig::Create();
  config->setCodec(proto.codec().c_str());
  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(
      proto.description().data(), proto.description().size());
  config->setDescription(
      ArrayBufferOrArrayBufferView::FromArrayBuffer(data_copy));
  return config;
}

AudioDecoderConfig* MakeAudioDecoderConfig(
    const wc_fuzzer::ConfigureAudioDecoder& proto) {
  AudioDecoderConfig* config = AudioDecoderConfig::Create();
  config->setCodec(proto.codec().c_str());
  config->setSampleRate(proto.sample_rate());
  config->setNumberOfChannels(proto.number_of_channels());

  DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(
      proto.description().data(), proto.description().size());
  config->setDescription(
      ArrayBufferOrArrayBufferView::FromArrayBuffer(data_copy));

  return config;
}

VideoEncoderConfig* MakeVideoEncoderConfig(
    const wc_fuzzer::ConfigureVideoEncoder& proto) {
  VideoEncoderConfig* config = VideoEncoderConfig::Create();
  config->setCodec(proto.codec().c_str());
  config->setHardwareAcceleration(ToAccelerationType(proto.acceleration()));
  config->setFramerate(proto.framerate());
  config->setWidth(proto.width());
  config->setHeight(proto.height());

  // Bitrate is truly optional, so don't just take the proto default value.
  if (proto.has_bitrate())
    config->setBitrate(proto.bitrate());

  return config;
}

AudioEncoderConfig* MakeAudioEncoderConfig(
    const wc_fuzzer::ConfigureAudioEncoder& proto) {
  auto* config = AudioEncoderConfig::Create();
  config->setCodec(proto.codec().c_str());
  config->setBitrate(proto.bitrate());
  config->setNumberOfChannels(proto.number_of_channels());
  config->setSampleRate(proto.sample_rate());

  return config;
}

String ToAccelerationType(
    wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference type) {
  switch (type) {
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_ALLOW:
      return "allow";
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_DENY:
      return "deny";
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_REQUIRE:
      return "require";
  }
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
  ArrayBufferOrArrayBufferView data;
  data.SetArrayBuffer(
      DOMArrayBuffer::Create(proto.data().data(), proto.data().size()));

  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setType(ToChunkType(proto.type()));
  init->setDuration(proto.duration());
  init->setData(data);
  return EncodedVideoChunk::Create(init);
}

EncodedAudioChunk* MakeEncodedAudioChunk(
    const wc_fuzzer::EncodedAudioChunk& proto) {
  ArrayBufferOrArrayBufferView data;
  data.SetArrayBuffer(
      DOMArrayBuffer::Create(proto.data().data(), proto.data().size()));

  auto* init = EncodedAudioChunkInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setType(ToChunkType(proto.type()));
  init->setData(data);
  return EncodedAudioChunk::Create(init);
}

VideoEncoderEncodeOptions* MakeEncodeOptions(
    const wc_fuzzer::EncodeVideo_EncodeOptions& proto) {
  VideoEncoderEncodeOptions* options = VideoEncoderEncodeOptions::Create();

  // Truly optional, so don't set it if its just a proto default value.
  if (proto.has_key_frame())
    options->setKeyFrame(proto.key_frame());

  return options;
}

VideoFrame* MakeVideoFrame(ScriptState* script_state,
                           const wc_fuzzer::VideoFrameBitmapInit& proto) {
  constexpr size_t kBytesPerPixel = 4;
  auto bitmap_size = proto.rgb_bitmap().size();
  // ImageData::Create() rejects inputs if data size is not a multiple of
  // width * 4.
  // Round down bitmap size to width * 4, it makes more fuzzer inputs
  // acceptable and incresease fuzzing penetration.
  if (proto.bitmap_width() > 0 && proto.bitmap_width() < bitmap_size)
    bitmap_size -= bitmap_size % (proto.bitmap_width() * kBytesPerPixel);
  NotShared<DOMUint8ClampedArray> data_u8(DOMUint8ClampedArray::Create(
      reinterpret_cast<const unsigned char*>(proto.rgb_bitmap().data()),
      bitmap_size));

  ImageData* image_data = ImageData::Create(data_u8, proto.bitmap_width(),
                                            IGNORE_EXCEPTION_FOR_TESTING);

  if (!image_data)
    return nullptr;

  ImageBitmap* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      image_data, absl::nullopt, ImageBitmapOptions::Create());

  VideoFrameInit* video_frame_init = VideoFrameInit::Create();
  video_frame_init->setTimestamp(proto.timestamp());
  video_frame_init->setDuration(proto.duration());

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  auto* source = MakeGarbageCollected<V8CanvasImageSource>(image_bitmap);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CanvasImageSourceUnion source;
  source.SetImageBitmap(image_bitmap);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  return VideoFrame::Create(script_state, source, video_frame_init,
                            IGNORE_EXCEPTION_FOR_TESTING);
}

AudioData* MakeAudioData(ScriptState* script_state,
                         const wc_fuzzer::AudioDataInit& proto) {
  if (proto.channels().size() > media::limits::kMaxChannels)
    return nullptr;

  if (proto.length() > media::limits::kMaxSamplesPerPacket)
    return nullptr;

  auto bus = AudioBus::Create(proto.channels().size(), proto.length());
  if (!bus)
    return nullptr;
  for (int i = 0; i < proto.channels().size(); i++) {
    size_t max_size = proto.length() * sizeof(float);
    memset(bus->Channel(i)->MutableData(), 0, max_size);

    auto* data = proto.channels().Get(i).data();
    auto size = std::min(proto.channels().Get(i).size(), max_size);
    memcpy(bus->Channel(i)->MutableData(), data, size);
  }

  auto* init = AudioDataInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setBuffer(AudioBuffer::CreateFromAudioBus(bus.get()));

  return AudioData::Create(init, IGNORE_EXCEPTION_FOR_TESTING);
}

}  // namespace blink
