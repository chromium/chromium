// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/scoped_web_frame.h"

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace extensions {

ScopedWebFrame::ScopedWebFrame()
    : agent_group_scheduler_(
          blink::scheduler::WebAgentGroupScheduler::CreateForTesting()),
      view_(blink::WebView::Create(
          /*client=*/nullptr,
          /*is_hidden=*/false,
          /*is_inside_portal=*/false,
          /*compositing_enabled=*/false,
          /*opener=*/nullptr,
          mojo::NullAssociatedReceiver(),
          *agent_group_scheduler_)),
      frame_(blink::WebLocalFrame::CreateMainFrame(view_,
                                                   &frame_client_,
                                                   nullptr,
                                                   blink::LocalFrameToken(),
                                                   nullptr)) {}

ScopedWebFrame::~ScopedWebFrame() {
  view_->Close();
  blink::WebHeap::CollectAllGarbageForTesting();
  agent_group_scheduler_ = nullptr;
}

}  // namespace extensions
