// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_MOJOM_TRAITS_H_

#include <string>

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::SharedStorageKeyArgumentDataView,
                 std::u16string> {
  static bool Read(blink::mojom::SharedStorageKeyArgumentDataView data,
                   std::u16string* out_key);

  static const std::u16string& data(const std::u16string& input) {
    return input;
  }
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::SharedStorageValueArgumentDataView,
                 std::u16string> {
  static bool Read(blink::mojom::SharedStorageValueArgumentDataView data,
                   std::u16string* out_value);

  static const std::u16string& data(const std::u16string& input) {
    return input;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_MOJOM_TRAITS_H_
