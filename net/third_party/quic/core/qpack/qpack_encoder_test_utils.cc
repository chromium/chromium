// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder_test_utils.h"

#include "net/third_party/quic/core/qpack/qpack_encoder.h"
#include "net/third_party/spdy/core/hpack/hpack_encoder.h"

namespace quic {
namespace test {

QuicString QpackEncode(const FragmentSizeGenerator& fragment_size_generator,
                       const spdy::SpdyHeaderBlock* header_list) {
  QpackEncoder encoder;
  auto progressive_encoder = encoder.EncodeHeaderList(header_list);

  QuicString output;
  while (progressive_encoder->HasNext()) {
    progressive_encoder->Next(fragment_size_generator(), &output);
  }

  return output;
}

}  // namespace test
}  // namespace quic
