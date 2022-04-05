// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browsertest_util.h"

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extension_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace browsertest_util {

std::string ExecuteScriptInBackgroundPage(
    content::BrowserContext* context,
    const std::string& extension_id,
    const std::string& script,
    ScriptUserActivation script_user_activation) {
  ExtensionHost* host =
      ProcessManager::Get(context)->GetBackgroundHostForExtension(extension_id);
  if (!host) {
    ADD_FAILURE() << "Extension " << extension_id << " has no background page.";
    return "";
  }

  std::string result;
  bool success;
  if (script_user_activation == ScriptUserActivation::kActivate) {
    success = content::ExecuteScriptAndExtractString(host->host_contents(),
                                                     script, &result);
  } else {
    DCHECK_EQ(script_user_activation, ScriptUserActivation::kDontActivate);
    success = content::ExecuteScriptWithoutUserGestureAndExtractString(
        host->host_contents(), script, &result);
  }

  // The maximum script size for which to print on failure.
  constexpr int kMaxFailingScriptSizeToLog = 1000;
  if (!success) {
    std::string message_detail = script.length() < kMaxFailingScriptSizeToLog
                                     ? script
                                     : "<script too large>";
    ADD_FAILURE() << "Executing script failed: " << message_detail;
    result.clear();
  }
  return result;
}

bool ExecuteScriptInBackgroundPageNoWait(content::BrowserContext* context,
                                         const std::string& extension_id,
                                         const std::string& script) {
  ExtensionHost* host =
      ProcessManager::Get(context)->GetBackgroundHostForExtension(extension_id);
  if (!host) {
    ADD_FAILURE() << "Extension " << extension_id << " has no background page.";
    return false;
  }
  content::ExecuteScriptAsync(host->host_contents(), script);
  return true;
}

void ExecuteScriptInServiceWorker(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const std::string& script,
    base::OnceCallback<void(base::Value)> callback) {
  ProcessManager* process_manager = ProcessManager::Get(browser_context);
  ASSERT_TRUE(process_manager);
  std::vector<WorkerId> worker_ids =
      process_manager->GetServiceWorkersForExtension(extension_id);
  ASSERT_EQ(1u, worker_ids.size())
      << "Incorrect number of workers registered for extension.";
  content::ServiceWorkerContext* service_worker_context =
      util::GetStoragePartitionForExtensionId(extension_id, browser_context)
          ->GetServiceWorkerContext();
  auto callback_adapter =
      [](base::OnceCallback<void(base::Value)> original_callback,
         base::Value value, const absl::optional<std::string>& error) {
        ASSERT_FALSE(error.has_value()) << *error;
        std::move(original_callback).Run(std::move(value));
      };
  service_worker_context->ExecuteScriptForTest(  // IN-TEST
      script, worker_ids[0].version_id,
      base::BindOnce(callback_adapter, std::move(callback)));
}

void StopServiceWorkerForExtensionGlobalScope(content::BrowserContext* context,
                                              const std::string& extension_id) {
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

}  // namespace browsertest_util
}  // namespace extensions
