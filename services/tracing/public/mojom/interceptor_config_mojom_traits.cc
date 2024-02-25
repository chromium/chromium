// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/interceptor_config_mojom_traits.h"

#include <optional>
#include <utility>

#include "services/tracing/public/mojom/console_config_mojom_traits.h"

namespace mojo {
// static
bool StructTraits<tracing::mojom::InterceptorConfigDataView,
                  perfetto::protos::gen::InterceptorConfig>::
    Read(tracing::mojom::InterceptorConfigDataView data,
         perfetto::protos::gen::InterceptorConfig* out) {
  std::string name;
  std::optional<perfetto::protos::gen::ConsoleConfig> console_config;
  if (!data.ReadName(&name) || name.empty()) {
    return false;
  }
  if (!data.ReadConsoleConfig(&console_config)) {
    // Config present but invalid.
    return false;
  }
  if (console_config && name != "console") {
    // Config present when it shouldn't be.
    return false;
  }
  if (console_config) {
    *out->mutable_console_config() = std::move(*console_config);
  }
  out->set_name(std::move(name));
  return true;
}
}  // namespace mojo
