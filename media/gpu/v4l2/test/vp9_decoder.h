// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_

#include <linux/v4l2-controls.h>

#include <set>

#include "base/files/memory_mapped_file.h"
#include "media/base/video_types.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp9_parser.h"

namespace media {
namespace v4l2_test {

// A Vp9Decoder decodes VP9-encoded IVF streams using v4l2 ioctl calls.
class Vp9Decoder : public VideoDecoder {
 public:
  Vp9Decoder(const Vp9Decoder&) = delete;
  Vp9Decoder& operator=(const Vp9Decoder&) = delete;
  ~Vp9Decoder() override;

  // Creates a Vp9Decoder after verifying that the underlying implementation
  // supports VP9 stateless decoding.
  static std::unique_ptr<Vp9Decoder> Create(
      const base::MemoryMappedFile& stream);

  // Parses next frame from IVF stream and decodes the frame. This method will
  // place the Y, U, and V values into the respective vectors and update the
  // size with the display area size of the decoded frame.
  VideoDecoder::Result DecodeNextFrame(const int frame_number,
                                       std::vector<uint8_t>& y_plane,
                                       std::vector<uint8_t>& u_plane,
                                       std::vector<uint8_t>& v_plane,
                                       gfx::Size& size,
                                       BitDepth& bit_depth) override;

 private:
  Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
             std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
             gfx::Size display_resolution);

  // Reads next frame from IVF stream and its size into |vp9_frame_header|
  // and |size| respectively.
  Vp9Parser::Result ReadNextFrame(Vp9FrameHeader& vp9_frame_header,
                                  gfx::Size& size);

  // Copies the frame data into the V4L2 buffer of OUTPUT |queue|.
  void CopyFrameData(const Vp9FrameHeader& frame_hdr,
                     std::unique_ptr<V4L2Queue>& queue);

  // Sets up per frame parameters |v4l2_frame_params| needed for VP9 decoding
  // with VIDIOC_S_EXT_CTRLS ioctl call.
  void SetupFrameParams(const Vp9FrameHeader& frame_hdr,
                        struct v4l2_ctrl_vp9_frame* v4l2_frame_params);

  // Refreshes |ref_frames_| slots with the current |buffer| and returns
  // |reusable_buffer_slots| to indicate which CAPTURE buffers can be reused
  // for VIDIOC_QBUF ioctl call.
  std::set<int> RefreshReferenceSlots(uint8_t refresh_frame_flags,
                                      scoped_refptr<MmappedBuffer> buffer,
                                      uint32_t last_queued_buffer_id);

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;

  // Supports parsed compressed headers
  const bool supports_compressed_headers_;

  // VP9-specific data.
  const std::unique_ptr<Vp9Parser> vp9_parser_;

  // Reference frames currently in use.
  std::array<scoped_refptr<MmappedBuffer>, kVp9NumRefFrames> ref_frames_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_
