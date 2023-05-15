// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_STATEFUL_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_V4L2_STATEFUL_VIDEO_DECODER_H_

#include <linux/videodev2.h>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// V4L2StatefulVideoDecoder is an implementation of VideoDecoderMixin
// (an augmented media::VideoDecoder) for stateful V4L2 decoding drivers
// (e.g. those in ChromeOS Qualcomm devices, and Mediatek 8173). This API has
// changed along the kernel versions, but a given copy can be found in [1]
// (the most up-to-date is in [2]).
//
// This class operates on a single thread, where it is constructed and
// destroyed.
//
// [1]
// https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/dev-decoder.html
// [2]
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-decoder.html
class MEDIA_GPU_EXPORT V4L2StatefulVideoDecoder : public VideoDecoderMixin {
 public:
  static std::unique_ptr<VideoDecoderMixin> Create(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
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
  void SetDmaIncoherentV4L2(bool incoherent) override;

 private:
  V4L2StatefulVideoDecoder(std::unique_ptr<MediaLog> media_log,
                           scoped_refptr<base::SequencedTaskRunner> task_runner,
                           base::WeakPtr<VideoDecoderMixin::Client> client);
  ~V4L2StatefulVideoDecoder() override;

  // Pegged to the construction and main work thread. Notably, |task_runner| is
  // not used.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_STATEFUL_VIDEO_DECODER_H_
