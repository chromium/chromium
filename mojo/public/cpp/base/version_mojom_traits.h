// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_VERSION_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_VERSION_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "base/version.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/version.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::VersionDataView, base::Version> {
  static const std::vector<uint32_t>& components(const base::Version& in) {
    return in.components();
  }

  static bool Read(mojo_base::mojom::VersionDataView data, base::Version* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_VERSION_MOJOM_TRAITS_H_
