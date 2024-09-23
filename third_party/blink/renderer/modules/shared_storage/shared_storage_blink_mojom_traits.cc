// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_blink_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::SharedStorageKeyArgumentDataView, WTF::String>::
    Read(blink::mojom::SharedStorageKeyArgumentDataView data,
         WTF::String* out_key) {
  // There is no need to convert `SharedStorageKeyArgument` back to
  // `WTF::String`. If we do need to implement deserialization later, we need to
  // validate its length.
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<
    blink::mojom::SharedStorageValueArgumentDataView,
    WTF::String>::Read(blink::mojom::SharedStorageValueArgumentDataView data,
                       WTF::String* out_value) {
  // There is no need to convert `SharedStorageValueArgument` back to
  // `WTF::String`. If we do need to implement deserialization later, we need to
  // validate its length.
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
