// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/scripting/scripting_utils.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/scripting/scripting_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/user_script.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
namespace scripting {

URLPatternSet GetPersistentScriptURLPatterns(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id) {
  URLPatternSet patterns;
  ExtensionPrefs::Get(browser_context)
      ->ReadPrefAsURLPatternSet(extension_id, kPrefPersistentScriptURLPatterns,
                                &patterns,
                                UserScript::ValidUserScriptSchemes());

  return patterns;
}

void SetPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                    const ExtensionId& extension_id,
                                    const URLPatternSet& patterns) {
  ExtensionPrefs::Get(browser_context)
      ->SetExtensionPrefURLPatternSet(
          extension_id, kPrefPersistentScriptURLPatterns, patterns);
}

void ClearPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                      const ExtensionId& extension_id) {
  ExtensionPrefs::Get(browser_context)
      ->UpdateExtensionPref(extension_id, kPrefPersistentScriptURLPatterns,
                            absl::nullopt);
}

}  // namespace scripting
}  // namespace extensions
