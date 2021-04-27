// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_BFCACHE_OPT_IN_PARSER_H_
#define SERVICES_NETWORK_BFCACHE_OPT_IN_PARSER_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"

namespace network {

// Parse `BFCache-Opt-In` header and returns true iff the token list includes
// `unload`.
// Explainer:
// https://docs.google.com/document/d/128Do3mrSAL2ngG12eXoD8585sm1j-c-sU6C9jMgM5Zg/edit
// TODO(crbug.com/1201653): Replace the above link with the published version.
COMPONENT_EXPORT(NETWORK_SERVICE)
bool ParseBFCacheOptInUnload(base::StringPiece header_value);

}  // namespace network

#endif  // SERVICES_NETWORK_BFCACHE_OPT_IN_PARSER_H_
