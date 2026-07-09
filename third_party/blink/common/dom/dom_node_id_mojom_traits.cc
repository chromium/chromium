// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/dom/dom_node_id_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::DOMNodeIdDataView, blink::DOMNodeIdType>::Read(
    blink::mojom::DOMNodeIdDataView data,
    blink::DOMNodeIdType* out) {
  *out = blink::DOMNodeIdType(data.value());
  return true;
}

}  // namespace mojo
