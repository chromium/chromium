// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
#define MEDIA_FILTERS_VPX_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/filters/offloading_video_decoder.h"

struct vpx_codec_ctx;
struct vpx_image;

namespace media {
class FrameBufferPool;

// Libvpx video decoder wrapper.
// Note: VpxVideoDecoder accepts only YV12A VP8 content or VP9 content. This is
// done to avoid usurping FFmpeg for all VP8 decoding, because the FFmpeg VP8
// decoder is faster than the libvpx VP8 decoder.
// Alpha channel, if any, is sent in the DecoderBuffer's side_data() as a frame
// on its own of which the Y channel is taken [1].
// [1] http://wiki.webmproject.org/alpha-channel
class MEDIA_EXPORT VpxVideoDecoder : public OffloadableVideoDecoder {
 public:
  explicit VpxVideoDecoder(OffloadState offload_state = OffloadState::kNormal);
  ~VpxVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;

  // OffloadableVideoDecoder implementation.
  void Detach() override;

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kError
  };

  // Return values for decoding alpha plane.
  enum AlphaDecodeStatus {
    kAlphaPlaneProcessed,  // Alpha plane (if found) was decoded successfully.
    kNoAlphaPlaneData,  // Alpha plane was found, but decoder did not return any
                        // data.
    kAlphaPlaneError  // Fatal error occured when trying to decode alpha plane.
  };

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true when initialization was successful.
  bool ConfigureDecoder(const VideoDecoderConfig& config);

  void CloseDecoder();

  // Try to decode |buffer| into |video_frame|. Return true if all decoding
  // succeeded. Note that decoding can succeed and still |video_frame| be
  // nullptr if there has been a partial decoding.
  bool VpxDecode(const DecoderBuffer* buffer,
                 scoped_refptr<VideoFrame>* video_frame);

  bool CopyVpxImageToVideoFrame(const struct vpx_image* vpx_image,
                                const struct vpx_image* vpx_image_alpha,
                                scoped_refptr<VideoFrame>* video_frame);

  AlphaDecodeStatus DecodeAlphaPlane(const struct vpx_image* vpx_image,
                                     const struct vpx_image** vpx_image_alpha,
                                     const DecoderBuffer* buffer);

  // Indicates if the decoder is being wrapped by OffloadVideoDecoder; controls
  // whether callbacks are bound to the current loop on calls.
  const bool bind_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  // |state_| must only be read and written to on |offload_task_runner_| if it
  // is non-null and there are outstanding tasks on the offload thread.
  DecoderState state_ = kUninitialized;

  OutputCB output_cb_;

  VideoDecoderConfig config_;

  std::unique_ptr<vpx_codec_ctx> vpx_codec_;
  std::unique_ptr<vpx_codec_ctx> vpx_codec_alpha_;

  // |memory_pool_| is a single-threaded memory pool used for VP9 decoding
  // with no alpha. |frame_pool_| is used for all other cases.
  scoped_refptr<FrameBufferPool> memory_pool_;
  VideoFramePool frame_pool_;

  DISALLOW_COPY_AND_ASSIGN(VpxVideoDecoder);
};

// Helper class for creating a VpxVideoDecoder which will offload > 720p VP9
// content from the media thread.
class OffloadingVpxVideoDecoder : public OffloadingVideoDecoder {
 public:
  OffloadingVpxVideoDecoder()
      : OffloadingVideoDecoder(
            1024,
            std::vector<VideoCodec>(1, kCodecVP9),
            std::make_unique<VpxVideoDecoder>(
                OffloadableVideoDecoder::OffloadState::kOffloaded)) {}
};

}  // namespace media

#endif  // MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
