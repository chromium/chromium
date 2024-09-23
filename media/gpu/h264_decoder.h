// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H264_DECODER_H_
#define MEDIA_GPU_H264_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/limits.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_types.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/media_gpu_export.h"
#include "media/parsers/h264_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Clients of this class are expected to pass H264 Annex-B byte stream
// and are expected to provide an implementation of H264Accelerator for
// offloading final steps of the decoding process.
//
// This class must be created, called and destroyed on a single thread, and
// does nothing internally on any other thread.
class MEDIA_GPU_EXPORT H264Decoder : public AcceleratedVideoDecoder {
 public:
  class MEDIA_GPU_EXPORT H264Accelerator {
   public:
    // Methods may return kTryAgain if they need additional data (provided
    // independently) in order to proceed. Examples are things like not having
    // an appropriate key to decode encrypted content, or needing to wait
    // until hardware buffers are available. This is not considered an
    // unrecoverable error, but rather a pause to allow an application to
    // independently provide the required data. When H264Decoder::Decode()
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

    H264Accelerator();

    H264Accelerator(const H264Accelerator&) = delete;
    H264Accelerator& operator=(const H264Accelerator&) = delete;

    virtual ~H264Accelerator();

    // Create a new H264Picture that the decoder client can use for decoding
    // and pass back to this accelerator for decoding or reference.
    // When the picture is no longer needed by decoder, it will just drop
    // its reference to it, and it may do so at any time.
    // Note that this may return nullptr if accelerator is not able to provide
    // any new pictures at given time. The decoder is expected to handle
    // this situation as normal and return from Decode() with kRanOutOfSurfaces.
    virtual scoped_refptr<H264Picture> CreateH264Picture() = 0;

    // |secure_handle| is a reference to the corresponding secure memory when
    // doing secure decoding on ARM. This is invoked instead of CreateAV1Picture
    // when doing secure decoding on ARM. Default implementation returns
    // nullptr.
    // TODO(jkardatzke): Remove this once we move to the V4L2 flat stateless
    // decoder and add a field to media::CodecPicture instead.
    virtual scoped_refptr<H264Picture> CreateH264PictureSecure(
        uint64_t secure_handle);

    // Provides the raw NALU data for an SPS. The |sps| passed to
    // SubmitFrameMetadata() is always the most recent SPS passed to
    // ProcessSPS() with the same |seq_parameter_set_id|.
    virtual void ProcessSPS(const H264SPS* sps,
                            base::span<const uint8_t> sps_nalu_data);

    // Provides the raw NALU data for a PPS. The |pps| passed to
    // SubmitFrameMetadata() is always the most recent PPS passed to
    // ProcessPPS() with the same |pic_parameter_set_id|.
    virtual void ProcessPPS(const H264PPS* pps,
                            base::span<const uint8_t> pps_nalu_data);

    // Submit metadata for the current frame, providing the current |sps| and
    // |pps| for it, |dpb| has to contain all the pictures in DPB for current
    // frame, and |ref_pic_p0/b0/b1| as specified in the H264 spec. Note that
    // depending on the frame type, either p0, or b0 and b1 are used. |pic|
    // contains information about the picture for the current frame.
    // Note that this does not run decode in the accelerator and the decoder
    // is expected to follow this call with one or more SubmitSlice() calls
    // before calling SubmitDecode().
    // Returns kOk if successful, kFail if there are errors, or kTryAgain if
    // the accelerator needs additional data before being able to proceed.
    virtual Status SubmitFrameMetadata(
        const H264SPS* sps,
        const H264PPS* pps,
        const H264DPB& dpb,
        const H264Picture::Vector& ref_pic_listp0,
        const H264Picture::Vector& ref_pic_listb0,
        const H264Picture::Vector& ref_pic_listb1,
        scoped_refptr<H264Picture> pic) = 0;

    // Used for handling CENCv1 streams where the entire slice header, except
    // for the NALU type byte, is encrypted. |data| represents the encrypted
    // ranges which will include any SEI NALUs along with the encrypted slice
    // NALU. |subsamples| specifies what is encrypted and should have just a
    // single clear byte for each and the rest is encrypted. |secure_handle| is
    // used on ARM to store the secure buffer reference to parse the header
    // from. |slice_header_out| should have its fields filled in upon successful
    // return. Returns kOk if successful, kFail if there are errors, or
    // kTryAgain if the accelerator needs additional data before being able to
    // proceed.
    virtual Status ParseEncryptedSliceHeader(
        const std::vector<base::span<const uint8_t>>& data,
        const std::vector<SubsampleEntry>& subsamples,
        uint64_t secure_handle,
        H264SliceHeader* slice_header_out);

    // Submit one slice for the current frame, passing the current |pps| and
    // |pic| (same as in SubmitFrameMetadata()), the parsed header for the
    // current slice in |slice_hdr|, and the reordered |ref_pic_listX|,
    // as per H264 spec.
    // |data| pointing to the full slice (including the unparsed header) of
    // |size| in bytes.
    // |subsamples| specifies which part of the slice data is encrypted.
    // This must be called one or more times per frame, before SubmitDecode().
    // Note that |data| does not have to remain valid after this call returns.
    // Returns kOk if successful, kFail if there are errors, or kTryAgain if
    // the accelerator needs additional data before being able to proceed.
    virtual Status SubmitSlice(
        const H264PPS* pps,
        const H264SliceHeader* slice_hdr,
        const H264Picture::Vector& ref_pic_list0,
        const H264Picture::Vector& ref_pic_list1,
        scoped_refptr<H264Picture> pic,
        const uint8_t* data,
        size_t size,
        const std::vector<SubsampleEntry>& subsamples) = 0;

    // Execute the decode in hardware for |pic|, using all the slices and
    // metadata submitted via SubmitFrameMetadata() and SubmitSlice() since
    // the previous call to SubmitDecode().
    // Returns kOk if successful, kFail if there are errors, or kTryAgain if
    // the accelerator needs additional data before being able to proceed.
    virtual Status SubmitDecode(scoped_refptr<H264Picture> pic) = 0;

    // Schedule output (display) of |pic|. Note that returning from this
    // method does not mean that |pic| has already been outputted (displayed),
    // but guarantees that all pictures will be outputted in the same order
    // as this method was called for them. Decoder may drop its reference
    // to |pic| after calling this method.
    // Return true if successful.
    virtual bool OutputPicture(scoped_refptr<H264Picture> pic) = 0;

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

    // Notifies whether or not the current platform requires reference lists.
    // In general, implementations don't need it.
    virtual bool RequiresRefLists();
  };

  H264Decoder(std::unique_ptr<H264Accelerator> accelerator,
              VideoCodecProfile profile,
              const VideoColorSpace& container_color_space = VideoColorSpace());

  H264Decoder(const H264Decoder&) = delete;
  H264Decoder& operator=(const H264Decoder&) = delete;

  ~H264Decoder() override;

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

  // Return true if we need to start a new picture.
  static bool IsNewPrimaryCodedPicture(const H264Picture* curr_pic,
                                       int curr_pps_id,
                                       const H264SPS* sps,
                                       const H264SliceHeader& slice_hdr);

  // Fill a H264Picture in |pic| from given |sps| and |slice_hdr|. Return false
  // when there is an error.
  static bool FillH264PictureFromSliceHeader(const H264SPS* sps,
                                             const H264SliceHeader& slice_hdr,
                                             H264Picture* pic);

 private:
  // Internal state of the decoder.
  enum class State {
    // After initialization, need an SPS.
    kNeedStreamMetadata,
    // Ready to decode from any point.
    kDecoding,
    // After Reset(), need a resume point.
    kAfterReset,
    // The following keep track of what step is next in Decode() processing
    // in order to resume properly after H264Decoder::kTryAgain (or another
    // retryable error) is returned. The next time Decode() is called the call
    // that previously failed will be retried and execution continues from
    // there (if possible).
    kParseSliceHeader,
    kTryPreprocessCurrentSlice,
    kEnsurePicture,
    kTryNewFrame,
    kTryCurrentSlice,
    // Error in decode, can't continue.
    kError,
  };

  // Process H264 stream structures.
  bool ProcessSPS(int sps_id, bool* need_new_buffers);

  // Processes a CENCv1 encrypted slice header and fills in |curr_slice_hdr_|
  // with the relevant parsed fields.
  H264Accelerator::Status ProcessEncryptedSliceHeader(
      const std::vector<SubsampleEntry>& subsamples);

  // Process current slice header to discover if we need to start a new picture,
  // finishing up the current one.
  H264Accelerator::Status PreprocessCurrentSlice();
  // Process current slice as a slice of the current picture.
  H264Accelerator::Status ProcessCurrentSlice();

  // Initialize the current picture according to data in |slice_hdr|.
  bool InitCurrPicture(const H264SliceHeader* slice_hdr);

  // Initialize |pic| as a "non-existing" picture (see spec) with |frame_num|,
  // to be used for frame gap concealment.
  bool InitNonexistingPicture(scoped_refptr<H264Picture> pic, int frame_num);

  // Calculate picture order counts for |pic| on initialization
  // of a new frame (see spec).
  bool CalculatePicOrderCounts(scoped_refptr<H264Picture> pic);

  // Update PicNum values in pictures stored in DPB on creation of
  // a picture with |frame_num|.
  void UpdatePicNums(int frame_num);

  bool UpdateMaxNumReorderFrames(const H264SPS* sps);

  // Prepare reference picture lists for the current frame.
  void PrepareRefPicLists();
  // Prepare reference picture lists for the given slice.
  bool ModifyReferencePicLists(const H264SliceHeader* slice_hdr,
                               H264Picture::Vector* ref_pic_list0,
                               H264Picture::Vector* ref_pic_list1);

  // Construct initial reference picture lists for use in decoding of
  // P and B pictures (see 8.2.4 in spec).
  void ConstructReferencePicListsP();
  void ConstructReferencePicListsB();

  // Helper functions for reference list construction, per spec.
  int PicNumF(const H264Picture& pic);
  int LongTermPicNumF(const H264Picture& pic);

  // Perform the reference picture lists' modification (reordering), as
  // specified in spec (8.2.4).
  //
  // |list| indicates list number and should be either 0 or 1.
  bool ModifyReferencePicList(const H264SliceHeader* slice_hdr,
                              int list,
                              H264Picture::Vector* ref_pic_listx);

  // Perform reference picture memory management operations (marking/unmarking
  // of reference pictures, long term picture management, discarding, etc.).
  // See 8.2.5 in spec.
  bool HandleMemoryManagementOps(scoped_refptr<H264Picture> pic);
  bool ReferencePictureMarking(scoped_refptr<H264Picture> pic);
  bool SlidingWindowPictureMarking();

  // Handle a gap in frame_num in the stream up to |frame_num|, by creating
  // "non-existing" pictures (see spec).
  bool HandleFrameNumGap(int frame_num);

  // Start processing a new frame.
  H264Accelerator::Status StartNewFrame(const H264SliceHeader* slice_hdr);

  // All data for a frame received, process it and decode.
  H264Accelerator::Status FinishPrevFrameIfPresent();

  // Called after we are done processing |pic|. Performs all operations to be
  // done after decoding, including DPB management, reference picture marking
  // and memory management operations.
  // This will also output pictures if any have become ready to be outputted
  // after processing |pic|.
  bool FinishPicture(scoped_refptr<H264Picture> pic);

  // Clear DPB contents and remove all surfaces in DPB from *in_use_ list.
  // Cleared pictures will be made available for decode, unless they are
  // at client waiting to be displayed.
  void ClearDPB();

  // Commits all pending data for HW decoder and starts HW decoder.
  H264Accelerator::Status DecodePicture();

  // Notifies client that a picture is ready for output.
  bool OutputPic(scoped_refptr<H264Picture> pic);

  // Output all pictures in DPB that have not been outputted yet.
  bool OutputAllRemainingPics();

  // Decoder state.
  State state_;

  // The colorspace for the h264 container.
  const VideoColorSpace container_color_space_;

  // Parser in use.
  H264Parser parser_;

  // Most recent call to SetStream().
  raw_ptr<const uint8_t, DanglingUntriaged> current_stream_ = nullptr;
  size_t current_stream_size_ = 0;

  // Decrypting config for the most recent data passed to SetStream().
  std::unique_ptr<DecryptConfig> current_decrypt_config_;

  // Secure handle to pass through to the accelerator when doing secure playback
  // on ARM.
  uint64_t secure_handle_ = 0;

  // Keep track of when SetStream() is called so that
  // H264Accelerator::SetStream() can be called.
  bool current_stream_has_been_changed_ = false;

  // DPB in use.
  H264DPB dpb_;

  // Current stream buffer id; to be assigned to pictures decoded from it.
  int32_t stream_id_ = -1;

  // Picture currently being processed/decoded.
  scoped_refptr<H264Picture> curr_pic_;

  // Reference picture lists, constructed for each frame.
  H264Picture::Vector ref_pic_list_p0_;
  H264Picture::Vector ref_pic_list_b0_;
  H264Picture::Vector ref_pic_list_b1_;

  // Global state values, needed in decoding. See spec.
  int max_frame_num_;
  int max_pic_num_;
  int max_long_term_frame_idx_;
  size_t max_num_reorder_frames_;

  int prev_frame_num_;
  int prev_ref_frame_num_;
  int prev_frame_num_offset_;
  bool prev_has_memmgmnt5_;

  // Values related to previously decoded reference picture.
  bool prev_ref_has_memmgmnt5_;
  int prev_ref_top_field_order_cnt_;
  int prev_ref_pic_order_cnt_msb_;
  int prev_ref_pic_order_cnt_lsb_;
  H264Picture::Field prev_ref_field_;

  // Currently active SPS and PPS.
  int curr_sps_id_;
  int curr_pps_id_;

  // Last PPS that was parsed. Used for full sample encryption, which has the
  // assumption this is streaming content which does not switch between
  // different PPSes in the stream (they are present once in the container for
  // the stream).
  int last_parsed_pps_id_;

  // Current NALU and slice header being processed.
  std::unique_ptr<H264NALU> curr_nalu_;
  std::unique_ptr<H264SliceHeader> curr_slice_hdr_;

  // Encrypted NALUs preceding a fully encrypted (CENCv1) slice NALU. We need to
  // save these that are part of a single sample so they can all be decrypted
  // together.
  std::vector<base::raw_span<const uint8_t, DanglingUntriaged>>
      prior_cencv1_nalus_;
  std::vector<SubsampleEntry> prior_cencv1_subsamples_;

  // These are std::nullopt unless get recovery point SEI message after Reset.
  // A frame_num of the frame at output order that is correct in content.
  std::optional<int> recovery_frame_num_;
  // A value in the recovery point SEI message to compute |recovery_frame_num_|
  // later.
  std::optional<int> recovery_frame_cnt_;

  // Output picture size.
  gfx::Size pic_size_;
  // Output visible cropping rect.
  gfx::Rect visible_rect_;

  // Profile of input bitstream.
  VideoCodecProfile profile_;
  // Bit depth of input bitstream.
  uint8_t bit_depth_ = 0;
  // Chroma subsampling format of input bitstream.
  VideoChromaSampling chroma_sampling_ = VideoChromaSampling::kUnknown;
  // Video picture color space of input bitstream.
  VideoColorSpace picture_color_space_;
  // HDR metadata in the bitstream.
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  // PicOrderCount of the previously outputted frame.
  int last_output_poc_;

  const std::unique_ptr<H264Accelerator> accelerator_;

  // Whether the current decoder will utilize reference lists.
  const bool requires_ref_lists_;
};

}  // namespace media

#endif  // MEDIA_GPU_H264_DECODER_H_
