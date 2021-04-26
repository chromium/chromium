// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_BROWSER_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_BROWSER_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/dp_browser.h"

namespace headless {
class HeadlessBrowserImpl;
namespace protocol {

class BrowserHandler : public DomainHandler, public Browser::Backend {
 public:
  BrowserHandler(HeadlessBrowserImpl* browser, const std::string& target_id);
  ~BrowserHandler() override;

  // DomainHandler implementation
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Browser::Backend implementation
  Response GetWindowForTarget(
      Maybe<std::string> target_id,
      int* out_window_id,
      std::unique_ptr<Browser::Bounds>* out_bounds) override;
  Response GetWindowBounds(
      int window_id,
      std::unique_ptr<Browser::Bounds>* out_bounds) override;
  Response Close() override;
  Response SetWindowBounds(
      int window_id,
      std::unique_ptr<Browser::Bounds> out_bounds) override;
  Response SetDockTile(Maybe<std::string> label, Maybe<Binary> image) override;

 private:
  HeadlessBrowserImpl* browser_;
  std::string target_id_;
  DISALLOW_COPY_AND_ASSIGN(BrowserHandler);
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_BROWSER_HANDLER_H_
