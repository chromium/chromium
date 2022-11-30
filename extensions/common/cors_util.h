// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CORS_UTIL_H_
#define EXTENSIONS_COMMON_CORS_UTIL_H_

#include <vector>

#include "services/network/public/mojom/cors_origin_pattern.mojom.h"

namespace extensions {

class Extension;

// Creates a CorsOriginPatternPtr vector that contains allowed origin list
// for the passed |extension|. Returned vector will be used to register the list
// to network::NetworkContext and blink::SecurityPolicy.
std::vector<network::mojom::CorsOriginPatternPtr>
CreateCorsOriginAccessAllowList(const Extension& extension);

// Creates a CorsOriginPatternPtr vector that contains blocked origin list
// for the passed |extension|. Returned vector will be used to register the list
// to network::NetworkContext and blink::SecurityPolicy.
std::vector<network::mojom::CorsOriginPatternPtr>
CreateCorsOriginAccessBlockList(const Extension& extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CORS_UTIL_H_
