// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_blink_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    network::mojom::SharedStorageKeyArgumentDataView,
    blink::String>::Read(network::mojom::SharedStorageKeyArgumentDataView data,
                         blink::String* out_key) {
  // There is no need to convert `SharedStorageKeyArgument` back to
  // `blink::String`. If we do need to implement deserialization later, we need
  // to validate its length.
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::SharedStorageValueArgumentDataView,
                  blink::String>::
    Read(network::mojom::SharedStorageValueArgumentDataView data,
         blink::String* out_value) {
  // There is no need to convert `SharedStorageValueArgument` back to
  // `blink::String`. If we do need to implement deserialization later, we need
  // to validate its length.
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::LockNameDataView, blink::String>::Read(
    network::mojom::LockNameDataView data,
    blink::String* out_value) {
  // There is no need to convert `LockNameDataView` back to `blink::String`. If
  // we do need to implement deserialization later, we need to validate its
  // content.
  NOTREACHED();
}

// static
bool StructTraits<
    network::mojom::SharedStorageBatchUpdateMethodsArgumentDataView,
    blink::Vector<
        network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr>>::
    Read(network::mojom::SharedStorageBatchUpdateMethodsArgumentDataView data,
         blink::Vector<
             network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr>*
             out_value) {
  // There is no need to convert
  // `SharedStorageBatchUpdateMethodsArgumentDataView` back to
  // `blink::Vector<network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr>`.
  // If we do need to implement deserialization later, we need to validate its
  // content.
  NOTREACHED();
}

}  // namespace mojo
