// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_UNGUESSABLE_TOKEN_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_UNGUESSABLE_TOKEN_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "mojo/public/mojom/base/unguessable_token.mojom-shared.h"

namespace mojo {

// If base::UnguessableToken is no longer 128 bits, the logic below and the
// mojom::UnguessableToken type should be updated.
static_assert(sizeof(base::UnguessableToken) == 2 * sizeof(uint64_t),
              "base::UnguessableToken should be of size 2 * sizeof(uint64_t).");

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::UnguessableTokenDataView,
                 base::UnguessableToken> {
  static uint64_t high(const base::UnguessableToken& token) {
    return token.GetHighForSerialization();
  }

  static uint64_t low(const base::UnguessableToken& token) {
    return token.GetLowForSerialization();
  }

  static bool Read(mojo_base::mojom::UnguessableTokenDataView data,
                   base::UnguessableToken* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_UNGUESSABLE_TOKEN_MOJOM_TRAITS_H_
