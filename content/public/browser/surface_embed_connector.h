// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SURFACE_EMBED_CONNECTOR_H_
#define CONTENT_PUBLIC_BROWSER_SURFACE_EMBED_CONNECTOR_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"

namespace blink {
struct FrameVisualProperties;
}  // namespace blink

namespace content {

class WebContents;

// This connector embeds a child/inner WebContents within a parent/outer
// WebContents without using the OOPIF connections (CrossProcessFrameConnector
// and such). It is created by SurfaceEmbedHost when the child WebContents is
// attached and subsequently owned by the child WebContents; it is destroyed
// when the child is detached.
class CONTENT_EXPORT SurfaceEmbedConnector {
 public:
  // The SurfaceEmbedConnector::Delegate class is implemented by
  // surface_embed::SurfaceEmbedHost.
  class Delegate {
   public:
    // Embeds a surface by its FrameSinkId.
    virtual void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) = 0;

    // Called when the child frame initiates a LocalSurfaceId update.
    // This typically happens when an auto-resized child frame has a new
    // intrinsic size. However, this has also been observed on non-auto-resized
    // frames (e.g., www.nytimes.com) due to an unclear triggering condition.
    virtual void UpdateLocalSurfaceIdFromChild(
        const viz::LocalSurfaceId& local_surface_id) = 0;

    // Called when the connector is being detached at a SurfaceEmbedHost's
    // request instead of the SurfaceEmbed plugin's request.
    virtual void DetachedByHost() = 0;

    // Returns whether this delegate's parent WebContents still has an attached
    // SurfaceEmbed child WebContents.
    virtual bool IsAttachedForTesting() const = 0;

    // Called when the process for the child frame crashed.
    virtual void ChildProcessGone() = 0;

    // Requests focus for the embedding element in the parent.
    virtual void RequestFocus() = 0;
  };

  // Attach a child WebContents to a parent WebContents. This creates a
  // SurfaceEmbedConnector owned by the child WebContents.
  static void Attach(WebContents* child_web_contents,
                     WebContents* parent_web_contents,
                     SurfaceEmbedConnector::Delegate* delegate);

  // Detach the SurfaceEmbedConnector from the child WebContents. This destroys
  // the SurfaceEmbedConnector owned by the child WebContents.
  static void Detach(WebContents* child_web_contents);

  virtual ~SurfaceEmbedConnector() = default;

  virtual Delegate* GetDelegate() = 0;

  // Called when the visibility of the parent frame changes.
  virtual void OnVisibilityChanged(
      blink::mojom::FrameVisibility visibility) = 0;

  // Called by the SecureEmbedHost to synchronize visual properties between the
  // parent and child WebContents.
  virtual void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) = 0;

  // Gets the FrameSinkId of the child's view.
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;

  // Returns the CSS zoom factor last received from the parent frame.
  // Exposed for testing to cleanly verify properties without creating flakes
  // from cross-process EvalJs layout evaluation delays.
  virtual double GetCssZoomFactorForTesting() = 0;

  // Returns the last received local frame size in physical pixels.
  // Exposed for testing to cleanly verify properties without creating flakes
  // from cross-process EvalJs layout evaluation delays.
  virtual const gfx::Size& GetLocalFrameSizeInPixelsForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SURFACE_EMBED_CONNECTOR_H_
