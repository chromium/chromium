// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_pool.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_output_callback.h"
#include "third_party/blink/renderer/modules/webcodecs/encoder_base.h"
#include "third_party/blink/renderer/modules/webcodecs/hardware_preference.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "ui/gfx/color_space.h"

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
class BackgroundReadback;

class MODULES_EXPORT VideoEncoderTraits {
 public:
  struct ParsedConfig final : public GarbageCollected<ParsedConfig> {
    media::VideoCodec codec;
    media::VideoCodecProfile profile;
    uint8_t level;

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
  static const char* GetName();
};

class MODULES_EXPORT VideoEncoder : public EncoderBase<VideoEncoderTraits> {
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

  // EventTarget interface
  const AtomicString& InterfaceName() const override;

  // ScriptWrappable override.
  bool HasPendingActivity() const override;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 protected:
  using Base = EncoderBase<VideoEncoderTraits>;
  using ParsedConfig = VideoEncoderTraits::ParsedConfig;

  void OnMediaEncoderInfoChanged(const media::VideoEncoderInfo& encoder_info);
  void CallOutputCallback(
      ParsedConfig* active_config,
      uint32_t reset_count,
      media::VideoEncoderOutput output,
      absl::optional<media::VideoEncoder::CodecDescription> codec_desc);
  bool ReadyToProcessNextRequest() override;
  void ProcessEncode(Request* request) override;
  void ProcessConfigure(Request* request) override;
  void ProcessReconfigure(Request* request) override;
  void ResetInternal() override;

  void OnEncodeDone(Request* request, media::EncoderStatus status);
  media::VideoEncoder::EncodeOptions CreateEncodeOptions(Request* request);
  // This will execute shortly after the async readback completes.
  void OnReadbackDone(Request* request,
                      scoped_refptr<media::VideoFrame> txt_frame,
                      media::VideoEncoder::EncoderStatusCB done_callback,
                      scoped_refptr<media::VideoFrame> result_frame);
  static std::unique_ptr<media::VideoEncoder> CreateSoftwareVideoEncoder(
      VideoEncoder* self,
      media::VideoCodec codec);

  ParsedConfig* ParseConfig(const VideoEncoderConfig*,
                            ExceptionState&) override;
  bool VerifyCodecSupport(ParsedConfig*, ExceptionState&) override;

  // Virtual for UTs.
  virtual std::unique_ptr<media::VideoEncoder> CreateMediaVideoEncoder(
      const ParsedConfig& config,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  void ContinueConfigureWithGpuFactories(
      Request* request,
      media::GpuVideoAcceleratorFactories* gpu_factories);
  std::unique_ptr<media::VideoEncoder> CreateAcceleratedVideoEncoder(
      media::VideoCodecProfile profile,
      const media::VideoEncoder::Options& options,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      HardwarePreference hw_pref);
  bool CanReconfigure(ParsedConfig& original_config,
                      ParsedConfig& new_config) override;

  using ReadbackDoneCallback =
      base::OnceCallback<void(scoped_refptr<media::VideoFrame>)>;
  bool StartReadback(scoped_refptr<media::VideoFrame> frame,
                     ReadbackDoneCallback result_cb);

  std::unique_ptr<WebGraphicsContext3DVideoFramePool> accelerated_frame_pool_;
  Member<BackgroundReadback> background_readback_;

  // True if an error occurs during frame pool usage.
  bool disable_accelerated_frame_pool_ = false;

  // The number of encoding requests currently handled by |media_encoder_|
  // Should not exceed |max_active_encodes_|.
  int active_encodes_ = 0;

  // The current upper limit on |active_encodes_|.
  int max_active_encodes_;

  // Per-frame metadata to be applied to outputs, linked by timestamp.
  struct FrameMetadata {
    base::TimeDelta duration;
  };
  base::flat_map<base::TimeDelta, FrameMetadata> frame_metadata_;

  // The color space corresponding to the last emitted output. Used to update
  // emitted VideoDecoderConfig when necessary.
  gfx::ColorSpace last_output_color_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_ENCODER_H_
