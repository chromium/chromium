// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_CONSOLE_CONFIG_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_CONSOLE_CONFIG_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom-shared.h"
#include "third_party/perfetto/protos/perfetto/config/interceptors/console_config.gen.h"

namespace mojo {

// perfetto::protos::gen::ConsoleConfig::Output
template <>
struct EnumTraits<tracing::mojom::ConsoleOutput,
                  perfetto::protos::gen::ConsoleConfig::Output> {
  static tracing::mojom::ConsoleOutput ToMojom(
      perfetto::protos::gen::ConsoleConfig::Output input) {
    switch (input) {
      case perfetto::protos::gen::ConsoleConfig::OUTPUT_UNSPECIFIED:
        return tracing::mojom::ConsoleOutput::kOutputUnspecified;
      case perfetto::protos::gen::ConsoleConfig::OUTPUT_STDOUT:
        return tracing::mojom::ConsoleOutput::kOutputStdOut;
      case perfetto::protos::gen::ConsoleConfig::OUTPUT_STDERR:
        return tracing::mojom::ConsoleOutput::kOutputStdErr;
    }
  }

  static bool FromMojom(tracing::mojom::ConsoleOutput input,
                        perfetto::protos::gen::ConsoleConfig::Output* out) {
    switch (input) {
      case tracing::mojom::ConsoleOutput::kOutputUnspecified:
        *out = perfetto::protos::gen::ConsoleConfig::OUTPUT_UNSPECIFIED;
        return true;
      case tracing::mojom::ConsoleOutput::kOutputStdOut:
        *out = perfetto::protos::gen::ConsoleConfig::OUTPUT_STDOUT;
        return true;
      case tracing::mojom::ConsoleOutput::kOutputStdErr:
        *out = perfetto::protos::gen::ConsoleConfig::OUTPUT_STDERR;
        return true;
    }
  }
};

template <>
class StructTraits<tracing::mojom::ConsoleConfigDataView,
                   perfetto::protos::gen::ConsoleConfig> {
 public:
  static perfetto::protos::gen::ConsoleConfig::Output output(
      const perfetto::protos::gen::ConsoleConfig& src) {
    return src.output();
  }

  static bool enable_colors(const perfetto::protos::gen::ConsoleConfig& src) {
    return src.enable_colors();
  }

  static bool Read(tracing::mojom::ConsoleConfigDataView data,
                   perfetto::protos::gen::ConsoleConfig* out);
};

}  // namespace mojo

#endif  // SERVICES_TRACING_PUBLIC_MOJOM_CONSOLE_CONFIG_MOJOM_TRAITS_H_
