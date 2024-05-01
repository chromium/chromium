// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_H264_DECODER_H_

#include <queue>

#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ref.h"
#include "media/gpu/v4l2/test/h264_dpb.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/gpu/v4l2/test/video_decoder.h"

namespace media {
namespace v4l2_test {

// PreviousRefPicOrder contains data regarding the picture
// order counts for the previously decoded frame.
struct PreviousRefPicOrder {
  int prev_ref_pic_order_cnt_msb = 0;
  int prev_ref_pic_order_cnt_lsb = 0;
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
  H264Decoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
              gfx::Size display_resolution,
              const base::MemoryMappedFile& data_stream);

  // Processes NALU's until reaching the start of the new frame. Function
  // starts by starting a new frame and transmitting control data through
  // ioctl calls. The function will then parse and process each NALU until
  // reaching the start of the next frame, at which point it will finish
  // processing the picture and add it to the picture queue.
  void ProcessNextFrame();

  // Sends IOCTL call to device with the frame's SPS, PPS, and Scaling Matrix
  // data which indicates the beginning of a new frame. Additionally
  // this initializes the decode parameter's dpb parameter from the DPB.
  VideoDecoder::Result StartNewFrame(bool is_OUTPUT_queue_new,
                                     H264SliceMetadata* slice_metadata);

  // Finishes frame processing for the current decoded frame. Performs decoded
  // ref picture marking process as defined in section 8.2.5. Finally, using
  // the DPB, transmit H264 Slices to the slice_ready_queue_.
  void FinishPicture(H264SliceMetadata picture, const int sps_id);

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

  // Calculates decoding parameters based on SPS corresponding to sps_id.
  // If decoding parameters change, this can result in flushing the DPB.
  void ProcessSPS(const int sps_id);

  // Transmits the current slice data to the OUTPUT queue and transmits it
  // to the device via an VIDIOC_QBUF ioctl call.
  VideoDecoder::Result SubmitSlice();

  // Moves all non output pictures in the DPB to the slice_ready_queue.
  // Finishes by clearing the entire DPB.
  void FlushDPB();

  // Initializes the H.264 Decoder to Process the initial SPS NALU as well
  // as to iterate until it reaches the start of a new frame for the
  // ProcessNextFrame function to be able to work properly.
  void InitializeDecoderLogic();

  std::unique_ptr<H264Parser> parser_;

  // Previous pic order counts from previous frame
  PreviousRefPicOrder prev_pic_order_;

  int global_pic_count_ = 0;

  H264DPB dpb_;

  std::queue<H264SliceMetadata> slice_ready_queue_;

  std::unique_ptr<H264SliceHeader> curr_slice_hdr_;

  // Number of non-outputted pictures needed in DPB before a picture
  // can be outputted.
  size_t max_num_reorder_frames_;

  // Decoding profile parameters
  gfx::Size pic_size_;
  VideoCodecProfile profile_;
  uint8_t bit_depth_ = -1;

  bool stream_finished_;

  const raw_ref<const base::MemoryMappedFile> data_stream_;

  int prev_frame_num_ = -1;
  int prev_frame_num_offset_ = -1;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
