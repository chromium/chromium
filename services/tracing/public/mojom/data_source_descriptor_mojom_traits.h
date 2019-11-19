// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_DATA_SOURCE_DESCRIPTOR_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_DATA_SOURCE_DESCRIPTOR_MOJOM_TRAITS_H_

#include <string>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"

namespace mojo {
template <>
class StructTraits<tracing::mojom::DataSourceRegistrationDataView,
                   perfetto::DataSourceDescriptor> {
 public:
  static const std::string& name(const perfetto::DataSourceDescriptor& src) {
    return src.name();
  }
  static bool will_notify_on_start(const perfetto::DataSourceDescriptor& src) {
    return src.will_notify_on_start();
  }
  static bool will_notify_on_stop(const perfetto::DataSourceDescriptor& src) {
    return src.will_notify_on_stop();
  }
  static bool handles_incremental_state_clear(
      const perfetto::DataSourceDescriptor& src) {
    return src.handles_incremental_state_clear();
  }

  static bool Read(tracing::mojom::DataSourceRegistrationDataView data,
                   perfetto::DataSourceDescriptor* out);
};
}  // namespace mojo
#endif  // SERVICES_TRACING_PUBLIC_MOJOM_DATA_SOURCE_DESCRIPTOR_MOJOM_TRAITS_H_
