// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/background_script_executor.h"

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/script_result_queue.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Returns a log-friendly script string.
std::string GetScriptToLog(const std::string& script) {
  // The maximum script size for which to print on failure.
  static constexpr int kMaxFailingScriptSizeToLog = 1000;
  return script.size() < kMaxFailingScriptSizeToLog ? script
                                                    : "<script too large>";
}

}  // namespace

BackgroundScriptExecutor::BackgroundScriptExecutor(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      registry_(ExtensionRegistry::Get(browser_context_)),
      process_manager_(ProcessManager::Get(browser_context_)) {}

BackgroundScriptExecutor::~BackgroundScriptExecutor() = default;

base::Value BackgroundScriptExecutor::ExecuteScript(
    const ExtensionId& extension_id,
    const std::string& script,
    ResultCapture result_capture,
    browsertest_util::ScriptUserActivation script_user_activation) {
  if (result_capture == ResultCapture::kNone) {
    AddTestFailure(
        "Cannot wait for a result with no result capture. "
        "Use ExecuteScriptAsync() instead");
    return base::Value();
  }

  ExecuteScriptAsync(extension_id, script, result_capture,
                     script_user_activation);
  return WaitForResult();
}

// static
base::Value BackgroundScriptExecutor::ExecuteScript(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& script,
    ResultCapture result_capture,
    browsertest_util::ScriptUserActivation script_user_activation) {
  return BackgroundScriptExecutor(browser_context)
      .ExecuteScript(extension_id, script, result_capture,
                     script_user_activation);
}

bool BackgroundScriptExecutor::ExecuteScriptAsync(
    const ExtensionId& extension_id,
    const std::string& script,
    ResultCapture result_capture,
    browsertest_util::ScriptUserActivation script_user_activation) {
  extension_ = registry_->enabled_extensions().GetByID(extension_id);
  script_ = script;
  result_capture_method_ = result_capture;
  if (!extension_) {
    AddTestFailure("No enabled extension with id: " + extension_id);
    return false;
  }

  if (BackgroundInfo::IsServiceWorkerBased(extension_)) {
    background_type_ = BackgroundType::kServiceWorker;
    DCHECK_NE(ResultCapture::kWindowDomAutomationController,
              result_capture_method_)
        << "Cannot use domAutomationController in a worker.";
    DCHECK_EQ(browsertest_util::ScriptUserActivation::kDontActivate,
              script_user_activation)
        << "Cannot provide a user gesture to service worker scripts";
    return ExecuteScriptInServiceWorker();
  }

  if (BackgroundInfo::HasBackgroundPage(extension_)) {
    background_type_ = BackgroundType::kPage;
    return ExecuteScriptInBackgroundPage(script_user_activation);
  }

  AddTestFailure(
      "Attempting to execute a background script for an extension"
      " with no background context");
  return false;
}

// static
bool BackgroundScriptExecutor::ExecuteScriptAsync(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& script,
    browsertest_util::ScriptUserActivation script_user_activation) {
  return BackgroundScriptExecutor(browser_context)
      .ExecuteScriptAsync(extension_id, script, ResultCapture::kNone,
                          script_user_activation);
}

base::Value BackgroundScriptExecutor::WaitForResult() {
  DCHECK(background_type_);
  DCHECK_NE(ResultCapture::kNone, result_capture_method_)
      << "Trying to wait for a result when no result was expected.";

  if (result_capture_method_ == ResultCapture::kSendScriptResult) {
    DCHECK(script_result_queue_);
    return script_result_queue_->GetNextResult();
  }

  DCHECK_EQ(ResultCapture::kWindowDomAutomationController,
            result_capture_method_);
  DCHECK(message_queue_);
  std::string next_message;
  if (!message_queue_->WaitForMessage(&next_message)) {
    AddTestFailure("Failed to wait for message");
    return base::Value();
  }
  std::optional<base::Value> value =
      base::JSONReader::Read(next_message, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value) {
    AddTestFailure("Received bad message: " + next_message);
    return base::Value();
  }
  return std::move(*value);
}

bool BackgroundScriptExecutor::ExecuteScriptInServiceWorker() {
  std::vector<WorkerId> worker_ids =
      process_manager_->GetServiceWorkersForExtension(extension_->id());
  if (worker_ids.size() != 1u) {
    AddTestFailure(base::StringPrintf(
        "Incorrect number of workers registered for extension: %zu",
        worker_ids.size()));
    return false;
  }

  if (result_capture_method_ == ResultCapture::kSendScriptResult) {
    script_result_queue_ = std::make_unique<ScriptResultQueue>();
  }

  content::ServiceWorkerContext* service_worker_context =
      util::GetServiceWorkerContextForExtensionId(extension_->id(),
                                                  browser_context_);

  service_worker_context->ExecuteScriptForTest(  // IN-TEST
      script_, worker_ids[0].version_id,
      base::BindOnce(
          [](std::string script, base::Value _ignored_value,
             const std::optional<std::string>& error) {
            // `_ignored_value` is ignored, because extension tests are expected
            // to communicate their result via `chrome.test.sendScriptResult`
            // instead (see also `BackgroundScriptExecutor::WaitForResult`).
            //
            // OTOH, we don't want to `base::DoNothing::Once` when
            // `error.has_value()`, because it oftentimes means that a newly
            // authored test has some bugs, throws an exception, and will never
            // call `chrome.test.sendScriptResult`.  To help debug these
            // scenarios we try to at least report the (asynchronously reported)
            // exception via `LOG(WARNING)`.
            if (error.has_value()) {
              LOG(WARNING)
                  << "BackgroundScriptExecutor::ExecuteScriptInServiceWorker "
                  << "resulted in the following exception:\n    "
                  << error.value() << "\nwhen executing the following script:\n"
                  << script;
            }
          },
          script_));
  return true;
}

bool BackgroundScriptExecutor::ExecuteScriptInBackgroundPage(
    browsertest_util::ScriptUserActivation script_user_activation) {
  ExtensionHost* host =
      process_manager_->GetBackgroundHostForExtension(extension_->id());
  if (!host) {
    AddTestFailure("Extension does not have an active background page");
    return false;
  }

  switch (result_capture_method_) {
    case ResultCapture::kNone:
      break;
    case ResultCapture::kSendScriptResult:
      script_result_queue_ = std::make_unique<ScriptResultQueue>();
      break;
    case ResultCapture::kWindowDomAutomationController:
      message_queue_ =
          std::make_unique<content::DOMMessageQueue>(host->host_contents());
      break;
  }

  if (script_user_activation ==
      browsertest_util::ScriptUserActivation::kActivate) {
    content::ExecuteScriptAsync(host->host_contents(), script_);
  } else {
    content::ExecuteScriptAsyncWithoutUserGesture(host->host_contents(),
                                                  script_);
  }
  return true;
}

void BackgroundScriptExecutor::AddTestFailure(const std::string& message) {
  ADD_FAILURE() << "Background script execution failed: " << message
                << ". Extension: "
                << (extension_ ? extension_->name() : "<not found>")
                << ", script: " << GetScriptToLog(script_);
}

}  // namespace extensions
