// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/impression_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::ImpressionDataView, blink::Impression>::Read(
    blink::mojom::ImpressionDataView data,
    blink::Impression* out) {
  return data.ReadAttributionSrcToken(&out->attribution_src_token);
}

}  // namespace mojo
