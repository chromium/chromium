// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_H264_DECODER_H_

// build_config.h must come before BUILDFLAG()
#include "build/build_config.h"

// ChromeOS specific header; does not exist upstream
#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/h264-ctrls-upstream.h>
#endif

#include <map>
#include <set>

#include "base/files/memory_mapped_file.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/video/h264_parser.h"

namespace media {
namespace v4l2_test {

struct H264SliceMetadata;

// PreviousRefPicOrder contains data regarding the picture
// order counts for the previously decoded frame.
struct PreviousRefPicOrder {
  int prev_ref_pic_order_cnt_msb = 0;
  int prev_ref_pic_order_cnt_lsb = 0;
};

// H264DPB is a class representing a Decoded Picture Buffer (DPB).
// The DPB is a map of H264 picture slice metadata objects that
// describe the pictures used in the H.264 decoding process.
class H264DPB : public std::map<uint64_t, H264SliceMetadata> {
 public:
  H264DPB() = default;
  ~H264DPB() = default;

  H264DPB(const H264DPB&) = delete;
  H264DPB& operator=(const H264DPB&) = delete;

  // Returns number of Reference H264SliceMetadata elements
  // in the DPB.
  int CountRefPics();
  // Deletes input H264SliceMetadata object from the DPB.
  void Delete(const H264SliceMetadata& pic);
  // Deletes any H264SliceMetadata object from DPB that is considered
  // to be unused by the decoder.
  // An H264SliceMetadata is unused if it has been outputted and is not a
  // reference picture.
  void DeleteUnused();
  // Removes the reference picture marking from the lowest frame
  // number H264SliceMetadata object in the DPB. This is used for implementing
  // a sliding window DPB replacement algorithm.
  void UnmarkLowestFrameNumWrapShortRefPic();
  // Returns a vector of H264SliceMetadata objects that have not been output
  // by the H264 Decoder.
  std::vector<H264SliceMetadata*> GetNotOutputtedPicsAppending();
  // Updates every H264SliceMetadata object in the DPB to indicate that they are
  // not reference elements.
  void MarkAllUnusedRef();
  // Updates each H264SliceMetadata object in DPB's frame num wrap
  // based on the max frame num.
  void UpdateFrameNumWrap(const int curr_frame_num, const int max_frame_num);
};

class H264Decoder : public VideoDecoder {
 public:
  H264Decoder(const H264Decoder&) = delete;
  H264Decoder& operator=(const H264Decoder&) = delete;
  ~H264Decoder() override;

  // Creates a H264Decoder after verifying that the bitstream is h.264
  // and the underlying implementation supports H.264 slice decoding.
  static std::unique_ptr<H264Decoder> Create(
      const base::MemoryMappedFile& stream);

  // Parses next frame from IVF stream and decodes the frame.  This method will
  // place the Y, U, and V values into the respective vectors and update the
  // size with the display area size of the decoded frame.
  VideoDecoder::Result DecodeNextFrame(std::vector<uint8_t>& y_plane,
                                       std::vector<uint8_t>& u_plane,
                                       std::vector<uint8_t>& v_plane,
                                       gfx::Size& size,
                                       const int frame_number) override;

 private:
  H264Decoder(std::unique_ptr<H264Parser> parser,
              std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
              std::unique_ptr<V4L2Queue> OUTPUT_queue,
              std::unique_ptr<V4L2Queue> CAPTURE_queue);

  // Processes NALU's until reaching the end of the current frame.  To
  // know the end of the current frame it may be necessary to start parsing
  // the next frame.  If this occurs the NALU that was parsed needs to be
  // held over until the next frame.  This is done in |pending_nalu_|
  // Not every frame has a SPS/PPS associated with it.  The SPS/PPS must
  // occur on an IDR frame.  Store the last seen slice header in
  // |pending_slice_header_| so it will be available for the next frame.
  H264Parser::Result ProcessNextFrame(
      const int frame_number,
      std::unique_ptr<H264SliceHeader>* resulting_slice_header);

  // Sends IOCTL call to device with the frame's SPS, PPS, and Scaling Matrix
  // data which indicates the beginning of a new frame.
  VideoDecoder::Result StartNewFrame(
      int sps_id,
      int pps_id,
      H264SliceHeader* slice_hdr,
      H264SliceMetadata* slice_metadata,
      v4l2_ctrl_h264_decode_params* v4l2_decode_param);

  // Finishes frame processing for the current decoded frame. Transmits decode
  // parameters via IOCTL Ext Ctrls. It continues to execute decoded ref
  // picture marking process as defined in section 8.2.5. Finally, using
  // the DPB, transmit H264 Slices to the device for the current frame.
  VideoDecoder::Result FinishFrame(
      const H264SliceHeader& curr_slice,
      int frame_num,
      v4l2_ctrl_h264_decode_params& v4l2_decode_param,
      H264SliceMetadata& slice_metadata);

  // Initializes H264 Slice Metadata based on slice header and
  // based on H264 specifications which it calculates its pic order count.
  VideoDecoder::Result InitializeSliceMetadata(
      const H264SliceHeader& slice_hdr,
      const H264SPS* sps,
      H264SliceMetadata* slice_metadata) const;

  // Returns all CAPTURE buffer indexes that can be reused for a
  // VIDIOC_QBUF ioctl call.
  std::set<uint32_t> GetReusableReferenceSlots(
      const MmappedBuffer& buffer,
      std::set<uint32_t> queued_buffer_ids);

  const std::unique_ptr<H264Parser> parser_;

  // Previous pic order counts from previous frame
  PreviousRefPicOrder prev_pic_order_;

  int global_pic_count_ = 0;

  H264DPB dpb_;

  std::unique_ptr<H264NALU> pending_nalu_;
  std::unique_ptr<H264SliceHeader> pending_slice_header_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
