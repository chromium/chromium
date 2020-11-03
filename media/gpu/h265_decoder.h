// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H265_DECODER_H_
#define MEDIA_GPU_H265_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "media/base/decrypt_config.h"
#include "media/base/video_codecs.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/h265_dpb.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/h265_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Clients of this class are expected to pass H265 Annex-B byte stream
// and are expected to provide an implementation of H265Accelerator for
// offloading final steps of the decoding process.
//
// This class must be created, called and destroyed on a single thread, and
// does nothing internally on any other thread.
//
// It is expected that when a DecoderBuffer is submitted, that it will contain a
// complete frame of data. Multiple slices per frame are handled. This class can
// also handle multiple frames in a DecoderBuffer, but that condition should
// never actually occur.
class MEDIA_GPU_EXPORT H265Decoder final : public AcceleratedVideoDecoder {
 public:
  class MEDIA_GPU_EXPORT H265Accelerator {
   public:
    // Methods may return kTryAgain if they need additional data (provided
    // independently) in order to proceed. Examples are things like not having
    // an appropriate key to decode encrypted content, or needing to wait
    // until hardware buffers are available. This is not considered an
    // unrecoverable error, but rather a pause to allow an application to
    // independently provide the required data. When H265Decoder::Decode()
    // is called again, it will attempt to resume processing of the stream
    // by calling the same method again.
    enum class Status {
      // Operation completed successfully.
      kOk,

      // Operation failed.
      kFail,

      // Operation failed because some external data is missing. Retry the same
      // operation later, once the data has been provided.
      kTryAgain,

      // Operation is not supported. Used by SetStream() to indicate that the
      // Accelerator can not handle this operation.
      kNotSupported,
    };

    H265Accelerator();

    H265Accelerator(const H265Accelerator&) = delete;
    H265Accelerator& operator=(const H265Accelerator&) = delete;

    virtual ~H265Accelerator();

    // Reset any current state that may be cached in the accelerator, dropping
    // any cached parameters/slices that have not been committed yet.
    virtual void Reset() = 0;

    // Notifies the accelerator whenever there is a new stream to process.
    // |stream| is the data in annex B format, which may include SPS and PPS
    // NALUs when there is a configuration change. The first frame must contain
    // the SPS and PPS NALUs. SPS and PPS NALUs may not be encrypted.
    // |decrypt_config| is the config for decrypting the stream. The accelerator
    // should use |decrypt_config| to keep track of the parts of |stream| that
    // are encrypted. If kTryAgain is returned, the decoder will retry this call
    // later. This method has a default implementation that returns
    // kNotSupported.
    virtual Status SetStream(base::span<const uint8_t> stream,
                             const DecryptConfig* decrypt_config);
  };

  H265Decoder(std::unique_ptr<H265Accelerator> accelerator,
              VideoCodecProfile profile,
              const VideoColorSpace& container_color_space = VideoColorSpace());

  H265Decoder(const H265Decoder&) = delete;
  H265Decoder& operator=(const H265Decoder&) = delete;

  ~H265Decoder() override;

  // AcceleratedVideoDecoder implementation.
  void SetStream(int32_t id, const DecoderBuffer& decoder) override;
  bool Flush() override WARN_UNUSED_RESULT;
  void Reset() override;
  DecodeResult Decode() override WARN_UNUSED_RESULT;
  gfx::Size GetPicSize() const override;
  gfx::Rect GetVisibleRect() const override;
  VideoCodecProfile GetProfile() const override;
  size_t GetRequiredNumOfPictures() const override;
  size_t GetNumReferenceFrames() const override;

 private:
  // Internal state of the decoder.
  enum State {
    // Ready to decode from any point.
    kDecoding,
    // After Reset(), need a resume point.
    kAfterReset,
    // The following keep track of what step is next in Decode() processing
    // in order to resume properly after H265Decoder::kTryAgain (or another
    // retryable error) is returned. The next time Decode() is called the call
    // that previously failed will be retried and execution continues from
    // there (if possible).
    kTryPreprocessCurrentSlice,
    kEnsurePicture,
    kTryNewFrame,
    kTryCurrentSlice,
    // Error in decode, can't continue.
    kError,
  };

  // Process H265 stream structures.
  bool ProcessPPS(int pps_id, bool* need_new_buffers);

  // Decoder state.
  State state_;

  // The colorspace for the h265 container.
  const VideoColorSpace container_color_space_;

  // Parser in use.
  H265Parser parser_;

  // Most recent call to SetStream().
  const uint8_t* current_stream_ = nullptr;
  size_t current_stream_size_ = 0;

  // Decrypting config for the most recent data passed to SetStream().
  std::unique_ptr<DecryptConfig> current_decrypt_config_;

  // Keep track of when SetStream() is called so that
  // H265Accelerator::SetStream() can be called.
  bool current_stream_has_been_changed_ = false;

  // DPB in use.
  H265DPB dpb_;

  // Current stream buffer id; to be assigned to pictures decoded from it.
  int32_t stream_id_ = -1;

  // Global state values, needed in decoding. See spec.
  int max_pic_order_cnt_lsb_;

  // Current NALU being processed.
  std::unique_ptr<H265NALU> curr_nalu_;

  // Output picture size.
  gfx::Size pic_size_;
  // Output visible cropping rect.
  gfx::Rect visible_rect_;

  // Profile of input bitstream.
  VideoCodecProfile profile_;

  const std::unique_ptr<H265Accelerator> accelerator_;
};

}  // namespace media

#endif  // MEDIA_GPU_H265_DECODER_H_