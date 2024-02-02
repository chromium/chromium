// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browsertest_util.h"

#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::browsertest_util {

namespace {

// Returns a log-friendly script string.
std::string GetScriptToLog(const std::string& script) {
  // The maximum script size for which to print on failure.
  static constexpr int kMaxFailingScriptSizeToLog = 1000;
  return (script.size() < kMaxFailingScriptSizeToLog) ? script
                                                      : "<script too large>";
}

}  // namespace

base::Value ExecuteScriptInBackgroundPage(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation) {
  BackgroundScriptExecutor script_executor(context);
  base::Value value = script_executor.ExecuteScript(
      extension_id, script,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult,
      script_user_activation);
  if (value.is_none()) {
    ADD_FAILURE() << "Bad return value. Script: " << GetScriptToLog(script);
  }
  return value;
}

bool ExecuteScriptInBackgroundPageNoWait(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation) {
  return BackgroundScriptExecutor::ExecuteScriptAsync(
      context, extension_id, script, script_user_activation);
}

std::string ExecuteScriptInBackgroundPageDeprecated(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation) {
  BackgroundScriptExecutor script_executor(context);
  // Legacy scripts were written to pass the (string) result via
  // window.domAutomationController.send().
  base::Value value = script_executor.ExecuteScript(
      extension_id, script,
      BackgroundScriptExecutor::ResultCapture::kWindowDomAutomationController,
      script_user_activation);
  if (!value.is_string()) {
    ADD_FAILURE() << "Bad return value: " << value.type()
                  << "; script: " << GetScriptToLog(script);
    return "";
  }

  return value.GetString();
}

void StopServiceWorkerForExtensionGlobalScope(content::BrowserContext* context,
                                              const ExtensionId& extension_id) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension) << "Unknown extension ID.";
  base::RunLoop run_loop;
  content::ServiceWorkerContext* service_worker_context =
      context->GetDefaultStoragePartition()->GetServiceWorkerContext();
  content::StopServiceWorkerForScope(service_worker_context, extension->url(),
                                     run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace extensions::browsertest_util
