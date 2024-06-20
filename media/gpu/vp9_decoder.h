// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_DECODER_H_
#define MEDIA_GPU_VP9_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/video_types.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/vp9_picture.h"
#include "media/gpu/vp9_reference_frame_vector.h"
#include "media/parsers/vp9_parser.h"
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
    // Methods may return kTryAgain if they need additional data (provided
    // independently) in order to proceed. Examples are things like not having
    // an appropriate key to decode encrypted content. This is not considered an
    // unrecoverable error, but rather a pause to allow an application to
    // independently provide the required data. When VP9Decoder::Decode()
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
    };
    VP9Accelerator();

    VP9Accelerator(const VP9Accelerator&) = delete;
    VP9Accelerator& operator=(const VP9Accelerator&) = delete;

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

    // |secure_handle| is a reference to the corresponding secure memory when
    // doing secure decoding on ARM. This is invoked instead of CreateAV1Picture
    // when doing secure decoding on ARM. Default implementation returns
    // nullptr.
    // TODO(jkardatzke): Remove this once we move to the V4L2 flat stateless
    // decoder and add a field to media::CodecPicture instead.
    virtual scoped_refptr<VP9Picture> CreateVP9PictureSecure(
        uint64_t secure_handle);

    // Submit decode for |pic| to be run in accelerator, taking as arguments
    // information contained in it, as well as current segmentation and loop
    // filter state in |segm_params| and |lf_params|, respectively, and using
    // pictures in |ref_pictures| for reference.
    //
    // Note that returning from this method does not mean that the decode
    // process is finished, but the caller may drop its references to |pic|
    // and |ref_pictures| immediately, and the data in |segm_params| and
    // |lf_params| does not need to remain valid after this method returns.
    //
    // Return true when successful, false otherwise.
    virtual Status SubmitDecode(
        scoped_refptr<VP9Picture> pic,
        const Vp9SegmentationParams& segm_params,
        const Vp9LoopFilterParams& lf_params,
        const Vp9ReferenceFrameVector& reference_frames) = 0;

    // Schedule output (display) of |pic|.
    //
    // If `show_existing_hdr` is not nullptr, then it contains the header of
    // a show_existing_frame frame that requests the output of `pic`.
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

    // Return true if the accelerator requires us to provide the compressed
    // header fully parsed.
    virtual bool NeedsCompressedHeaderParsed() const = 0;
  };

  explicit VP9Decoder(
      std::unique_ptr<VP9Accelerator> accelerator,
      VideoCodecProfile profile,
      const VideoColorSpace& container_color_space = VideoColorSpace());

  VP9Decoder(const VP9Decoder&) = delete;
  VP9Decoder& operator=(const VP9Decoder&) = delete;

  ~VP9Decoder() override;

  // AcceleratedVideoDecoder implementation.
  void SetStream(int32_t id, const DecoderBuffer& decoder_buffer) override;
  [[nodiscard]] bool Flush() override;
  void Reset() override;
  [[nodiscard]] DecodeResult Decode() override;
  gfx::Size GetPicSize() const override;
  gfx::Rect GetVisibleRect() const override;
  VideoCodecProfile GetProfile() const override;
  uint8_t GetBitDepth() const override;
  VideoChromaSampling GetChromaSampling() const override;
  VideoColorSpace GetVideoColorSpace() const override;
  std::optional<gfx::HDRMetadata> GetHDRMetadata() const override;
  size_t GetRequiredNumOfPictures() const override;
  size_t GetNumReferenceFrames() const override;

  void set_ignore_resolution_changes_to_smaller_for_testing(bool value) {
    ignore_resolution_changes_to_smaller_for_testing_ = value;
  }

 private:
  // Decode and possibly output |pic| (if the picture is to be shown).
  // Return kOk on success, kTryAgain if this should be attempted again on the
  // next Decode call, and kFail otherwise.
  VP9Accelerator::Status DecodeAndOutputPicture(scoped_refptr<VP9Picture> pic);

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

  // Current frame header and decrypt config to be used in decoding the next
  // picture.
  std::unique_ptr<Vp9FrameHeader> curr_frame_hdr_;
  std::unique_ptr<DecryptConfig> decrypt_config_;

  // Secure handle to pass through to the accelerator when doing secure playback
  // on ARM.
  uint64_t secure_handle_ = 0;

  // Current frame size that is necessary to decode |curr_frame_hdr_|.
  gfx::Size curr_frame_size_;

  // Color space provided by the container.
  const VideoColorSpace container_color_space_;

  // For VP9 validation purposes, this class can be indicated that it's OK to
  // keep the decoding reference frames etc when the resolution decreases
  // without a keyframe; this is an arcane feature of VP9, and are rare in the
  // wild, but part of VP9 verification sets (see[1] "frm_resize" and
  // "sub8x8_sf"). [1] https://www.webmproject.org/vp9/levels/#test-descriptions
  bool ignore_resolution_changes_to_smaller_for_testing_ = false;

  // Reference frames currently in use.
  Vp9ReferenceFrameVector ref_frames_;

  // Current coded resolution.
  gfx::Size pic_size_;
  // Visible rectangle on the most recent allocation.
  gfx::Rect visible_rect_;
  // Profile of input bitstream.
  VideoCodecProfile profile_;
  // Bit depth of input bitstream.
  uint8_t bit_depth_ = 0;
  // Chroma subsampling format of input bitstream.
  VideoChromaSampling chroma_sampling_ = VideoChromaSampling::kUnknown;
  // Video color space of input bitstream.
  VideoColorSpace picture_color_space_;

  // Pending picture for decode when accelerator returns kTryAgain.
  scoped_refptr<VP9Picture> pending_pic_;

  size_t size_change_failure_counter_ = 0;

  const std::unique_ptr<VP9Accelerator> accelerator_;

  Vp9Parser parser_;
};

}  // namespace media

#endif  // MEDIA_GPU_VP9_DECODER_H_
