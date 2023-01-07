// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/shared_storage/shared_storage_mojom_traits.h"

#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"

namespace mojo {

// static
bool StructTraits<
    blink::mojom::SharedStorageKeyArgumentDataView,
    std::u16string>::Read(blink::mojom::SharedStorageKeyArgumentDataView data,
                          std::u16string* out_key) {
  if (!data.ReadData(out_key))
    return false;

  return blink::IsValidSharedStorageKeyStringLength(out_key->size());
}

// static
bool StructTraits<
    blink::mojom::SharedStorageValueArgumentDataView,
    std::u16string>::Read(blink::mojom::SharedStorageValueArgumentDataView data,
                          std::u16string* out_value) {
  if (!data.ReadData(out_value))
    return false;

  return blink::IsValidSharedStorageValueStringLength(out_value->size());
}

}  // namespace mojo
