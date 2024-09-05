// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_embedder.h"

#include "base/containers/contains.h"
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
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

namespace extensions {

namespace {
using EmbedderMap = base::flat_map<content::FrameTreeNodeId,
                                   std::unique_ptr<MimeHandlerViewEmbedder>>;

EmbedderMap* GetMimeHandlerViewEmbeddersMap() {
  static base::NoDestructor<EmbedderMap> instance;
  return instance.get();
}
}  // namespace

// static
MimeHandlerViewEmbedder* MimeHandlerViewEmbedder::Get(
    content::FrameTreeNodeId frame_tree_node_id) {
  const auto& map = *GetMimeHandlerViewEmbeddersMap();
  auto it = map.find(frame_tree_node_id);
  if (it == map.cend())
    return nullptr;
  return it->second.get();
}

// static
void MimeHandlerViewEmbedder::Create(
    content::FrameTreeNodeId frame_tree_node_id,
    const GURL& resource_url,
    const std::string& stream_id,
    const std::string& internal_id) {
  DCHECK(
      !base::Contains(*GetMimeHandlerViewEmbeddersMap(), frame_tree_node_id));
  GetMimeHandlerViewEmbeddersMap()->insert_or_assign(
      frame_tree_node_id,
      base::WrapUnique(new MimeHandlerViewEmbedder(
          frame_tree_node_id, resource_url, stream_id, internal_id)));
}

MimeHandlerViewEmbedder::MimeHandlerViewEmbedder(
    content::FrameTreeNodeId frame_tree_node_id,
    const GURL& resource_url,
    const std::string& stream_id,
    const std::string& internal_id)
    : content::WebContentsObserver(
          content::WebContents::FromFrameTreeNodeId(frame_tree_node_id)),
      frame_tree_node_id_(frame_tree_node_id),
      resource_url_(resource_url),
      stream_id_(stream_id),
      internal_id_(internal_id) {}

MimeHandlerViewEmbedder::~MimeHandlerViewEmbedder() = default;

void MimeHandlerViewEmbedder::DestroySelf() {
  GetMimeHandlerViewEmbeddersMap()->erase(frame_tree_node_id_);
}

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
    DestroySelf();
  }
}

void MimeHandlerViewEmbedder::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  if (handle->GetFrameTreeNodeId() != frame_tree_node_id_)
    return;
  if (render_frame_host_)
    return;

  // It's possible for the navigation to the template HTML document to fail
  // (e.g. attempting to load a PDF in a fenced frame).
  if (handle->GetNetErrorCode() != net::OK) {
    DestroySelf();
    return;
  }

  // We should've deleted the MimeHandlerViewEmbedder at this point if the frame
  // is sandboxed.
  DCHECK_EQ(network::mojom::WebSandboxFlags::kNone,
            handle->SandboxFlagsToCommit() &
                network::mojom::WebSandboxFlags::kPlugins);

  render_frame_host_ = handle->GetRenderFrameHost();
  GetContainerManager()->SetInternalId(internal_id_);
}

void MimeHandlerViewEmbedder::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->HasCommitted() ||
      render_frame_host_ != handle->GetRenderFrameHost()) {
    return;
  }
  // We should've deleted the MimeHandlerViewEmbedder at this point if the frame
  // is sandboxed.
  DCHECK(render_frame_host_);
  DCHECK(!render_frame_host_->IsSandboxed(
      network::mojom::WebSandboxFlags::kPlugins));
}

void MimeHandlerViewEmbedder::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // If an extension injects a frame before we finish attaching the PDF's
  // iframe, then we don't want to early out by mistake. We also put an
  // unguessable name element on the <embed> to guard against the possibility
  // that someone injects an <embed>, but that can be done in a separate CL.
  if (!render_frame_host_ ||
      render_frame_host_ != render_frame_host->GetParent() ||
      render_frame_host_->GetLastCommittedURL() != resource_url_ ||
      render_frame_host->GetFrameOwnerElementType() !=
          blink::FrameOwnerElementType::kEmbed ||
      render_frame_host->GetFrameName() != internal_id_) {
    return;
  }
  if (!ready_to_create_mime_handler_view_) {
    // Renderer notifies the browser about creating MimeHandlerView right after
    // HTMLPlugInElement::RequestObject, which is before the plugin element is
    // navigated.
    DestroySelf();
    return;
  }

  placeholder_render_frame_host_for_inner_contents_ = render_frame_host;

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
    content::FrameTreeNodeId frame_tree_node_id) {
  // TODO(mcnee): RenderFrameDeleted seems like a better fit for the child frame
  // case (i.e. |placeholder_render_frame_host_for_inner_contents_|), though
  // we'd still need FrameDeleted for |frame_tree_node_id_|.
  if (frame_tree_node_id == frame_tree_node_id_ ||
      (placeholder_render_frame_host_for_inner_contents_ &&
       placeholder_render_frame_host_for_inner_contents_
               ->GetFrameTreeNodeId() == frame_tree_node_id)) {
    DestroySelf();
  }
}

void MimeHandlerViewEmbedder::CreateMimeHandlerViewGuest(
    mojo::PendingRemote<mime_handler::BeforeUnloadControl>
        before_unload_control_remote) {
  auto* browser_context = web_contents()->GetBrowserContext();
  auto* manager =
      guest_view::GuestViewManager::FromBrowserContext(browser_context);
  if (!manager) {
    manager = guest_view::GuestViewManager::CreateWithDelegate(
        browser_context,
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }
  CHECK(render_frame_host_);
  base::Value::Dict create_params;
  create_params.Set(mime_handler_view::kStreamId, stream_id_);
  manager->CreateGuestAndTransferOwnership(
      MimeHandlerViewGuest::Type, render_frame_host_, create_params,
      base::BindOnce(&MimeHandlerViewEmbedder::DidCreateMimeHandlerViewGuest,
                     weak_factory_.GetWeakPtr(),
                     std::move(before_unload_control_remote)));
}

void MimeHandlerViewEmbedder::DidCreateMimeHandlerViewGuest(
    mojo::PendingRemote<mime_handler::BeforeUnloadControl>
        before_unload_control_remote,
    std::unique_ptr<guest_view::GuestViewBase> guest) {
  auto* raw_guest_view = static_cast<MimeHandlerViewGuest*>(guest.release());
  std::unique_ptr<MimeHandlerViewGuest> guest_view =
      base::WrapUnique(raw_guest_view);

  if (!guest_view)
    return;
  guest_view->SetBeforeUnloadController(
      std::move(before_unload_control_remote));

  DCHECK(placeholder_render_frame_host_for_inner_contents_);
  DCHECK(render_frame_host_);
  DCHECK_EQ(placeholder_render_frame_host_for_inner_contents_->GetParent(),
            render_frame_host_);

  const int embedder_frame_process_id =
      render_frame_host_->GetProcess()->GetID();
  const int element_instance_id =
      placeholder_render_frame_host_for_inner_contents_->GetRoutingID();
  const int guest_instance_id = guest_view->guest_instance_id();

  // TODO(ekaramad): This URL is used to communicate with
  // MimeHandlerViewFrameContainer which is only the case if the embedder frame
  // is the content frame of a plugin element (https://crbug.com/957373).
  guest_view->set_original_resource_url(resource_url_);
  guest_view::GuestViewManager::FromBrowserContext(
      web_contents()->GetBrowserContext())
      ->AttachGuest(embedder_frame_process_id, element_instance_id,
                    guest_instance_id,
                    base::Value::Dict() /* unused attach_params */);
  // Full page plugin refers to <iframe> or main frame navigations to a
  // MimeHandlerView resource. In such cases MHVG does not have a frame
  // container.
  bool is_full_page =
      !guest_view->maybe_has_frame_container() &&
      !guest_view->GetEmbedderFrame()->GetParentOrOuterDocument();
  MimeHandlerViewAttachHelper::Get(embedder_frame_process_id)
      ->AttachToOuterWebContents(
          std::move(guest_view), embedder_frame_process_id,
          placeholder_render_frame_host_for_inner_contents_,
          element_instance_id, is_full_page /* is_full_page_plugin */);
  // MHVE is no longer required.
  DestroySelf();
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
    DestroySelf();
}

void MimeHandlerViewEmbedder::OnFrameSandboxed() {
  DCHECK(!render_frame_host_);
  DCHECK(!container_manager_);
  DestroySelf();
}

}  // namespace extensions
