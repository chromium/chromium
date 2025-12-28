// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SYNC_METHOD_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SYNC_METHOD_TRAITS_H_

#include <stdint.h>

#include <concepts>

#include "base/containers/span.h"

namespace mojo::internal {

template <typename Interface>
struct SyncMethodTraits {
  static constexpr base::span<const uint32_t> GetOrdinals() {
    if constexpr (requires {
                    {
                      Interface::kSyncMethodOrdinals
                    } -> std::convertible_to<base::span<const uint32_t>>;
                  }) {
      return Interface::kSyncMethodOrdinals;
    }
    return {};
  }
};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SYNC_METHOD_TRAITS_H_
