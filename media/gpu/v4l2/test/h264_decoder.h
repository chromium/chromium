// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_H264_DECODER_H_

#include <set>

#include "base/files/memory_mapped_file.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/video/h264_parser.h"

namespace media {
namespace v4l2_test {

struct H264SliceMetadata;

// H264DPB is a class representing a Decoded Picture Buffer (DPB).
// The DPB is a vector of H264 picture slice metadata objects that
// describe the pictures used in the H.264 decoding process.
class H264DPB : std::vector<std::unique_ptr<H264SliceMetadata>> {
 public:
  H264DPB() = default;
  ~H264DPB() = default;

  H264DPB(const H264DPB&) = delete;
  H264DPB& operator=(const H264DPB&) = delete;

  // Deletes all H264SliceMetadata elements from DPB.
  void ClearDPB();
  // Returns number of Reference H264SliceMetadata elements
  // in the DPB.
  int CountRefPics();
  // Deletes input H264SliceMetadata object from the DPB.
  void Delete(H264SliceMetadata* pic);
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
  void StorePic(H264SliceMetadata* pic);

 private:
  // A vector of H264SliceMetadata objects which will be created by the H264
  // Decoder and sent to this class for slice processing.
  std::vector<std::unique_ptr<H264SliceMetadata>> dpb_;
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
  VideoDecoder::Result DecodeNextFrame(std::vector<char>& y_plane,
                                       std::vector<char>& u_plane,
                                       std::vector<char>& v_plane,
                                       gfx::Size& size,
                                       const int frame_number) override;

 private:
  H264Decoder(std::unique_ptr<H264Parser> parser,
              std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
              std::unique_ptr<V4L2Queue> OUTPUT_queue,
              std::unique_ptr<V4L2Queue> CAPTURE_queue);

  // Processes NALU's until reaching the end of the current frame.  This method
  // will send Ext Ctrls via IOCTL calls to indicate the start of a frame.
  H264Parser::Result ProcessNextFrame(
      std::unique_ptr<H264SliceHeader>* resulting_slice_header);

  // Sends IOCTL call to device with the frame's SPS, PPS, and Scaling Matrix
  // data which indicates the beginning of a new frame.
  VideoDecoder::Result StartNewFrame(int sps_id, int pps_id);

  // Transmits each H264 Slice associated with the current frame to the
  // device.  Additionally sends Decode Parameters and Decode Mode
  // via IOCTL Ext Ctrls.
  VideoDecoder::Result SubmitSlice(H264SliceHeader curr_slice, int frame_num);

  // Returns all CAPTURE buffer indexes that can be reused for a
  // VIDIOC_QBUF ioctl call.
  std::set<uint32_t> GetReusableReferenceSlots(
      const MmapedBuffer& buffer,
      std::set<uint32_t> queued_buffer_indexes);

  const std::unique_ptr<H264Parser> parser_;

  std::unique_ptr<H264NALU> pending_nalu_;
  std::unique_ptr<H264SliceHeader> pending_slice_header_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_H264_DECODER_H_
