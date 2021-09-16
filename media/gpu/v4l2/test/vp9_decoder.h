// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

#include "media/filters/vp9_parser.h"

namespace media {

class IvfParser;

namespace v4l2_test {

// A Vp9Decoder decodes VP9-encoded IVF streams using v4l2 ioctl calls.
class Vp9Decoder {
 public:
  Vp9Decoder(const Vp9Decoder&) = delete;
  Vp9Decoder& operator=(const Vp9Decoder&) = delete;
  ~Vp9Decoder();

  // Creates a Vp9Decoder after verifying that the underlying implementation
  // supports VP9 stateless decoding.
  static std::unique_ptr<Vp9Decoder> Create(
      std::unique_ptr<IvfParser> ivf_parser);

 private:
  Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
             std::unique_ptr<V4L2IoctlShim> v4l2_ioctl);

  // Reads next frame from IVF stream and its size into |vp9_frame_header|
  // and |size| respectively.
  Vp9Parser::Result ReadNextFrame(Vp9FrameHeader* vp9_frame_header,
                                  gfx::Size& size);

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;

  // VP9-specific data.
  const std::unique_ptr<Vp9Parser> vp9_parser_;

  // Wrapper for V4L2 ioctl requests.
  const std::unique_ptr<V4L2IoctlShim> v4l2_ioctl_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_
