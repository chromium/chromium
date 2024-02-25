// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::UnguessableTokenDataView,
                  base::UnguessableToken>::
    Read(mojo_base::mojom::UnguessableTokenDataView data,
         base::UnguessableToken* out) {
  uint64_t high = data.high();
  uint64_t low = data.low();

  // This is not mapped as nullable_is_same_type, so any UnguessableToken
  // deserialized by the traits should always yield a non-empty token.
  // If deserialization results in an empty token, the data is malformed.
  std::optional<base::UnguessableToken> token =
      base::UnguessableToken::Deserialize(high, low);
  if (!token.has_value()) {
    return false;
  }
  *out = token.value();
  return true;
}

}  // namespace mojo
