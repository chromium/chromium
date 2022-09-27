// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_plugin_script_forbidden_scope.h"

#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"

namespace blink {

bool WebPluginScriptForbiddenScope::IsForbidden() {
  return PluginScriptForbiddenScope::IsForbidden();
}

}  // namespace blink
