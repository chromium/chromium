// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_STRING_PIECE_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_STRING_PIECE_IMPL_H_

#include "third_party/abseil-cpp/absl/hash/hash.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace quiche {

inline size_t QuicheHashStringPairImpl(absl::string_view a,
                                       absl::string_view b) {
  auto pair = std::make_pair(a, b);
  return absl::Hash<decltype(pair)>()(pair);
}

}  // namespace quiche

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_STRING_PIECE_IMPL_H_
