// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/core_account_id_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<identity::mojom::CoreAccountId::DataView, ::CoreAccountId>::
    Read(identity::mojom::CoreAccountId::DataView data, ::CoreAccountId* out) {
  std::string id;

  if (!data.ReadId(&id)) {
    return false;
  }

  out->id = id;

  return true;
}

// static
bool StructTraits<identity::mojom::CoreAccountId::DataView,
                  ::CoreAccountId>::IsNull(const ::CoreAccountId& input) {
  return input.empty();
}

}  // namespace mojo