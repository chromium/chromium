// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_
#define MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_

#include <linux/videodev2.h>

#include "base/files/memory_mapped_file.h"
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

  // Creates |Vp9Decoder| after calling VerifyCapabilities() function to make
  // sure provided OUTPUT and CAPTURE formats are supported.
  static std::unique_ptr<Vp9Decoder> Create(
      std::unique_ptr<IvfParser> ivf_parser,
      base::File& v4l2_dev_file);

 private:
  Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser, base::File& v4l2_dev_file);

  // Queries |v4l_fd| to see if it can use the specified |fourcc| format
  // for the given buffer |type|.
  static bool QueryFormat(base::PlatformFile v4l_fd,
                          enum v4l2_buf_type type,
                          uint32_t fourcc);

  // Verifies |v4l_fd| supports |compressed_format| for OUTPUT queues
  // and |uncompressed_format| for CAPTURE queues, respectively.
  static bool VerifyCapabilities(base::PlatformFile v4l_fd,
                                 uint32_t compressed_format,
                                 uint32_t uncompressed_format);

  // Reads next frame from IVF stream and its size into |vp9_frame_header|
  // and |size| respectively.
  Vp9Parser::Result ReadNextFrame(Vp9FrameHeader* vp9_frame_header,
                                  gfx::Size& size);

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;

  // VP9-specific data.
  const std::unique_ptr<Vp9Parser> vp9_parser_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_VP9_DECODER_H_
