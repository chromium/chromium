// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_AV1_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_AV1_DECODER_H_

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/gpu/v4l2/test/video_decoder.h"

#include "media/filters/ivf_parser.h"
// For libgav1::ObuSequenceHeader. absl::optional demands ObuSequenceHeader to
// fulfill std::is_trivially_constructible if it is forward-declared. But
// ObuSequenceHeader doesn't.
#include "third_party/libgav1/src/src/obu_parser.h"

// TODO(stevecho): This is temporary until the change to define
// V4L2_PIX_FMT_AV1_FRAME lands in videodev2.h.
// https://patchwork.linuxtv.org/project/linux-media/patch/20210810220552.298140-2-daniel.almeida@collabora.com/
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME                        \
  v4l2_fourcc('A', 'V', '1', 'F') /* AV1 parsed frame \
                                   */
#endif

namespace media {

class IvfParser;

namespace v4l2_test {

// A Av1Decoder decodes AV1-encoded IVF streams using v4l2 ioctl calls.
class Av1Decoder : public VideoDecoder {
 public:
  Av1Decoder(const Av1Decoder&) = delete;
  Av1Decoder& operator=(const Av1Decoder&) = delete;
  ~Av1Decoder() override;

  // Creates a Av1Decoder after verifying that the underlying implementation
  // supports AV1 stateless decoding.
  static std::unique_ptr<Av1Decoder> Create(
      std::unique_ptr<IvfParser> ivf_parser,
      const media::IvfFileHeader& file_header);

  // TODO(stevecho): implement DecodeNextFrame() function
  // Parses next frame from IVF stream and decodes the frame. This method will
  // place the Y, U, and V values into the respective vectors and update the
  // size with the display area size of the decoded frame.
  VideoDecoder::Result DecodeNextFrame(std::vector<char>& y_plane,
                                       std::vector<char>& u_plane,
                                       std::vector<char>& v_plane,
                                       gfx::Size& size,
                                       const int frame_number) override;

 private:
  enum class ParsingResult {
    kFailed,
    kOk,
    kEOStream,
  };

  Av1Decoder(std::unique_ptr<IvfParser> ivf_parser,
             std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
             std::unique_ptr<V4L2Queue> OUTPUT_queue,
             std::unique_ptr<V4L2Queue> CAPTURE_queue);

  // Reads an OBU frame, if there is one available. If an |obu_parser_|
  // didn't exist and there is data to be read, |obu_parser_| will be
  // created. If there is an existing |current_sequence_header_|, this
  // will be passed to the ObuParser that is created. If successful
  // (indicated by returning VideoDecoder::kOk), then the fields
  // |ivf_frame_header_|, |ivf_frame_data_|, and |current_frame_| will be
  // set upon completion.
  ParsingResult ReadNextFrame(libgav1::RefCountedBufferPtr& current_frame);

  IvfFrameHeader ivf_frame_header_{};
  const uint8_t* ivf_frame_data_ = nullptr;

  // AV1-specific data.
  std::unique_ptr<libgav1::ObuParser> obu_parser_;
  std::unique_ptr<libgav1::BufferPool> buffer_pool_;
  std::unique_ptr<libgav1::DecoderState> state_;
  absl::optional<libgav1::ObuSequenceHeader> current_sequence_header_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_AV1_DECODER_H_
