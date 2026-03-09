// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/surface_embed/surface_embed_connector_impl.h"

#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/surface_embed/dummy_surface_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "ui/compositor/compositor.h"

namespace content {

SurfaceEmbedConnectorImpl::SurfaceEmbedConnectorImpl(
    WebContents* child_web_contents,
    WebContentsImpl* parent_web_contents,
    SurfaceEmbedConnector::Delegate* delegate)
    : delegate_(delegate),
      parent_web_contents_(parent_web_contents->GetWeakPtr()),
      child_web_contents_(static_cast<WebContentsImpl*>(child_web_contents)),
      dummy_surface_provider_(std::make_unique<DummySurfaceProvider>()) {}

SurfaceEmbedConnectorImpl::~SurfaceEmbedConnectorImpl() = default;

WebContentsView* SurfaceEmbedConnectorImpl::GetParentWebContentsView() const {
  return parent_web_contents() ? parent_web_contents()->GetView() : nullptr;
}

RenderViewHostDelegateView*
SurfaceEmbedConnectorImpl::GetParentRenderViewHostDelegateView() const {
  return parent_web_contents() ? parent_web_contents()->GetDelegateView()
                               : nullptr;
}

input::RenderWidgetHostInputEventRouter*
SurfaceEmbedConnectorImpl::GetInputEventRouter() {
  return parent_web_contents() ? parent_web_contents()->GetInputEventRouter()
                               : nullptr;
}

TextInputManager* SurfaceEmbedConnectorImpl::GetTextInputManager() {
  return parent_web_contents() ? parent_web_contents()->GetTextInputManager()
                               : nullptr;
}

SurfaceEmbedConnector::Delegate* SurfaceEmbedConnectorImpl::GetDelegate() {
  return delegate_;
}

const viz::FrameSinkId& SurfaceEmbedConnectorImpl::GetFrameSinkId() const {
  return dummy_surface_provider_->frame_sink_id();
}

void SurfaceEmbedConnectorImpl::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  dummy_surface_provider_->SubmitCompositorFrame(
      visual_properties.local_surface_id,
      visual_properties.screen_infos.current().device_scale_factor,
      visual_properties.local_frame_size);
}

WebContentsImpl* SurfaceEmbedConnectorImpl::parent_web_contents() const {
  return static_cast<WebContentsImpl*>(parent_web_contents_.get());
}

}  // namespace content
