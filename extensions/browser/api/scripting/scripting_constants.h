// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_CONSTANTS_H_

namespace extensions {
namespace scripting {

// The all_urls_includes_chrome_urls flag is only true for the legacy ChromeVox
// extension, which does not call this API. Therefore we can assume it to be
// always false.
inline constexpr bool kAllUrlsIncludesChromeUrls = false;

// The key for the field in the extension's StateStore for dynamic content
// script metadata that persists across sessions.
inline constexpr char kRegisteredScriptsStorageKey[] = "dynamic_scripts";

// The key for storing a dynamic content script's id.
inline constexpr char kId[] = "id";

// Key corresponding to the set of URL patterns from the extension's persistent
// dynamic content scripts.
inline constexpr const char kPrefPersistentScriptURLPatterns[] =
    "persistent_script_url_patterns";

}  // namespace scripting
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_CONSTANTS_H_
