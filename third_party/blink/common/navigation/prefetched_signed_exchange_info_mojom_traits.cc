// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/prefetched_signed_exchange_info_mojom_traits.h"

#include "base/containers/span.h"
#include "base/notreached.h"

namespace mojo {

bool StructTraits<blink::mojom::SHA256HashValueDataView, net::SHA256HashValue>::
    Read(blink::mojom::SHA256HashValueDataView input,
         net::SHA256HashValue* out) {
  std::string data;
  if (!input.ReadData(&data))
    return false;

  if (data.size() != out->size()) {
    NOTREACHED();
  }

  base::span(*out).copy_from(base::as_byte_span(data));
  return true;
}

}  // namespace mojo
