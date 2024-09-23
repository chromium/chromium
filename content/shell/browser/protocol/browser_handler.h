// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_PROTOCOL_BROWSER_HANDLER_H_
#define CONTENT_SHELL_BROWSER_PROTOCOL_BROWSER_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "content/shell/browser/protocol/browser.h"
#include "content/shell/browser/protocol/domain_handler.h"

namespace content {
class BrowserContext;
namespace shell::protocol {

class BrowserHandler : public DomainHandler, public Browser::Backend {
 public:
  BrowserHandler(const raw_ref<const BrowserContext> browser_context,
                 std::string target_id);

  BrowserHandler(const BrowserHandler&) = delete;
  BrowserHandler& operator=(const BrowserHandler&) = delete;

  ~BrowserHandler() override;

 private:
  // DomainHandler implementation
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Browser::Backend implementation
  Response GetWindowForTarget(
      Maybe<std::string> target_id,
      int* out_window_id,
      std::unique_ptr<Browser::Bounds>* out_bounds) override;

  const raw_ref<const BrowserContext> browser_context_;
  const std::string target_id_;
};

}  // namespace shell::protocol
}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_PROTOCOL_BROWSER_HANDLER_H_
