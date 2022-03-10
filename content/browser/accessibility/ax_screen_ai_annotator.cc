// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/ax_screen_ai_annotator.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace content {

AXScreenAIAnnotator::AXScreenAIAnnotator(
    RenderFrameHost* const render_frame_host,
    mojo::AssociatedRemote<screen_ai::mojom::ScreenAIAnnotator>
        screen_ai_annotator)
    : render_frame_host_(render_frame_host),
      screen_ai_annotator_(std::move(screen_ai_annotator)) {}

AXScreenAIAnnotator::~AXScreenAIAnnotator() = default;

void AXScreenAIAnnotator::Run() {
  DCHECK(render_frame_host_->IsInPrimaryMainFrame());

  // Request screenshot from content area.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_);
  if (!web_contents)
    return;
  gfx::NativeWindow native_window = web_contents->GetContentNativeView();
  if (!native_window)
    return;
  ui::GrabViewSnapshotAsync(
      native_window, gfx::Rect(web_contents->GetSize()),
      base::BindOnce(&AXScreenAIAnnotator::OnScreenshotReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AXScreenAIAnnotator::OnScreenshotReceived(gfx::Image snapshot) {
  screen_ai_annotator_->Annotate(
      snapshot.AsBitmap(),
      base::BindOnce(&AXScreenAIAnnotator::OnAnnotationReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AXScreenAIAnnotator::OnAnnotationReceived(
    screen_ai::mojom::ErrorType error_type,
    std::vector<screen_ai::mojom::NodePtr> annotation) {
  if (error_type != screen_ai::mojom::ErrorType::kOK)
    return;

  // TODO(https://crbug.com/1278249): Convert and send annotation through
  // |render_frame_host_->AccessibilityPerformAction|.
}

}  // namespace content