// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EXTENSION_CONSOLE_OBSERVER_H_
#define EXTENSIONS_BROWSER_TEST_EXTENSION_CONSOLE_OBSERVER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/scoped_observation.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
struct ConsoleMessage;
class RenderFrameHost;
}  // namespace content

namespace extensions {

// Monitors an extension's console for errors or warnings.
class TestExtensionConsoleObserver
    : public content::ServiceWorkerContextObserver,
      public content::WebContentsObserver {
 public:
  TestExtensionConsoleObserver(content::BrowserContext* context,
                               const ExtensionId& extension_id,
                               bool fail_on_error);
  ~TestExtensionConsoleObserver() override;

  TestExtensionConsoleObserver(const TestExtensionConsoleObserver&) = delete;
  TestExtensionConsoleObserver& operator=(const TestExtensionConsoleObserver&) =
      delete;

  // Add a set of allowed error messages, to ignore.
  void SetAllowedErrorMessages(base::flat_set<std::u16string> allowed_messages);
  size_t GetErrorCount() { return messages_.size(); }
  std::string GetMessageAt(size_t index);

 private:
  void HandleConsoleMessage(const std::u16string& message,
                            blink::mojom::ConsoleMessageLevel log_level);

  // ServiceWorkerContextObserver:
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const content::ConsoleMessage& message) override;

  // WebContentsObserver:
  void OnDidAddMessageToConsole(
      content::RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;

  ExtensionId extension_id_;
  bool fail_on_error_;
  base::flat_set<std::u16string> allowed_errors_;
  std::vector<std::u16string> messages_;

  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EXTENSION_CONSOLE_OBSERVER_H_
