// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_BROWSER_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_BROWSER_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "headless/lib/browser/protocol/browser.h"
#include "headless/lib/browser/protocol/domain_handler.h"

namespace headless {
class HeadlessBrowserImpl;
namespace protocol {

class BrowserHandler : public DomainHandler, public Browser::Backend {
 public:
  BrowserHandler(HeadlessBrowserImpl* browser, const std::string& target_id);

  BrowserHandler(const BrowserHandler&) = delete;
  BrowserHandler& operator=(const BrowserHandler&) = delete;

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
  raw_ptr<HeadlessBrowserImpl> browser_;
  std::string target_id_;
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_BROWSER_HANDLER_H_
