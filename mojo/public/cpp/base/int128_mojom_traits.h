// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_INT128_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_INT128_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/mojom/base/int128.mojom-shared.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::Int128DataView, absl::int128> {
  static int64_t high(const absl::int128& v) { return absl::Int128High64(v); }
  static uint64_t low(const absl::int128& v) { return absl::Int128Low64(v); }
  static bool Read(mojo_base::mojom::Int128DataView data, absl::int128* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::Uint128DataView, absl::uint128> {
  static uint64_t high(const absl::uint128& v) {
    return absl::Uint128High64(v);
  }
  static uint64_t low(const absl::uint128& v) { return absl::Uint128Low64(v); }
  static bool Read(mojo_base::mojom::Uint128DataView data, absl::uint128* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_INT128_MOJOM_TRAITS_H_
