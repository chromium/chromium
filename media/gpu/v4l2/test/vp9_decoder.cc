// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/vp9_decoder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"

namespace media {

namespace v4l2_test {

Vp9Decoder::Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl)
    : ivf_parser_(std::move(ivf_parser)),
      vp9_parser_(
          std::make_unique<Vp9Parser>(/*parsing_compressed_header=*/false)),
      v4l2_ioctl_(std::move(v4l2_ioctl)) {}

Vp9Decoder::~Vp9Decoder() = default;

// static
std::unique_ptr<Vp9Decoder> Vp9Decoder::Create(
    std::unique_ptr<IvfParser> ivf_parser) {
  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_VP9_FRAME;

  // MM21 is an uncompressed opaque format that is produced by MediaTek
  // video decoders.
  const uint32_t kUncompressedFourcc = v4l2_fourcc('M', 'M', '2', '1');

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                      kUncompressedFourcc)) {
    LOG(ERROR) << "Device doesn't support the provided FourCCs.";
    return nullptr;
  }

  return base::WrapUnique(
      new Vp9Decoder(std::move(ivf_parser), std::move(v4l2_ioctl)));
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
