// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_TEST_VDA_VIDEO_DECODER_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_TEST_VDA_VIDEO_DECODER_H_

#include <stdint.h>
#include <map>
#include <memory>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_decoder.h"
#include "media/gpu/test/video_player/decoder_wrapper.h"
#include "media/media_buildflags.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

class VideoFrame;

namespace test {

class FrameRendererDummy;

// The test VDA video decoder translates between the media::VideoDecoder and the
// media::VideoDecodeAccelerator interfaces. This makes it possible to run
// VD-based tests against VDA's.
class TestVDAVideoDecoder : public media::VideoDecoder,
                            public VideoDecodeAccelerator::Client {
 public:
  using OnProvidePictureBuffersCB = base::RepeatingCallback<bool(void)>;
  // Constructor for the TestVDAVideoDecoder.
  TestVDAVideoDecoder(bool use_vd_vda,
                      OnProvidePictureBuffersCB on_provide_picture_buffers_cb,
                      const gfx::ColorSpace& target_color_space,
                      FrameRendererDummy* const frame_renderer,
                      bool linear_output = false);

  TestVDAVideoDecoder(const TestVDAVideoDecoder&) = delete;
  TestVDAVideoDecoder& operator=(const TestVDAVideoDecoder&) = delete;

  ~TestVDAVideoDecoder() override;

  // media::VideoDecoder implementation
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
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

 private:
  // media::VideoDecodeAccelerator::Client implementation
  void NotifyInitializationComplete(DecoderStatus status) override;
  void ProvidePictureBuffersWithVisibleRect(
      uint32_t requested_num_of_buffers,
      VideoPixelFormat format,
      const gfx::Size& dimensions,
      const gfx::Rect& visible_rect) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const Picture& picture) override;
  void ReusePictureBufferTask(int32_t picture_buffer_id);
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(VideoDecodeAccelerator::Error error) override;

  // Helper thunk to avoid dereferencing WeakPtrs on the wrong thread.
  static void ReusePictureBufferThunk(
      std::optional<base::WeakPtr<TestVDAVideoDecoder>> decoder_client,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      int32_t picture_buffer_id);

  // Get the next bitstream buffer id to be used.
  int32_t GetNextBitstreamBufferId();
  // Get the next picture buffer id to be used.
  int32_t GetNextPictureBufferId();

  // Called when initialization is done. The callback is stored only when VDA
  // allows deferred initialization.
  InitCB init_cb_;
  // Called when a buffer is decoded.
  OutputCB output_cb_;
  // Called when the decoder finished flushing.
  DecodeCB flush_cb_;
  // Called when the decoder finished resetting.
  base::OnceClosure reset_cb_;

  // Whether VdVideoDecodeAccelerator is used.
  bool use_vd_vda_;

  // Called when the decoder requests buffers for decoding onto. The callback
  // returns true if the request should be completed.
  OnProvidePictureBuffersCB on_provide_picture_buffers_cb_;

  // Output color space, used as hint to decoder to avoid conversions.
  const gfx::ColorSpace target_color_space_;

  // Frame renderer used to manage GL context.
  const raw_ptr<FrameRendererDummy> frame_renderer_;

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  // Whether the decoder output buffers should be allocated with a linear
  // layout.
  const bool linear_output_;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  // Map of video frames the decoder uses as output, keyed on picture buffer id.
  std::map<int32_t, scoped_refptr<VideoFrame>> video_frames_;
  // Map of video frame decoded callbacks, keyed on bitstream buffer id.
  std::map<int32_t, DecodeCB> decode_cbs_;
  // Records the time at which each bitstream buffer decode operation started.
  base::LRUCache<int32_t, base::TimeDelta> decode_start_timestamps_;

  int32_t next_bitstream_buffer_id_ = 0;
  int32_t next_picture_buffer_id_ = 0;

  std::unique_ptr<VideoDecodeAccelerator> decoder_;

  scoped_refptr<base::SequencedTaskRunner> vda_wrapper_task_runner_;

  SEQUENCE_CHECKER(vda_wrapper_sequence_checker_);

  base::WeakPtr<TestVDAVideoDecoder> weak_this_;
  base::WeakPtrFactory<TestVDAVideoDecoder> weak_this_factory_{this};
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_TEST_VDA_VIDEO_DECODER_H_
