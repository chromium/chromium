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

// Parses an `Avail-Language` header and returns the parsed representation,
// which is used to pass Avail-Language headers between processes.
//
// Returns `std::nullopt` if parsing failed and the header should be ignored;
// otherwise returns a (possibly empty) list of string. Note that the case of
// the original language tokens is retained in the return value; it is up to
// the user of these values to perform matching case-insensitively, if
// appropriate.
//
// See
// https://projects.mnot.net/I-D/draft-nottingham-http-availability-hints.html#name-content-language
// for details.
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<std::vector<std::string>> ParseAvailLanguage(
    const std::string& header);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_AVAIL_LANGUAGE_HEADER_PARSER_H_
