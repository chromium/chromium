// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"

#include <map>
#include <set>

#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_document.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace extensions {

MimeHandlerViewContainer::MimeHandlerViewContainer(
    content::RenderFrame* render_frame,
    const content::WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url)
    : GuestViewContainer(render_frame),
      MimeHandlerViewContainerBase(render_frame, info, mime_type, original_url),
      guest_proxy_routing_id_(-1),
      is_resource_accessible_to_embedder_(
          GetSourceFrame()->GetSecurityOrigin().CanAccess(
              blink::WebSecurityOrigin::Create(original_url))) {
  RecordInteraction(
      MimeHandlerViewUMATypes::Type::kDidCreateMimeHandlerViewContainerBase);
  is_embedded_ = !render_frame->GetWebFrame()->GetDocument().IsPluginDocument();
}

MimeHandlerViewContainer::~MimeHandlerViewContainer() {
}

void MimeHandlerViewContainer::OnReady() {
  if (!render_frame() || !is_embedded_)
    return;

  SendResourceRequest();
}

bool MimeHandlerViewContainer::OnMessage(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MimeHandlerViewContainer, message)
    IPC_MESSAGE_HANDLER(ExtensionsGuestViewMsg_CreateMimeHandlerViewGuestACK,
                        OnCreateMimeHandlerViewGuestACK)
    IPC_MESSAGE_HANDLER(GuestViewMsg_GuestAttached, OnGuestAttached)
    IPC_MESSAGE_HANDLER(
        ExtensionsGuestViewMsg_MimeHandlerViewGuestOnLoadCompleted,
        OnMimeHandlerViewGuestOnLoadCompleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MimeHandlerViewContainer::PluginDidFinishLoading() {
  DCHECK(!is_embedded_);
  CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewContainer::OnRenderFrameDestroyed() {
  EmbedderRenderFrameWillBeGone();
}

void MimeHandlerViewContainer::PluginDidReceiveData(const char* data,
                                                    int data_length) {
  view_id_ += std::string(data, data_length);
}


void MimeHandlerViewContainer::DidResizeElement(const gfx::Size& new_size) {
  element_size_ = new_size;

  CreateMimeHandlerViewGuestIfNecessary();

  // Don't try to resize a guest that hasn't been created yet. It is enough to
  // initialise |element_size_| here and then we'll send that to the browser
  // during guest creation.
  if (!guest_created())
    return;

  render_frame()->Send(new ExtensionsGuestViewHostMsg_ResizeGuest(
      render_frame()->GetRoutingID(), element_instance_id(), new_size));
}

v8::Local<v8::Object> MimeHandlerViewContainer::V8ScriptableObject(
    v8::Isolate* isolate) {
  return GetScriptableObjectInternal(isolate);
}

void MimeHandlerViewContainer::OnCreateMimeHandlerViewGuestACK(
    int element_instance_id) {
  DCHECK_NE(this->element_instance_id(), guest_view::kInstanceIDNone);
  DCHECK_EQ(this->element_instance_id(), element_instance_id);

  if (!render_frame())
    return;

  render_frame()->AttachGuest(element_instance_id);
}

void MimeHandlerViewContainer::OnGuestAttached(int /* unused */,
                                               int guest_proxy_routing_id) {
  // Save the RenderView routing ID of the guest here so it can be used to route
  // PostMessage calls.
  guest_proxy_routing_id_ = guest_proxy_routing_id;
}

void MimeHandlerViewContainer::OnMimeHandlerViewGuestOnLoadCompleted(
    int32_t element_instance_id) {
  DidLoadInternal();
}

void MimeHandlerViewContainer::CreateMimeHandlerViewGuestIfNecessary() {
  if (!element_size_.has_value())
    return;
  MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary();
}

int32_t MimeHandlerViewContainer::GetInstanceId() const {
  return element_instance_id();
}

gfx::Size MimeHandlerViewContainer::GetElementSize() const {
  return *element_size_;
}

blink::WebLocalFrame* MimeHandlerViewContainer::GetSourceFrame() {
  return render_frame()->GetWebFrame();
}

blink::WebFrame* MimeHandlerViewContainer::GetTargetFrame() {
  content::RenderView* guest_proxy_render_view =
      content::RenderView::FromRoutingID(guest_proxy_routing_id_);
  if (!guest_proxy_render_view)
    return nullptr;
  return guest_proxy_render_view->GetWebView()->MainFrame()->ToWebRemoteFrame();
}

bool MimeHandlerViewContainer::IsEmbedded() const {
  return is_embedded_;
}

bool MimeHandlerViewContainer::IsResourceAccessibleBySource() const {
  return is_resource_accessible_to_embedder_;
}

}  // namespace extensions
