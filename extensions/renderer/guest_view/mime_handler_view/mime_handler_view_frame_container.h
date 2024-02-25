// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_element.h"

#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace blink {
class WebFrame;
class WebLocalFrame;
}  // namespace blink

namespace extensions {
class MimeHandlerViewContainerManager;

// A container for loading an extension in a guest to handle a MIME type.
// This is created in the embedder to support postMessage from the embedder side
// to the corresponding MimeHandlerViewGuest. It is owned and managed by the
// MimeHandlerViewContainerManager instance of the embedder frame.
// To understand the role of MHVFC container consider the rough sketch:
//
//  #document <1-- arbitrary origin -->
//
//    <embed>
//      #document <!-- origin = |resource_url| -->
//      <iframe>
//        #document <!-- MimeHandlerView extension -->
//           <embed type="application/x-google-chrome=pdf></embed>
//      </iframe>
//    </embed>
// Note that MimeHandlerViewFrameContainer is created for the <embed> in the
// ancestor document and will facilitate postMessage to the extensions's
// document. The postMessages are then forwarded to the inner most pepper
// plugin.
class MimeHandlerViewFrameContainer : public PostMessageSupport::Delegate {
 public:
  MimeHandlerViewFrameContainer(
      MimeHandlerViewContainerManager* container_manager,
      const blink::WebElement& plugin_element,
      const GURL& resource_url,
      const std::string& mime_type);

  ~MimeHandlerViewFrameContainer() override;

  // PostMessageHelper::Delegate.
  blink::WebLocalFrame* GetSourceFrame() override;
  blink::WebFrame* GetTargetFrame() override;
  bool IsEmbedded() const override;
  bool IsResourceAccessibleBySource() const override;

  int32_t element_instance_id() const { return element_instance_id_; }
  const blink::WebElement& plugin_element() { return plugin_element_; }
  const GURL& resource_url() const { return resource_url_; }
  const std::string& mime_type() const { return mime_type_; }

  blink::WebFrame* GetContentFrame() const;

  // Verifies if the frames are valid and if so, the proxy IDs match the
  // expected values. Note: calling this method might lead to the destruction of
  // MimeHandlerViewFrameContainer (if the frames are not alive).
  bool AreFramesAlive();

  // Establishes the expected FrameTokens for the content frame and its first
  // child (guest). These are verified every time GetTargetFrame is called.
  void SetFrameTokens(const blink::FrameToken& content_frame_id,
                      const blink::FrameToken& guest_frame_id);

  void set_element_instance_id(int32_t id) { element_instance_id_ = id; }

 private:
  // Verifies that the frames are alive and the routing IDs match the expected
  // values.
  bool AreFramesValid();

  // Controls the lifetime of |this| (always alive).
  const raw_ptr<MimeHandlerViewContainerManager> container_manager_;
  blink::WebElement plugin_element_;
  const GURL resource_url_;
  const std::string mime_type_;
  // The |element_instance_id| of the MimeHandlerViewGuest associated with this
  // frame container. This is updated in DidLoad().
  int32_t element_instance_id_ = guest_view::kInstanceIDNone;
  // The FrameToken of the content frame (frame or proxy) and guest frame
  // (proxy) which will be confirmed by the browser. Used to validate the
  // destination for postMessage.
  blink::FrameToken content_frame_token_;
  blink::FrameToken guest_frame_token_;
  bool frame_tokens_set_ = false;
  // Determines whether the embedder can access |original_url_|. Used for UMA.
  bool is_resource_accessible_to_embedder_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_FRAME_CONTAINER_H_
