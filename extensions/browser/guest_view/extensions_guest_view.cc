// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extensions_guest_view.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_embedder.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"

namespace extensions {

ExtensionsGuestView::ExtensionsGuestView(int render_process_id)
    : guest_view::GuestViewMessageHandler(render_process_id) {}
ExtensionsGuestView::~ExtensionsGuestView() = default;

// static
void ExtensionsGuestView::CreateForComponents(
    int render_process_id,
    mojo::PendingAssociatedReceiver<guest_view::mojom::GuestViewHost>
        receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new ExtensionsGuestView(render_process_id)),
      std::move(receiver));
}

// static
void ExtensionsGuestView::CreateForExtensions(
    int render_process_id,
    mojo::PendingAssociatedReceiver<extensions::mojom::GuestView> receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new ExtensionsGuestView(render_process_id)),
      std::move(receiver));
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ExtensionsGuestView::CreateGuestViewManagerDelegate() const {
  return ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate();
}

void ExtensionsGuestView::ReadyToCreateMimeHandlerView(int32_t render_frame_id,
                                                       bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id(), render_frame_id);
  if (!render_frame_host) {
    return;
  }
  if (auto* mhve = MimeHandlerViewEmbedder::Get(
          render_frame_host->GetFrameTreeNodeId())) {
    mhve->ReadyToCreateMimeHandlerView(success);
  }
}

void ExtensionsGuestView::CanExecuteContentScript(
    int routing_id,
    const std::string& script_id,
    CanExecuteContentScriptCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebViewRendererState::WebViewInfo info;
  const bool success = WebViewRendererState::GetInstance()->GetInfo(
      render_process_id(), routing_id, &info);
  // GetInfo can fail if the process id does not correspond to a WebView. Those
  // cases are just defaulted to false.
  if (!success) {
    std::move(callback).Run(false);
    return;
  }
  const bool can_execute = base::Contains(info.content_script_ids, script_id);
  std::move(callback).Run(can_execute);
}

}  // namespace extensions
