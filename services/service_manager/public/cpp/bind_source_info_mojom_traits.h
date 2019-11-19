// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BIND_SOURCE_INFO_MOJOM_TRAITS_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BIND_SOURCE_INFO_MOJOM_TRAITS_H_

#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(SERVICE_MANAGER_MOJOM)
    StructTraits<service_manager::mojom::BindSourceInfo::DataView,
                 service_manager::BindSourceInfo> {
  static const service_manager::Identity& identity(
      const service_manager::BindSourceInfo& source) {
    return source.identity;
  }
  static const service_manager::CapabilitySet& required_capabilities(
      const service_manager::BindSourceInfo& source) {
    return source.required_capabilities;
  }
  static bool Read(service_manager::mojom::BindSourceInfoDataView data,
                   service_manager::BindSourceInfo* out) {
    return data.ReadIdentity(&out->identity) &&
           data.ReadRequiredCapabilities(&out->required_capabilities);
  }
};

}  // namespace mojo

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BIND_SOURCE_INFO_MOJOM_TRAITS_H_
