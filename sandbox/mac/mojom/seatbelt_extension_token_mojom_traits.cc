// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/mojom/seatbelt_extension_token_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<sandbox::mac::mojom::SeatbeltExtensionTokenDataView,
                  sandbox::SeatbeltExtensionToken>::
    Read(sandbox::mac::mojom::SeatbeltExtensionTokenDataView data,
         sandbox::SeatbeltExtensionToken* out) {
  std::string token;
  if (!data.ReadToken(&token))
    return false;

  out->set_token(token);
  return true;
}

}  // namespace mojo
