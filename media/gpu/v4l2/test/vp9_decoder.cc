// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/vp9_decoder.h"

#include <sys/ioctl.h>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/base/video_types.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"

namespace media {

namespace v4l2_test {

Vp9Decoder::Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       base::File& v4l2_dev_file)
    : vp9_parser_(
          std::make_unique<Vp9Parser>(/*parsing_compressed_header=*/false)) {}

Vp9Decoder::~Vp9Decoder() = default;

// static
std::unique_ptr<Vp9Decoder> Vp9Decoder::Create(
    std::unique_ptr<IvfParser> ivf_parser,
    base::File& v4l2_dev_file) {
  DCHECK(v4l2_dev_file.IsValid());

  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_VP9_FRAME;

  // MM21 is an uncompressed opaque format that is produced by MediaTek
  // video decoders.
  const uint32_t kUncompressedFourcc = v4l2_fourcc('M', 'M', '2', '1');

  if (!VerifyCapabilities(v4l2_dev_file.GetPlatformFile(), kDriverCodecFourcc,
                          kUncompressedFourcc)) {
    LOG(ERROR) << "Device doesn't support the provided FourCCs.";
    return nullptr;
  }

  return base::WrapUnique(new Vp9Decoder(std::move(ivf_parser), v4l2_dev_file));
}

// static
bool Vp9Decoder::QueryFormat(base::PlatformFile v4l_fd,
                             enum v4l2_buf_type type,
                             uint32_t fourcc) {
  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));

  fmtdesc.type = type;

  while (ioctl(v4l_fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (fourcc == fmtdesc.pixelformat)
      return true;

    fmtdesc.index++;
  }

  return false;
}

// static
bool Vp9Decoder::VerifyCapabilities(base::PlatformFile v4l_fd,
                                    uint32_t compressed_format,
                                    uint32_t uncompressed_format) {
  struct v4l2_capability cap;
  memset(&cap, 0, sizeof(cap));

  if (ioctl(v4l_fd, VIDIOC_QUERYCAP, &cap) != 0) {
    PLOG(ERROR) << "VIDIOC_QUERYCAP failed";
    return false;
  }

  LOG(INFO) << "Driver=\"" << cap.driver << "\" bus_info=\"" << cap.bus_info
            << "\" card=\"" << cap.card << "\" fd=0x" << std::hex << v4l_fd;

  const bool is_compressed_format_supported =
      QueryFormat(v4l_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, compressed_format);

  LOG_IF(ERROR, !is_compressed_format_supported)
      << media::FourccToString(compressed_format)
      << " is not a supported compressed OUTPUT format.";

  const bool is_uncompressed_format_supported = QueryFormat(
      v4l_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, uncompressed_format);

  LOG_IF(ERROR, !is_uncompressed_format_supported)
      << media::FourccToString(uncompressed_format)
      << " is not a supported uncompressed CAPTURE format.";

  return is_compressed_format_supported && is_uncompressed_format_supported;
}

Vp9Parser::Result Vp9Decoder::ReadNextFrame(Vp9FrameHeader* vp9_frame_header,
                                            gfx::Size& size) {
  DCHECK(vp9_frame_header);

  // TODO(jchinlee): reexamine this loop for cleanup
  while (true) {
    std::unique_ptr<DecryptConfig> null_config;
    Vp9Parser::Result res =
        vp9_parser_->ParseNextFrame(vp9_frame_header, &size, &null_config);
    if (res == Vp9Parser::kEOStream) {
      IvfFrameHeader ivf_frame_header{};
      const uint8_t* ivf_frame_data;

      if (!ivf_parser_->ParseNextFrame(&ivf_frame_header, &ivf_frame_data))
        return Vp9Parser::kEOStream;

      vp9_parser_->SetStream(ivf_frame_data, ivf_frame_header.frame_size,
                             /*stream_config=*/nullptr);
      continue;
    }

    return res;
  }
}

}  // namespace v4l2_test
}  // namespace media
