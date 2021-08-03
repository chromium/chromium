// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_

#include <memory>

#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_pool.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_output_callback.h"
#include "third_party/blink/renderer/modules/webcodecs/encoder_base.h"
#include "third_party/blink/renderer/modules/webcodecs/hardware_preference.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

namespace media {
class GpuVideoAcceleratorFactories;
class VideoEncoder;
struct VideoEncoderOutput;
}  // namespace media

namespace blink {

class VideoEncoderConfig;
class VideoEncoderInit;
class VideoEncoderEncodeOptions;
class WebGraphicsContext3DVideoFramePool;

class MODULES_EXPORT VideoEncoderTraits {
 public:
  struct ParsedConfig final : public GarbageCollected<ParsedConfig> {
    media::VideoCodec codec;
    media::VideoCodecProfile profile;
    uint8_t level;
    media::VideoColorSpace color_space;

    HardwarePreference hw_pref;

    media::VideoEncoder::Options options;
    String codec_string;
    absl::optional<gfx::Size> display_size;

    void Trace(Visitor*) const {}
  };

  using Init = VideoEncoderInit;
  using Config = VideoEncoderConfig;
  using InternalConfig = ParsedConfig;
  using Input = VideoFrame;
  using EncodeOptions = VideoEncoderEncodeOptions;
  using OutputChunk = EncodedVideoChunk;
  using OutputCallback = V8EncodedVideoChunkOutputCallback;
  using MediaEncoder = media::VideoEncoder;

  // Can't be a virtual method, because it's used from base ctor.
  static const char* GetNameForDevTools();
  static const char* GetName();
};

class MODULES_EXPORT VideoEncoder final
    : public EncoderBase<VideoEncoderTraits> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoEncoder* Create(ScriptState*,
                              const VideoEncoderInit*,
                              ExceptionState&);
  VideoEncoder(ScriptState*, const VideoEncoderInit*, ExceptionState&);
  ~VideoEncoder() override;

  static ScriptPromise isConfigSupported(ScriptState*,
                                         const VideoEncoderConfig*,
                                         ExceptionState&);

 private:
  using Base = EncoderBase<VideoEncoderTraits>;
  using ParsedConfig = VideoEncoderTraits::ParsedConfig;

  void CallOutputCallback(
      ParsedConfig* active_config,
      uint32_t reset_count,
      media::VideoEncoderOutput output,
      absl::optional<media::VideoEncoder::CodecDescription> codec_desc);
  void ProcessEncode(Request* request) override;
  void ProcessConfigure(Request* request) override;
  void ProcessReconfigure(Request* request) override;
  void ResetInternal() override;

  void UpdateEncoderLog(std::string encoder_name, bool is_hw_accelerated);


  ParsedConfig* ParseConfig(const VideoEncoderConfig*,
                            ExceptionState&) override;
  bool VerifyCodecSupport(ParsedConfig*, ExceptionState&) override;

  void ContinueConfigureWithGpuFactories(
      Request* request,
      media::GpuVideoAcceleratorFactories* gpu_factories);
  std::unique_ptr<media::VideoEncoder> CreateMediaVideoEncoder(
      const ParsedConfig& config,
      media::GpuVideoAcceleratorFactories* gpu_factories);
  bool CanReconfigure(ParsedConfig& original_config,
                      ParsedConfig& new_config) override;
  scoped_refptr<media::VideoFrame> ReadbackTextureBackedFrameToMemory(
      scoped_refptr<media::VideoFrame> txt_frame);

  media::VideoFramePool readback_frame_pool_;
  std::unique_ptr<WebGraphicsContext3DVideoFramePool> accelerated_frame_pool_;

  // The number of encoding requests currently handled by |media_encoder_|
  // Should not exceed |kMaxActiveEncodes|.
  int active_encodes_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
