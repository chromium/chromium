// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_DECODER_H_
#define MEDIA_GPU_VP9_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/vp9_picture.h"
#include "media/gpu/vp9_reference_frame_vector.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// This class implements an AcceleratedVideoDecoder for VP9 decoding.
// Clients of this class are expected to pass raw VP9 stream and are expected
// to provide an implementation of VP9Accelerator for offloading final steps
// of the decoding process.
//
// This class must be created, called and destroyed on a single thread, and
// does nothing internally on any other thread.
class MEDIA_GPU_EXPORT VP9Decoder : public AcceleratedVideoDecoder {
 public:
  class MEDIA_GPU_EXPORT VP9Accelerator {
   public:
    VP9Accelerator();
    virtual ~VP9Accelerator();

    // Create a new VP9Picture that the decoder client can use for initial
    // stages of the decoding process and pass back to this accelerator for
    // final, accelerated stages of it, or for reference when decoding other
    // pictures.
    //
    // When a picture is no longer needed by the decoder, it will just drop
    // its reference to it, and it may do so at any time.
    //
    // Note that this may return nullptr if the accelerator is not able to
    // provide any new pictures at the given time. The decoder must handle this
    // case and treat it as normal, returning kRanOutOfSurfaces from Decode().
    virtual scoped_refptr<VP9Picture> CreateVP9Picture() = 0;

    // Submit decode for |pic| to be run in accelerator, taking as arguments
    // information contained in it, as well as current segmentation and loop
    // filter state in |segm_params| and |lf_params|, respectively, and using
    // pictures in |ref_pictures| for reference.
    // If done_cb_ is not null, it will be run once decode is done in hardware.
    //
    // Note that returning from this method does not mean that the decode
    // process is finished, but the caller may drop its references to |pic|
    // and |ref_pictures| immediately, and the data in |segm_params| and
    // |lf_params| does not need to remain valid after this method returns.
    //
    // Return true when successful, false otherwise.
    virtual bool SubmitDecode(scoped_refptr<VP9Picture> pic,
                              const Vp9SegmentationParams& segm_params,
                              const Vp9LoopFilterParams& lf_params,
                              const Vp9ReferenceFrameVector& reference_frames,
                              const base::Closure& done_cb) = 0;

    // Schedule output (display) of |pic|.
    //
    // Note that returning from this method does not mean that |pic| has already
    // been outputted (displayed), but guarantees that all pictures will be
    // outputted in the same order as this method was called for them, and that
    // they are decoded before outputting (assuming SubmitDecode() has been
    // called for them beforehand). Decoder may drop its references to |pic|
    // immediately after calling this method.
    //
    // Return true when successful, false otherwise.
    virtual bool OutputPicture(scoped_refptr<VP9Picture> pic) = 0;

    // Return true if the accelerator requires the client to provide frame
    // context in order to decode. If so, the Vp9FrameHeader provided by the
    // client must contain a valid compressed header and frame context data.
    virtual bool IsFrameContextRequired() const = 0;

    // Set |frame_ctx| to the state after decoding |pic|, returning true on
    // success, false otherwise.
    virtual bool GetFrameContext(scoped_refptr<VP9Picture> pic,
                                 Vp9FrameContext* frame_ctx) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(VP9Accelerator);
  };

  explicit VP9Decoder(
      std::unique_ptr<VP9Accelerator> accelerator,
      const VideoColorSpace& container_color_space = VideoColorSpace());
  ~VP9Decoder() override;

  // AcceleratedVideoDecoder implementation.
  void SetStream(int32_t id, const DecoderBuffer& decoder_buffer) override;
  bool Flush() override WARN_UNUSED_RESULT;
  void Reset() override;
  DecodeResult Decode() override WARN_UNUSED_RESULT;
  gfx::Size GetPicSize() const override;
  gfx::Rect GetVisibleRect() const override;
  size_t GetRequiredNumOfPictures() const override;
  size_t GetNumReferenceFrames() const override;

 private:
  // Decode and possibly output |pic| (if the picture is to be shown).
  // Return true on success, false otherwise.
  bool DecodeAndOutputPicture(scoped_refptr<VP9Picture> pic);

  // Get frame context state after decoding |pic| from the accelerator, and call
  // |context_refresh_cb| with the acquired state.
  void UpdateFrameContext(
      scoped_refptr<VP9Picture> pic,
      const base::Callback<void(const Vp9FrameContext&)>& context_refresh_cb);

  // Called on error, when decoding cannot continue. Sets state_ to kError and
  // releases current state.
  void SetError();

  enum State {
    kNeedStreamMetadata,  // After initialization, need a keyframe.
    kDecoding,            // Ready to decode from any point.
    kAfterReset,          // After Reset(), need a resume point.
    kError,               // Error in decode, can't continue.
  };

  // Current decoder state.
  State state_;

  // Current stream buffer id; to be assigned to pictures decoded from it.
  int32_t stream_id_ = -1;

  // Current frame header to be used in decoding the next picture.
  std::unique_ptr<Vp9FrameHeader> curr_frame_hdr_;
  // Current frame size that is necessary to decode |curr_frame_hdr_|.
  gfx::Size curr_frame_size_;

  // Color space provided by the container.
  const VideoColorSpace container_color_space_;

  // Reference frames currently in use.
  Vp9ReferenceFrameVector ref_frames_;

  // Current coded resolution.
  gfx::Size pic_size_;

  // Visible rectangle on the most recent allocation.
  gfx::Rect visible_rect_;

  size_t size_change_failure_counter_ = 0;

  const std::unique_ptr<VP9Accelerator> accelerator_;

  Vp9Parser parser_;

  DISALLOW_COPY_AND_ASSIGN(VP9Decoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VP9_DECODER_H_
