// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_OVERLAY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_OVERLAY_HANDLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/overlay.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class OverlayHandler : public DevToolsDomainHandler, public Overlay::Backend {
 public:
  OverlayHandler();

  OverlayHandler(const OverlayHandler&) = delete;
  OverlayHandler& operator=(const OverlayHandler&) = delete;

  ~OverlayHandler() override;
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  Response SetInspectMode(
      const String& in_mode,
      Maybe<protocol::Overlay::HighlightConfig> in_highlightConfig) override;
  Response SetPausedInDebuggerMessage(Maybe<String> in_message) override;
  Response Disable() override;

 private:
  void UpdateCaptureInputEvents();

  raw_ptr<RenderFrameHostImpl> host_ = nullptr;
  std::string inspect_mode_;
  std::string paused_message_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_OVERLAY_HANDLER_H_
