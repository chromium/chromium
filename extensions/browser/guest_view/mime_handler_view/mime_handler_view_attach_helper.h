// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_ATTACH_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_ATTACH_HELPER_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
class WebContents;
}  // namespace content

class GURL;

namespace extensions {

class MimeHandlerViewGuest;

// Helper class for attaching a MimeHandlerViewGuest to its outer WebContents.
// This class is exclusively used for frame-based MimeHandlerView. There is one
// instance of this class per process. All the associated state in this class
// is accessed on UI thread.
class MimeHandlerViewAttachHelper : content::RenderProcessHostObserver {
 public:
  // Returns the unique helper for process identified with |render_process_id|.
  static MimeHandlerViewAttachHelper* Get(int render_process_id);

  // Creates and returns a template HTML page containing an embed for a MIME
  // handler. The MIME handler is expected to use this embed to load the MIME
  // extension. `internal_id` is assigned to the embed.
  static std::string CreateTemplateMimeHandlerPage(
      const GURL& resource_url,
      const std::string& mime_type,
      const std::string& internal_id);

  // Called on IO thread to override the response body for frame-based
  // MimeHandlerView. The resulting payload will be populated with a template
  // HTML page which appends a child frame to the frame associated with
  // |navigating_frame_tree_node_id|. Then, an observer of the associated
  // WebContents will observe the newly created RenderFrameHosts. As soon as the
  // expected RFH (i.e., the one added by the HTML string) is found, the
  // renderer is notified to start the MimHandlerView creation process. The
  // mentioned child frame will be used to attach the GuestView's WebContents to
  // the outer WebContents (WebContents associated with
  // |navigating_frame_tree_node_id|). The corresponding resource load will be
  // halted until |resume| is invoked. This provides an opportunity for UI
  // thread initializations.
  static std::string OverrideBodyForInterceptedResponse(
      content::FrameTreeNodeId navigating_frame_tree_node_id,
      const GURL& resource_url,
      const std::string& mime_type,
      const std::string& stream_id,
      const std::string& internal_id,
      base::OnceClosure resume_load);

  MimeHandlerViewAttachHelper(const MimeHandlerViewAttachHelper&) = delete;
  MimeHandlerViewAttachHelper& operator=(const MimeHandlerViewAttachHelper&) =
      delete;

  ~MimeHandlerViewAttachHelper() override;

  // content::RenderProcessHostObserver overrides.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Starts the attaching process for the |guest_view|'s WebContents to its
  // outer WebContents (embedder WebContents) on the UI thread.
  void AttachToOuterWebContents(
      std::unique_ptr<MimeHandlerViewGuest> guest_view,
      int32_t embedder_render_process_id,
      content::RenderFrameHost* outer_contents_frame,
      int32_t element_instance_id,
      bool is_full_page_plugin);

  // When set, the next asynchronous guestview attachment operation will call
  // `callback` when it reaches ResumeAttachOrDestroy() rather than continuing.
  // The attachment must then be continued manually by the caller, by invoking
  // the closure provided as an argument to `callback`, when desired.  Used
  // only in tests to exercise races which depend on tasks running between
  // AttachToOuterWebContents() and ResumeAttachOrDestroy().
  void set_resume_attach_callback_for_testing(
      base::OnceCallback<void(base::OnceClosure)> callback) {
    resume_attach_callback_for_testing_ = std::move(callback);
  }

 private:
  // Called after the content layer finishes preparing a frame for attaching to
  // the embedder WebContents. If |plugin_render_frame_host| is nullptr then
  // attaching is not possible and the guest should be destroyed; otherwise it
  // is safe to proceed to attaching the WebContentses.
  void ResumeAttachOrDestroy(
      std::unique_ptr<MimeHandlerViewGuest> guest_view,
      int32_t element_instance_id,
      bool is_full_page_plugin,
      content::RenderFrameHost* plugin_render_frame_host);

  // Called on UI thread to start observing the frame associated with
  // |frame_tree_node_id| and have the renderer create a
  // MimeHandlerViewFrameContainer as soon as the observed frame is ready, i.e.,
  // the frame has committed the |resource_url| and its child frame (in the same
  // SiteInstance) has been created.
  static void CreateFullPageMimeHandlerView(
      content::FrameTreeNodeId frame_tree_node_id,
      const GURL& resource_url,
      const std::string& stream_id,
      const std::string& token);

  explicit MimeHandlerViewAttachHelper(
      content::RenderProcessHost* render_process_host);

  const raw_ptr<content::RenderProcessHost> render_process_host_;

  // Allows delaying ResumeAttachOrDestroy for testing.
  base::OnceCallback<void(base::OnceClosure)>
      resume_attach_callback_for_testing_;

  base::WeakPtrFactory<MimeHandlerViewAttachHelper> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_ATTACH_HELPER_H_
