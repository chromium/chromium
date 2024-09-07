// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_resource_provider.h"

#include <memory>
#include "base/functional/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "media/base/limits.h"
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
    viz::SharedBitmapReporter* shared_bitmap_reporter,
    scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface) {
  context_provider_ = media_context_provider;
  resource_provider_ = std::make_unique<viz::ClientResourceProvider>();

  int max_texture_size;
  if (context_provider_) {
    max_texture_size =
        context_provider_->ContextCapabilities().max_texture_size;
  } else {
    max_texture_size = media::limits::kMaxDimension;
  }

  resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
      media_context_provider, shared_bitmap_reporter, resource_provider_.get(),
      std::move(shared_image_interface), settings_.use_stream_video_draw_quad,
      settings_.use_gpu_memory_buffer_resources, max_texture_size);
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
    viz::CompositorRenderPass* render_pass,
    scoped_refptr<media::VideoFrame> frame,
    media::VideoTransformation media_transform,
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
    resource_updater_->ObtainFrameResource(frame);
  } else {
    resource_updater_->ObtainFrameResource(frame);
  }

  auto transform = gfx::Transform();

  // The quad's rect is in pre-transform space so that applying the transform on
  // it will produce the bounds in target space.
  auto quad_rect = gfx::Rect(frame->natural_size());

  switch (media_transform.rotation) {
    case media::VIDEO_ROTATION_90:
      transform.RotateAboutZAxis(90.0);
      transform.Translate(0, -quad_rect.height());
      break;
    case media::VIDEO_ROTATION_180:
      transform.RotateAboutZAxis(180.0);
      transform.Translate(-quad_rect.width(), -quad_rect.height());
      break;
    case media::VIDEO_ROTATION_270:
      transform.RotateAboutZAxis(270.0);
      transform.Translate(-quad_rect.width(), 0);
      break;
    case media::VIDEO_ROTATION_0:
      break;
  }

  if (media_transform.mirrored) {
    transform.RotateAboutYAxis(180.0);
    transform.Translate(-quad_rect.width(), 0);
  }

  gfx::Rect visible_quad_rect = quad_rect;
  gfx::MaskFilterInfo mask_filter_info;
  float draw_opacity = 1.0f;
  int sorting_context_id = 0;

  resource_updater_->AppendQuad(render_pass, std::move(frame), transform,
                                quad_rect, visible_quad_rect, mask_filter_info,
                                /*clip_rect=*/std::nullopt, is_opaque,
                                draw_opacity, sorting_context_id);
}

void VideoFrameResourceProvider::ReleaseFrameResources() {
  resource_updater_->ReleaseFrameResource();
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
    Vector<viz::ReturnedResource> transferable_resources) {
  std::vector<viz::ReturnedResource> returned_resources(
      std::make_move_iterator(transferable_resources.begin()),
      std::make_move_iterator(transferable_resources.end()));
  resource_provider_->ReceiveReturnsFromParent(std::move(returned_resources));
}

}  // namespace blink
