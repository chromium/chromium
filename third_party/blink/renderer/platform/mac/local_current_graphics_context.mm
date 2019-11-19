/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/platform/mac/local_current_graphics_context.h"

#include <AppKit/NSGraphicsContext.h>
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace blink {

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(
    GraphicsContext& graphics_context,
    const IntRect& dirty_rect)
    : LocalCurrentGraphicsContext(graphics_context.Canvas(),
                                  graphics_context.DeviceScaleFactor(),
                                  dirty_rect) {}

static const int kMaxDirtyRectPixelSize = 10000;

static SkIRect LocalToClampedDeviceRect(cc::PaintCanvas* canvas,
                                        const IntRect& local) {
  const SkMatrix& matrix = canvas->getTotalMatrix();
  SkRect device;
  if (!matrix.mapRect(&device, local))
    return SkIRect();
  // Constrain the maximum size of what we paint to something reasonable. This
  // accordingly means we will not paint the entirety of truly huge native form
  // elements, which is deemed an acceptable tradeoff for this simple approach
  // to manage such an edge case.
  SkIRect idevice = device.roundOut();
  idevice.intersect(SkIRect::MakeXYWH(idevice.x(), idevice.y(),
                                      kMaxDirtyRectPixelSize,
                                      kMaxDirtyRectPixelSize));
  return idevice;
}

LocalCurrentGraphicsContext::LocalCurrentGraphicsContext(
    cc::PaintCanvas* canvas,
    float device_scale_factor,
    const IntRect& dirty_rect)
    : did_set_graphics_context_(false),
      inflated_dirty_rect_(InflateRectForAA(dirty_rect)),
      graphics_context_canvas_(
          canvas,
          LocalToClampedDeviceRect(canvas, inflated_dirty_rect_),
          device_scale_factor) {
  saved_canvas_ = canvas;
  canvas->save();

  CGContextRef cg_context = this->CgContext();
  if (cg_context == [[NSGraphicsContext currentContext] graphicsPort]) {
    saved_ns_graphics_context_ = 0;
    return;
  }

  saved_ns_graphics_context_ = [[NSGraphicsContext currentContext] retain];
  NSGraphicsContext* new_context =
      [NSGraphicsContext graphicsContextWithGraphicsPort:cg_context
                                                 flipped:YES];
  [NSGraphicsContext setCurrentContext:new_context];
  did_set_graphics_context_ = true;
}

LocalCurrentGraphicsContext::~LocalCurrentGraphicsContext() {
  if (did_set_graphics_context_) {
    [NSGraphicsContext setCurrentContext:saved_ns_graphics_context_];
    [saved_ns_graphics_context_ release];
  }

  saved_canvas_->restore();
}

CGContextRef LocalCurrentGraphicsContext::CgContext() {
  // This synchronizes the CGContext to reflect the current SkCanvas state.
  // The implementation may not return the same CGContext each time.
  CGContextRef cg_context = graphics_context_canvas_.CgContext();

  return cg_context;
}

IntRect LocalCurrentGraphicsContext::InflateRectForAA(const IntRect& rect) {
  const int kMargin = 2;
  return IntRect(rect.X() - kMargin, rect.Y() - kMargin,
                 rect.Width() + 2 * kMargin, rect.Height() + 2 * kMargin);
}
}
