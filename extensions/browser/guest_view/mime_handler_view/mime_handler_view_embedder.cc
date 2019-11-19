// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_embedder.h"

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"

namespace extensions {

namespace {
using EmbedderMap =
    base::flat_map<int32_t, std::unique_ptr<MimeHandlerViewEmbedder>>;

EmbedderMap* GetMimeHandlerViewEmbeddersMap() {
  static base::NoDestructor<EmbedderMap> instance;
  return instance.get();
}
}  // namespace

// static
MimeHandlerViewEmbedder* MimeHandlerViewEmbedder::Get(
    int32_t frame_tree_node_id) {
  const auto& map = *GetMimeHandlerViewEmbeddersMap();
  auto it = map.find(frame_tree_node_id);
  if (it == map.cend())
    return nullptr;
  return it->second.get();
}

// static
void MimeHandlerViewEmbedder::Create(int32_t frame_tree_node_id,
                                     const GURL& resource_url,
                                     const std::string& mime_type,
                                     const std::string& stream_id,
                                     const std::string& internal_id) {
  DCHECK(
      !base::Contains(*GetMimeHandlerViewEmbeddersMap(), frame_tree_node_id));
  GetMimeHandlerViewEmbeddersMap()->insert_or_assign(
      frame_tree_node_id, base::WrapUnique(new MimeHandlerViewEmbedder(
                              frame_tree_node_id, resource_url, mime_type,
                              stream_id, internal_id)));
}

MimeHandlerViewEmbedder::MimeHandlerViewEmbedder(int32_t frame_tree_node_id,
                                                 const GURL& resource_url,
                                                 const std::string& mime_type,
                                                 const std::string& stream_id,
                                                 const std::string& internal_id)
    : content::WebContentsObserver(
          content::WebContents::FromFrameTreeNodeId(frame_tree_node_id)),
      frame_tree_node_id_(frame_tree_node_id),
      resource_url_(resource_url),
      mime_type_(mime_type),
      stream_id_(stream_id),
      internal_id_(internal_id) {}

MimeHandlerViewEmbedder::~MimeHandlerViewEmbedder() {}

void MimeHandlerViewEmbedder::DidStartNavigation(
    content::NavigationHandle* handle) {
  // This observer is created after the observed |frame_tree_node_id_| started
  // its navigation to the |resource_url|. If any new navigations start then
  // we should stop now and do not create a MHVG. Same document navigations
  // could occur for {replace,push}State among other reasons and should not
  // lead to deleting the MVHE (e.g., the test
  // PDFExtensionLinkClickTest.OpenPDFWithReplaceState reaches here).
  if (handle->GetFrameTreeNodeId() == frame_tree_node_id_ &&
      !handle->IsSameDocument()) {
    GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
  }
}

void MimeHandlerViewEmbedder::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  if (handle->GetFrameTreeNodeId() != frame_tree_node_id_)
    return;
  if (!render_frame_host_) {
    render_frame_host_ = handle->GetRenderFrameHost();
    GetContainerManager()->SetInternalId(internal_id_);
  }
}

void MimeHandlerViewEmbedder::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (frame_tree_node_id_ != handle->GetFrameTreeNodeId())
    return;
  CheckSandboxFlags();
}

void MimeHandlerViewEmbedder::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host_ ||
      render_frame_host_ != render_frame_host->GetParent() ||
      render_frame_host_->GetLastCommittedURL() != resource_url_) {
    return;
  }
  if (!ready_to_create_mime_handler_view_) {
    // Renderer notifies the browser about creating MimeHandlerView right after
    // HTMLPlugInElement::RequestObject, which is before the plugin element is
    // navigated.
    GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
    return;
  }
  outer_contents_frame_tree_node_id_ = render_frame_host->GetFrameTreeNodeId();
  element_instance_id_ = render_frame_host->GetRoutingID();
  // This suggests that a same-origin child frame is created under the
  // RFH associated with |frame_tree_node_id_|. This suggests that the HTML
  // string is loaded in the observed frame's document and now the renderer
  // can initiate the MimeHandlerViewFrameContainer creation process.
  // TODO(ekaramad): We shouldn't have to wait for the response from the
  // renderer; instead we should proceed with creating MHVG. Instead, the
  // interface request in MHVG for beforeunload should wait until this response
  // comes back.
  GetContainerManager()->CreateBeforeUnloadControl(
      base::BindOnce(&MimeHandlerViewEmbedder::CreateMimeHandlerViewGuest,
                     weak_factory_.GetWeakPtr()));
}

void MimeHandlerViewEmbedder::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetFrameTreeNodeId() == frame_tree_node_id_ ||
      render_frame_host->GetFrameTreeNodeId() ==
          outer_contents_frame_tree_node_id_) {
    GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
  }
}

void MimeHandlerViewEmbedder::CreateMimeHandlerViewGuest(
    mojo::PendingRemote<mime_handler::BeforeUnloadControl>
        before_unload_control) {
  auto* browser_context = web_contents()->GetBrowserContext();
  auto* manager =
      guest_view::GuestViewManager::FromBrowserContext(browser_context);
  if (!manager) {
    manager = guest_view::GuestViewManager::CreateWithDelegate(
        browser_context,
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
            browser_context));
  }
  pending_before_unload_control_ = std::move(before_unload_control);
  base::DictionaryValue create_params;
  create_params.SetString(mime_handler_view::kViewId, stream_id_);
  manager->CreateGuest(
      MimeHandlerViewGuest::Type, web_contents(), create_params,
      base::BindOnce(&MimeHandlerViewEmbedder::DidCreateMimeHandlerViewGuest,
                     weak_factory_.GetWeakPtr()));
}

void MimeHandlerViewEmbedder::DidCreateMimeHandlerViewGuest(
    content::WebContents* guest_web_contents) {
  auto* guest_view = MimeHandlerViewGuest::FromWebContents(guest_web_contents);
  if (!guest_view)
    return;
  // Manager was created earlier in the stack in CreateMimeHandlerViewGuest (if
  // it had not existed before that).
  guest_view->SetBeforeUnloadController(
      std::move(pending_before_unload_control_));
  int guest_instance_id = guest_view->guest_instance_id();
  auto* outer_contents_rfh = web_contents()->UnsafeFindFrameByFrameTreeNodeId(
      outer_contents_frame_tree_node_id_);
  int32_t embedder_frame_process_id =
      outer_contents_rfh->GetParent()->GetProcess()->GetID();
  guest_view->SetEmbedderFrame(embedder_frame_process_id,
                               outer_contents_rfh->GetParent()->GetRoutingID());
  // TODO(ekaramad): This URL is used to communicate with
  // MimeHandlerViewFrameContainer which is only the case if the embedder frame
  // is the content frame of a plugin element (https://crbug.com/957373).
  guest_view->set_original_resource_url(resource_url_);
  guest_view::GuestViewManager::FromBrowserContext(
      web_contents()->GetBrowserContext())
      ->AttachGuest(embedder_frame_process_id, element_instance_id_,
                    guest_instance_id,
                    base::DictionaryValue() /* unused attach_params */);
  // Full page plugin refers to <iframe> or main frame navigations to a
  // MimeHandlerView resource. In such cases MHVG does not have a frame
  // container.
  bool is_full_page = !guest_view->maybe_has_frame_container() &&
                      !guest_view->GetEmbedderFrame()->GetParent();
  MimeHandlerViewAttachHelper::Get(embedder_frame_process_id)
      ->AttachToOuterWebContents(guest_view, embedder_frame_process_id,
                                 outer_contents_rfh, element_instance_id_,
                                 is_full_page /* is_full_page_plugin */);
  // MHVE is no longer required.
  GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
}

mojom::MimeHandlerViewContainerManager*
MimeHandlerViewEmbedder::GetContainerManager() {
  if (!container_manager_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &container_manager_);
  }
  return container_manager_.get();
}

void MimeHandlerViewEmbedder::ReadyToCreateMimeHandlerView(
    bool ready_to_create_mime_handler_view) {
  ready_to_create_mime_handler_view_ = ready_to_create_mime_handler_view;
  if (!ready_to_create_mime_handler_view_)
    GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
}

void MimeHandlerViewEmbedder::CheckSandboxFlags() {
  // If the FrameTreeNode is deleted while it has ownership of the ongoing
  // NavigationRequest, DidFinishNavigation is called before FrameDeleted (see
  // https://crbug.com/969840).
  if (render_frame_host_ &&
      !render_frame_host_->IsSandboxed(blink::WebSandboxFlags::kPlugins)) {
    return;
  }
  if (render_frame_host_) {
    // Notify the renderer to load an empty page instead.
    GetContainerManager()->LoadEmptyPage(resource_url_);
  }
  GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
}

}  // namespace extensions
