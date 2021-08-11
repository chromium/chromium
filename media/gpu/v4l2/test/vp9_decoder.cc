// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/vp9_decoder.h"

#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"

namespace media {

namespace v4l2_test {

Vp9Decoder::Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser)
    : vp9_parser_(
          std::make_unique<Vp9Parser>(/*parsing_compressed_header=*/false)) {}

Vp9Decoder::~Vp9Decoder() = default;

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
