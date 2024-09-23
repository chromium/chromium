// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_

#include "base/memory/raw_ptr.h"
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

namespace guest_view {
class GuestViewBase;
}  // namespace guest_view

namespace extensions {

// MimeHandlerViewEmbedder is instantiated in response to a frame navigation to
// a resource with a MIME type handled by MimeHandlerViewGuest. MHVE tracks the
// navigation to the template HTML document injected by the
// MimeHandlerViewAttachHelper and when the <embed>'s RenderFrameHost is ready,
// proceeds with creating a BeforeUnloadControl on the renderer side. After the
// renderer confirms the creation of BUC the MHVE will create and attach a
// MHVG. At this point MHVE is no longer needed and it clears itself.
// Note: the MHVE might go away sooner if:
//   - A new navigation starts in the embedder frame,
//   - the navigation to the resource fails, or,
//.  - the embedder or the injected <embed> are removed from DOM.
class MimeHandlerViewEmbedder : public content::WebContentsObserver {
 public:
  // Returns the instance associated with an ongoing navigation in a frame
  // identified by |frame_tree_node_id| if it exists.
  static MimeHandlerViewEmbedder* Get(
      content::FrameTreeNodeId frame_tree_node_id);

  static void Create(content::FrameTreeNodeId frame_tree_node_id,
                     const GURL& resource_url,
                     const std::string& stream_id,
                     const std::string& internal_id);

  ~MimeHandlerViewEmbedder() override;
  MimeHandlerViewEmbedder(const MimeHandlerViewEmbedder&) = delete;
  MimeHandlerViewEmbedder& operator=(const MimeHandlerViewEmbedder&) = delete;

  // content::WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  void ReadyToCreateMimeHandlerView(bool result);

  // Called when we've finished calculating the sandbox flags for the frame
  // associated with this MimeHandlerViewEmbedder and found that it's sandboxed.
  // This signals that the navigation to the resource will fail.
  void OnFrameSandboxed();

 private:
  MimeHandlerViewEmbedder(content::FrameTreeNodeId frame_tree_node_id,
                          const GURL& resource_url,
                          const std::string& stream_id,
                          const std::string& internal_id);
  void DestroySelf();
  void CreateMimeHandlerViewGuest(
      mojo::PendingRemote<mime_handler::BeforeUnloadControl>
          before_unload_control_remote);
  void DidCreateMimeHandlerViewGuest(
      mojo::PendingRemote<mime_handler::BeforeUnloadControl>
          before_unload_control_remote,
      std::unique_ptr<guest_view::GuestViewBase> guest);
  // Returns null before |render_frame_host_| is known.
  mojom::MimeHandlerViewContainerManager* GetContainerManager();

  // The ID for the embedder frame of MimeHandlerViewGuest.
  const content::FrameTreeNodeId frame_tree_node_id_;
  const GURL resource_url_;
  const std::string stream_id_;
  const std::string internal_id_;

  // The frame associated with |frame_tree_node_id_|. Known to MHVE after the
  // navigation commits.
  raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;
  mojo::AssociatedRemote<mojom::MimeHandlerViewContainerManager>
      container_manager_;

  // The child frame of the template page at which we attach the guest contents.
  raw_ptr<content::RenderFrameHost>
      placeholder_render_frame_host_for_inner_contents_ = nullptr;

  bool ready_to_create_mime_handler_view_ = false;

  base::WeakPtrFactory<MimeHandlerViewEmbedder> weak_factory_{this};
};

}  // namespace extensions
#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_EMBEDDER_H_
