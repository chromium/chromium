// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CONNECTION_ALLOWLIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CONNECTION_ALLOWLIST_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class ExecutionContext;
class KURL;

// This will check the given URL against the allowlists specified in
// |allowlists|. If the request doesn't match the enforced allowlist, this
// function will return `true` (signaling that the request should be blocked),
// and a report will be generated.
//
CORE_EXPORT bool ShouldBlockRequestViaConnectionAllowlist(
    ExecutionContext* context,
    const KURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CONNECTION_ALLOWLIST_H_
