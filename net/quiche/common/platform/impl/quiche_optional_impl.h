// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_OPTIONAL_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_OPTIONAL_IMPL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace quiche {

template <typename T>
using QuicheOptionalImpl = absl::optional<T>;

#define QUICHE_NULLOPT_IMPL absl::nullopt

}  // namespace quiche

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_OPTIONAL_IMPL_H_
