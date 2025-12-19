// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browsertest_util.h"

#include "base/test/bind.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/script_executor.h"
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

void ExecuteUserScriptInternal(
    ScriptExecutor& script_executor,
    const ExtensionId& extension_id,
    const std::string& script,
    const std::optional<std::string>& world_id,
    ScriptExecutor::ScriptFinishedCallback callback) {
  std::vector<mojom::JSSourcePtr> sources;
  sources.push_back(mojom::JSSource::New(script, GURL()));
  script_executor.ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension_id),
      mojom::CodeInjection::NewJs(mojom::JSInjection::New(
          std::move(sources), mojom::ExecutionWorld::kUserScript, world_id,
          blink::mojom::WantResultOption::kWantResult,
          blink::mojom::UserActivationOption::kDoNotActivate,
          blink::mojom::PromiseResultOption::kAwait)),
      ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
      mojom::MatchOriginAsFallbackBehavior::kNever,
      mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
      /*webview_src=*/GURL(), std::move(callback));
}

}  // namespace

base::Value ExecuteUserScript(content::WebContents* web_contents,
                              const ExtensionId& extension_id,
                              const std::string& script,
                              const std::optional<std::string>& world_id) {
  base::RunLoop run_loop;

  ScriptExecutor script_executor(web_contents);
  std::vector<ScriptExecutor::FrameResult> script_results;
  auto on_complete =
      [&run_loop, &script_results](
          std::vector<ScriptExecutor::FrameResult> frame_results) {
        script_results = std::move(frame_results);
        run_loop.Quit();
      };

  ExecuteUserScriptInternal(script_executor, extension_id, script, world_id,
                            base::BindLambdaForTesting(on_complete));

  run_loop.Run();

  if (script_results.size() != 1) {
    ADD_FAILURE() << "Incorrect script execution result count: "
                  << script_results.size();
    return base::Value();
  }

  ScriptExecutor::FrameResult& frame_result = script_results[0];
  if (!frame_result.error.empty()) {
    ADD_FAILURE() << "Unexpected script error: " << frame_result.error;
    return base::Value();
  }

  return std::move(frame_result.value);
}

void ExecuteUserScriptNoWait(content::WebContents* web_contents,
                             const ExtensionId& extension_id,
                             const std::string& script,
                             const std::optional<std::string>& world_id) {
  ScriptExecutor script_executor(web_contents);
  ExecuteUserScriptInternal(script_executor, extension_id, script, world_id,
                            base::DoNothing());
}

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
  StopServiceWorkerForExtensionGlobalScope(context, extension_id,
                                           base::RunLoop::Type::kDefault);
}
void StopServiceWorkerForExtensionGlobalScope(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    base::RunLoop::Type stop_waiter_type) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension) << "Unknown extension ID.";
  base::RunLoop stop_waiter(stop_waiter_type);
  content::ServiceWorkerContext* service_worker_context =
      context->GetDefaultStoragePartition()->GetServiceWorkerContext();
  content::StopServiceWorkerForScope(service_worker_context, extension->url(),
                                     stop_waiter.QuitClosure());
  stop_waiter.Run();
}

bool DidChangeTitle(content::WebContents& web_contents,
                    const std::u16string& original_title,
                    const std::u16string& changed_title) {
  const std::u16string& title = web_contents.GetTitle();
  if (title == changed_title) {
    return true;
  }
  if (title == original_title) {
    return false;
  }
  ADD_FAILURE() << "Unexpected page title found:  " << title;
  return false;
}

}  // namespace extensions::browsertest_util
