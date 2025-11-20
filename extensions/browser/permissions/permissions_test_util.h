// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PERMISSIONS_PERMISSIONS_TEST_UTIL_H_
#define EXTENSIONS_BROWSER_PERMISSIONS_PERMISSIONS_TEST_UTIL_H_

#include <string>
#include <vector>

#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;
class PermissionSet;
class URLPatternSet;

namespace permissions_test_util {

// Returns a list of `patterns` as strings, making it easy to compare for
// equality with readable errors. This will omit the chrome://favicon host, if
// present, from the result.
std::vector<std::string> GetPatternsAsStrings(const URLPatternSet& patterns);

// Calls corresponding PermissionsUpdater method respectively and wait for its
// asynchronous completion.
void GrantOptionalPermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions);
void GrantRuntimePermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions);
void RevokeOptionalPermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsUpdater::RemoveType remove_type);
void RevokeRuntimePermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions);

}  // namespace permissions_test_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PERMISSIONS_PERMISSIONS_TEST_UTIL_H_
