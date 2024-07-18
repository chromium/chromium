// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/big_string_mojom_traits.h"

#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

namespace mojo {

// static
mojo_base::BigBuffer StructTraits<mojo_base::mojom::BigStringDataView,
                                  std::string>::data(const std::string& str) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(str.data());
  return mojo_base::BigBuffer(
      base::make_span(bytes, str.size() * sizeof(char)));
}

// static
bool StructTraits<mojo_base::mojom::BigStringDataView, std::string>::Read(
    mojo_base::mojom::BigStringDataView data,
    std::string* out) {
  mojo_base::BigBuffer buffer;
  if (!data.ReadData(&buffer))
    return false;
  if (buffer.size() % sizeof(char))
    return false;
  *out = std::string(reinterpret_cast<const char*>(buffer.data()),
                     buffer.size() / sizeof(char));
  return true;
}

}  // namespace mojo
