// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_DOM_DOM_NODE_ID_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_DOM_DOM_NODE_ID_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "third_party/blink/public/mojom/dom/dom_node_id.mojom-data-view.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::DOMNodeIdDataView, blink::DOMNodeIdType> {
  static int32_t value(const blink::DOMNodeIdType& id) { return id.value(); }

  static bool Read(blink::mojom::DOMNodeIdDataView data,
                   blink::DOMNodeIdType* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DOM_DOM_NODE_ID_MOJOM_TRAITS_H_
