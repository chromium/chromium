// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MEDIA_GPU_V4L2_TEST_VP8_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_VP8_DECODER_H_

#include <linux/v4l2-controls.h>

#include <set>

#include "base/files/memory_mapped_file.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp8_parser.h"

namespace media {
namespace v4l2_test {

class Vp8Decoder : public VideoDecoder {
 public:
  Vp8Decoder(const Vp8Decoder&) = delete;
  Vp8Decoder& operator=(const Vp8Decoder&) = delete;
  ~Vp8Decoder() override;

  // Creates a Vp8Decoder after verifying that the underlying implementation
  // supports VP8 stateless decoding.
  static std::unique_ptr<Vp8Decoder> Create(
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
  Vp8Decoder(std::unique_ptr<IvfParser> ivf_parser,
             std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
             gfx::Size display_resolution);
  enum ParseResult { kOk, kEOStream, kError };

  // Reads next frame from IVF stream into |vp8_frame_header|
  ParseResult ReadNextFrame(Vp8FrameHeader& vp8_frame_header);

  // Sets up per frame parameters |v4l2_frame_headers| needed for VP8 decoding
  // with VIDIOC_S_EXT_CTRLS ioctl call.
  // Syntax from VP8 specs: https://datatracker.ietf.org/doc/rfc6386/
  struct v4l2_ctrl_vp8_frame SetupFrameHeaders(const Vp8FrameHeader& frame_hdr);

  // Refreshes |ref_frames_| slots and returns the CAPTURE buffers that
  // can be reused for VIDIOC_QBUF ioctl call.
  std::set<int> RefreshReferenceSlots(const Vp8FrameHeader& frame_hdr,
                                      MmappedBuffer* buffer,
                                      std::set<uint32_t> queued_buffer_ids);

  // Manages buffers holding reference frames and return buffer indexes
  // |reusable_buffer_slots| that can be reused in CAPTURE queue.
  void UpdateReusableReferenceBufferSlots(const Vp8FrameHeader& frame_hdr,
                                          const size_t curr_ref_frame_index,
                                          std::set<int>& reusable_buffer_slots);

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;

  // VP8-specific data.
  const std::unique_ptr<Vp8Parser> vp8_parser_;

  // Reference frames currently in use.
  std::array<scoped_refptr<MmappedBuffer>, kNumVp8ReferenceBuffers> ref_frames_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_VP8_DECODER_H_
