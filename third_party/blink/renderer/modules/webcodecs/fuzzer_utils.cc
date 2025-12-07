// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"

#include <algorithm>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "media/base/limits.h"
#include "media/base/sample_format.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_aac_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_opus_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_plane_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options_for_av_1.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options_for_vp_9.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_buffer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// 16 MiB ought to be enough for anybody.
constexpr size_t kMaxBufferLength = 16 * 1024 * 1024;

// Override for maximum frame dimensions to avoid huge allocations.
constexpr uint32_t kMaxVideoFrameDimension = 1024;

}  // namespace

base::ScopedClosureRunner MakeScopedGarbageCollectionRequest(
    v8::Isolate* isolate) {
  return base::ScopedClosureRunner(BindOnce(
      [](v8::Isolate* isolate) {
        // Request a V8 GC. Oilpan will be invoked by the GC epilogue.
        //
        // Multiple GCs may be required to ensure everything is collected (due
        // to a chain of persistent handles), so some objects may not be
        // collected until a subsequent iteration. This is slow enough as is, so
        // we compromise on one major GC, as opposed to the 5 used in
        // V8GCController for unit tests.
        isolate->RequestGarbageCollectionForTesting(
            v8::Isolate::kFullGarbageCollection);
      },
      Unretained(isolate)));
}

FakeFunction::FakeFunction(std::string name) : name_(std::move(name)) {}

ScriptValue FakeFunction::Call(ScriptState*, ScriptValue) {
  return ScriptValue();
}

VideoDecoderConfig* MakeVideoDecoderConfig(
    const wc_fuzzer::ConfigureVideoDecoder& proto) {
  auto* config = VideoDecoderConfig::Create();
  config->setCodec(proto.codec().c_str());
  DOMArrayBuffer* data_copy =
      DOMArrayBuffer::Create(base::as_byte_span(proto.description()));
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

  DOMArrayBuffer* data_copy =
      DOMArrayBuffer::Create(base::as_byte_span(proto.description()));
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
  config->setWidth(std::min(proto.width(), kMaxVideoFrameDimension));
  config->setHeight(std::min(proto.height(), kMaxVideoFrameDimension));
  config->setDisplayWidth(proto.display_width());
  config->setDisplayHeight(proto.display_height());

  if (proto.has_alpha()) {
    config->setAlpha(ToAlphaOption(proto.alpha()));
  }
  if (proto.has_bitrate_mode()) {
    config->setBitrateMode(ToBitrateMode(proto.bitrate_mode()));
  }
  if (proto.has_scalability_mode()) {
    config->setScalabilityMode(ToScalabilityMode(proto.scalability_mode()));
  }
  if (proto.has_latency_mode()) {
    config->setLatencyMode(ToLatencyMode(proto.latency_mode()));
  }

  if (proto.has_content_hint()) {
    config->setContentHint(ToContentHint(proto.content_hint()));
  }

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

  if (proto.has_bitrate_mode()) {
    config->setBitrateMode(ToBitrateMode(proto.bitrate_mode()));
  }

  if (proto.has_aac()) {
    auto* aac = AacEncoderConfig::Create();
    config->setAac(aac);
    if (proto.aac().has_format()) {
      aac->setFormat(ToAacFormat(proto.aac().format()));
    }
  }

  if (proto.has_opus()) {
    auto* opus = OpusEncoderConfig::Create();
    config->setOpus(opus);
    if (proto.opus().has_frame_duration()) {
      opus->setFrameDuration(proto.opus().frame_duration());
    }
    if (proto.opus().has_complexity()) {
      opus->setComplexity(proto.opus().complexity());
    }
    if (proto.opus().has_packetlossperc()) {
      opus->setPacketlossperc(proto.opus().packetlossperc());
    }
    if (proto.opus().has_useinbandfec()) {
      opus->setUseinbandfec(proto.opus().useinbandfec());
    }
    if (proto.opus().has_usedtx()) {
      opus->setUsedtx(proto.opus().usedtx());
    }
    if (proto.opus().has_signal()) {
      opus->setSignal(ToOpusSignal(proto.opus().signal()));
    }
    if (proto.opus().has_application()) {
      opus->setApplication(ToOpusApplication(proto.opus().application()));
    }
  }

  return config;
}

V8HardwarePreference::Enum ToAccelerationType(
    wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference type) {
  switch (type) {
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_ALLOW:
      return V8HardwarePreference::Enum::kNoPreference;
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_DENY:
      return V8HardwarePreference::Enum::kPreferSoftware;
    case wc_fuzzer::ConfigureVideoEncoder_EncoderAccelerationPreference_REQUIRE:
      return V8HardwarePreference::Enum::kPreferHardware;
  }
}

V8VideoEncoderBitrateMode::Enum ToBitrateMode(
    wc_fuzzer::ConfigureVideoEncoder_VideoEncoderBitrateMode mode) {
  switch (mode) {
    case wc_fuzzer::ConfigureVideoEncoder_VideoEncoderBitrateMode_CONSTANT:
      return V8VideoEncoderBitrateMode::Enum::kConstant;
    case wc_fuzzer::ConfigureVideoEncoder_VideoEncoderBitrateMode_VARIABLE:
      return V8VideoEncoderBitrateMode::Enum::kVariable;
    case wc_fuzzer::ConfigureVideoEncoder_VideoEncoderBitrateMode_QUANTIZER:
      return V8VideoEncoderBitrateMode::Enum::kQuantizer;
  }
}

String ToScalabilityMode(
    wc_fuzzer::ConfigureVideoEncoder_ScalabilityMode mode) {
  switch (mode) {
    case wc_fuzzer::ConfigureVideoEncoder_ScalabilityMode_L1T1:
      return "L1T1";
    case wc_fuzzer::ConfigureVideoEncoder_ScalabilityMode_L1T2:
      return "L1T2";
    case wc_fuzzer::ConfigureVideoEncoder_ScalabilityMode_L1T3:
      return "L1T3";
  }
}

V8LatencyMode::Enum ToLatencyMode(
    wc_fuzzer::ConfigureVideoEncoder_LatencyMode mode) {
  switch (mode) {
    case wc_fuzzer::ConfigureVideoEncoder_LatencyMode_QUALITY:
      return V8LatencyMode::Enum::kQuality;
    case wc_fuzzer::ConfigureVideoEncoder_LatencyMode_REALTIME:
      return V8LatencyMode::Enum::kRealtime;
  }
}

String ToContentHint(wc_fuzzer::ConfigureVideoEncoder_ContentHint hint) {
  switch (hint) {
    case wc_fuzzer::ConfigureVideoEncoder_ContentHint_NONE:
      return "";
    case wc_fuzzer::ConfigureVideoEncoder_ContentHint_TEXT:
      return "text";
    case wc_fuzzer::ConfigureVideoEncoder_ContentHint_MOTION:
      return "motion";
    case wc_fuzzer::ConfigureVideoEncoder_ContentHint_DETAIL:
      return "detail";
  }
}

V8AlphaOption::Enum ToAlphaOption(
    wc_fuzzer::ConfigureVideoEncoder_AlphaOption option) {
  switch (option) {
    case wc_fuzzer::ConfigureVideoEncoder_AlphaOption_KEEP:
      return V8AlphaOption::Enum::kKeep;
    case wc_fuzzer::ConfigureVideoEncoder_AlphaOption_DISCARD:
      return V8AlphaOption::Enum::kDiscard;
  }
}

V8AacBitstreamFormat::Enum ToAacFormat(wc_fuzzer::AacFormat format) {
  switch (format) {
    case wc_fuzzer::AAC:
      return V8AacBitstreamFormat::Enum::kAac;
    case wc_fuzzer::ADTS:
      return V8AacBitstreamFormat::Enum::kAdts;
  }
}

V8BitrateMode::Enum ToBitrateMode(wc_fuzzer::BitrateMode bitrate_mode) {
  switch (bitrate_mode) {
    case wc_fuzzer::VARIABLE:
      return V8BitrateMode::Enum::kVariable;
    case wc_fuzzer::CONSTANT:
      return V8BitrateMode::Enum::kConstant;
  }
}

V8OpusSignal::Enum ToOpusSignal(wc_fuzzer::OpusSignal opus_signal) {
  switch (opus_signal) {
    case wc_fuzzer::AUTO:
      return V8OpusSignal::Enum::kAuto;
    case wc_fuzzer::MUSIC:
      return V8OpusSignal::Enum::kMusic;
    case wc_fuzzer::VOICE:
      return V8OpusSignal::Enum::kVoice;
  }
}

V8OpusApplication::Enum ToOpusApplication(
    wc_fuzzer::OpusApplication opus_application) {
  switch (opus_application) {
    case wc_fuzzer::VOIP:
      return V8OpusApplication::Enum::kVoip;
    case wc_fuzzer::AUDIO:
      return V8OpusApplication::Enum::kAudio;
    case wc_fuzzer::LOWDELAY:
      return V8OpusApplication::Enum::kLowdelay;
  }
}

V8EncodedAudioChunkType::Enum ToAudioChunkType(
    wc_fuzzer::EncodedChunkType type) {
  switch (type) {
    case wc_fuzzer::EncodedChunkType::KEY:
      return V8EncodedAudioChunkType::Enum::kKey;
    case wc_fuzzer::EncodedChunkType::DELTA:
      return V8EncodedAudioChunkType::Enum::kDelta;
  }
}

V8EncodedVideoChunkType::Enum ToVideoChunkType(
    wc_fuzzer::EncodedChunkType type) {
  switch (type) {
    case wc_fuzzer::EncodedChunkType::KEY:
      return V8EncodedVideoChunkType::Enum::kKey;
    case wc_fuzzer::EncodedChunkType::DELTA:
      return V8EncodedVideoChunkType::Enum::kDelta;
  }
}

V8AudioSampleFormat::Enum ToAudioSampleFormat(
    wc_fuzzer::AudioSampleFormat format) {
  switch (format) {
    case wc_fuzzer::AudioSampleFormat::U8:
      return V8AudioSampleFormat::Enum::kU8;
    case wc_fuzzer::AudioSampleFormat::S16:
      return V8AudioSampleFormat::Enum::kS16;
    case wc_fuzzer::AudioSampleFormat::S32:
      return V8AudioSampleFormat::Enum::kS32;
    case wc_fuzzer::AudioSampleFormat::F32:
      return V8AudioSampleFormat::Enum::kF32;
    case wc_fuzzer::AudioSampleFormat::U8_PLANAR:
      return V8AudioSampleFormat::Enum::kU8Planar;
    case wc_fuzzer::AudioSampleFormat::S16_PLANAR:
      return V8AudioSampleFormat::Enum::kS16Planar;
    case wc_fuzzer::AudioSampleFormat::S32_PLANAR:
      return V8AudioSampleFormat::Enum::kS32Planar;
    case wc_fuzzer::AudioSampleFormat::F32_PLANAR:
      return V8AudioSampleFormat::Enum::kF32Planar;
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
    ScriptState* script_state,
    const wc_fuzzer::EncodedVideoChunk& proto) {
  auto* data = MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(base::as_byte_span(proto.data())));

  auto* init = EncodedVideoChunkInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setType(ToVideoChunkType(proto.type()));
  init->setData(data);

  if (proto.has_duration())
    init->setDuration(proto.duration());

  return EncodedVideoChunk::Create(script_state, init,
                                   IGNORE_EXCEPTION_FOR_TESTING);
}

EncodedAudioChunk* MakeEncodedAudioChunk(
    ScriptState* script_state,
    const wc_fuzzer::EncodedAudioChunk& proto) {
  auto* data = MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(base::as_byte_span(proto.data())));

  auto* init = EncodedAudioChunkInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setType(ToAudioChunkType(proto.type()));
  init->setData(data);

  if (proto.has_duration())
    init->setDuration(proto.duration());

  return EncodedAudioChunk::Create(script_state, init,
                                   IGNORE_EXCEPTION_FOR_TESTING);
}

VideoEncoderEncodeOptions* MakeEncodeOptions(
    const wc_fuzzer::EncodeVideo_EncodeOptions& proto) {
  VideoEncoderEncodeOptions* options = VideoEncoderEncodeOptions::Create();

  // Truly optional, so don't set it if its just a proto default value.
  if (proto.has_key_frame())
    options->setKeyFrame(proto.key_frame());

  if (proto.has_av1() && proto.av1().has_quantizer()) {
    auto* av1 = VideoEncoderEncodeOptionsForAv1::Create();
    av1->setQuantizer(proto.av1().quantizer());
    options->setAv1(av1);
  }

  if (proto.has_vp9() && proto.vp9().has_quantizer()) {
    auto* vp9 = VideoEncoderEncodeOptionsForVp9::Create();
    vp9->setQuantizer(proto.vp9().quantizer());
    options->setVp9(vp9);
  }

  return options;
}

BufferAndSource MakeAllowSharedBufferSource(
    const wc_fuzzer::AllowSharedBufferSource& proto) {
  BufferAndSource result = {};
  size_t length =
      std::min(static_cast<size_t>(proto.length()), kMaxBufferLength);

  DOMArrayBufferBase* buffer = nullptr;
  if (proto.shared()) {
    buffer = DOMSharedArrayBuffer::Create(static_cast<unsigned>(length), 1);
  } else {
    auto* array_buffer = DOMArrayBuffer::Create(length, 1);
    buffer = array_buffer;
    if (proto.transfer()) {
      result.buffer = array_buffer;
    }
  }
  DCHECK(buffer);

  size_t view_offset =
      std::min(static_cast<size_t>(proto.view_offset()), length);
  size_t view_length =
      std::min(static_cast<size_t>(proto.view_length()), length - view_offset);
  switch (proto.view_type()) {
    case wc_fuzzer::AllowSharedBufferSource_ViewType_NONE:
      result.source = MakeGarbageCollected<AllowSharedBufferSource>(buffer);
      break;
    case wc_fuzzer::AllowSharedBufferSource_ViewType_INT8:
      result.source = MakeGarbageCollected<AllowSharedBufferSource>(
          MaybeShared<DOMInt8Array>(
              DOMInt8Array::Create(buffer, view_offset, view_length)));
      break;
    case wc_fuzzer::AllowSharedBufferSource_ViewType_UINT32:
      // View must be element-aligned and is sized by element count.
      view_offset = std::min(view_offset, length / 4) * 4;
      view_length = std::min(view_length, length / 4 - view_offset / 4);
      result.source = MakeGarbageCollected<AllowSharedBufferSource>(
          MaybeShared<DOMUint32Array>(
              DOMUint32Array::Create(buffer, view_offset, view_length)));
      break;
    case wc_fuzzer::AllowSharedBufferSource_ViewType_DATA:
      result.source = MakeGarbageCollected<AllowSharedBufferSource>(
          MaybeShared<DOMDataView>(
              DOMDataView::Create(buffer, view_offset, view_length)));
  }

  return result;
}

PlaneLayout* MakePlaneLayout(const wc_fuzzer::PlaneLayout& proto) {
  PlaneLayout* plane_layout = PlaneLayout::Create();
  plane_layout->setOffset(proto.offset());
  plane_layout->setStride(proto.stride());
  return plane_layout;
}

DOMRectInit* MakeDOMRectInit(const wc_fuzzer::DOMRectInit& proto) {
  DOMRectInit* init = DOMRectInit::Create();
  init->setX(proto.x());
  init->setY(proto.y());
  init->setWidth(proto.width());
  init->setHeight(proto.height());
  return init;
}

VideoColorSpaceInit* MakeVideoColorSpaceInit(
    const wc_fuzzer::VideoColorSpaceInit& proto) {
  VideoColorSpaceInit* init = VideoColorSpaceInit::Create();

  if (proto.has_primaries()) {
    switch (proto.primaries()) {
      case wc_fuzzer::VideoColorSpaceInit_VideoColorPrimaries_VCP_BT709:
        init->setPrimaries(V8VideoColorPrimaries::Enum::kBt709);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoColorPrimaries_VCP_BT470BG:
        init->setPrimaries(V8VideoColorPrimaries::Enum::kBt470Bg);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoColorPrimaries_VCP_SMPTE170M:
        init->setPrimaries(V8VideoColorPrimaries::Enum::kSmpte170M);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoColorPrimaries_VCP_BT2020:
        init->setPrimaries(V8VideoColorPrimaries::Enum::kBt2020);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoColorPrimaries_VCP_SMPTE432:
        init->setPrimaries(V8VideoColorPrimaries::Enum::kSmpte432);
        break;
    }
  }

  if (proto.has_transfer()) {
    switch (proto.transfer()) {
      case wc_fuzzer::
          VideoColorSpaceInit_VideoTransferCharacteristics_VTC_BT709:
        init->setTransfer(V8VideoTransferCharacteristics::Enum::kBt709);
        break;
      case wc_fuzzer::
          VideoColorSpaceInit_VideoTransferCharacteristics_VTC_SMPTE170M:
        init->setTransfer(V8VideoTransferCharacteristics::Enum::kSmpte170M);
        break;
      case wc_fuzzer::
          VideoColorSpaceInit_VideoTransferCharacteristics_VTC_IEC61966_2_1:
        init->setTransfer(V8VideoTransferCharacteristics::Enum::kIec6196621);
        break;
      case wc_fuzzer::
          VideoColorSpaceInit_VideoTransferCharacteristics_VTC_LINEAR:
        init->setTransfer(V8VideoTransferCharacteristics::Enum::kLinear);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoTransferCharacteristics_VTC_PQ:
        init->setTransfer(V8VideoTransferCharacteristics::Enum::kPq);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoTransferCharacteristics_VTC_HLG:
        init->setTransfer(V8VideoTransferCharacteristics::Enum::kHlg);
        break;
    }
  }

  if (proto.has_matrix()) {
    switch (proto.matrix()) {
      case wc_fuzzer::VideoColorSpaceInit_VideoMatrixCoefficients_VMC_RGB:
        init->setMatrix(V8VideoMatrixCoefficients::Enum::kRgb);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoMatrixCoefficients_VMC_BT709:
        init->setMatrix(V8VideoMatrixCoefficients::Enum::kBt709);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoMatrixCoefficients_VMC_BT470BG:
        init->setMatrix(V8VideoMatrixCoefficients::Enum::kBt470Bg);
        break;
      case wc_fuzzer::VideoColorSpaceInit_VideoMatrixCoefficients_VMC_SMPTE170M:
        init->setMatrix(V8VideoMatrixCoefficients::Enum::kSmpte170M);
        break;
      case wc_fuzzer::
          VideoColorSpaceInit_VideoMatrixCoefficients_VMC_BT2020_NCL:
        init->setMatrix(V8VideoMatrixCoefficients::Enum::kBt2020Ncl);
        break;
    }
  }

  if (proto.has_full_range()) {
    init->setFullRange(proto.full_range());
  }

  return init;
}

VideoFrame* MakeVideoFrame(
    ScriptState* script_state,
    const wc_fuzzer::VideoFrameBufferInitInvocation& proto) {
  BufferAndSource data = MakeAllowSharedBufferSource(proto.data());
  VideoFrameBufferInit* init = VideoFrameBufferInit::Create();

  switch (proto.init().format()) {
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_I420:
      init->setFormat(V8VideoPixelFormat::Enum::kI420);
      break;
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_I420A:
      init->setFormat(V8VideoPixelFormat::Enum::kI420A);
      break;
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_I444:
      init->setFormat(V8VideoPixelFormat::Enum::kI444);
      break;
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_NV12:
      init->setFormat(V8VideoPixelFormat::Enum::kNV12);
      break;
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_RGBA:
      init->setFormat(V8VideoPixelFormat::Enum::kRGBA);
      break;
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_RGBX:
      init->setFormat(V8VideoPixelFormat::Enum::kRGBX);
      break;
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_BGRA:
      init->setFormat(V8VideoPixelFormat::Enum::kBGRA);
      break;
    case wc_fuzzer::VideoFrameBufferInit_VideoPixelFormat_BGRX:
      init->setFormat(V8VideoPixelFormat::Enum::kBGRX);
      break;
  }

  if (proto.init().layout_size()) {
    HeapVector<Member<PlaneLayout>> layout{};
    for (const auto& plane_proto : proto.init().layout())
      layout.push_back(MakePlaneLayout(plane_proto));
    init->setLayout(layout);
  }

  init->setTimestamp(proto.init().timestamp());
  if (proto.init().has_duration())
    init->setDuration(proto.init().duration());

  init->setCodedWidth(
      std::min(proto.init().coded_width(), kMaxVideoFrameDimension));
  init->setCodedHeight(
      std::min(proto.init().coded_height(), kMaxVideoFrameDimension));

  if (proto.init().has_visible_rect())
    init->setVisibleRect(MakeDOMRectInit(proto.init().visible_rect()));

  if (proto.init().has_display_width())
    init->setDisplayWidth(proto.init().display_width());
  if (proto.init().has_display_height())
    init->setDisplayHeight(proto.init().display_height());

  if (proto.init().has_color_space()) {
    init->setColorSpace(MakeVideoColorSpaceInit(proto.init().color_space()));
  }

  if (data.buffer) {
    HeapVector<Member<DOMArrayBuffer>> transfer;
    transfer.push_back(data.buffer);
    init->setTransfer(std::move(transfer));
  }

  return VideoFrame::Create(script_state, data.source, init,
                            IGNORE_EXCEPTION_FOR_TESTING);
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
      base::as_byte_span(proto.rgb_bitmap()).first(bitmap_size)));

  ImageData* image_data = ImageData::Create(data_u8, proto.bitmap_width(),
                                            IGNORE_EXCEPTION_FOR_TESTING);

  if (!image_data)
    return nullptr;

  ImageBitmap* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      image_data, std::nullopt, ImageBitmapOptions::Create());

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

  if (!proto.length() ||
      proto.length() > media::limits::kMaxSamplesPerPacket / 4) {
    return nullptr;
  }

  V8AudioSampleFormat format(ToAudioSampleFormat(proto.format()));

  int size_per_sample = SampleFormatToSampleSize(format);
  int number_of_samples = proto.channels().size() * proto.length();

  auto* buffer = DOMArrayBuffer::Create(number_of_samples, size_per_sample);

  UNSAFE_TODO(memset(buffer->Data(), 0, number_of_samples * size_per_sample));

  for (int i = 0; i < proto.channels().size(); i++) {
    size_t max_plane_size = proto.length() * size_per_sample;

    auto* data = proto.channels().Get(i).data();
    auto size = std::min(proto.channels().Get(i).size(), max_plane_size);

    void* plane_start = UNSAFE_TODO(reinterpret_cast<uint8_t*>(buffer->Data()) +
                                    i * max_plane_size);
    UNSAFE_TODO(memcpy(plane_start, data, size));
  }

  auto* init = AudioDataInit::Create();
  init->setTimestamp(proto.timestamp());
  init->setNumberOfFrames(proto.length());
  init->setNumberOfChannels(proto.channels().size());
  init->setSampleRate(proto.sample_rate());
  init->setFormat(format);
  init->setData(MakeGarbageCollected<AllowSharedBufferSource>(buffer));

  if (proto.transfer()) {
    HeapVector<Member<DOMArrayBuffer>> transfer;
    transfer.push_back(buffer);
    init->setTransfer(std::move(transfer));
  }

  return AudioData::Create(script_state, init, IGNORE_EXCEPTION_FOR_TESTING);
}

AudioDataCopyToOptions* MakeAudioDataCopyToOptions(
    const wc_fuzzer::AudioDataCopyToOptions& options_proto) {
  AudioDataCopyToOptions* options = AudioDataCopyToOptions::Create();
  options->setPlaneIndex(options_proto.plane_index());
  if (options_proto.has_frame_offset())
    options->setFrameOffset(options_proto.frame_offset());
  if (options_proto.has_frame_count())
    options->setFrameCount(options_proto.frame_count());
  if (options_proto.has_format())
    options->setFormat(ToAudioSampleFormat(options_proto.format()));
  return options;
}

}  // namespace blink
