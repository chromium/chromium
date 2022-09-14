// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/token_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::TokenDataView, base::Token>::Read(
    mojo_base::mojom::TokenDataView data,
    base::Token* out) {
  *out = base::Token{data.high(), data.low()};
  return true;
}

}  // namespace mojo
