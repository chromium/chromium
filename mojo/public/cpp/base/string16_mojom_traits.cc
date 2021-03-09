// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/string16_mojom_traits.h"

#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::String16DataView, base::string16>::Read(
    mojo_base::mojom::String16DataView data,
    base::string16* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  out->assign(reinterpret_cast<const char16_t*>(view.data()), view.size());
  return true;
}

// static
mojo_base::BigBuffer
StructTraits<mojo_base::mojom::BigString16DataView, base::string16>::data(
    const base::string16& str) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(str.data());
  return mojo_base::BigBuffer(
      base::make_span(bytes, str.size() * sizeof(char16_t)));
}

// static
bool StructTraits<mojo_base::mojom::BigString16DataView, base::string16>::Read(
    mojo_base::mojom::BigString16DataView data,
    base::string16* out) {
  mojo_base::BigBuffer buffer;
  if (!data.ReadData(&buffer))
    return false;
  if (buffer.size() % sizeof(char16_t))
    return false;
  *out = base::string16(reinterpret_cast<const char16_t*>(buffer.data()),
                        buffer.size() / sizeof(char16_t));
  return true;
}

}  // namespace mojo
