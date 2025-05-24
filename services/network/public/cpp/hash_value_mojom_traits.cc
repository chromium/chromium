// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/hash_value_mojom_traits.h"

#include <string.h>

#include "base/containers/span.h"

namespace mojo {

// static
bool StructTraits<
    network::mojom::SHA256HashValueDataView,
    net::SHA256HashValue>::Read(network::mojom::SHA256HashValueDataView in,
                                net::SHA256HashValue* out) {
  mojo::ArrayDataView<uint8_t> bytes;
  in.GetDataDataView(&bytes);
  if (bytes.size() != 32) {
    // TODO(crbug.com/40061960): Mojo should validate this array size. We can
    // CHECK it instead when it does.
    return false;
  }
  base::span(*out).copy_from(base::as_byte_span(bytes));
  return true;
}

}  // namespace mojo
