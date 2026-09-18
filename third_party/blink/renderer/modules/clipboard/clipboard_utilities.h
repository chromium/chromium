// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_UTILITIES_H_

#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// Returns true if the context's security origin uses a registered extension
// scheme.
// Note: In Blink renderer code, checking the security origin protocol is the
// standard heuristic for identifying extension contexts. This is used solely
// for renderer-side feature gating and API exposure. Authoritative security
// verification (including extensions::ProcessMap and ExtensionRegistry checks)
// is enforced in the browser process via ChromeContentBrowserClient.
//
// The caller keeps ownership of `context` and nothing here outlives the call.
// Must be called on the thread that owns `context`, since it reads the
// security origin.
inline bool IsExtensionContext(const ExecutionContext* context) {
  return context && CommonSchemeRegistry::IsExtensionScheme(
                        context->GetSecurityOrigin()->Protocol().Ascii());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_UTILITIES_H_
