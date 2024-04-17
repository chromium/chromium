// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_extension_console_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/console_message.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace extensions {

TestExtensionConsoleObserver::TestExtensionConsoleObserver(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    bool fail_on_error)
    : extension_id_(extension_id), fail_on_error_(fail_on_error) {
  int manifest_version = ExtensionRegistry::Get(context)
                             ->enabled_extensions()
                             .GetByID(extension_id)
                             ->manifest_version();
  if (manifest_version == 3) {
    scoped_observation_.Observe(
        service_worker_test_utils::GetServiceWorkerContext(context));
  } else {
    CHECK_EQ(manifest_version, 2);
    ExtensionHost* host =
        ProcessManager::Get(context)->GetBackgroundHostForExtension(
            extension_id);
    WebContentsObserver::Observe(host->host_contents());
  }
}

TestExtensionConsoleObserver::~TestExtensionConsoleObserver() = default;

void TestExtensionConsoleObserver::SetAllowedErrorMessages(
    base::flat_set<std::u16string> allowed_messages) {
  if (!fail_on_error_) {
    return;
  }
  allowed_errors_ = std::move(allowed_messages);
}

std::string TestExtensionConsoleObserver::GetMessageAt(size_t index) {
  if (index < 0 || static_cast<size_t>(index) >= messages_.size()) {
    ADD_FAILURE() << "Tried to retrieve a non-existent message at index: "
                  << index;
    return std::string();
  }
  return base::UTF16ToUTF8(messages_[index]);
}

void TestExtensionConsoleObserver::HandleConsoleMessage(
    const std::u16string& message,
    blink::mojom::ConsoleMessageLevel log_level) {
  if (allowed_errors_.contains(message)) {
    return;
  }
  if (log_level == blink::mojom::ConsoleMessageLevel::kWarning ||
      log_level == blink::mojom::ConsoleMessageLevel::kError) {
    messages_.push_back(message);
  }
}

void TestExtensionConsoleObserver::OnReportConsoleMessage(
    int64_t version_id,
    const GURL& scope,
    const content::ConsoleMessage& message) {
  HandleConsoleMessage(message.message, message.message_level);
}

void TestExtensionConsoleObserver::OnDidAddMessageToConsole(
    content::RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  if (source_frame->GetLastCommittedURL().host_piece() != extension_id_) {
    return;
  }
  HandleConsoleMessage(message, log_level);
}

}  // namespace extensions
