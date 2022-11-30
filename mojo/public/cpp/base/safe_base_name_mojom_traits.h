// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_SAFE_BASE_NAME_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_SAFE_BASE_NAME_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/files/safe_base_name.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/safe_base_name.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::SafeBaseNameDataView, base::SafeBaseName> {
  static const base::FilePath& path(const base::SafeBaseName& path) {
    return path.path();
  }

  static bool Read(mojo_base::mojom::SafeBaseNameDataView data,
                   base::SafeBaseName* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_SAFE_BASE_NAME_MOJOM_TRAITS_H_
