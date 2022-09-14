// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/wstring_mojom_traits.h"

#include "mojo/public/cpp/bindings/array_data_view.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::WStringDataView, std::wstring>::Read(
    mojo_base::mojom::WStringDataView data,
    std::wstring* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  out->assign(reinterpret_cast<const wchar_t*>(view.data()), view.size());
  return true;
}

}  // namespace mojo
