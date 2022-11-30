// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_cursor.h"

#include <memory>

#include "remoting/base/util.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/display/gl_math.h"
#include "remoting/client/display/gl_render_layer.h"
#include "remoting/client/display/gl_texture_ids.h"
#include "remoting/proto/control.pb.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"

namespace remoting {

namespace {
const int kDefaultCursorDataSize = 32 * 32 * GlRenderLayer::kBytesPerPixel;
}  // namespace

GlCursor::GlCursor() {}

GlCursor::~GlCursor() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GlCursor::SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) {
  int data_size = cursor_shape.width() * cursor_shape.height() *
                  GlRenderLayer::kBytesPerPixel;
  if (current_cursor_data_size_ < data_size) {
    current_cursor_data_size_ =
        kDefaultCursorDataSize > data_size ? kDefaultCursorDataSize : data_size;
    current_cursor_data_.reset(new uint8_t[current_cursor_data_size_]);
  }
  int stride = cursor_shape.width() * GlRenderLayer::kBytesPerPixel;
  libyuv::ABGRToARGB(
      reinterpret_cast<const uint8_t*>(cursor_shape.data().data()), stride,
      current_cursor_data_.get(), stride, cursor_shape.width(),
      cursor_shape.height());

  bool size_changed = current_cursor_width_ != cursor_shape.width() ||
                      current_cursor_height_ != cursor_shape.height();

  current_cursor_width_ = cursor_shape.width();
  current_cursor_height_ = cursor_shape.height();
  current_cursor_hotspot_x_ = cursor_shape.hotspot_x();
  current_cursor_hotspot_y_ = cursor_shape.hotspot_y();

  SetCurrentCursorShape(size_changed);

  SetCursorPosition(cursor_x_, cursor_y_);
}

void GlCursor::SetCursorPosition(float x, float y) {
  cursor_x_ = x;
  cursor_y_ = y;
  if (!current_cursor_data_) {
    return;
  }
  std::array<float, 8> positions;
  FillRectangleVertexPositions(
      x - current_cursor_hotspot_x_, y - current_cursor_hotspot_y_,
      current_cursor_width_, current_cursor_height_, &positions);
  if (layer_) {
    layer_->SetVertexPositions(positions);
  }
}

void GlCursor::SetCursorVisible(bool visible) {
  visible_ = visible;
}

void GlCursor::SetCanvas(base::WeakPtr<Canvas> canvas) {
  if (!canvas) {
    layer_.reset();
    return;
  }
  layer_ = std::make_unique<GlRenderLayer>(kGlCursorTextureId, canvas);
  if (current_cursor_data_) {
    SetCurrentCursorShape(true);
  }
  SetCursorPosition(cursor_x_, cursor_y_);
}

bool GlCursor::Draw() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (layer_ && current_cursor_data_ && visible_) {
    layer_->Draw(1.f);
  }
  return false;
}

int GlCursor::GetZIndex() {
  return Drawable::ZIndex::CURSOR;
}

void GlCursor::SetCurrentCursorShape(bool size_changed) {
  if (layer_) {
    if (size_changed) {
      layer_->SetTexture(current_cursor_data_.get(), current_cursor_width_,
                         current_cursor_height_, 0);
    } else {
      layer_->UpdateTexture(current_cursor_data_.get(), 0, 0,
                            current_cursor_width_, current_cursor_width_, 0);
    }
  }
}

base::WeakPtr<Drawable> GlCursor::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
