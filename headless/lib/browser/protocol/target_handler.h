// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_TARGET_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_TARGET_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/dp_target.h"

namespace headless {
class HeadlessBrowserImpl;
namespace protocol {

class TargetHandler : public DomainHandler, public Target::Backend {
 public:
  explicit TargetHandler(HeadlessBrowserImpl* browser);
  ~TargetHandler() override;

  // DomainHandler implementation
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Target::Backend implementation
  Response CreateTarget(const std::string& url,
                        Maybe<int> width,
                        Maybe<int> height,
                        Maybe<std::string> context_id,
                        Maybe<bool> enable_begin_frame_control,
                        Maybe<bool> new_window,
                        Maybe<bool> background,
                        std::string* out_target_id) override;
  Response CloseTarget(const std::string& target_id,
                       bool* out_success) override;

 private:
  HeadlessBrowserImpl* browser_;
  DISALLOW_COPY_AND_ASSIGN(TargetHandler);
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_TARGET_HANDLER_H_
