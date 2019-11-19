// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_

#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_remote_frame.h"

namespace blink {
class WebURLRequest;
struct WebRect;

class WebRemoteFrameClient {
 public:
  // Specifies the reason for the detachment.
  enum class DetachType { kRemove, kSwap };

  // Notify the embedder that it should remove this frame from the frame tree
  // and release any resources associated with it.
  virtual void FrameDetached(DetachType) {}

  // Notifies the remote frame to check whether it is done loading, after one
  // of its children finishes loading.
  virtual void CheckCompleted() {}

  // Notifies the embedder that a postMessage was issued to a remote frame.
  virtual void ForwardPostMessage(WebLocalFrame* source_frame,
                                  WebRemoteFrame* target_frame,
                                  WebSecurityOrigin target_origin,
                                  WebDOMMessageEvent) {}

  // A remote frame was asked to start a navigation.
  virtual void Navigate(
      const WebURLRequest& request,
      bool should_replace_current_entry,
      bool is_opener_navigation,
      bool has_download_sandbox_flag,
      bool blocking_downloads_in_sandbox_without_user_activation_enabled,
      bool initiator_frame_is_ad,
      mojo::ScopedMessagePipeHandle blob_url_token) {}

  virtual void FrameRectsChanged(const WebRect& local_frame_rect,
                                 const WebRect& screen_space_rect) {}

  virtual void UpdateRemoteViewportIntersection(
      const ViewportIntersectionState& intersection_state) {}

  // Set or clear the inert property on the remote frame.
  virtual void SetIsInert(bool) {}

  // Toggles render throttling for the remote frame.
  virtual void UpdateRenderThrottlingStatus(bool is_throttled,
                                            bool subtree_throttled) {}

  // This frame updated its opener to another frame.
  virtual void DidChangeOpener(WebFrame* opener) {}

  // Continue sequential focus navigation in this frame.  This is called when
  // the |source| frame is searching for the next focusable element (e.g., in
  // response to <tab>) and encounters a remote frame.
  virtual void AdvanceFocus(WebFocusType type, WebLocalFrame* source) {}

  // Returns token to be used as a frame id in the devtools protocol.
  // It is derived from the content's devtools_frame_token, is
  // defined by the browser and passed into Blink upon frame creation.
  virtual base::UnguessableToken GetDevToolsFrameToken() {
    return base::UnguessableToken::Create();
  }

  // Print out this frame.
  // |rect| is the rectangular area where this frame resides in its parent
  // frame.
  // |canvas| is the canvas we are printing on.
  // Returns the id of the placeholder content.
  virtual uint32_t Print(const WebRect& rect, cc::PaintCanvas* canvas) {
    return 0;
  }

 protected:
  virtual ~WebRemoteFrameClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_REMOTE_FRAME_CLIENT_H_
