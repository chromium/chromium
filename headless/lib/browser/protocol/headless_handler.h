// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_HANDLER_H_

#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/dp_headless_experimental.h"

namespace content {
class WebContents;
}

namespace headless {
class HeadlessBrowserImpl;

namespace protocol {

class HeadlessHandler : public DomainHandler,
                        public HeadlessExperimental::Backend {
 public:
  HeadlessHandler(HeadlessBrowserImpl* browser,
                  content::WebContents* web_contents);
  ~HeadlessHandler() override;

  // DomainHandler implementation
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;  // Also Headless::Backend implementation

  // Headless::Backend implementation
  Response Enable() override;
  void BeginFrame(Maybe<double> in_frame_time_ticks,
                  Maybe<double> in_interval,
                  Maybe<bool> no_display_updates,
                  Maybe<HeadlessExperimental::ScreenshotParams> screenshot,
                  std::unique_ptr<BeginFrameCallback> callback) override;

 private:
  HeadlessBrowserImpl* browser_;
  content::WebContents* web_contents_;
  std::unique_ptr<HeadlessExperimental::Frontend> frontend_;
  DISALLOW_COPY_AND_ASSIGN(HeadlessHandler);
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_HANDLER_H_
