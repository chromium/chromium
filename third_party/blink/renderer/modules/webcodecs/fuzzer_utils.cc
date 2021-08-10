// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"

#include <string>

#include "media/base/limits.h"
#include "media/base/sample_format.h"
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
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
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
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy));
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
      MakeGarbageCollected<AllowSharedBufferSource>(data_copy));

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
      return "no-preference";
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_DENY:
      return "prefer-software";
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_REQUIRE:
      return "prefer-hardware";
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

String ToAudioSampleFormat(wc_fuzzer::AudioSampleFormat format) {
  switch (format) {
    case wc_fuzzer::AudioSampleFormat::U8:
      return "u8";
    case wc_fuzzer::AudioSampleFormat::S16:
      return "s16";
    case wc_fuzzer::AudioSampleFormat::S32:
      return "s32";
    case wc_fuzzer::AudioSampleFormat::F32:
      return "f32";
    case wc_fuzzer::AudioSampleFormat::U8_PLANAR:
      return "u8-planar";
    case wc_fuzzer::AudioSampleFormat::S16_PLANAR:
      return "s16-planar";
    case wc_fuzzer::AudioSampleFormat::S32_PLANAR:
      return "s32-planar";
    case wc_fuzzer::AudioSampleFormat::F32_PLANAR:
      return "f32-planar";
  }
}

int SampleFormatToSampleSize(V8AudioSampleFormat format) {
  using FormatEnum = V8AudioSampleFormat::Enum;

  switch (format.AsEnum()) {
    case FormatEnum::kU8:
    case FormatEnum::kU8Planar:
      return 1;

    case FormatEnum::kS16:
    case FormatEnum::kS16Planar:
      return 2;

    case FormatEnum::kS32:
    case FormatEnum::kS32Planar:
    case FormatEnum::kF32:
    case FormatEnum::kF32Planar:
      return 4;
  }
}

EncodedVideoChunk* MakeEncodedVideoChunk(
    const wc_fuzzer::EncodedVideoChunk& proto) {
  auto* data = MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(proto.data().data(), proto.data().size()));

  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setType(ToChunkType(proto.type()));
  init->setData(data);

  if (proto.has_duration())
    init->setDuration(proto.duration());

  return EncodedVideoChunk::Create(init);
}

EncodedAudioChunk* MakeEncodedAudioChunk(
    const wc_fuzzer::EncodedAudioChunk& proto) {
  auto* data = MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(proto.data().data(), proto.data().size()));

  auto* init = EncodedAudioChunkInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setType(ToChunkType(proto.type()));
  init->setData(data);

  if (proto.has_duration())
    init->setDuration(proto.duration());

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

  auto* source = MakeGarbageCollected<V8CanvasImageSource>(image_bitmap);

  return VideoFrame::Create(script_state, source, video_frame_init,
                            IGNORE_EXCEPTION_FOR_TESTING);
}

AudioData* MakeAudioData(ScriptState* script_state,
                         const wc_fuzzer::AudioDataInit& proto) {
  if (!proto.channels().size() ||
      proto.channels().size() > media::limits::kMaxChannels)
    return nullptr;

  if (!proto.length() || proto.length() > media::limits::kMaxSamplesPerPacket)
    return nullptr;

  V8AudioSampleFormat format =
      V8AudioSampleFormat::Create(ToAudioSampleFormat(proto.format())).value();

  int size_per_sample = SampleFormatToSampleSize(format);
  int number_of_samples = proto.channels().size() * proto.length();

  auto* buffer = DOMArrayBuffer::Create(number_of_samples, size_per_sample);

  memset(buffer->Data(), 0, number_of_samples * size_per_sample);

  for (int i = 0; i < proto.channels().size(); i++) {
    size_t max_plane_size = proto.length() * size_per_sample;

    auto* data = proto.channels().Get(i).data();
    auto size = std::min(proto.channels().Get(i).size(), max_plane_size);

    void* plane_start =
        reinterpret_cast<uint8_t*>(buffer->Data()) + i * max_plane_size;
    memcpy(plane_start, data, size);
  }

  auto* init = AudioDataInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setNumberOfFrames(proto.length());
  init->setNumberOfChannels(proto.channels().size());
  init->setSampleRate(proto.sample_rate());
  init->setFormat(format);
  init->setData(MakeGarbageCollected<AllowSharedBufferSource>(buffer));

  return AudioData::Create(init, IGNORE_EXCEPTION_FOR_TESTING);
}

}  // namespace blink
