// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREFS_SCOPE_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREFS_SCOPE_H_


namespace extensions {

// Scope for a preference.
enum ExtensionPrefsScope {
  // Regular profile and incognito.
  kExtensionPrefsScopeRegular,
  // Regular profile only.
  kExtensionPrefsScopeRegularOnly,
  // Incognito profile; preference is persisted to disk and remains active
  // after a browser restart.
  kExtensionPrefsScopeIncognitoPersistent,
  // Incognito profile; preference is kept in memory and deleted when the
  // incognito session is terminated.
  kExtensionPrefsScopeIncognitoSessionOnly
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREFS_SCOPE_H_
