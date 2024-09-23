// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/display/gl_render_layer.h"

#include "base/check.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/display/gl_helpers.h"

namespace remoting {

namespace {

// Assign texture coordinates to buffers for use in shader program.
const float kVertices[] = {
    // Points order: upper-left, bottom-left, upper-right, bottom-right.

    // Positions to draw the texture on the normalized canvas coordinate.
    0, 0, 0, 0, 0, 0, 0, 0,

    // Region of the texture to be used (normally the whole texture).
    0, 0, 0, 1, 1, 0, 1, 1};

const int kDefaultUpdateBufferCapacity =
    2048 * 2048 * GlRenderLayer::kBytesPerPixel;

void PackDirtyRegion(uint8_t* dest,
                     const uint8_t* source,
                     int width,
                     int height,
                     int stride) {
  for (int i = 0; i < height; i++) {
    memcpy(dest, source, width * GlRenderLayer::kBytesPerPixel);
    source += stride;
    dest += GlRenderLayer::kBytesPerPixel * width;
  }
}

}  // namespace

GlRenderLayer::GlRenderLayer(int texture_id, base::WeakPtr<Canvas> canvas)
    : texture_id_(texture_id), canvas_(canvas) {
  texture_handle_ = CreateTexture();
  buffer_handle_ = CreateBuffer(kVertices, sizeof(kVertices));
}

GlRenderLayer::~GlRenderLayer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  glDeleteBuffers(1, &buffer_handle_);
  glDeleteTextures(1, &texture_handle_);
}

void GlRenderLayer::SetTexture(const uint8_t* texture,
                               int width,
                               int height,
                               int stride) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(width >= 0 && height >= 0);
  texture_set_ = true;
  glActiveTexture(GL_TEXTURE0 + texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_handle_);

  bool should_reset_row_length;
  const void* buffer_to_update = PrepareTextureBuffer(
      texture, width, height, stride, &should_reset_row_length);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, buffer_to_update);

  if (should_reset_row_length) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void GlRenderLayer::UpdateTexture(const uint8_t* subtexture,
                                  int offset_x,
                                  int offset_y,
                                  int width,
                                  int height,
                                  int stride) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(texture_set_);
  DCHECK(width >= 0 && height >= 0);
  if (width == 0 || height == 0) {
    // There is nothing to update.
    return;
  }
  glActiveTexture(GL_TEXTURE0 + texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_handle_);

  bool should_reset_row_length;
  const void* buffer_to_update = PrepareTextureBuffer(
      subtexture, width, height, stride, &should_reset_row_length);

  glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, GL_RGBA,
                  GL_UNSIGNED_BYTE, buffer_to_update);

  if (should_reset_row_length) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

void GlRenderLayer::SetVertexPositions(const std::array<float, 8>& positions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  glBindBuffer(GL_ARRAY_BUFFER, buffer_handle_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(kVertices) / 2, positions.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  vertex_position_set_ = true;
}

void GlRenderLayer::SetTextureVisibleArea(
    const std::array<float, 8>& positions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  glBindBuffer(GL_ARRAY_BUFFER, buffer_handle_);
  glBufferSubData(GL_ARRAY_BUFFER, sizeof(kVertices) / 2, sizeof(kVertices) / 2,
                  positions.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GlRenderLayer::Draw(float alpha_multiplier) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(texture_set_ && vertex_position_set_);
  canvas_->DrawTexture(texture_id_, texture_handle_, buffer_handle_,
                       alpha_multiplier);
}

const uint8_t* GlRenderLayer::PrepareTextureBuffer(
    const uint8_t* data,
    int width,
    int height,
    int stride,
    bool* should_reset_row_length) {
  *should_reset_row_length = false;

  bool stride_multiple_of_bytes_per_pixel = stride % kBytesPerPixel == 0;
  bool loosely_packed = !stride_multiple_of_bytes_per_pixel ||
                        (stride > 0 && stride != kBytesPerPixel * width);

  if (!loosely_packed) {
    return data;
  }

  if (stride_multiple_of_bytes_per_pixel && canvas_->GetVersion() >= 3) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / kBytesPerPixel);
    *should_reset_row_length = true;
    return data;
  }

  // Doesn't support GL_UNPACK_ROW_LENGTH or stride not multiple of
  // kBytesPerPixel. Manually pack the data.
  int required_size = width * height * kBytesPerPixel;
  if (update_buffer_size_ < required_size) {
    if (required_size < kDefaultUpdateBufferCapacity) {
      update_buffer_size_ = kDefaultUpdateBufferCapacity;
    } else {
      update_buffer_size_ = required_size;
    }
    update_buffer_.reset(new uint8_t[update_buffer_size_]);
  }
  PackDirtyRegion(update_buffer_.get(), data, width, height, stride);
  return update_buffer_.get();
}

}  // namespace remoting
