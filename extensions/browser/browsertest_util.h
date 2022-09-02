// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_
#define EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionRegistry;
class ProcessManager;
class ScriptResultQueue;

namespace browsertest_util {

// Determine if a user activation notification should be triggered before
// executing a script
enum class ScriptUserActivation {
  kActivate,
  kDontActivate,
};

// A helper class to execute a script in an extension's background context,
// either its service worker or its (possibly lazy) background page.
// Returning results:
//  Return results with chrome.test.sendScriptResult(). This can be called
//  either synchronously or asynchronously from the injected script.
//  For compatibility with legacy scripts, background page contexts can choose
//  send results via window.domAutomationController.send(). New code should not
//  do this.
// This class is designed for single-use executions.
class BackgroundScriptExecutor {
 public:
  // The manner in which the script will use to send the result.
  enum class ResultCapture {
    // No result will be captured. The caller only cares about injecting the
    // script and may wait for another signal of execution.
    kNone,
    // Result sent with chrome.test.sendScriptResult().
    kSendScriptResult,
    // Result sent with window.domAutomationController.send().
    // DON'T USE. This is only here for backwards compatibility with tests that
    // were written before chrome.test.sendScriptResult() exists, and this
    // doesn't work with service worker contexts.
    kWindowDomAutomationController,
  };

  explicit BackgroundScriptExecutor(content::BrowserContext* browser_context);
  ~BackgroundScriptExecutor();

  // Executes the given `script` and waits for execution to complete, returning
  // the result. `script_user_activation` is used to determine whether the
  // script executes with a user gesture, and must be be `kDontActivate` for
  // service worker-based extensions.
  base::Value ExecuteScript(const ExtensionId& extension_id,
                            const std::string& script,
                            ResultCapture result_capture,
                            ScriptUserActivation script_user_activation =
                                ScriptUserActivation::kDontActivate);
  // Static variant of the above.
  static base::Value ExecuteScript(content::BrowserContext* browser_context,
                                   const ExtensionId& extension_id,
                                   const std::string& script,
                                   ResultCapture result_capture,
                                   ScriptUserActivation script_user_activation =
                                       ScriptUserActivation::kDontActivate);

  // Executes the given `script` and returns immediately, without waiting for
  // the script to finish. `script_user_activation` is used to determine
  // whether the script executes with a user gesture, and must be
  // `kDontActivate` for service worker-based extensions.
  bool ExecuteScriptAsync(const ExtensionId& extension_id,
                          const std::string& script,
                          ResultCapture result_capture,
                          ScriptUserActivation script_user_activation =
                              ScriptUserActivation::kDontActivate);
  // Static variant of the above. Inherently, this cannot handle a result
  // (because it is not returned synchronously and there's no exposed instance
  // of BackgroundScriptExecutor).
  static bool ExecuteScriptAsync(content::BrowserContext* browser_context,
                                 const ExtensionId& extension_id,
                                 const std::string& script,
                                 ScriptUserActivation script_user_activation =
                                     ScriptUserActivation::kDontActivate);

  // Waits for the result of the script execution; for use with
  // `ExecuteScriptAsync()`.
  base::Value WaitForResult();

 private:
  enum class BackgroundType {
    kServiceWorker,
    kPage,
  };

  // Helper method to execute the script in a service worker context.
  bool ExecuteScriptInServiceWorker();

  // Helper method to execute the script in a background page context.
  bool ExecuteScriptInBackgroundPage(
      ScriptUserActivation script_user_activation);

  // Method to ADD_FAILURE() to the currently-running test with the given
  // `message` and other debugging info, like the injected script and associated
  // extension.
  void AddTestFailure(const std::string& message);

  // The associated BrowserContext. Must outlive this object.
  content::BrowserContext* const browser_context_;
  // The associated ExtensionRegistry; tied to `browser_context_`.
  ExtensionRegistry* const registry_;
  // The associated ProcessManager; tied to `browser_context_`.
  ProcessManager* const process_manager_;

  // The type of background context the extension uses; lazily instantiated in
  // ExecuteScript*().
  absl::optional<BackgroundType> background_type_;

  // The method the script will use to send the result.
  ResultCapture result_capture_method_ = ResultCapture::kNone;

  // The DOMMessageQueue used for retrieving results from background page-based
  // extensions with `ResultCapture::kWindowDomAutomationController`.
  std::unique_ptr<content::DOMMessageQueue> message_queue_;

  // The ScriptResultQueue for retrieving results from contexts using
  // `ResultCapture::kSendScriptResult`.
  std::unique_ptr<ScriptResultQueue> script_result_queue_;

  // The associated Extension.
  const Extension* extension_ = nullptr;

  // The script to inject; cached mostly for logging purposes.
  std::string script_;
};

// Waits until |script| calls "window.domAutomationController.send(result)",
// where |result| is a string, and returns |result|. Fails the test and returns
// an empty string if |extension_id| isn't installed in |context| or doesn't
// have a background page, or if executing the script fails. The argument
// |script_user_activation| determines if the script should be executed after a
// user activation.
std::string ExecuteScriptInBackgroundPage(
    content::BrowserContext* context,
    const std::string& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation =
        ScriptUserActivation::kActivate);

// Same as ExecuteScriptInBackgroundPage, but doesn't wait for the script
// to return a result. Fails the test and returns false if |extension_id|
// isn't installed in |context| or doesn't have a background page, or if
// executing the script fails.
bool ExecuteScriptInBackgroundPageNoWait(content::BrowserContext* context,
                                         const std::string& extension_id,
                                         const std::string& script);

// Synchronously stops the service worker registered by the extension with the
// given `extension_id` at global scope. The extension must be installed and
// enabled.
void StopServiceWorkerForExtensionGlobalScope(content::BrowserContext* context,
                                              const std::string& extension_id);

}  // namespace browsertest_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSERTEST_UTIL_H_
