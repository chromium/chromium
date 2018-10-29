// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_TEST_UTILS_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_TEST_UTILS_H_

#include "net/third_party/quic/core/qpack/qpack_test_utils.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/spdy/core/spdy_header_block.h"

namespace quic {
namespace test {

QuicString QpackEncode(const FragmentSizeGenerator& fragment_size_generator,
                       const spdy::SpdyHeaderBlock* header_list);

}  // namespace test
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_ENCODER_TEST_UTILS_H_
