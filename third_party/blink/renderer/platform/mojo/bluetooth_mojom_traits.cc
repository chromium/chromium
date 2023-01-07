// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/bluetooth_mojom_traits.h"

#include "mojo/public/cpp/bindings/string_traits_wtf.h"

namespace mojo {

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
