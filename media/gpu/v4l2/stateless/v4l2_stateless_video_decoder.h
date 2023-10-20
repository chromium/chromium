// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_V4L2_STATELESS_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_STATELESS_V4L2_STATELESS_VIDEO_DECODER_H_

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
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/v4l2/stateless/queue.h"
#include "media/gpu/v4l2/stateless/stateless_decode_surface_handler.h"
#include "media/gpu/v4l2/stateless/stateless_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// V4L2 Stateless Video Decoder implements the Request API for decoding video
// using a memory to memory interface.
// https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/request-api.html
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-stateless-decoder.html
class MEDIA_GPU_EXPORT V4L2StatelessVideoDecoder
    : public VideoDecoderMixin,
      public StatelessDecodeSurfaceHandler {
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

  // StatelessDecodeSurfaceHandler implementation.
  scoped_refptr<V4L2DecodeSurface> CreateSurface() override;
  void SurfaceReady(scoped_refptr<V4L2DecodeSurface> dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;
  bool SubmitFrame(void* ctrls,
                   const uint8_t* data,
                   size_t size,
                   int32_t bitstream_id) override;

 private:
  V4L2StatelessVideoDecoder(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client,
      scoped_refptr<StatelessDevice> device);
  ~V4L2StatelessVideoDecoder() override;

  // Create a codec specific decoder. When successful this decoder is stored in
  // the |decoder_| member variable.
  bool CreateDecoder(VideoCodecProfile profile, VideoColorSpace color_space);

  // Create a queue of buffers for compressed frames to go into. V4L2 needs
  // to know |profile| and |resolution| in order to know if the queue
  // can be created.
  bool CreateInputQueue(VideoCodecProfile profile, const gfx::Size resolution);

  // Process the data in the |compressed_buffer| using the |decoder_|.
  void ProcessCompressedBuffer(scoped_refptr<DecoderBuffer> compressed_buffer,
                               VideoDecoder::DecodeCB decode_cb,
                               int32_t bitstream_id);

  SEQUENCE_CHECKER(decoder_sequence_checker_);

  const scoped_refptr<StatelessDevice> device_;

  // Callback obtained from Initialize() to be called after every frame
  // has finished decoding and is ready for the client to display.
  OutputCB output_cb_;

  // Video decoder used to parse stream headers by software.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;

  std::unique_ptr<InputQueue> input_queue_;

  // Int32 safe ID generator, starting at 0. Generated IDs are used to uniquely
  // identify a Decode() request for stateless backends. BitstreamID is just
  // a "phantom type" (see StrongAlias), essentially just a name.
  struct BitstreamID {};
  base::IdType32<BitstreamID>::Generator bitstream_id_generator_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_V4L2_STATELESS_VIDEO_DECODER_H_
