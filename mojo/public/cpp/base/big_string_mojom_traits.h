// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BIG_STRING_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_BIG_STRING_MOJOM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/big_string.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::BigStringDataView, std::string> {
  static mojo_base::BigBuffer data(const std::string& str);

  static bool Read(mojo_base::mojom::BigStringDataView data, std::string* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_BIG_STRING_MOJOM_TRAITS_H_
