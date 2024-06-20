
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_H265_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_H265_DECODER_H_

#include <queue>

#include "base/memory/raw_ref.h"
#include "media/base/video_codecs.h"
#include "media/gpu/v4l2/test/h265_dpb.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/parsers/h265_parser.h"

namespace media {
namespace v4l2_test {

class H265Decoder : public VideoDecoder {
 public:
  H265Decoder(const H265Decoder&) = delete;
  H265Decoder& operator=(const H265Decoder&) = delete;
  ~H265Decoder() override;

  // Creates a H265Decoder after verifying that the bitstream is HEVC
  // and the underlying implementation supports HEVC slice decoding.
  static std::unique_ptr<H265Decoder> Create(
      const base::MemoryMappedFile& stream);

  // Parses next frame from the input and decodes the frame. This method will
  // place the Y, U, and V values into the respective vectors and update the
  // size with the display area size of the decoded frame.
  VideoDecoder::Result DecodeNextFrame(const int frame_number,
                                       std::vector<uint8_t>& y_plane,
                                       std::vector<uint8_t>& u_plane,
                                       std::vector<uint8_t>& v_plane,
                                       gfx::Size& size,
                                       BitDepth& bit_depth) override;

 private:
  H265Decoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
              gfx::Size display_resolution,
              const base::MemoryMappedFile& data_stream);

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
    // Processes PPS before processing the current slice.
    kTryPreprocessCurrentSlice,
    // Makes sure processing the previous frame is finished.
    kEnsurePicture,
    // Processes a new frame.
    kTryNewFrame,
    // Processes the current slice.
    kTryCurrentSlice,
    // Error in decode, can't continue.
    kError,
  };

  enum DecodeResult {
    kConfigChange,        // This is returned when some configuration (e.g.
                          // profile or picture size) is changed. A client may
                          // need to apply the client side configuration
                          // properly (e.g. allocate buffers with the new
                          // resolution).
    kRanOutOfStreamData,  // Need more stream data to proceed.
    kOk,
  };

  // Output all pictures in DPB that have not been outputted yet.
  bool OutputAllRemainingPics();

  // Output all pictures in DPB and clear the DPB.
  bool Flush();

  // Process H265 bitstream PPS (picture parameter set).
  bool ProcessPPS(int pps_id, bool* need_new_buffers);

  // Process current slice header to discover if we need to start a new picture,
  // finishing up the current one.
  bool PreprocessCurrentSlice();

  // Process current slice as a slice of the current picture.
  bool ProcessCurrentSlice();

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

  // Notifies client that a picture is ready for output.
  bool OutputPic(scoped_refptr<H265Picture> pic);

  // Performs DPB management operations for |curr_pic_| by removing no longer
  // needed entries from the DPB and outputting pictures from the DPB. |sps|
  // should be the corresponding SPS for |curr_pic_|.
  bool PerformDpbOperations(const H265SPS* sps);

  // Start processing a new frame. This also generates all the POC and output
  // variables for the frame, generates reference picture lists, performs
  // reference picture marking, DPB management and picture output.
  bool StartNewFrame(const H265SliceHeader* slice_hdr);

  // Returns all CAPTURE buffer indexes that can be reused
  // for VIDIOC_QBUF ioctl call.
  std::set<uint32_t> GetReusableReferenceSlots(
      const MmappedBuffer& buffer,
      const std::set<uint32_t>& queued_buffer_ids);

  // Commits all pending data for HW decoder and starts HW decoder.
  bool DecodePicture();

  // Called after we are done processing |pic|.
  void FinishPicture(scoped_refptr<H265Picture> pic);

  // All data for a frame received, process it and decode.
  void FinishPrevFrameIfPresent();

  // This is the main method used for running the decode loop. It will try to
  // decode all frames in the stream until there is a configuration change,
  // error or the end of the stream is reached.
  DecodeResult Decode();

  // Decoder state.
  State state_ = kAfterReset;

  std::unique_ptr<H265Parser> parser_;

  // DPB in use.
  H265DPB dpb_;

  // Picture currently being processed/decoded.
  scoped_refptr<H265Picture> curr_pic_;

  int global_pic_count_ = 0;

  // Used to identify first picture in decoding order
  // or first picture that follows an EOS NALU.
  bool first_picture_ = true;

  const raw_ref<const base::MemoryMappedFile> data_stream_;

  // Global state values needed for decoding. See spec.
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
  // Chroma sampling format of input bitstream.
  VideoChromaSampling chroma_sampling_ = VideoChromaSampling::kUnknown;

  // If this is true, then the entire steam has been parsed.
  bool is_stream_over_ = false;

  // Checks whether |OUTPUT_queue_| is newly created at the current frame.
  bool is_OUTPUT_queue_new_ = true;

  // Stores decoded frames ready to be outputted in a queue.
  std::queue<scoped_refptr<H265Picture>> frames_ready_to_be_outputted_;
};
}  // namespace v4l2_test
}  // namespace media
#endif  // MEDIA_GPU_V4L2_TEST_H265_DECODER_H_
