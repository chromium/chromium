// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_FILTER_MOJOM_TRAITS_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_FILTER_MOJOM_TRAITS_H_

#include <optional>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/service_manager/public/cpp/service_filter.h"
#include "services/service_manager/public/mojom/service_filter.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(SERVICE_MANAGER_MOJOM_TRAITS)
    StructTraits<service_manager::mojom::ServiceFilterDataView,
                 service_manager::ServiceFilter> {
  static const std::string& service_name(
      const service_manager::ServiceFilter& in) {
    return in.service_name();
  }
  static const std::optional<base::Token>& instance_group(
      const service_manager::ServiceFilter& in) {
    return in.instance_group();
  }
  static const std::optional<base::Token>& instance_id(
      const service_manager::ServiceFilter& in) {
    return in.instance_id();
  }
  static const std::optional<base::Token>& globally_unique_id(
      const service_manager::ServiceFilter& in) {
    return in.globally_unique_id();
  }

  static bool Read(service_manager::mojom::ServiceFilterDataView data,
                   service_manager::ServiceFilter* out);
};

}  // namespace mojo

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_FILTER_MOJOM_TRAITS_H_
