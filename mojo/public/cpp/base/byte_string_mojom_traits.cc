// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/byte_string_mojom_traits.h"

#include "mojo/public/cpp/bindings/array_data_view.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::ByteStringDataView, std::string>::Read(
    mojo_base::mojom::ByteStringDataView data,
    std::string* out) {
  mojo::ArrayDataView<uint8_t> bytes;
  data.GetDataDataView(&bytes);
  out->assign(reinterpret_cast<const char*>(bytes.data()),
              bytes.size() / sizeof(char));
  return true;
}

}  // namespace mojo
