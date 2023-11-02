// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_LINK_HEADER_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_LINK_HEADER_PARSER_H_

#include <vector>

#include "base/component_export.h"
#include "services/network/public/mojom/link_header.mojom.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

// Parses Link headers and returns the parsed representations of them. The
// parsed representations are used to pass Link headers between processes.
//
// A parsed representation does not contain all attributes and values of the
// original Link header. It only populates pre-defined attributes/values. See
// mojom::LinkHeader for pre-defined attributes and values.
//
// When an attribute is specified more than once the first one is parsed. The
// second and subsequent ones are ignored.
//
// The number of parsed representations can be different from the number of
// Link headers in `headers`. The parser ignores a Link header in the following
// situations to avoid processing unknown inputs:
//  * Any error occurs while parsing.
//  * Resolving the value of `href` fails.
//  * The `rel` attribute is not specified.
//  * The header contains an attribute which isn't pre-defined.
//  * The header contains an attribute of which value isn't pre-defined.
COMPONENT_EXPORT(NETWORK_CPP)
std::vector<mojom::LinkHeaderPtr> ParseLinkHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& base_url);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_LINK_HEADER_PARSER_H_
