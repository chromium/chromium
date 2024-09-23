// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_VIDEO_FUCHSIA_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_FUCHSIA_VIDEO_FUCHSIA_VIDEO_ENCODE_ACCELERATOR_H_

#include <fuchsia/media/cpp/fidl.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/sysmem_client.h"
#include "media/fuchsia/common/vmo_buffer.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class VideoFrame;

class MEDIA_EXPORT FuchsiaVideoEncodeAccelerator final
    : public VideoEncodeAccelerator,
      public StreamProcessorHelper::Client {
 public:
  FuchsiaVideoEncodeAccelerator();

  FuchsiaVideoEncodeAccelerator(const FuchsiaVideoEncodeAccelerator&) = delete;
  FuchsiaVideoEncodeAccelerator& operator=(
      const FuchsiaVideoEncodeAccelerator&) = delete;

  // VideoEncodeAccelerator implementation.
  SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config,
                  VideoEncodeAccelerator::Client* client,
                  std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void Destroy() override;
  bool IsFlushSupported() override;
  bool IsGpuFrameResizeSupported() override;

 protected:
  ~FuchsiaVideoEncodeAccelerator() override;

 private:
  class VideoFrameWriterQueue;
  class OutputPacketsQueue;

  // StreamProcessorHelper::Client implementation.
  void OnStreamProcessorAllocateInputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints)
      override;
  void OnStreamProcessorAllocateOutputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints)
      override;
  void OnStreamProcessorEndOfStream() override;
  void OnStreamProcessorOutputFormat(
      fuchsia::media::StreamOutputFormat format) override;
  void OnStreamProcessorOutputPacket(
      StreamProcessorHelper::IoPacket packet) override;
  void OnStreamProcessorNoKey() override;
  void OnStreamProcessorError() override;

  void ReleaseEncoder();
  void OnError(EncoderStatus status);
  void OnInputBuffersAcquired(
      std::vector<VmoBuffer> buffers,
      const fuchsia::sysmem2::SingleBufferSettings& buffer_settings);
  void OnOutputBuffersAcquired(
      std::vector<VmoBuffer> buffers,
      const fuchsia::sysmem2::SingleBufferSettings& buffer_settings);
  fuchsia::media::FormatDetails CreateFormatDetails(
      VideoEncodeAccelerator::Config& config);

  raw_ptr<VideoEncodeAccelerator::Client> vea_client_;
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<Config> config_;

  SysmemAllocatorClient sysmem_allocator_;
  std::unique_ptr<StreamProcessorHelper> encoder_;

  std::unique_ptr<SysmemCollectionClient> input_buffer_collection_;
  std::unique_ptr<SysmemCollectionClient> output_buffer_collection_;

  std::unique_ptr<VideoFrameWriterQueue> input_queue_;
  std::unique_ptr<OutputPacketsQueue> output_queue_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_VIDEO_FUCHSIA_VIDEO_ENCODE_ACCELERATOR_H_
