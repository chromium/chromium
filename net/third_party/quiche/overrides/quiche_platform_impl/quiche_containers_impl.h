// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_

#include <vector>

#include "base/containers/flat_set.h"

namespace quiche {

// TODO(wub): Switch to absl::InlinedVector once it is allowed.
template <typename T, size_t N, typename A = std::allocator<T>>
using QuicheInlinedVectorImpl = std::vector<T, A>;

template <typename Key, typename Compare>
using QuicheSmallOrderedSetImpl = base::flat_set<Key, Compare>;

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_
