// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_
#define EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_

#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions::scripting {

// Returns a dynamic content script ID by appending the prefix for dynamic
// scripts to a user provided `script_id`.
inline std::string CreateDynamicScriptID(const std::string& script_id) {
  return UserScript::kDynamicContentScriptPrefix + script_id;
}

// Returns the set of URL patterns from persistent dynamic content scripts.
// Patterns are stored in prefs so UserScriptListener can access them
// synchronously as the persistent scripts themselves are stored in a
// StateStore.
URLPatternSet GetPersistentScriptURLPatterns(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id);

// Updates the set of URL patterns from persistent dynamic content scripts. This
// preference gets cleared on extension update.
void SetPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                    const ExtensionId& extension_id,
                                    const URLPatternSet& patterns);

// Clears the set of URL patterns from persistent dynamic content scripts.
void ClearPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                      const ExtensionId& extension_id);

}  // namespace extensions::scripting

#endif  // EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_
