// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_H265_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_H265_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/subsample_entry.h"
#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/test/h265_dpb.h"
#include "media/gpu/vaapi/test/h265_vaapi_wrapper.h"
#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/parsers/h265_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media::vaapi_test {

// A H265Decoder decodes hevc streams using direct libva calls.
//
// NOTE(b/239718937): This decoder does not handle encrypted streams,
// since it not considered practical to implement this in a testing
// binary.
// TODO(b/241479848): Revisit the decoder implementations for each codec
// for refactoring out pieces that can be shared between the browser and
// the test binary.
class H265Decoder : public VideoDecoder {
 public:
  H265Decoder(const uint8_t* stream_data,
              size_t stream_len,
              const VaapiDevice& va_device,
              SharedVASurface::FetchPolicy fetch_policy);

  H265Decoder(const H265Decoder&) = delete;
  H265Decoder& operator=(const H265Decoder&) = delete;

  ~H265Decoder() override;

  void Flush();
  void Reset();

  // VideoDecoder implementation.
  [[nodiscard]] VideoDecoder::Result DecodeNextFrame() override;

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

  enum DecodeResult {
    kConfigChange,        // This is returned when some configuration (e.g.
                          // profile or picture size) is changed. A client may
                          // need to apply the client side configuration
                          // properly (e.g. allocate buffers with the new
                          // resolution).
    kRanOutOfStreamData,  // Need more stream data to proceed.
    kOk,                  // Decoded a frame successfully.
  };

  // Process H265 stream structures.
  bool ProcessPPS(int pps_id, bool* need_new_buffers);

  // Process current slice header to discover if we need to start a new picture,
  // finishing up the current one.
  bool PreprocessCurrentSlice();

  // Process current slice as a slice of the current picture.
  bool ProcessCurrentSlice();

  // Start processing a new frame. This also generates all the POC and output
  // variables for the frame, generates reference picture lists, performs
  // reference picture marking, DPB management and picture output.
  bool StartNewFrame(const H265SliceHeader* slice_hdr);

  // All data for a frame received, process it and decode.
  bool FinishPrevFrameIfPresent();

  // Called after we are done processing |pic|.
  void FinishPicture(scoped_refptr<H265Picture> pic);

  // Commits all pending data for HW decoder and starts HW decoder.
  bool DecodePicture();

  // Notifies client that a picture is ready for output.
  void OutputPic(scoped_refptr<H265Picture> pic);

  // Output all pictures in DPB that have not been outputted yet.
  void OutputAllRemainingPics();

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

  // This is the main method used for running the decode loop. It will try to
  // decode a single frame in the stream, or up until it reaches either a
  // configuration change, or the end of the stream.
  DecodeResult DecodeNALUs();

  // Decoder state.
  State state_;

  // Parser in use.
  H265Parser parser_;

  // DPB in use.
  H265DPB dpb_;

  // Picture currently being processed/decoded.
  scoped_refptr<H265Picture> curr_pic_;

  // Used to identify first picture in decoding order or first picture that
  // follows an EOS NALU.
  bool first_picture_ = true;

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

  H265VaapiWrapper wrapper_;

  // If this is true, then the entire steam has been parsed.
  bool is_stream_over_ = false;

  std::queue<scoped_refptr<H265Picture>> output_queue;
};

}  // namespace media::vaapi_test

#endif  // MEDIA_GPU_VAAPI_TEST_H265_DECODER_H_
