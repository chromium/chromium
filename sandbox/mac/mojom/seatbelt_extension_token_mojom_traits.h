// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_MOJOM_SEATBELT_EXTENSION_TOKEN_MOJOM_TRAITS_H_
#define SANDBOX_MAC_MOJOM_SEATBELT_EXTENSION_TOKEN_MOJOM_TRAITS_H_

#include <string>

#include "sandbox/mac/mojom/seatbelt_extension_token.mojom-shared.h"
#include "sandbox/mac/seatbelt_extension_token.h"

namespace mojo {

template <>
struct StructTraits<sandbox::mac::mojom::SeatbeltExtensionTokenDataView,
                    sandbox::SeatbeltExtensionToken> {
  static const std::string& token(const sandbox::SeatbeltExtensionToken& t) {
    return t.token();
  }
  static bool Read(sandbox::mac::mojom::SeatbeltExtensionTokenDataView data,
                   sandbox::SeatbeltExtensionToken* out);
};

}  // namespace mojo

#endif  // SANDBOX_MAC_MOJOM_SEATBELT_EXTENSION_TOKEN_MOJOM_TRAITS_H_
