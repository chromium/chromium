// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_TEST_UTILS_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_TEST_UTILS_H_

#include <cstddef>
#include <functional>

namespace quic {
namespace test {

// Called repeatedly to determine the size of each fragment when encoding or
// decoding.  Must return a positive value.
using FragmentSizeGenerator = std::function<size_t()>;

enum class FragmentMode {
  kSingleChunk,
  kOctetByOctet,
};

FragmentSizeGenerator FragmentModeToFragmentSizeGenerator(
    FragmentMode fragment_mode);

}  // namespace test
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_TEST_UTILS_H_
