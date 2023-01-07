// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/int128_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::Int128DataView, absl::int128>::Read(
    mojo_base::mojom::Int128DataView data,
    absl::int128* out) {
  *out = absl::MakeInt128(data.high(), data.low());
  return true;
}

// static
bool StructTraits<mojo_base::mojom::Uint128DataView, absl::uint128>::Read(
    mojo_base::mojom::Uint128DataView data,
    absl::uint128* out) {
  *out = absl::MakeUint128(data.high(), data.low());
  return true;
}

}  // namespace mojo
