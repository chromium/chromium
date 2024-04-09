// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_AVAIL_LANGUAGE_HEADER_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_AVAIL_LANGUAGE_HEADER_PARSER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"

namespace network {

// Parses `Avail-Language` headers and returns the parsed representations of
// them. The parsed representations are used to pass Avail-Language headers
// between processes.
//
// Returns base::nullopt if parsing failed and the header should be ignored;
// otherwise returns a (possibly empty) list of string. See
// https://mnot.github.io/I-D/draft-nottingham-http-availability-hints.html.
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<std::vector<std::string>> ParseAvailLanguage(
    const std::string& header);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_AVAIL_LANGUAGE_HEADER_PARSER_H_
