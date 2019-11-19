// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_impl.h"

#include "base/location.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/layer.h"

namespace content {

struct ReflectorImpl::LayerData {
  LayerData(ui::Layer* layer) : layer(layer) {}

  ui::Layer* layer;
  bool needs_set_mailbox = false;
};

ReflectorImpl::ReflectorImpl(ui::Compositor* mirrored_compositor,
                             ui::Layer* mirroring_layer)
    : mirrored_compositor_(mirrored_compositor),
      flip_texture_(false),
      output_surface_(nullptr) {
  if (mirroring_layer)
    AddMirroringLayer(mirroring_layer);
}

ReflectorImpl::~ReflectorImpl() = default;

void ReflectorImpl::Shutdown() {
  if (output_surface_)
    DetachFromOutputSurface();
  // Prevent the ReflectorImpl from picking up a new output surface.
  mirroring_layers_.clear();
}

void ReflectorImpl::DetachFromOutputSurface() {
  DCHECK(output_surface_);
  output_surface_->SetReflector(nullptr);
  DCHECK(mailbox_);
  mailbox_.reset();
  output_surface_ = nullptr;
  for (const auto& layer_data : mirroring_layers_)
    layer_data->layer->SetShowSolidColorContent();
}

void ReflectorImpl::OnSourceSurfaceReady(
    BrowserCompositorOutputSurface* output_surface) {
  if (mirroring_layers_.empty())
    return;  // Was already Shutdown().
  if (output_surface == output_surface_)
    return;  // Is already attached.
  if (output_surface_)
    DetachFromOutputSurface();

  output_surface_ = output_surface;

  flip_texture_ = !output_surface->capabilities().flipped_output_surface;

  output_surface_->SetReflector(this);
}

void ReflectorImpl::OnMirroringCompositorResized() {
  for (const auto& layer_data : mirroring_layers_)
    layer_data->layer->SchedulePaint(layer_data->layer->bounds());
}

void ReflectorImpl::AddMirroringLayer(ui::Layer* layer) {
  DCHECK(layer->GetCompositor());
  DCHECK(mirroring_layers_.end() == FindLayerData(layer));

  mirroring_layers_.push_back(std::make_unique<LayerData>(layer));
  if (mailbox_)
    mirroring_layers_.back()->needs_set_mailbox = true;
  mirrored_compositor_->ScheduleFullRedraw();
}

void ReflectorImpl::RemoveMirroringLayer(ui::Layer* layer) {
  DCHECK(layer->GetCompositor());

  auto iter = FindLayerData(layer);
  DCHECK(iter != mirroring_layers_.end());
  (*iter)->layer->SetShowSolidColorContent();
  mirroring_layers_.erase(iter);

  if (mirroring_layers_.empty() && output_surface_)
    DetachFromOutputSurface();
}

void ReflectorImpl::OnSourceTextureMailboxUpdated(
    scoped_refptr<OwnedMailbox> mailbox) {
  mailbox_ = mailbox;
  if (mailbox_.get()) {
    for (const auto& layer_data : mirroring_layers_)
      layer_data->needs_set_mailbox = true;

    // The texture doesn't have the data. Request full redraw on mirrored
    // compositor so that the full content will be copied to mirroring
    // compositor. This full redraw should land us in OnSourceSwapBuffers() to
    // resize the texture appropriately.
    mirrored_compositor_->ScheduleFullRedraw();
  }
}

void ReflectorImpl::OnSourceSwapBuffers(const gfx::Size& surface_size) {
  if (mirroring_layers_.empty())
    return;

  // Should be attached to the source output surface already.
  DCHECK(mailbox_.get());

  // Request full redraw on mirroring compositor.
  for (const auto& layer_data : mirroring_layers_)
    UpdateTexture(layer_data.get(), surface_size, layer_data->layer->bounds());
}

void ReflectorImpl::OnSourcePostSubBuffer(const gfx::Rect& swap_rect,
                                          const gfx::Size& surface_size) {
  if (mirroring_layers_.empty())
    return;

  // Should be attached to the source output surface already.
  DCHECK(mailbox_.get());

  gfx::Rect mirroring_rect = swap_rect;
  if (flip_texture_) {
    // Flip the coordinates to compositor's one.
    mirroring_rect.set_y(surface_size.height() - swap_rect.y() -
                         swap_rect.height());
  }

  // Request redraw of the dirty portion in mirroring compositor.
  for (const auto& layer_data : mirroring_layers_)
    UpdateTexture(layer_data.get(), surface_size, mirroring_rect);
}

std::vector<std::unique_ptr<ReflectorImpl::LayerData>>::iterator
ReflectorImpl::FindLayerData(ui::Layer* layer) {
  return std::find_if(mirroring_layers_.begin(), mirroring_layers_.end(),
                      [layer](const std::unique_ptr<LayerData>& layer_data) {
                        return layer_data->layer == layer;
                      });
}

void ReflectorImpl::UpdateTexture(ReflectorImpl::LayerData* layer_data,
                                  const gfx::Size& source_size,
                                  const gfx::Rect& redraw_rect) {
  if (layer_data->needs_set_mailbox) {
    layer_data->layer->SetTransferableResource(
        viz::TransferableResource::MakeGL(
            mailbox_->holder().mailbox, GL_LINEAR,
            mailbox_->holder().texture_target, mailbox_->holder().sync_token,
            source_size, false /* is_overlay_candidate */),
        mailbox_->GetSingleReleaseCallback(), source_size);
    layer_data->needs_set_mailbox = false;
  } else {
    layer_data->layer->SetTextureSize(source_size);
  }
  layer_data->layer->SetBounds(gfx::Rect(source_size));
  layer_data->layer->SetTextureFlipped(flip_texture_);
  layer_data->layer->SchedulePaint(redraw_rect);
}

}  // namespace content
