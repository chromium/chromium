// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace extensions {

// MimeHandlerViewEmbedder is instantiated in response to a frame navigation to
// a resource with a MIME type handled by MimeHandlerViewGuest. MHVE tracks the
// navigation to the template HTML document injected by the
// MimeHandlerViewAttachHelper and when the <iframe>'s RenderFrameHost is ready,
// proceeds with creating a BeforeUnloadControl on the renderer side. After the
// renderer confirms the creation of BUC the MHVE will create and attach a
// MHVG. At this point MHVE is no longer needed and it clears itself.
// Note: the MHVE might go away sooner if:
//   - A new navigation starts in the embedder frame or <iframe>,
//   - the navigation to the resource fails, or,
//.  - the embedder or the <iframe> are removed from DOM.
class MimeHandlerViewEmbedder : public content::WebContentsObserver {
 public:
  // Returns the instances associated with an ongoing navigation in a frame
  // identified by |frame_tree_node_id|.
  static MimeHandlerViewEmbedder* Get(int32_t frame_tree_node_id);

  static void Create(int32_t frame_tree_node_id,
                     const GURL& resource_url,
                     const std::string& mime_type,
                     const std::string& stream_id,
                     const std::string& internal_id);

  ~MimeHandlerViewEmbedder() override;

  // content::WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void FrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  void ReadyToCreateMimeHandlerView(bool result);

 private:
  MimeHandlerViewEmbedder(int32_t frame_tree_node_id,
                          const GURL& resource_url,
                          const std::string& mime_type,
                          const std::string& stream_id,
                          const std::string& internal_id);
  void CreateMimeHandlerViewGuest(
      mojo::PendingRemote<mime_handler::BeforeUnloadControl>
          before_unload_control_remote);
  void DidCreateMimeHandlerViewGuest(content::WebContents* guest_web_contents);
  // Returns null before |render_frame_host_| is known.
  mojom::MimeHandlerViewContainerManager* GetContainerManager();

  // Checks the sandbox state of |render_frame_host_|. If the frame is sandboxed
  // it will send an IPC to renderer to show an empty page and immediately
  // deletes |this|.
  void CheckSandboxFlags();

  // The ID for the embedder frame of MimeHandlerViewGuest.
  int32_t frame_tree_node_id_;
  const GURL resource_url_;
  const std::string mime_type_;
  const std::string stream_id_;
  // This will be initialized to the RenderFrameHost corresponding to the
  // <iframe> in the HTML page.
  int32_t outer_contents_frame_tree_node_id_ =
      content::RenderFrameHost::kNoFrameTreeNodeId;
  // The frame associated with |frame_tree_node_id_|. Known to MHVE after the
  // navigation commits.
  content::RenderFrameHost* render_frame_host_ = nullptr;
  // Used in attaching the GuestView. Will be initialized to the routing ID of
  // the child frame which will be used to attach the GuestView to its outer
  // WebContents.
  int32_t element_instance_id_ = -1;
  // Initialized before creating MimeHandlerViewGuest and will be passed on to
  // to after it is created.
  mojo::PendingRemote<mime_handler::BeforeUnloadControl>
      pending_before_unload_control_;

  mojo::AssociatedRemote<mojom::MimeHandlerViewContainerManager>
      container_manager_;

  const std::string internal_id_;

  bool ready_to_create_mime_handler_view_ = false;

  base::WeakPtrFactory<MimeHandlerViewEmbedder> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewEmbedder);
};

}  // namespace extensions
#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_
