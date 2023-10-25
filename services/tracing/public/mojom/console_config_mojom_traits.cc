// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/console_config_mojom_traits.h"

#include <utility>

namespace mojo {
// static
bool StructTraits<tracing::mojom::ConsoleConfigDataView,
                  perfetto::protos::gen::ConsoleConfig>::
    Read(tracing::mojom::ConsoleConfigDataView data,
         perfetto::protos::gen::ConsoleConfig* out) {
  perfetto::protos::gen::ConsoleConfig::Output output;
  if (!data.ReadOutput(&output)) {
    return false;
  }
  out->set_output(output);
  out->set_enable_colors(data.enable_colors());
  return true;
}
}  // namespace mojo
