// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_STATELESS_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_V4L2_STATELESS_VIDEO_DECODER_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder.h"
#include "media/base/media_log.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/waiting.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// V4L2 Stateless Video Decoder implements the Request API for decoding video
// using a memory to memory interface.
// https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/request-api.html
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-stateless-decoder.html
class MEDIA_GPU_EXPORT V4L2StatelessVideoDecoder
    : public VideoDecoderMixin,
      public V4L2DecodeSurfaceHandler {
 public:
  static std::unique_ptr<VideoDecoderMixin> Create(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);

  static absl::optional<SupportedVideoDecoderConfigs> GetSupportedConfigs();

  // VideoDecoderMixin implementation, VideoDecoder part.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
  // VideoDecoderMixin implementation, specific part.
  void ApplyResolutionChange() override;
  size_t GetMaxOutputFramePoolSize() const override;

  // V4L2DecodeSurfaceHandler implementation.
  scoped_refptr<V4L2DecodeSurface> CreateSurface() override;
  bool SubmitSlice(V4L2DecodeSurface* dec_surface,
                   const uint8_t* data,
                   size_t size) override;
  void DecodeSurface(scoped_refptr<V4L2DecodeSurface> dec_surface) override;
  void SurfaceReady(scoped_refptr<V4L2DecodeSurface> dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;

 private:
  V4L2StatelessVideoDecoder(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);
  ~V4L2StatelessVideoDecoder() override;

  SEQUENCE_CHECKER(decoder_sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_STATELESS_VIDEO_DECODER_H_
