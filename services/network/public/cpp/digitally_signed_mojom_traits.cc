// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/digitally_signed_mojom_traits.h"

#include <vector>

namespace mojo {

// static
bool StructTraits<network::mojom::DigitallySignedDataView,
                  net::ct::DigitallySigned>::
    Read(network::mojom::DigitallySignedDataView data,
         net::ct::DigitallySigned* out) {
  std::vector<uint8_t> signature_data;
  if (!data.ReadHashAlgorithm(&out->hash_algorithm) ||
      !data.ReadSignatureAlgorithm(&out->signature_algorithm) ||
      !data.ReadSignature(&signature_data)) {
    return false;
  }
  if (signature_data.empty())
    return false;
  out->signature_data.assign(
      reinterpret_cast<const char*>(signature_data.data()),
      signature_data.size());
  return true;
}

}  // namespace mojo
