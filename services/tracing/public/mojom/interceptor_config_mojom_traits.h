// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_INTERCEPTOR_CONFIG_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_INTERCEPTOR_CONFIG_MOJOM_TRAITS_H_

#include <string>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom-shared.h"
#include "third_party/perfetto/protos/perfetto/config/interceptor_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/interceptors/console_config.gen.h"

namespace mojo {

template <>
class StructTraits<tracing::mojom::InterceptorConfigDataView,
                   perfetto::protos::gen::InterceptorConfig> {
 public:
  static const std::string& name(
      const perfetto::protos::gen::InterceptorConfig& src) {
    return src.name();
  }

  static std::optional<perfetto::protos::gen::ConsoleConfig> console_config(
      const perfetto::protos::gen::InterceptorConfig& src) {
    if (src.has_console_config()) {
      src.console_config();
    }
    return std::nullopt;
  }

  static bool Read(tracing::mojom::InterceptorConfigDataView data,
                   perfetto::protos::gen::InterceptorConfig* out);
};

}  // namespace mojo

#endif  // SERVICES_TRACING_PUBLIC_MOJOM_INTERCEPTOR_CONFIG_MOJOM_TRAITS_H_
