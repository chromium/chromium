// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BACKGROUND_SCRIPT_EXECUTOR_H_
#define EXTENSIONS_BROWSER_BACKGROUND_SCRIPT_EXECUTOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionRegistry;
class ProcessManager;
class ScriptResultQueue;

// A helper class to execute a script in an extension's background context,
// either its service worker or its (possibly lazy) background page.
// Returning results:
//  Return results with chrome.test.sendScriptResult(). This can be called
//  either synchronously or asynchronously from the injected script.
//  For compatibility with legacy scripts, background page contexts can choose
//  send results via window.domAutomationController.send(). New code should not
//  do this.
// This class is designed for single-use executions and is only meant to be used
// in tests.
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
  base::Value ExecuteScript(
      const ExtensionId& extension_id,
      const std::string& script,
      ResultCapture result_capture,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate);
  // Static variant of the above.
  static base::Value ExecuteScript(
      content::BrowserContext* browser_context,
      const ExtensionId& extension_id,
      const std::string& script,
      ResultCapture result_capture,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate);

  // Executes the given `script` and returns immediately, without waiting for
  // the script to finish. `script_user_activation` is used to determine
  // whether the script executes with a user gesture, and must be
  // `kDontActivate` for service worker-based extensions.
  bool ExecuteScriptAsync(
      const ExtensionId& extension_id,
      const std::string& script,
      ResultCapture result_capture,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate);
  // Static variant of the above. Inherently, this cannot handle a result
  // (because it is not returned synchronously and there's no exposed instance
  // of BackgroundScriptExecutor).
  static bool ExecuteScriptAsync(
      content::BrowserContext* browser_context,
      const ExtensionId& extension_id,
      const std::string& script,
      browsertest_util::ScriptUserActivation script_user_activation =
          browsertest_util::ScriptUserActivation::kDontActivate);

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
      browsertest_util::ScriptUserActivation script_user_activation);

  // Method to ADD_FAILURE() to the currently-running test with the given
  // `message` and other debugging info, like the injected script and associated
  // extension.
  void AddTestFailure(const std::string& message);

  // The associated BrowserContext. Must outlive this object.
  const raw_ptr<content::BrowserContext> browser_context_;
  // The associated ExtensionRegistry; tied to `browser_context_`.
  const raw_ptr<ExtensionRegistry> registry_;
  // The associated ProcessManager; tied to `browser_context_`.
  const raw_ptr<ProcessManager> process_manager_;

  // The type of background context the extension uses; lazily instantiated in
  // ExecuteScript*().
  std::optional<BackgroundType> background_type_;

  // The method the script will use to send the result.
  ResultCapture result_capture_method_ = ResultCapture::kNone;

  // The DOMMessageQueue used for retrieving results from background page-based
  // extensions with `ResultCapture::kWindowDomAutomationController`.
  std::unique_ptr<content::DOMMessageQueue> message_queue_;

  // The ScriptResultQueue for retrieving results from contexts using
  // `ResultCapture::kSendScriptResult`.
  std::unique_ptr<ScriptResultQueue> script_result_queue_;

  // The associated Extension.
  raw_ptr<const Extension, FlakyDanglingUntriaged> extension_ = nullptr;

  // The script to inject; cached mostly for logging purposes.
  std::string script_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BACKGROUND_SCRIPT_EXECUTOR_H_
