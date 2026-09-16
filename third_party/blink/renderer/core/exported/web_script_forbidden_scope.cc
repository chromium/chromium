// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_script_forbidden_scope.h"

#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

bool WebScriptForbiddenScope::IsForbidden() {
  return ScriptForbiddenScope::IsScriptForbidden();
}

}  // namespace blink
