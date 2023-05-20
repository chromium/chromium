// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/attribution_utils.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

std::string GetAttributionSupportHeader(
    mojom::AttributionSupport attribution_support) {
  std::vector<net::structured_headers::DictionaryMember> registrars;
  const auto add_registrar = [&registrars](std::string registrar) {
    registrars.emplace_back(std::move(registrar),
                            net::structured_headers::ParameterizedMember(
                                net::structured_headers::Item(true),
                                net::structured_headers::Parameters()));
  };

  if (HasAttributionOsSupport(attribution_support)) {
    add_registrar("os");
  }
  if (HasAttributionWebSupport(attribution_support)) {
    add_registrar("web");
  }

  absl::optional<std::string> support_header =
      net::structured_headers::SerializeDictionary(
          net::structured_headers::Dictionary(std::move(registrars)));
  DCHECK(support_header.has_value());
  return std::move(*support_header);
}

bool HasAttributionOsSupport(mojom::AttributionSupport attribution_support) {
  switch (attribution_support) {
    case mojom::AttributionSupport::kOs:
    case mojom::AttributionSupport::kWebAndOs:
      return true;
    case mojom::AttributionSupport::kWeb:
    case mojom::AttributionSupport::kNone:
      return false;
  }
}

bool HasAttributionWebSupport(mojom::AttributionSupport attribution_support) {
  switch (attribution_support) {
    case mojom::AttributionSupport::kWeb:
    case mojom::AttributionSupport::kWebAndOs:
      return true;
    case mojom::AttributionSupport::kOs:
    case mojom::AttributionSupport::kNone:
      return false;
  }
}

bool HasAttributionSupport(mojom::AttributionSupport attribution_support) {
  return HasAttributionWebSupport(attribution_support) ||
         HasAttributionOsSupport(attribution_support);
}

}  // namespace network
