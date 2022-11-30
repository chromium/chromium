// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_VARIANTS_HEADER_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_VARIANTS_HEADER_PARSER_H_

#include <vector>

#include "base/component_export.h"
#include "services/network/public/mojom/variants_header.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

using SupportedVariantsNameSet = base::flat_set<std::string>;

// Parses `Variants` headers and returns the parsed representations of them.
// The parsed representations are used to pass Variants headers between
// processes.
//
// Returns base::nullopt if parsing failed and the header should be ignored;
// otherwise returns a (possibly empty) list of Variant representations. See
// https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-variants-06.
COMPONENT_EXPORT(NETWORK_CPP)
absl::optional<std::vector<mojom::VariantsHeaderPtr>> ParseVariantsHeaders(
    const std::string& header);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_VARIANTS_HEADER_PARSER_H_