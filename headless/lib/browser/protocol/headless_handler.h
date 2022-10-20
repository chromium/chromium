// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/headless_experimental.h"

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

  HeadlessHandler(const HeadlessHandler&) = delete;
  HeadlessHandler& operator=(const HeadlessHandler&) = delete;

  ~HeadlessHandler() override;

 private:
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

  raw_ptr<HeadlessBrowserImpl> browser_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<HeadlessExperimental::Frontend> frontend_;
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_HEADLESS_HANDLER_H_
