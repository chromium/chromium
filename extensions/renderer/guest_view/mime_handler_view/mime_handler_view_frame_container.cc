// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"

#include <string>

#include "content/public/renderer/render_frame.h"
#include "extensions/common/guest_view/mime_handler_view_uma_types.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {
using UMATypes = MimeHandlerViewUMATypes::Type;

MimeHandlerViewFrameContainer::MimeHandlerViewFrameContainer(
    MimeHandlerViewContainerManager* container_manager,
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type)
    : container_manager_(container_manager),
      plugin_element_(plugin_element),
      resource_url_(resource_url),
      mime_type_(mime_type),
      is_resource_accessible_to_embedder_(
          GetSourceFrame()->GetSecurityOrigin().CanAccess(
              blink::WebSecurityOrigin::Create(resource_url))) {}

MimeHandlerViewFrameContainer::~MimeHandlerViewFrameContainer() {}

blink::WebLocalFrame* MimeHandlerViewFrameContainer::GetSourceFrame() {
  return container_manager_->render_frame()->GetWebFrame();
}

blink::WebFrame* MimeHandlerViewFrameContainer::GetTargetFrame() {
  if (!AreFramesValid())
    return nullptr;
  return GetContentFrame()->FirstChild();
}

bool MimeHandlerViewFrameContainer::IsEmbedded() const {
  return true;
}

bool MimeHandlerViewFrameContainer::IsResourceAccessibleBySource() const {
  return is_resource_accessible_to_embedder_;
}

blink::WebFrame* MimeHandlerViewFrameContainer::GetContentFrame() const {
  return blink::WebFrame::FromFrameOwnerElement(plugin_element_);
}

bool MimeHandlerViewFrameContainer::AreFramesAlive() {
  if (!GetContentFrame() || !GetContentFrame()->FirstChild()) {
    container_manager_->RemoveFrameContainerForReason(
        this, UMATypes::kRemoveFrameContainerUnexpectedFrames,
        false /* retain_manager */);
    return false;
  }
  return true;
}

void MimeHandlerViewFrameContainer::SetRoutingIds(int32_t content_frame_id,
                                                  int32_t guest_frame_id) {
  DCHECK_EQ(content_frame_id_, MSG_ROUTING_NONE);
  DCHECK(!post_message_support()->is_active());
  content_frame_id_ = content_frame_id;
  guest_frame_id_ = guest_frame_id;
  post_message_support()->SetActive();
}

bool MimeHandlerViewFrameContainer::AreFramesValid() {
  if (!AreFramesAlive())
    return false;
  if (content_frame_id_ ==
      content::RenderFrame::GetRoutingIdForWebFrame(GetContentFrame())) {
    if (guest_frame_id_ == content::RenderFrame::GetRoutingIdForWebFrame(
                               GetContentFrame()->FirstChild())) {
      return true;
    }
  }
  container_manager_->RemoveFrameContainerForReason(
      this, UMATypes::kRemoveFrameContainerUnexpectedFrames,
      false /* retain_manager */);
  return false;
}

}  // namespace extensions
