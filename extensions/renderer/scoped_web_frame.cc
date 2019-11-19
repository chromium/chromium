// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/scoped_web_frame.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace extensions {

ScopedWebFrame::ScopedWebFrame()
    : view_(blink::WebView::Create(/*client=*/nullptr,
                                   /*is_hidden=*/false,
                                   /*compositing_enabled=*/false,
                                   /*opener=*/nullptr)),
      frame_(blink::WebLocalFrame::CreateMainFrame(view_,
                                                   &frame_client_,
                                                   nullptr,
                                                   nullptr)) {}

ScopedWebFrame::~ScopedWebFrame() {
  view_->Close();
  blink::WebHeap::CollectAllGarbageForTesting();
}

}  // namespace extensions
