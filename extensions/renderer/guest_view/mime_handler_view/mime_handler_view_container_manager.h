// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/renderer/render_frame_observer.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/web/web_element.h"
#include "url/gurl.h"

namespace blink {
class WebDocumentLoader;
class WebFrame;
class WebLocalFrame;
}  // namespace blink

namespace content {
class RenderFrame;
struct WebPluginInfo;
}

namespace extensions {

class MimeHandlerViewFrameContainer;

// TODO(ekaramad): Verify if we have to make this class a
// PostMessageSupport::Delegate (for full page MHV). Currently we do postMessage
// internally from ChromePrintRenderFrameHelperDelegate.
// This class is the entry point for browser issued commands related to
// MimeHandlerViews. A MHVCM helps with
// 1- Setting up beforeunload support for full page MHV
// 2- Provide postMessage support for embedded MHV.
// 3- Managing lifetime of classes and MHV state on the renderer side.
class MimeHandlerViewContainerManager
    : public content::RenderFrameObserver,
      public mojom::MimeHandlerViewContainerManager,
      public mime_handler::BeforeUnloadControl,
      public PostMessageSupport::Delegate {
 public:
  static void BindReceiver(
      content::RenderFrame* render_frame,
      mojo::PendingAssociatedReceiver<mojom::MimeHandlerViewContainerManager>
          receiver);
  // Returns the container manager associated with |render_frame|. If none
  // exists and |create_if_does_not_exist| is set true, creates and returns a
  // new instance for |render_frame|.
  static MimeHandlerViewContainerManager* Get(
      content::RenderFrame* render_frame,
      bool create_if_does_not_exist = false);

  explicit MimeHandlerViewContainerManager(content::RenderFrame* render_frame);

  MimeHandlerViewContainerManager(const MimeHandlerViewContainerManager&) =
      delete;
  MimeHandlerViewContainerManager& operator=(
      const MimeHandlerViewContainerManager&) = delete;

  ~MimeHandlerViewContainerManager() override;

  // Called to create a MimeHandlerViewFrameContainer for an <embed> or <object>
  // element.
  bool CreateFrameContainer(const blink::WebElement& plugin_element,
                            const GURL& resource_url,
                            const std::string& mime_type,
                            const content::WebPluginInfo& plugin_info);
  // Called to notify about a failed plugin load; this could happen if a
  // <webview> with permissions API tries to load a plugin.
  void DidBlockMimeHandlerViewForDisallowedPlugin(
      const blink::WebElement& plugin_element);
  // A wrapper for custom postMessage scripts. There should already be a
  // MimeHandlerViewFrameContainer for |plugin_element|.
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate);
  // Removes the |frame_container| from |frame_containers_| and destroys it.
  // Note: Calling this function may delete |this| if we are removing the last
  // frame container, unless |retain_manager| is set to true.
  void RemoveFrameContainer(MimeHandlerViewFrameContainer* frame_container,
                            bool retain_manager);
  MimeHandlerViewFrameContainer* GetFrameContainer(
      const blink::WebElement& plugin_element);
  MimeHandlerViewFrameContainer* GetFrameContainer(int32_t element_instance_id);
  // Returns the instance of PostMessageSupport, if any, for the page navigation
  // to MHV. This is used when |did_create_beforeunload_control_| is set.
  PostMessageSupport* GetPostMessageSupport();

  // content::RenderFrameObserver.
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void OnDestruct() override;

  // mojom::MimeHandlerViewContainerManager overrides.
  void SetInternalId(const std::string& token_id) override;
  void CreateBeforeUnloadControl(
      CreateBeforeUnloadControlCallback callback) override;

  // Note: Calling this function may delete |this| if we are destroying the last
  // frame container.
  void DestroyFrameContainer(int32_t element_instance_id) override;
  void DidLoad(int32_t mime_handler_view_guest_element_instance_id,
               const GURL& resource_url) override;

 private:
  // PostMessageSupport::Delegate overrides.
  blink::WebLocalFrame* GetSourceFrame() override;
  blink::WebFrame* GetTargetFrame() override;
  bool IsEmbedded() const override;
  bool IsResourceAccessibleBySource() const override;

  // mime_handler::BeforeUnloadControl implementation.
  void SetShowBeforeUnloadDialog(
      bool show_dialog,
      SetShowBeforeUnloadDialogCallback callback) override;

  // Returns true if the |element| is managed by
  // MimeHandlerViewContainerManager; this would be the element that is added by
  // the HTML string injected at MimeHandlerViewAttachHelper.
  bool IsManagedByContainerManager(const blink::WebElement& plugin_element);

  // If this MimeHandlerViewContainerManager isn't serving a full-page PDF, and
  // there are no frame containers, then it should delete itself. This function
  // looks after this.
  void SelfDeleteIfNecessary();

  // Instantiated if this MHVFC is for a full-page MHV. This means MHV is
  // created when a frame was navigated to MHV resource by means other than
  // HTMLPlugInElement::RequestObjectInternal (e.g., omnibox). Note: the
  // |post_message_support_| is only used for internal print messages and there
  // is no web-accessible scriptable object exposed.
  std::unique_ptr<PostMessageSupport> post_message_support_;

  // Contains all the MimeHandlerViewFrameContainers under |render-frame()|.
  std::vector<std::unique_ptr<MimeHandlerViewFrameContainer>> frame_containers_;

  // Used to match against plugin elements that request a scriptable object. The
  // one that matches is the one inserted in the HTML string injected by the
  // MimeHandlerViewAttachHelper (and hence requires a scriptable object to for
  // postMessage purposes). This will only be non-empty for full-page MHV.
  std::string internal_id_;
  // The plugin element that is managed by MimeHandlerViewContainerManager.
  blink::WebElement plugin_element_;
  blink::LocalFrameToken frame_token_;

  mojo::AssociatedReceiverSet<mojom::MimeHandlerViewContainerManager>
      receivers_;
  mojo::Receiver<mime_handler::BeforeUnloadControl>
      before_unload_control_receiver_{this};
  mojo::AssociatedRemote<mojom::GuestView> remote_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_MANAGER_H_
