// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/ax_screen_ai_annotator.h"

#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace {
const char kScreenAIServiceKeyName[] = "screen_ai_service";

class ScreenAIServiceWrapper : public base::SupportsUserData::Data {
 public:
  ScreenAIServiceWrapper() {
    content::ServiceProcessHost::Launch(
        screen_ai_service_.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("Screen AI Service")
            .Pass());
  }

  ScreenAIServiceWrapper(const ScreenAIServiceWrapper&) = delete;
  ScreenAIServiceWrapper& operator=(const ScreenAIServiceWrapper&) = delete;

  ~ScreenAIServiceWrapper() override = default;

  mojo::Remote<screen_ai::mojom::ScreenAIService>& GetService() {
    return screen_ai_service_;
  }

 private:
  mojo::Remote<screen_ai::mojom::ScreenAIService> screen_ai_service_;
};

}  // namespace

namespace content {
AXScreenAIAnnotator::AXScreenAIAnnotator(
    RenderFrameHost* const render_frame_host,
    BrowserContext* browser_context)
    : render_frame_host_(render_frame_host) {
  mojo::PendingReceiver<screen_ai::mojom::ScreenAIAnnotator>
      screen_ai_receiver = screen_ai_annotator_.BindNewPipeAndPassReceiver();
  GetScreenAIServiceForBrowserContext(browser_context)
      ->BindAnnotator(std::move(screen_ai_receiver));
}

AXScreenAIAnnotator::~AXScreenAIAnnotator() = default;

mojo::Remote<screen_ai::mojom::ScreenAIService>&
AXScreenAIAnnotator::GetScreenAIServiceForBrowserContext(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browser_context->GetUserData(kScreenAIServiceKeyName)) {
    browser_context->SetUserData(kScreenAIServiceKeyName,
                                 std::make_unique<ScreenAIServiceWrapper>());
  }
  return static_cast<ScreenAIServiceWrapper*>(
             browser_context->GetUserData(kScreenAIServiceKeyName))
      ->GetService();
}

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