// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SURFACE_EMBED_CONNECTOR_H_
#define CONTENT_PUBLIC_BROWSER_SURFACE_EMBED_CONNECTOR_H_

#include "base/observer_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
struct FrameVisualProperties;
}  // namespace blink

namespace content {

class RenderFrameHost;
class WebContents;

// This permits an embedder to host one WebContents within another;
// without them being involved in usual frame hierarchy. The user of this API
// is responsible for forwarding a lot of neccessary information around; see
// components/surface_embed for one way of doing it.
class CONTENT_EXPORT SurfaceEmbedConnector {
 public:
  enum class FocusOperation {
    kFocusSurface,
    kFocusBeforeSurface,
    kFocusAfterSurface
  };

  class Delegate {
   public:
    virtual void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) = 0;

    virtual void UpdateLocalSurfaceIdFromChild(
        const viz::LocalSurfaceId& local_surface_id) = 0;

    // Requests focus in the embedder document for either the embedding element,
    // or the elements before or after it in the tab order, based on `focus_op`.
    virtual void FocusInEmbedder(FocusOperation focus_op) = 0;

    // Called when the process for the embedded frame crashed.
    virtual void ChildProcessGone() = 0;

    // Called when the connector is being detached at the host's request instead
    // of the SurfaceEmbed's request.
    virtual void DetachedByHost() = 0;

    // Returns whether this delegate's host still has an attached guest.
    virtual bool IsAttachedForTesting() const = 0;
  };

  // Attach a WebContents to a parent WebContents. This creates a
  // SurfaceEmbedConnector owned by the child WebContents.
  static void Attach(WebContents* parent_web_contents,
                     WebContents* child_web_contents,
                     SurfaceEmbedConnector::Delegate* delegate);

  // Detach the SurfaceEmbedConnector from the child WebContents. This destroys
  // the SurfaceEmbedConnector owned by the child WebContents.
  static void Detach(WebContents* child_web_contents);

  virtual ~SurfaceEmbedConnector() = default;

  virtual Delegate* GetDelegate() = 0;

  // Called by the embedder to synchronize visual properties with the guest.
  virtual void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) = 0;

  // Called by the embedder to report the content being hidden or shown.
  virtual void OnVisibilityChanged(
      blink::mojom::FrameVisibility visibility) = 0;

  // Called by the embedder to either set or clear focus on the embedded frame.
  virtual void SetFocus(bool focused, blink::mojom::FocusType focus_type) = 0;

  // Gets the FrameSinkId of the guest's view.
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SURFACE_EMBED_CONNECTOR_H_
