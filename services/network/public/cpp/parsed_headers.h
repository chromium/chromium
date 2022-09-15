// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PARSED_HEADERS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PARSED_HEADERS_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"

class GURL;
namespace net {
class HttpResponseHeaders;
}

namespace network {

// This function must remain:
// - As 'pure' as possible. See [pure-function].
// - Callable from any processes.
//   For security reasons, highly privileged processes must never call it
//   directly against untrustworthy inputs. See [rule-of-2]. In this case, you
//   can call it indirectly via NetworkContext.ParseHeaders().
//
// [rule-of-2]: //docs/security/rule-of-2.md
// [pure-function]: https://en.wikipedia.org/wiki/Pure_function
COMPONENT_EXPORT(NETWORK_CPP)
mojom::ParsedHeadersPtr PopulateParsedHeaders(
    const net::HttpResponseHeaders* headers,
    const GURL& url);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PARSED_HEADERS_H_
