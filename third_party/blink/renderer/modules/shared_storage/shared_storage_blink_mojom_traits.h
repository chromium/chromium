// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_BLINK_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_BLINK_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/mojom/shared_storage.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT StructTraits<
    network::mojom::SharedStorageKeyArgumentDataView,
    blink::String> {
  static bool Read(network::mojom::SharedStorageKeyArgumentDataView data,
                   blink::String* out_key);

  static const blink::String& data(const blink::String& input) { return input; }
};

template <>
struct PLATFORM_EXPORT StructTraits<
    network::mojom::SharedStorageValueArgumentDataView,
    blink::String> {
  static bool Read(network::mojom::SharedStorageValueArgumentDataView data,
                   blink::String* out_value);

  static const blink::String& data(const blink::String& input) { return input; }
};

template <>
struct PLATFORM_EXPORT StructTraits<network::mojom::LockNameDataView,
                                    blink::String> {
  static bool Read(network::mojom::LockNameDataView data,
                   blink::String* out_value);

  static const blink::String& data(const blink::String& input) { return input; }
};

template <>
struct PLATFORM_EXPORT StructTraits<
    network::mojom::SharedStorageBatchUpdateMethodsArgumentDataView,
    blink::Vector<
        network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr>> {
  static bool Read(
      network::mojom::SharedStorageBatchUpdateMethodsArgumentDataView data,
      blink::Vector<
          network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr>*
          out_value);

  static const blink::Vector<
      network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr>&
  data(const blink::Vector<
       network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr>&
           input) {
    return input;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_BLINK_MOJOM_TRAITS_H_
