// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/guid.h"
#include "components/guest_view/browser/bad_message.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_embedder.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;
using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;
using guest_view::GuestViewMessageFilter;

namespace extensions {

const uint32_t ExtensionsGuestViewMessageFilter::kFilteredMessageClasses[] = {
    GuestViewMsgStart, ExtensionsGuestViewMsgStart};

ExtensionsGuestViewMessageFilter::ExtensionsGuestViewMessageFilter(
    int render_process_id,
    BrowserContext* context)
    : GuestViewMessageFilter(kFilteredMessageClasses,
                             base::size(kFilteredMessageClasses),
                             render_process_id,
                             context),
      content::BrowserAssociatedInterface<mojom::GuestView>(this) {}

bool ExtensionsGuestViewMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionsGuestViewMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionsGuestViewHostMsg_CanExecuteContentScriptSync,
                        OnCanExecuteContentScript)
    IPC_MESSAGE_UNHANDLED(
        handled = GuestViewMessageFilter::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
  return handled;
}

GuestViewManager* ExtensionsGuestViewMessageFilter::
    GetOrCreateGuestViewManager() {
  DCHECK(browser_context_);
  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager) {
    manager = GuestViewManager::CreateWithDelegate(
        browser_context_,
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
            browser_context_));
  }
  return manager;
}

void ExtensionsGuestViewMessageFilter::OnCanExecuteContentScript(
    int render_view_id,
    const std::string& script_id,
    bool* allowed) {
  WebViewRendererState::WebViewInfo info;
  WebViewRendererState::GetInstance()->GetInfo(render_process_id_,
                                               render_view_id, &info);

  *allowed =
      info.content_script_ids.find(script_id) != info.content_script_ids.end();
}

void ExtensionsGuestViewMessageFilter::ReadyToCreateMimeHandlerView(
    int32_t render_frame_id,
    bool success) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExtensionsGuestViewMessageFilter::ReadyToCreateMimeHandlerView,
            this, render_frame_id, success));
    return;
  }
  auto* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id);
  if (!rfh)
    return;
  if (auto* mhve = MimeHandlerViewEmbedder::Get(rfh->GetFrameTreeNodeId()))
    mhve->ReadyToCreateMimeHandlerView(success);
}

}  // namespace extensions
