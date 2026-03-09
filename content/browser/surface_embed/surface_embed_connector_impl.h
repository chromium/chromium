// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SURFACE_EMBED_SURFACE_EMBED_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_SURFACE_EMBED_SURFACE_EMBED_CONNECTOR_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/surface_embed_connector.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace input {

class RenderWidgetHostInputEventRouter;

}  // namespace input

namespace content {
class DummySurfaceProvider;

class RenderViewHostDelegateView;
class RenderWidgetHostViewChildFrame;
class TextInputManager;
class WebContentsImpl;
class WebContentsView;

class CONTENT_EXPORT SurfaceEmbedConnectorImpl : public SurfaceEmbedConnector {
 public:
  // `child_web_contents` will have ownership of this. `delegate` is required to
  // outlive this.
  //  Note: Since this will live on `child_web_contents`'s WebContentsUserData,
  //  the constructor must have its first parameter be a `WebContents*` pointing
  //  to the web contents that owns the WebContentsUserData.
  // TODO(cammie): Remove the note above about WCUD once we land changes to
  // store as a std::unique_ptr instead.
  SurfaceEmbedConnectorImpl(WebContents* child_web_contents,
                            WebContentsImpl* parent_web_contents,
                            SurfaceEmbedConnector::Delegate* delegate);
  ~SurfaceEmbedConnectorImpl() override;

  WebContentsView* GetParentWebContentsView() const;
  RenderViewHostDelegateView* GetParentRenderViewHostDelegateView() const;

  // Returns the InputEventRouter appropriate for the child web contents to
  // register with. Note that this is the parent web contents's
  // InputEventRouter, and this will return nullptr if the parent web contents
  // is null.
  input::RenderWidgetHostInputEventRouter* GetInputEventRouter();

  // Returns the parent web contents's TextInputManager, or nullptr if the
  // parent web contents is null.
  TextInputManager* GetTextInputManager();

  // SurfaceEmbedConnector:
  SurfaceEmbedConnector::Delegate* GetDelegate() override;
  void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) override;
  const viz::FrameSinkId& GetFrameSinkId() const override;

 private:
  WebContentsImpl* parent_web_contents() const;

  raw_ptr<SurfaceEmbedConnector::Delegate> delegate_ = nullptr;

  base::WeakPtr<WebContents> parent_web_contents_;
  raw_ptr<WebContentsImpl> child_web_contents_ = nullptr;  // Owns us.
  raw_ptr<RenderWidgetHostViewChildFrame> view_ = nullptr;

  std::unique_ptr<DummySurfaceProvider> dummy_surface_provider_;

  // The last received LocalSurfaceId from the SurfaceEmbed.
  viz::LocalSurfaceId local_surface_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SURFACE_EMBED_SURFACE_EMBED_CONNECTOR_IMPL_H_
