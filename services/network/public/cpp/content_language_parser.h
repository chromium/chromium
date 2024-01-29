// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_LANGUAGE_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_LANGUAGE_PARSER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"

namespace network {

// Parses `Content-Language` headers and returns the parsed representations of
// content languages. The parsed representations are used to pass content
// language headers between processes.
//
// Returns base::nullopt if parsing failed and the header should be ignored;
// otherwise returns a (possibly empty) list of string. See
// https://datatracker.ietf.org/doc/html/rfc3282#section-2.
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<std::vector<std::string>> ParseContentLanguages(
    const std::string& header);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_LANGUAGE_PARSER_H_
