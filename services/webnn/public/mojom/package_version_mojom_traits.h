// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_PACKAGE_VERSION_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_PACKAGE_VERSION_MOJOM_TRAITS_H_

#include <Windows.h>

#include <appmodel.h>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/mojom/ep_package_info.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(WEBNN_MOJOM_TRAITS)
    StructTraits<webnn::mojom::PackageVersionDataView, PACKAGE_VERSION> {
  static uint16_t major(const PACKAGE_VERSION& version) {
    return version.Major;
  }

  static uint16_t minor(const PACKAGE_VERSION& version) {
    return version.Minor;
  }

  static uint16_t build(const PACKAGE_VERSION& version) {
    return version.Build;
  }

  static uint16_t revision(const PACKAGE_VERSION& version) {
    return version.Revision;
  }

  static bool Read(webnn::mojom::PackageVersionDataView data,
                   PACKAGE_VERSION* out) {
    out->Major = data.major();
    out->Minor = data.minor();
    out->Build = data.build();
    out->Revision = data.revision();
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_PACKAGE_VERSION_MOJOM_TRAITS_H_
