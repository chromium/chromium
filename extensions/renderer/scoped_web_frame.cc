// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/scoped_web_frame.h"

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace extensions {

ScopedWebFrame::ScopedWebFrame()
    : agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              .CreateWebAgentGroupScheduler()),
      view_(blink::WebView::Create(
          /*client=*/nullptr,
          /*is_hidden=*/false,
          /*is_prerendering=*/false,
          /*is_inside_portal=*/false,
          /*fenced_frame_mode=*/std::nullopt,
          /*compositing_enabled=*/false,
          /*widgets_never_composited=*/false,
          /*opener=*/nullptr,
          mojo::NullAssociatedReceiver(),
          *agent_group_scheduler_,
          /*session_storage_namespace_id=*/base::EmptyString(),
          /*page_base_background_color=*/std::nullopt,
          blink::BrowsingContextGroupInfo::CreateUnique())),
      frame_(blink::WebLocalFrame::CreateMainFrame(view_,
                                                   &frame_client_,
                                                   nullptr,
                                                   blink::LocalFrameToken(),
                                                   blink::DocumentToken(),
                                                   nullptr)) {
  view_->DidAttachLocalMainFrame();
}

ScopedWebFrame::~ScopedWebFrame() {
  view_->Close();
  blink::WebHeap::CollectAllGarbageForTesting();
  agent_group_scheduler_ = nullptr;
}

}  // namespace extensions
