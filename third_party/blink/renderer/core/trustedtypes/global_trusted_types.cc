// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/global_trusted_types.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

namespace blink {

// static
TrustedTypePolicyFactory* GlobalTrustedTypes::trustedTypes(
    ScriptState* script_state,
    LocalDOMWindow& window) {
  return window.GetTrustedTypesForWorld(script_state->World());
}

// static
TrustedTypePolicyFactory* GlobalTrustedTypes::trustedTypes(
    ScriptState* script_state,
    WorkerGlobalScope& worker) {
  return worker.GetTrustedTypes();
}

}  // namespace blink
