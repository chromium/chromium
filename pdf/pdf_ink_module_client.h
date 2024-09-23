// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_MODULE_CLIENT_H_
#define PDF_PDF_INK_MODULE_CLIENT_H_

#include "pdf/buildflags.h"
#include "pdf/page_orientation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace gfx {
class PointF;
}

namespace chrome_pdf {

class PdfInkModuleClient {
 public:
  virtual ~PdfInkModuleClient() = default;

  // Gets the current page orientation.
  virtual PageOrientation GetOrientation() const = 0;

  // Gets the current scaled and rotated rectangle area of the page in CSS
  // screen coordinates for the 0-based page index.  Must be non-empty for any
  // non-negative index returned from `VisiblePageIndexFromPoint()`.
  virtual gfx::Rect GetPageContentsRect(int index) = 0;

  // Gets the offset within the rendering viewport to where the page images
  // will be drawn.  Since the offset is a location within the viewport, it
  // must always contain non-negative values.  Values are in scaled CSS
  // screen coordinates, where the amount of scaling matches that of
  // `GetZoom()`.  The page orientation does not apply to the viewport.
  virtual gfx::Vector2dF GetViewportOriginOffset() = 0;

  // Gets current zoom factor.
  virtual float GetZoom() const = 0;

  // Notifies the client to invalidate the `rect`.  Coordinates are
  // screen-based, based on the same viewport origin that was used to specify
  // the `blink::WebMouseEvent` positions during stroking.
  virtual void Invalidate(const gfx::Rect& rect) {}

  // Returns whether the page at `page_index` is visible or not.
  virtual bool IsPageVisible(int page_index) = 0;

  // Notifies the client whether annotation mode is enabled or not.
  virtual void OnAnnotationModeToggled(bool enable) {}

  // Notifies the client that a stroke has finished drawing or erasing.
  virtual void StrokeFinished() {}

  // Asks the client to change the cursor to `bitmap`.
  virtual void UpdateInkCursorImage(SkBitmap bitmap) {}

  // Asks the client to update the thumbnail for `page_index`.
  virtual void UpdateThumbnail(int page_index) {}

  // Returns the 0-based page index for the given `point` if it is on a
  // visible page, or -1 if `point` is not on a visible page.
  virtual int VisiblePageIndexFromPoint(const gfx::PointF& point) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_MODULE_CLIENT_H_
