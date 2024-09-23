// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H265_DECODER_H_
#define MEDIA_GPU_H265_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/decrypt_config.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_codecs.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/h265_dpb.h"
#include "media/gpu/media_gpu_export.h"
#include "media/parsers/h265_parser.h"
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

    // Create a new H265Picture that the decoder client can use for decoding
    // and pass back to this accelerator for decoding or reference.
    // When the picture is no longer needed by decoder, it will just drop
    // its reference to it, and it may do so at any time.
    // Note that this may return nullptr if accelerator is not able to provide
    // any new pictures at given time. The decoder is expected to handle
    // this situation as normal and return from Decode() with kRanOutOfSurfaces.
    virtual scoped_refptr<H265Picture> CreateH265Picture() = 0;

    // |secure_handle| is a reference to the corresponding secure memory when
    // doing secure decoding on ARM. This is invoked instead of CreateAV1Picture
    // when doing secure decoding on ARM. Default implementation returns
    // nullptr.
    // TODO(jkardatzke): Remove this once we move to the V4L2 flat stateless
    // decoder and add a field to media::CodecPicture instead.
    virtual scoped_refptr<H265Picture> CreateH265PictureSecure(
        uint64_t secure_handle);

    // Provides the raw NALU data for a VPS. The |vps| passed to
    // SubmitFrameMetadata() is always the most recent VPS passed to
    // ProcessVPS() with the same |vps_video_parameter_set_id|.
    virtual void ProcessVPS(const H265VPS* vps,
                            base::span<const uint8_t> vps_nalu_data);

    // Provides the raw NALU data for an SPS. The |sps| passed to
    // SubmitFrameMetadata() is always the most recent SPS passed to
    // ProcessSPS() with the same |sps_video_parameter_set_id|.
    virtual void ProcessSPS(const H265SPS* sps,
                            base::span<const uint8_t> sps_nalu_data);

    // Provides the raw NALU data for a PPS. The |pps| passed to
    // SubmitFrameMetadata() is always the most recent PPS passed to
    // ProcessPPS() with the same |pps_pic_parameter_set_id|.
    virtual void ProcessPPS(const H265PPS* pps,
                            base::span<const uint8_t> pps_nalu_data);

    // Submit metadata for the current frame, providing the current |sps|, |pps|
    // and |slice_hdr| for it. |ref_pic_list| contains the set of pictures as
    // described in 8.3.2 from the lists RefPicSetLtCurr, RefPicSetLtFoll,
    // RefPicSetStCurrBefore, RefPicSetStCurrAfter and RefPicSetStFoll.
    // |pic| contains information about the picture for the current frame.
    // Note that this does not run decode in the accelerator and the decoder
    // is expected to follow this call with one or more SubmitSlice() calls
    // before calling SubmitDecode().
    // Returns kOk if successful, kFail if there are errors, or kTryAgain if
    // the accelerator needs additional data before being able to proceed.
    virtual Status SubmitFrameMetadata(
        const H265SPS* sps,
        const H265PPS* pps,
        const H265SliceHeader* slice_hdr,
        const H265Picture::Vector& ref_pic_list,
        const H265Picture::Vector& ref_pic_set_lt_curr,
        const H265Picture::Vector& ref_pic_set_st_curr_after,
        const H265Picture::Vector& ref_pic_set_st_curr_before,
        scoped_refptr<H265Picture> pic) = 0;

    // Submit one slice for the current frame, passing the current |pps| and
    // |pic| (same as in SubmitFrameMetadata()), the parsed header for the
    // current slice in |slice_hdr|, the |ref_pic_listX| and |ref_pic_set_XX|,
    // as per H265 spec. |data| pointing to the full slice (including the
    // unparsed header) of |size| in bytes.
    // |subsamples| specifies which part of the slice data is encrypted.
    // This must be called one or more times per frame, before SubmitDecode().
    // Note that |data| does not have to remain valid after this call returns.
    // Returns kOk if successful, kFail if there are errors, or kTryAgain if
    // the accelerator needs additional data before being able to proceed.
    virtual Status SubmitSlice(
        const H265SPS* sps,
        const H265PPS* pps,
        const H265SliceHeader* slice_hdr,
        const H265Picture::Vector& ref_pic_list0,
        const H265Picture::Vector& ref_pic_list1,
        const H265Picture::Vector& ref_pic_set_lt_curr,
        const H265Picture::Vector& ref_pic_set_st_curr_after,
        const H265Picture::Vector& ref_pic_set_st_curr_before,
        scoped_refptr<H265Picture> pic,
        const uint8_t* data,
        size_t size,
        const std::vector<SubsampleEntry>& subsamples) = 0;

    // Execute the decode in hardware for |pic|, using all the slices and
    // metadata submitted via SubmitFrameMetadata() and SubmitSlice() since
    // the previous call to SubmitDecode().
    // Returns kOk if successful, kFail if there are errors, or kTryAgain if
    // the accelerator needs additional data before being able to proceed.
    virtual Status SubmitDecode(scoped_refptr<H265Picture> pic) = 0;

    // Schedule output (display) of |pic|. Note that returning from this
    // method does not mean that |pic| has already been outputted (displayed),
    // but guarantees that all pictures will be outputted in the same order
    // as this method was called for them. Decoder may drop its reference
    // to |pic| after calling this method.
    // Return true if successful.
    virtual bool OutputPicture(scoped_refptr<H265Picture> pic) = 0;

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

    // Indicates whether the accelerator supports bitstreams with
    // specific chroma subsampling format.
    virtual bool IsChromaSamplingSupported(VideoChromaSampling format) = 0;

    // Indicates whether the accelerator supports an alpha layer.
    virtual bool IsAlphaLayerSupported();
  };

  H265Decoder(std::unique_ptr<H265Accelerator> accelerator,
              VideoCodecProfile profile,
              const VideoColorSpace& container_color_space = VideoColorSpace());

  H265Decoder(const H265Decoder&) = delete;
  H265Decoder& operator=(const H265Decoder&) = delete;

  ~H265Decoder() override;

  // AcceleratedVideoDecoder implementation.
  void SetStream(int32_t id, const DecoderBuffer& decoder) override;
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

  // Process current slice header to discover if we need to start a new picture,
  // finishing up the current one.
  H265Accelerator::Status PreprocessCurrentSlice();

  // Process current slice as a slice of the current picture.
  H265Accelerator::Status ProcessCurrentSlice();

  // Start processing a new frame. This also generates all the POC and output
  // variables for the frame, generates reference picture lists, performs
  // reference picture marking, DPB management and picture output.
  H265Accelerator::Status StartNewFrame(const H265SliceHeader* slice_hdr);

  // All data for a frame received, process it and decode.
  H265Accelerator::Status FinishPrevFrameIfPresent();

  // Called after we are done processing |pic|.
  bool FinishPicture(scoped_refptr<H265Picture> pic,
                     std::unique_ptr<H265SliceHeader> slice_hdr);

  // Commits all pending data for HW decoder and starts HW decoder.
  H265Accelerator::Status DecodePicture();

  // Notifies client that a picture is ready for output.
  bool OutputPic(scoped_refptr<H265Picture> pic);

  // Output all pictures in DPB that have not been outputted yet.
  bool OutputAllRemainingPics();

  // Calculates the picture output flags using |slice_hdr| for |curr_pic_|.
  void CalcPicOutputFlags(const H265SliceHeader* slice_hdr);

  // Calculates picture order count (POC) using |pps| and|slice_hdr| for
  // |curr_pic_|.
  void CalcPictureOrderCount(const H265PPS* pps,
                             const H265SliceHeader* slice_hdr);

  // Calculates the POCs for the reference pictures for |curr_pic_| using
  // |sps|, |pps| and |slice_hdr| and stores them in the member variables.
  // Returns false if bitstream conformance is not maintained, true otherwise.
  bool CalcRefPicPocs(const H265SPS* sps,
                      const H265PPS* pps,
                      const H265SliceHeader* slice_hdr);

  // Builds the reference pictures lists for |curr_pic_| using |sps|, |pps|,
  // |slice_hdr| and the member variables calculated in CalcRefPicPocs. Returns
  // false if bitstream conformance is not maintained or needed reference
  // pictures are missing, true otherwise. At the end of this,
  // |ref_pic_list{0,1}| will be populated with the required reference pictures
  // for submitting to the accelerator.
  bool BuildRefPicLists(const H265SPS* sps,
                        const H265PPS* pps,
                        const H265SliceHeader* slice_hdr);

  // Performs DPB management operations for |curr_pic_| by removing no longer
  // needed entries from the DPB and outputting pictures from the DPB. |sps|
  // should be the corresponding SPS for |curr_pic_|.
  bool PerformDpbOperations(const H265SPS* sps);

  // Decoder state.
  State state_;

  // The colorspace for the h265 container.
  const VideoColorSpace container_color_space_;

  // Parser in use.
  H265Parser parser_;

  // Most recent call to SetStream().
  raw_ptr<const uint8_t, DanglingUntriaged> current_stream_ = nullptr;
  size_t current_stream_size_ = 0;

  // Decrypting config for the most recent data passed to SetStream().
  std::unique_ptr<DecryptConfig> current_decrypt_config_;

  // Secure handle to pass through to the accelerator when doing secure playback
  // on ARM.
  uint64_t secure_handle_ = 0;

  // Keep track of when SetStream() is called so that
  // H265Accelerator::SetStream() can be called.
  bool current_stream_has_been_changed_ = false;

  // DPB in use.
  H265DPB dpb_;

  // Current stream buffer id; to be assigned to pictures decoded from it.
  int32_t stream_id_ = -1;

  // Picture currently being processed/decoded.
  scoped_refptr<H265Picture> curr_pic_;

  // Used to identify first picture in decoding order or first picture that
  // follows an EOS NALU.
  bool first_picture_ = true;

  // Used to keep NoRaslOutputFlag state since last IRAP, to decide if we
  // drop a RASL picture.
  bool no_rasl_output_flag_ = true;

  // Global state values, needed in decoding. See spec.
  scoped_refptr<H265Picture> prev_tid0_pic_;
  int max_pic_order_cnt_lsb_;
  bool curr_delta_poc_msb_present_flag_[kMaxDpbSize];
  bool foll_delta_poc_msb_present_flag_[kMaxDpbSize];
  int num_poc_st_curr_before_;
  int num_poc_st_curr_after_;
  int num_poc_st_foll_;
  int num_poc_lt_curr_;
  int num_poc_lt_foll_;
  int poc_st_curr_before_[kMaxDpbSize];
  int poc_st_curr_after_[kMaxDpbSize];
  int poc_st_foll_[kMaxDpbSize];
  int poc_lt_curr_[kMaxDpbSize];
  int poc_lt_foll_[kMaxDpbSize];
  H265Picture::Vector ref_pic_list0_;
  H265Picture::Vector ref_pic_list1_;
  H265Picture::Vector ref_pic_set_lt_curr_;
  H265Picture::Vector ref_pic_set_st_curr_after_;
  H265Picture::Vector ref_pic_set_st_curr_before_;

  // |ref_pic_list_| is the collection of all pictures from StCurrBefore,
  // StCurrAfter, StFoll, LtCurr and LtFoll.
  H265Picture::Vector ref_pic_list_;

  // Currently active SPS and PPS.
  int curr_sps_id_ = -1;
  int curr_pps_id_ = -1;

  // If this value larger than 0, then that means the current NALU contain alpha
  // layer.
  int aux_alpha_layer_id_ = 0;

  // Current NALU and slice header being processed.
  std::unique_ptr<H265NALU> curr_nalu_;
  std::unique_ptr<H265SliceHeader> curr_slice_hdr_;
  std::unique_ptr<H265SliceHeader> last_slice_hdr_;

  // Output picture size.
  gfx::Size pic_size_;
  // Output visible cropping rect.
  gfx::Rect visible_rect_;

  // Profile of input bitstream.
  VideoCodecProfile profile_;
  // Bit depth of input bitstream.
  uint8_t bit_depth_ = 0;
  // Chroma sampling format of input bitstream
  VideoChromaSampling chroma_sampling_ = VideoChromaSampling::kUnknown;
  // Video color space of input bitstream.
  VideoColorSpace picture_color_space_;
  // HDR metadata in the bitstream.
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  const std::unique_ptr<H265Accelerator> accelerator_;
};

}  // namespace media

#endif  // MEDIA_GPU_H265_DECODER_H_
