// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_frame_container.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

using RenderFrameMap =
    base::flat_map<blink::LocalFrameToken,
                   std::unique_ptr<MimeHandlerViewContainerManager>>;

RenderFrameMap* GetRenderFrameMap() {
  static base::NoDestructor<RenderFrameMap> instance;
  return instance.get();
}

}  // namespace

// static
void MimeHandlerViewContainerManager::BindReceiver(
    content::RenderFrame* render_frame,
    mojo::PendingAssociatedReceiver<mojom::MimeHandlerViewContainerManager>
        receiver) {
  auto* manager = Get(render_frame, true /* create_if_does_not_exist */);
  manager->receivers_.Add(manager, std::move(receiver));
}

// static
MimeHandlerViewContainerManager* MimeHandlerViewContainerManager::Get(
    content::RenderFrame* render_frame,
    bool create_if_does_not_exist) {
  if (!render_frame) {
    // Through some |adoptNode| magic, blink could still call this method for
    // a plugin element which does not have a frame (https://crbug.com/966371).
    return nullptr;
  }
  auto frame_token = render_frame->GetWebFrame()->GetLocalFrameToken();
  auto& map = *GetRenderFrameMap();
  auto it = map.find(frame_token);
  if (it != map.end()) {
    return it->second.get();
  }
  if (!create_if_does_not_exist) {
    return nullptr;
  }

  auto& new_entry = map[frame_token];
  new_entry = std::make_unique<MimeHandlerViewContainerManager>(render_frame);

  return new_entry.get();
}

bool MimeHandlerViewContainerManager::CreateFrameContainer(
    const blink::WebElement& plugin_element,
    const GURL& resource_url,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin_info) {
  if (plugin_info.type != content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN) {
    // TODO(ekaramad): Rename this plugin type once https://crbug.com/659750 is
    // fixed. We only create a MHVFC for the plugin types of BrowserPlugin
    // (which used to create a MimeHandlerViewContainer).
    return false;
  }
  if (IsManagedByContainerManager(plugin_element)) {
    // This is the one injected by HTML string. Return true so that the
    // HTMLPlugInElement creates a child frame to be used as the outer
    // WebContents frame.
    return true;
  }
  if (auto* old_frame_container = GetFrameContainer(plugin_element)) {
    if (old_frame_container->resource_url().EqualsIgnoringRef(resource_url) &&
        old_frame_container->mime_type() == mime_type) {
      // TODO(ekaramad): Fix page transitions using the 'ref' in GURL (see
      // https://crbug.com/318458 for context).
      // This should translate into a same document navigation.
      return true;
    }
    // If there is already a MHVFC for this |plugin_element|, destroy it.
    RemoveFrameContainer(old_frame_container, true /* retain_manager */);
  }
  auto frame_container = std::make_unique<MimeHandlerViewFrameContainer>(
      this, plugin_element, resource_url, mime_type);
  frame_containers_.push_back(std::move(frame_container));
  return true;
}

void MimeHandlerViewContainerManager::
    DidBlockMimeHandlerViewForDisallowedPlugin(
        const blink::WebElement& plugin_element) {
  if (IsManagedByContainerManager(plugin_element)) {
    // This is the one injected by HTML string. Return true so that the
    // HTMLPlugInElement creates a child frame to be used as the outer
    // WebContents frame.
    remote_->ReadyToCreateMimeHandlerView(false);
  }
}

v8::Local<v8::Object> MimeHandlerViewContainerManager::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
  if (IsManagedByContainerManager(plugin_element)) {
    return GetPostMessageSupport()->GetScriptableObject(isolate);
  }
  if (auto* frame_container = GetFrameContainer(plugin_element)) {
    return frame_container->post_message_support()->GetScriptableObject(
        isolate);
  }
  return v8::Local<v8::Object>();
}

MimeHandlerViewContainerManager::MimeHandlerViewContainerManager(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      frame_token_(render_frame->GetWebFrame()->GetLocalFrameToken()) {
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
}

MimeHandlerViewContainerManager::~MimeHandlerViewContainerManager() = default;

void MimeHandlerViewContainerManager::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // At this point the <embed> element is not yet attached to the DOM and the
  // |post_message_support_|, if any, had been created for a previous page. Note
  // that this scenario comes up when a same-process navigation between two PDF
  // files happens in the same RenderFrame (e.g., some of
  // PDFExtensionLoadTest tests).
  post_message_support_.reset();
  plugin_element_ = blink::WebElement();
}

void MimeHandlerViewContainerManager::OnDestruct() {
  receivers_.Clear();
  // This will delete the class.
  GetRenderFrameMap()->erase(frame_token_);
}

void MimeHandlerViewContainerManager::SetInternalId(
    const std::string& token_id) {
  internal_id_ = token_id;
}

void MimeHandlerViewContainerManager::CreateBeforeUnloadControl(
    CreateBeforeUnloadControlCallback callback) {
  if (!post_message_support_)
    post_message_support_ = std::make_unique<PostMessageSupport>(this);
  // It might be bound when reloading the same page.
  before_unload_control_receiver_.reset();
  std::move(callback).Run(
      before_unload_control_receiver_.BindNewPipeAndPassRemote());
}

void MimeHandlerViewContainerManager::SelfDeleteIfNecessary() {
  // |internal_id| is only populated when |render_frame()| is embedder of a
  // MimeHandlerViewGuest. Full-page PDF is one such case. In these cases
  // we don't want to self-delete.
  if (!frame_containers_.empty() || !internal_id_.empty())
    return;

  // There are no frame containers left, and we're not serving a full-page
  // MimeHandlerView, so we remove ourselves from the map.
  GetRenderFrameMap()->erase(frame_token_);
}

void MimeHandlerViewContainerManager::DestroyFrameContainer(
    int32_t element_instance_id) {
  if (auto* frame_container = GetFrameContainer(element_instance_id))
    RemoveFrameContainer(frame_container, false /* retain manager */);
  else
    SelfDeleteIfNecessary();
}

void MimeHandlerViewContainerManager::DidLoad(int32_t element_instance_id,
                                              const GURL& resource_url) {
  if (post_message_support_ && !post_message_support_->is_active()) {
    // We don't need verification here, if any MHV has loaded inside this
    // |render_frame()| then the one corresponding to full-page must have done
    // so first.
    post_message_support_->SetActive();
    return;
  }
  for (auto& frame_container : frame_containers_) {
    if (frame_container->post_message_support()->is_active())
      continue;
    if (frame_container->resource_url() != resource_url)
      continue;
    // To ensure the postMessages will be sent to the right target frame, we
    // double check with the browser if the target frame's Routing ID makes
    // sense.
    if (!frame_container->AreFramesAlive()) {
      // At this point we expect a content frame inside the |plugin_element|
      // as well as a first child corresponding to the <iframe> used to attach
      // MimeHandlerViewGuest.
      return;
    }

    frame_container->set_element_instance_id(element_instance_id);
    auto* content_frame = frame_container->GetContentFrame();
    blink::FrameToken content_frame_token = content_frame->GetFrameToken();
    blink::FrameToken guest_frame_token;
    if (content_frame->FirstChild())
      guest_frame_token = content_frame->FirstChild()->GetFrameToken();
    // TODO(ekaramad); The FrameTokens here might have changed since the plugin
    // has been navigated to load MimeHandlerView. We should double check these
    // with the browser first (https://crbug.com/957373).
    // This will end up activating the post_message_support(). The FrameTokens
    // are double checked in every upcoming call to GetTargetFrame() to ensure
    // postMessages are sent to the intended WebFrame only.
    frame_container->SetFrameTokens(content_frame_token, guest_frame_token);

    return;
  }
}

MimeHandlerViewFrameContainer*
MimeHandlerViewContainerManager::GetFrameContainer(
    int32_t mime_handler_view_instance_id) {
  for (auto& frame_container : frame_containers_) {
    if (frame_container->element_instance_id() == mime_handler_view_instance_id)
      return frame_container.get();
  }
  return nullptr;
}

MimeHandlerViewFrameContainer*
MimeHandlerViewContainerManager::GetFrameContainer(
    const blink::WebElement& plugin_element) {
  for (auto& frame_container : frame_containers_) {
    if (frame_container->plugin_element() == plugin_element)
      return frame_container.get();
  }
  return nullptr;
}

void MimeHandlerViewContainerManager::RemoveFrameContainer(
    MimeHandlerViewFrameContainer* frame_container,
    bool retain_manager) {
  auto it =
      base::ranges::find(frame_containers_, frame_container,
                         &std::unique_ptr<MimeHandlerViewFrameContainer>::get);
  if (it == frame_containers_.cend())
    return;
  frame_containers_.erase(it);

  if (!retain_manager)
    SelfDeleteIfNecessary();

  return;
}

void MimeHandlerViewContainerManager::SetShowBeforeUnloadDialog(
    bool show_dialog,
    SetShowBeforeUnloadDialogCallback callback) {
  render_frame()->GetWebFrame()->GetDocument().SetShowBeforeUnloadDialog(
      show_dialog);
  std::move(callback).Run();
}

PostMessageSupport* MimeHandlerViewContainerManager::GetPostMessageSupport() {
  if (!post_message_support_)
    post_message_support_ = std::make_unique<PostMessageSupport>(this);
  return post_message_support_.get();
}

blink::WebLocalFrame* MimeHandlerViewContainerManager::GetSourceFrame() {
  return render_frame()->GetWebFrame();
}

blink::WebFrame* MimeHandlerViewContainerManager::GetTargetFrame() {
  // Search for the frame using the 'name' attribute, since if an extension
  // injects other frames into the embedder, there will be more than one frame.
  return GetSourceFrame()->FindFrameByName(
      blink::WebString::FromUTF8(internal_id_));
}

bool MimeHandlerViewContainerManager::IsEmbedded() const {
  return false;
}

bool MimeHandlerViewContainerManager::IsResourceAccessibleBySource() const {
  return true;
}

bool MimeHandlerViewContainerManager::IsManagedByContainerManager(
    const blink::WebElement& plugin_element) {
  if (plugin_element_.IsNull() && plugin_element.HasAttribute("internalid") &&
      base::ToUpperASCII(plugin_element.GetAttribute("internalid").Utf8()) ==
          internal_id_) {
    plugin_element_ = plugin_element;
    remote_->ReadyToCreateMimeHandlerView(true);
  }
  return plugin_element_ == plugin_element;
}

}  // namespace extensions
