// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCOPED_WEB_FRAME_H_
#define EXTENSIONS_RENDERER_SCOPED_WEB_FRAME_H_

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"

namespace extensions {

// ScopedWebFrame is a class to create a dummy webview and frame for testing.
// The dymmy webview and frame will be destructed when the scope exits.
class ScopedWebFrame {
public:
  ScopedWebFrame();

  ScopedWebFrame(const ScopedWebFrame&) = delete;
  ScopedWebFrame& operator=(const ScopedWebFrame&) = delete;

  ~ScopedWebFrame();

  blink::WebLocalFrame* frame() { return frame_; }

private:
 blink::WebLocalFrameClient frame_client_;

 // The `WebAgentGroupScheduler`, webview and the frame are kept alive by
 // the ScopedWebFrame because they are not destructed unless
 // ~ScopedWebFrame explicitly closes the WebView.
 std::unique_ptr<blink::scheduler::WebAgentGroupScheduler>
     agent_group_scheduler_;
 raw_ptr<blink::WebView, DanglingUntriaged> view_;
 raw_ptr<blink::WebLocalFrame> frame_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCOPED_WEB_FRAME_H_
