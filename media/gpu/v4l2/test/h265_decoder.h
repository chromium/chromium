
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_H265_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_H265_DECODER_H_

#include "media/gpu/v4l2/test/video_decoder.h"

#include "media/gpu/v4l2/test/h265_dpb.h"
#include "media/video/h265_parser.h"

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
                                       gfx::Size& size) override;

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

  // This is the main method used for running the decode loop. It will try to
  // decode all frames in the stream until there is a configuration change,
  // error or the end of the stream is reached.
  DecodeResult Decode();

  // Decoder state.
  State state_ = kAfterReset;

  std::unique_ptr<H265Parser> parser_;

  // Picture currently being processed/decoded.
  scoped_refptr<H265Picture> curr_pic_;

  // Used to identify first picture in decoding order
  // or first picture that follows an EOS NALU.
  bool first_picture_ = true;

  const base::MemoryMappedFile& data_stream_;

  // Currently active PPS.
  int curr_pps_id_ = -1;

  // Current NALU and slice header being processed.
  std::unique_ptr<H265NALU> curr_nalu_;
  std::unique_ptr<H265SliceHeader> curr_slice_hdr_;
  std::unique_ptr<H265SliceHeader> last_slice_hdr_;

  // If this is true, then the entire steam has been parsed.
  bool is_stream_over_ = false;
};
}  // namespace v4l2_test
}  // namespace media
#endif  // MEDIA_GPU_V4L2_TEST_H265_DECODER_H_
