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

  // Receiving a zeroed UnguessableToken is a security issue.
  if (high == 0 && low == 0)
    return false;

  *out = base::UnguessableToken::Deserialize(high, low);
  return true;
}

}  // namespace mojo