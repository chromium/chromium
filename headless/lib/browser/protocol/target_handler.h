// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_TARGET_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_TARGET_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/target.h"

namespace headless {
class HeadlessBrowserImpl;
namespace protocol {

class TargetHandler : public DomainHandler, public Target::Backend {
 public:
  explicit TargetHandler(HeadlessBrowserImpl* browser);

  TargetHandler(const TargetHandler&) = delete;
  TargetHandler& operator=(const TargetHandler&) = delete;

  ~TargetHandler() override;

  // DomainHandler implementation
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Target::Backend implementation
  Response CreateTarget(const std::string& url,
                        std::optional<int> left,
                        std::optional<int> top,
                        std::optional<int> width,
                        std::optional<int> height,
                        std::optional<std::string> window_state,
                        std::optional<std::string> context_id,
                        std::optional<bool> enable_begin_frame_control,
                        std::optional<bool> new_window,
                        std::optional<bool> background,
                        std::optional<bool> for_tab,
                        std::optional<bool> hidden,
                        std::string* out_target_id) override;
  Response CloseTarget(const std::string& target_id,
                       bool* out_success) override;

 private:
  raw_ptr<HeadlessBrowserImpl> browser_;
  // Keeps hidden targets' ids to close them when the session is closed.
  std::unordered_set<std::string> hidden_web_contents_;
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_TARGET_HANDLER_H_
