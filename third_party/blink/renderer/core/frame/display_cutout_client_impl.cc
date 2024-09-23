// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

DisplayCutoutClientImpl::DisplayCutoutClientImpl(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient> receiver)
    : frame_(frame),
      receiver_(this, frame->DomWindow()->GetExecutionContext()) {
  receiver_.Bind(std::move(receiver), frame->GetFrameScheduler()->GetTaskRunner(
                                          TaskType::kInternalDefault));
}

void DisplayCutoutClientImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>
        receiver) {
  if (!frame) {
    return;
  }
  MakeGarbageCollected<DisplayCutoutClientImpl>(frame, std::move(receiver));
}

void DisplayCutoutClientImpl::SetSafeArea(const gfx::Insets& safe_area) {
  frame_->GetDocument()->GetPage()->SetMaxSafeAreaInsets(frame_, safe_area);
}

void DisplayCutoutClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(receiver_);
}

}  // namespace blink
