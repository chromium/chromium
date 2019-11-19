// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_desktop.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/client/display/canvas.h"
#include "remoting/client/display/gl_math.h"
#include "remoting/client/display/gl_render_layer.h"
#include "remoting/client/display/gl_texture_ids.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

void UpdateDesktopRegion(const webrtc::DesktopFrame& frame,
                         const webrtc::DesktopRegion& region,
                         const webrtc::DesktopRect& texture_rect,
                         GlRenderLayer* layer) {
  for (webrtc::DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
    const uint8_t* rect_start = frame.GetFrameDataAtPos(i.rect().top_left());
    webrtc::DesktopVector update_pos =
        i.rect().top_left().subtract(texture_rect.top_left());
    layer->UpdateTexture(rect_start, update_pos.x(), update_pos.y(),
                         i.rect().width(), i.rect().height(), frame.stride());
  }
}

void SetFrameForTexture(const webrtc::DesktopFrame& frame,
                        GlRenderLayer* layer,
                        const webrtc::DesktopRect& rect) {
  if (!layer->is_texture_set()) {
    // First frame received.
    layer->SetTexture(frame.GetFrameDataAtPos(rect.top_left()), rect.width(),
                      rect.height(), frame.stride());
    return;
  }
  // Incremental update.
  if (frame.size().equals(rect.size())) {
    // Single texture. No intersection is needed.
    UpdateDesktopRegion(frame, frame.updated_region(), rect, layer);
  } else {
    webrtc::DesktopRegion intersected_region = frame.updated_region();
    intersected_region.IntersectWith(rect);
    UpdateDesktopRegion(frame, intersected_region, rect, layer);
  }
}

}  // namespace

struct GlDesktop::GlDesktopTextureContainer {
  std::unique_ptr<GlRenderLayer> layer;
  webrtc::DesktopRect rect;
};

GlDesktop::GlDesktop() {}

GlDesktop::~GlDesktop() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GlDesktop::SetCanvas(base::WeakPtr<Canvas> canvas) {
  last_desktop_size_.set(0, 0);
  textures_.clear();
  canvas_ = canvas;
  if (canvas) {
    max_texture_size_ = canvas->GetMaxTextureSize();
  }
}

void GlDesktop::SetVideoFrame(const webrtc::DesktopFrame& frame) {
  if (!canvas_) {
    return;
  }
  if (!frame.size().equals(last_desktop_size_)) {
    last_desktop_size_.set(frame.size().width(), frame.size().height());
    ReallocateTextures(last_desktop_size_);
  }
  for (std::unique_ptr<GlDesktopTextureContainer>& texture : textures_) {
    SetFrameForTexture(frame, texture->layer.get(), texture->rect);
  }
}

bool GlDesktop::Draw() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!textures_.empty() && !last_desktop_size_.is_empty()) {
    for (std::unique_ptr<GlDesktopTextureContainer>& texture : textures_) {
      texture->layer->Draw(1.0f);
    }
  }
  return false;
}

int GlDesktop::GetZIndex() {
  return Drawable::ZIndex::DESKTOP;
}

void GlDesktop::ReallocateTextures(const webrtc::DesktopSize& size) {
  DCHECK(max_texture_size_);
  DCHECK(canvas_);
  textures_.clear();

  int textures_per_row =
      (size.width() + max_texture_size_ - 1) / max_texture_size_;

  int textures_per_column =
      (size.height() + max_texture_size_ - 1) / max_texture_size_;

  webrtc::DesktopRect desktop_rect = webrtc::DesktopRect::MakeSize(size);

  int texture_id = kGlDesktopFirstTextureId;
  std::array<float, 8> positions;
  for (int x = 0; x < textures_per_row; x++) {
    for (int y = 0; y < textures_per_column; y++) {
      webrtc::DesktopRect rect = webrtc::DesktopRect::MakeXYWH(
          x * max_texture_size_, y * max_texture_size_, max_texture_size_,
          max_texture_size_);
      rect.IntersectWith(desktop_rect);
      std::unique_ptr<GlDesktopTextureContainer> container = base::WrapUnique(
          new GlDesktopTextureContainer{std::make_unique<GlRenderLayer>(
                                            texture_id, canvas_->GetWeakPtr()),
                                        rect});
      FillRectangleVertexPositions(rect.left(), rect.top(), rect.width(),
                                   rect.height(), &positions);
      container->layer->SetVertexPositions(positions);
      textures_.push_back(std::move(container));
      texture_id++;
    }
  }
}

base::WeakPtr<Drawable> GlDesktop::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
