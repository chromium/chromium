// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/chrome_config_mojom_traits.h"

#include <utility>

namespace mojo {
// static
bool StructTraits<
    tracing::mojom::ChromeConfigDataView,
    perfetto::ChromeConfig>::Read(tracing::mojom::ChromeConfigDataView data,
                                  perfetto::ChromeConfig* out) {
  std::string config;
  if (!data.ReadTraceConfig(&config)) {
    return false;
  }
  out->set_trace_config(std::move(config));
  out->set_privacy_filtering_enabled(data.privacy_filtering_enabled());
  return true;
}
}  // namespace mojo
