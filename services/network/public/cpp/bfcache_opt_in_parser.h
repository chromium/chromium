// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_BFCACHE_OPT_IN_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_BFCACHE_OPT_IN_PARSER_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"

namespace network {

// Parse `BFCache-Opt-In` header and returns true iff the token list includes
// `unload`.
// Explainer: https://github.com/nyaxt/bfcache-opt-in-header
COMPONENT_EXPORT(NETWORK_CPP)
bool ParseBFCacheOptInUnload(base::StringPiece header_value);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_BFCACHE_OPT_IN_PARSER_H_
