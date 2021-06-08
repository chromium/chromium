// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_

#include <stdint.h>
#include <memory>

#include "media/base/status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/media_buildflags.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_output_callback.h"
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

class VideoFrame;
class DecoderBuffer;
class MediaLog;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
class H264ToAnnexBBitstreamConverter;
namespace mp4 {
struct AVCDecoderConfigurationRecord;
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace media

namespace blink {

class EncodedVideoChunk;
class ExceptionState;
class VideoDecoderConfig;
class VideoDecoderInit;
class VideoFrame;
class V8VideoFrameOutputCallback;
class ScriptPromise;

class MODULES_EXPORT VideoDecoderTraits {
 public:
  using InitType = VideoDecoderInit;
  using OutputType = VideoFrame;
  using MediaOutputType = media::VideoFrame;
  using MediaDecoderType = media::VideoDecoder;
  using OutputCallbackType = V8VideoFrameOutputCallback;
  using ConfigType = VideoDecoderConfig;
  using MediaConfigType = media::VideoDecoderConfig;
  using InputType = EncodedVideoChunk;

  static constexpr bool kNeedsGpuFactories = true;

  static std::unique_ptr<MediaDecoderType> CreateDecoder(
      ExecutionContext& execution_context,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      media::MediaLog* media_log);
  static void InitializeDecoder(MediaDecoderType& decoder,
                                bool low_delay,
                                const MediaConfigType& media_config,
                                MediaDecoderType::InitCB init_cb,
                                MediaDecoderType::OutputCB output_cb);
  static int GetMaxDecodeRequests(const MediaDecoderType& decoder);
  static void UpdateDecoderLog(const MediaDecoderType& decoder,
                               const MediaConfigType& media_config,
                               media::MediaLog* media_log);
  static media::StatusOr<OutputType*> MakeOutput(scoped_refptr<MediaOutputType>,
                                                 ExecutionContext*);
  static const char* GetName();
};

class MODULES_EXPORT VideoDecoder : public DecoderTemplate<VideoDecoderTraits> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoDecoder* Create(ScriptState*,
                              const VideoDecoderInit*,
                              ExceptionState&);

  static ScriptPromise isConfigSupported(ScriptState*,
                                         const VideoDecoderConfig*,
                                         ExceptionState&);

  static HardwarePreference GetHardwareAccelerationPreference(
      const ConfigType& config);

  // For use by MediaSource and by ::MakeMediaConfig.
  static CodecConfigEval MakeMediaVideoDecoderConfig(
      const ConfigType& config,
      MediaConfigType& out_media_config,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      std::unique_ptr<media::H264ToAnnexBBitstreamConverter>&
          out_h264_converter,
      std::unique_ptr<media::mp4::AVCDecoderConfigurationRecord>& out_h264_avcc,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
      String& out_console_message);

  VideoDecoder(ScriptState*, const VideoDecoderInit*, ExceptionState&);
  ~VideoDecoder() override = default;

 protected:
  CodecConfigEval MakeMediaConfig(const ConfigType& config,
                                  MediaConfigType* out_media_config,
                                  String* out_console_message) override;
  media::StatusOr<scoped_refptr<media::DecoderBuffer>> MakeDecoderBuffer(
      const InputType& input,
      bool verify_key_frame) override;

  static ScriptPromise IsAcceleratedConfigSupported(ScriptState* script_state,
                                                    const VideoDecoderConfig*,
                                                    ExceptionState&);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  std::unique_ptr<media::H264ToAnnexBBitstreamConverter> h264_converter_;
  std::unique_ptr<media::mp4::AVCDecoderConfigurationRecord> h264_avcc_;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  media::VideoCodec current_codec_ = media::kUnknownVideoCodec;

 private:
  // DecoderTemplate implementation.
  HardwarePreference GetHardwarePreference(const ConfigType& config) override;
  bool GetLowDelayPreference(const ConfigType& config) override;
  void SetHardwarePreference(HardwarePreference preference) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_DECODER_H_
