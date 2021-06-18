// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame_client_impl.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/web/web_remote_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {

RemoteFrameClientImpl::RemoteFrameClientImpl(WebRemoteFrameImpl* web_frame)
    : web_frame_(web_frame) {}

void RemoteFrameClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(web_frame_);
  RemoteFrameClient::Trace(visitor);
}

bool RemoteFrameClientImpl::InShadowTree() const {
  return web_frame_->GetTreeScopeType() == mojom::blink::TreeScopeType::kShadow;
}

void RemoteFrameClientImpl::Detached(FrameDetachType type) {
  // Alert the client that the frame is being detached.
  WebRemoteFrameClient* client = web_frame_->Client();
  if (!client)
    return;

  // We only notify the browser process when the frame is being detached for
  // removal, not after a swap.
  if (type == FrameDetachType::kRemove)
    web_frame_->GetFrame()->GetRemoteFrameHostRemote().Detach();

  client->FrameDetached(static_cast<WebRemoteFrameClient::DetachType>(type));

  if (web_frame_->Parent()) {
    if (type == FrameDetachType::kRemove)
      WebFrame::ToCoreFrame(*web_frame_)->DetachFromParent();
  } else if (web_frame_->View()) {
    // If the RemoteFrame being detached is also the main frame in the renderer
    // process, we need to notify the webview to allow it to clean things up.
    web_frame_->View()->DidDetachRemoteMainFrame();
  }

  // Clear our reference to RemoteFrame at the very end, in case the client
  // refers to it.
  web_frame_->SetCoreFrame(nullptr);
}

unsigned RemoteFrameClientImpl::BackForwardLength() {
  return To<WebViewImpl>(web_frame_->View())->HistoryListLength();
}

AssociatedInterfaceProvider*
RemoteFrameClientImpl::GetRemoteAssociatedInterfaces() {
  return web_frame_->Client()->GetRemoteAssociatedInterfaces();
}

}  // namespace blink
