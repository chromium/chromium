// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"

#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

namespace {

const char kSafeAreaInsetTopName[] = "safe-area-inset-top";
const char kSafeAreaInsetLeftName[] = "safe-area-inset-left";
const char kSafeAreaInsetBottomName[] = "safe-area-inset-bottom";
const char kSafeAreaInsetRightName[] = "safe-area-inset-right";

String GetPx(int value) {
  return String::Format("%dpx", value);
}

}  // namespace

DisplayCutoutClientImpl::DisplayCutoutClientImpl(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient> receiver)
    : frame_(frame), receiver_(this, std::move(receiver)) {}

void DisplayCutoutClientImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>
        receiver) {
  if (!frame)
    return;
  MakeGarbageCollected<DisplayCutoutClientImpl>(frame, std::move(receiver));
}

void DisplayCutoutClientImpl::SetSafeArea(
    mojom::blink::DisplayCutoutSafeAreaPtr safe_area) {
  DocumentStyleEnvironmentVariables& vars =
      frame_->GetDocument()->GetStyleEngine().EnsureEnvironmentVariables();
  vars.SetVariable(kSafeAreaInsetTopName, GetPx(safe_area->top));
  vars.SetVariable(kSafeAreaInsetLeftName, GetPx(safe_area->left));
  vars.SetVariable(kSafeAreaInsetBottomName, GetPx(safe_area->bottom));
  vars.SetVariable(kSafeAreaInsetRightName, GetPx(safe_area->right));
}

void DisplayCutoutClientImpl::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
}

}  // namespace blink
