// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_resource_provider.h"

#include <memory>
#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "media/base/video_frame.h"
#include "media/renderers/video_resource_updater.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

VideoFrameResourceProvider::VideoFrameResourceProvider(
    const cc::LayerTreeSettings& settings,
    bool use_sync_primitives)
    : settings_(settings), use_sync_primitives_(use_sync_primitives) {}

VideoFrameResourceProvider::~VideoFrameResourceProvider() {
  // Drop all resources before closing the ClientResourceProvider.
  resource_updater_ = nullptr;
  if (resource_provider_)
    resource_provider_->ShutdownAndReleaseAllResources();
}

void VideoFrameResourceProvider::Initialize(
    viz::RasterContextProvider* media_context_provider,
    viz::SharedBitmapReporter* shared_bitmap_reporter) {
  context_provider_ = media_context_provider;
  resource_provider_ = std::make_unique<viz::ClientResourceProvider>(
      /*delegated_sync_points_required=*/true);

  int max_texture_size;
  if (context_provider_) {
    max_texture_size =
        context_provider_->ContextCapabilities().max_texture_size;
  } else {
    // Pick an arbitrary limit here similar to what hardware might.
    max_texture_size = 16 * 1024;
  }

  resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
      nullptr, media_context_provider, shared_bitmap_reporter,
      resource_provider_.get(), settings_.use_stream_video_draw_quad,
      settings_.resource_settings.use_gpu_memory_buffer_resources,
      settings_.resource_settings.use_r16_texture, max_texture_size);
}

void VideoFrameResourceProvider::OnContextLost() {
  // Drop all resources before closing the ClientResourceProvider.
  resource_updater_ = nullptr;
  if (resource_provider_)
    resource_provider_->ShutdownAndReleaseAllResources();
  resource_provider_ = nullptr;
  context_provider_ = nullptr;
}

void VideoFrameResourceProvider::AppendQuads(
    viz::RenderPass* render_pass,
    scoped_refptr<media::VideoFrame> frame,
    media::VideoRotation rotation,
    bool is_opaque) {
  TRACE_EVENT0("media", "VideoFrameResourceProvider::AppendQuads");
  DCHECK(resource_updater_);
  DCHECK(resource_provider_);

  // When obtaining frame resources, we end up having to wait. See
  // https://crbug/878070.
  // Unfortunately, we have no idea if blocking is allowed on the current thread
  // or not.  If we're on the cc impl thread, the answer is yes, and further
  // the thread is marked as not allowing blocking primitives.  On the various
  // media threads, however, blocking is not allowed but the blocking scopes
  // are.  So, we use ScopedAllow only if we're told that we should do so.
  if (use_sync_primitives_) {
    base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
    resource_updater_->ObtainFrameResources(frame);
  } else {
    resource_updater_->ObtainFrameResources(frame);
  }

  gfx::Transform transform = gfx::Transform();
  // The quad's rect is in pre-transform space so that applying the transform on
  // it will produce the bounds in target space.
  gfx::Rect quad_rect = gfx::Rect(frame->natural_size());

  switch (rotation) {
    case media::VIDEO_ROTATION_90:
      transform.Rotate(90.0);
      transform.Translate(0.0, -quad_rect.height());
      break;
    case media::VIDEO_ROTATION_180:
      transform.Rotate(180.0);
      transform.Translate(-quad_rect.width(), -quad_rect.height());
      break;
    case media::VIDEO_ROTATION_270:
      transform.Rotate(270.0);
      transform.Translate(-quad_rect.width(), 0);
      break;
    case media::VIDEO_ROTATION_0:
      break;
  }

  gfx::Rect visible_quad_rect = quad_rect;
  gfx::Rect clip_rect;
  gfx::RRectF rounded_corner_bounds;
  bool is_clipped = false;
  float draw_opacity = 1.0f;
  int sorting_context_id = 0;

  resource_updater_->AppendQuads(render_pass, std::move(frame), transform,
                                 quad_rect, visible_quad_rect,
                                 rounded_corner_bounds, clip_rect, is_clipped,
                                 is_opaque, draw_opacity, sorting_context_id);
}

void VideoFrameResourceProvider::ReleaseFrameResources() {
  resource_updater_->ReleaseFrameResources();
}

void VideoFrameResourceProvider::PrepareSendToParent(
    const WebVector<viz::ResourceId>& resource_ids,
    WebVector<viz::TransferableResource>* transferable_resources) {
  std::vector<viz::TransferableResource> resources_list;
  resource_provider_->PrepareSendToParent(
      const_cast<WebVector<viz::ResourceId>&>(resource_ids).ReleaseVector(),
      &resources_list, context_provider_);
  *transferable_resources = std::move(resources_list);
}

void VideoFrameResourceProvider::ReceiveReturnsFromParent(
    const Vector<viz::ReturnedResource>& transferable_resources) {
  resource_provider_->ReceiveReturnsFromParent(
      WebVector<viz::ReturnedResource>(transferable_resources).ReleaseVector());
}

}  // namespace blink
