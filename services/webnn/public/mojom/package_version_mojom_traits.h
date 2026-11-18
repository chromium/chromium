// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_PACKAGE_VERSION_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_PACKAGE_VERSION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/package_version.h"
#include "services/webnn/public/mojom/ep_package_info.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(WEBNN_MOJOM_TRAITS)
    StructTraits<webnn::mojom::PackageVersionDataView, webnn::PackageVersion> {
  static uint16_t major(const webnn::PackageVersion& version) {
    return version.major;
  }

  static uint16_t minor(const webnn::PackageVersion& version) {
    return version.minor;
  }

  static uint16_t build(const webnn::PackageVersion& version) {
    return version.build;
  }

  static uint16_t revision(const webnn::PackageVersion& version) {
    return version.revision;
  }

  static bool Read(webnn::mojom::PackageVersionDataView data,
                   webnn::PackageVersion* out) {
    out->major = data.major();
    out->minor = data.minor();
    out->build = data.build();
    out->revision = data.revision();
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_PACKAGE_VERSION_MOJOM_TRAITS_H_
