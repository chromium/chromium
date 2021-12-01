// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/bluetooth_mojom_traits.h"

#include "mojo/public/cpp/bindings/string_traits_wtf.h"

namespace mojo {

// static
bool StructTraits<::blink::mojom::WebBluetoothDeviceIdDataView, WTF::String>::
    Read(::blink::mojom::WebBluetoothDeviceIdDataView data,
         WTF::String* output) {
  return data.ReadDeviceId(output);
}

// static
bool StructTraits<bluetooth::mojom::UUIDDataView, WTF::String>::Read(
    bluetooth::mojom::UUIDDataView data,
    WTF::String* output) {
  return data.ReadUuid(output);
}

// static
void StructTraits<bluetooth::mojom::UUIDDataView, WTF::String>::SetToNull(
    WTF::String* output) {
  if (output->IsNull())
    return;
  WTF::String result;
  output->swap(result);
}

}  // namespace mojo
