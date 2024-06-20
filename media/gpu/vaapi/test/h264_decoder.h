// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_H264_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_H264_DECODER_H_

#include <queue>

#include "media/gpu/vaapi/test/h264_dpb.h"
#include "media/gpu/vaapi/test/h264_vaapi_wrapper.h"
#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/parsers/h264_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace vaapi_test {

class H264Decoder : public VideoDecoder {
 public:
  H264Decoder(const uint8_t* stream_data,
              size_t stream_len,
              const VaapiDevice& va_device,
              SharedVASurface::FetchPolicy fetch_policy);
  H264Decoder(const H264Decoder&) = delete;
  H264Decoder& operator=(const H264Decoder&) = delete;
  ~H264Decoder() override;

  // VideoDecoder implementation.
  VideoDecoder::Result DecodeNextFrame() override;

 private:
  // Decode the next frame in the bitstream.
  void DecodeNextFrameInStream();

  // Process a slice.
  void ProcessSlice();

  // Process an SPS NALU.
  void UpdateSequenceParams();

  // Process a PPS NALU.
  void UpdatePictureParams();

  // Issue the final commands to VAAPI to decode the slices we have been
  // inputing.
  void DecodeFrame();

  // Finalize the picture and handle reference picture buffer memory management.
  void FinishPicture(scoped_refptr<H264Picture> pic);

  // Process SPS and PPS NALUs until we hit a slice header.
  bool GetStreamMetadata();

  // Detects if the last slice header refers to a new frame.
  bool IsNewFrame();

  // Populate some local variables based on a slice NALU.
  void ExtractSliceHeader();

  // Create a new picture and hand some metadata to VAAPI.
  void StartNewFrame();

  // Initialize a picture.
  bool InitCurrPicture(const H264SliceHeader* slice_hdr);

  // Some helper functions for preparing the reference picture buffers. H.264
  // has separate P and B frame reference picture buffers.
  void ConstructReferencePicListsP();
  void ConstructReferencePicListsB();

  // Re-order the reference frame buf based on the current frame number.
  void UpdatePicNums(int frame_num);

  // Some helper functions for handling gaps in the frame data.
  bool InitNonexistingPicture(scoped_refptr<H264Picture> pic,
                              int frame_num,
                              bool ref);
  bool HandleFrameNumGap(int frame_num);

  // Calculate Pic Order Counts. This is approximately what we would intuitively
  // think of as the frame number in display order, except it wraps around.
  bool CalculatePicOrderCounts(scoped_refptr<H264Picture> pic);
  void UpdateMaxNumReorderFrames(const H264SPS* sps);

  // Populate the reference lists that we send to VAAPI using data from the
  // DPB.
  bool ModifyReferencePicLists(const H264SliceHeader* slice_hdr,
                               H264Picture::Vector* ref_pic_list0,
                               H264Picture::Vector* ref_pic_list1);
  bool ModifyReferencePicList(const H264SliceHeader* slice_hdr,
                              int list,
                              H264Picture::Vector* ref_pic_listx);

  // Helps manage the DPB and mark what we need to keep for reference and what
  // we can mark as unused. Note that a frame might still be kept around even if
  // it isn't being used as a reference because we might need to output it.
  // Normal H.264 decoder implementations have finer grained cache control, but
  // since we can only output one picture DecodeNextFrame() call, we have to
  // keep a pretty big cache around until the output catches up to the decode.
  bool ReferencePictureMarking(scoped_refptr<H264Picture> pic);
  bool HandleMemoryManagementOps(scoped_refptr<H264Picture> pic);
  bool SlidingWindowPictureMarking();
  int PicNumF(const H264Picture& pic) const;
  int LongTermPicNumF(const H264Picture& pic) const;
  static void ShiftRightAndInsert(H264Picture::Vector* v,
                                  int from,
                                  int to,
                                  scoped_refptr<H264Picture> pic);

  // Used for computing how large the DPB should be for each level.
  static uint32_t H264LevelToMaxDpbMbs(uint8_t level);

  // Output all remaining images in the DPB.
  void FlushDPB();

  // H.264 NALU parser
  std::unique_ptr<H264Parser> parser_;

  // The last sequence param id.
  int curr_sps_id_;

  // The last picture param id.
  int curr_pps_id_;

  // The last slice header parsed.
  std::unique_ptr<H264SliceHeader> curr_slice_hdr_;

  // The last NALU that was parsed.
  std::unique_ptr<H264NALU> curr_nalu_;

  // Picture we are currently decoding. Not necessarily the one we will output
  // since H.264 sends pictures out of order.
  scoped_refptr<H264Picture> curr_picture_;

  // Set once we hit EOS.
  bool is_stream_over_;

  // Reference picture lists. P = forward prediction, B = bidirectional
  // prediction.
  H264Picture::Vector ref_pic_list_p0_;
  H264Picture::Vector ref_pic_list_b0_;
  H264Picture::Vector ref_pic_list_b1_;

  // Some primitive related to frame number to help with the ordering of the
  // reference pictures. Note that "frame_num" in this instance does not
  // intuitively refer to the index of the frame when it gets output, but just
  // the index of the frame in the bitstream. Pic order count is more closely
  // related to output order, but it can also wrap.
  int max_frame_num_;
  int max_pic_num_;
  int max_long_term_frame_idx_;
  size_t max_num_reorder_frames_;
  int prev_frame_num_;
  int prev_ref_frame_num_;
  int prev_frame_num_offset_;
  bool prev_has_memmgmnt5_;

  // These are std::nullopt unless get recovery point SEI message after Reset.
  // A frame_num of the frame at output order that is correct in content.
  std::optional<int> recovery_frame_num_;
  // A value in the recovery point SEI message to compute |recovery_frame_num_|
  // later.
  std::optional<int> recovery_frame_cnt_;

  // Buffer object to keep track of our reference images.
  H264DPB dpb_;

  // Some handy abstractions for issuing VAAPI calls.
  H264VaapiWrapper va_wrapper_;

  // Values related to previously decoded reference picture.
  bool prev_ref_has_memmgmnt5_;
  int prev_ref_top_field_order_cnt_;
  int prev_ref_pic_order_cnt_msb_;
  int prev_ref_pic_order_cnt_lsb_;
  H264Picture::Field prev_ref_field_;

  // The actual size of the output picture.
  gfx::Rect visible_rect_;

  // Pictures to flush in descending POC order.
  std::queue<scoped_refptr<H264Picture>> output_queue;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_H264_DECODER_H_
