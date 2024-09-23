// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_
#define EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_

#include <string>

#include "extensions/common/extension_id.h"

namespace base {
class Value;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions::browsertest_util {

// Determine if a user activation notification should be triggered before
// executing a script
enum class ScriptUserActivation {
  kActivate,
  kDontActivate,
};

// Waits until |script| calls "chrome.test.sendScriptResult(result)",
// where |result| is a serializable value, and returns |result|. Fails
// the test and returns an empty base::Value if |extension_id| isn't
// installed in |context| or doesn't have a background page, or if
// executing the script fails. The argument |script_user_activation|
// determines if the script should be executed after a user activation.
base::Value ExecuteScriptInBackgroundPage(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation =
        ScriptUserActivation::kDontActivate);

// Same as ExecuteScriptInBackgroundPage, but doesn't wait for the script
// to return a result. Fails the test and returns false if |extension_id|
// isn't installed in |context| or doesn't have a background page, or if
// executing the script fails. The argument |script_user_activation|
// determines if the script should be executed after a user activation.
bool ExecuteScriptInBackgroundPageNoWait(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation =
        ScriptUserActivation::kDontActivate);

// Waits until |script| calls "window.domAutomationController.send(result)",
// where |result| is a string, and returns |result|. Fails the test and returns
// an empty string if |extension_id| isn't installed in |context| or doesn't
// have a background page, or if executing the script fails. The argument
// |script_user_activation| determines if the script should be executed after a
// user activation.
std::string ExecuteScriptInBackgroundPageDeprecated(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation =
        ScriptUserActivation::kDontActivate);

// Synchronously stops the service worker registered by the extension with the
// given `extension_id` at global scope. The extension must be installed and
// enabled.
void StopServiceWorkerForExtensionGlobalScope(content::BrowserContext* context,
                                              const ExtensionId& extension_id);

}  // namespace extensions::browsertest_util

#endif  // EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_
