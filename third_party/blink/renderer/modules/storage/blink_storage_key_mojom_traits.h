// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_BLINK_STORAGE_KEY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_BLINK_STORAGE_KEY_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom-shared.h"
#include "third_party/blink/renderer/modules/storage/blink_storage_key.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MODULES_STORAGE_BLINK_STORAGE_KEY)
    StructTraits<blink::mojom::StorageKeyDataView, blink::BlinkStorageKey> {
  static const scoped_refptr<const blink::SecurityOrigin>& origin(
      const blink::BlinkStorageKey& input) {
    return input.GetSecurityOrigin();
  }

  static bool Read(blink::mojom::StorageKeyDataView data,
                   blink::BlinkStorageKey* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_BLINK_STORAGE_KEY_MOJOM_TRAITS_H_
