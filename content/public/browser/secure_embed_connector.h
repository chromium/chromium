// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURE_EMBED_CONNECTOR_H_
#define CONTENT_PUBLIC_BROWSER_SECURE_EMBED_CONNECTOR_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
struct FrameVisualProperties;
}  // namespace blink

namespace content {

class RenderFrameHost;
class WebContents;

// This is created and owned by WebContents when it's created with
// `secure_embed_embedder` in its CreateParams.
class CONTENT_EXPORT SecureEmbedConnector {
 public:
  enum class FocusOperation {
    kFocusPlugin,
    kFocusBeforePlugin,
    kFocusAfterPlugin
  };

  class Delegate {
   public:
    virtual void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) = 0;

    // Requests focus in the embedder document for either the embedding element,
    // or the elements before or after it in the tab order, based on `focus_op`.
    virtual void FocusInEmbedder(FocusOperation focus_op) = 0;
  };

  virtual ~SecureEmbedConnector() = default;

  // Returns true if the WebContents this is owned by is configured to be
  // embedded in `web_contents`.
  //
  // TODO(secure-embed): There needs to be a way of updating this for when
  // tabs are moved between windows (including potentially an in-between
  // windows detached tab state).
  virtual bool IsConfiguredToBeEmbeddedIn(WebContents* web_contents) = 0;

  // Set by the embedder side to help communicate back to it.
  // This can switch between a value and null, but not two delegates.
  virtual void SetDelegate(Delegate* delegate) = 0;
  virtual Delegate* GetDelegate() = 0;

  // Called by the embedder to synchronize visual properties with the guest.
  virtual void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) = 0;

  // Called by the embedder to forwards a single keyboard event to the
  // embedded frame.
  virtual void ForwardKeyboardEvent(
      const blink::WebKeyboardEvent& keyboard_event) = 0;

  // Called by the embedder to either set or clear focus on the embedded frame.
  virtual void SetFocus(bool focused, blink::mojom::FocusType focus_type) = 0;

  // Gets the FrameSinkId of the guest's view.
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURE_EMBED_CONNECTOR_H_
