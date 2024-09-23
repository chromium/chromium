// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/string16_mojom_traits.h"

#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::String16DataView, std::u16string>::Read(
    mojo_base::mojom::String16DataView data,
    std::u16string* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  out->assign(reinterpret_cast<const char16_t*>(view.data()), view.size());
  return true;
}

// static
mojo_base::BigBuffer
StructTraits<mojo_base::mojom::BigString16DataView, std::u16string>::data(
    const std::u16string& str) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(str.data());
  return mojo_base::BigBuffer(
      base::make_span(bytes, str.size() * sizeof(char16_t)));
}

// static
bool StructTraits<mojo_base::mojom::BigString16DataView, std::u16string>::Read(
    mojo_base::mojom::BigString16DataView data,
    std::u16string* out) {
  mojo_base::BigBuffer buffer;
  if (!data.ReadData(&buffer))
    return false;
  if (buffer.size() % sizeof(char16_t))
    return false;
  *out = std::u16string(reinterpret_cast<const char16_t*>(buffer.data()),
                        buffer.size() / sizeof(char16_t));
  return true;
}

}  // namespace mojo
