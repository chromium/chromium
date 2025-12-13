// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_TRANSFORM_H_
#define PDF_PDF_INK_TRANSFORM_H_

#include "pdf/buildflags.h"
#include "pdf/page_orientation.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace gfx {
class Size;
class SizeF;
class Vector2dF;
}  // namespace gfx

namespace ink {
class Envelope;
}  // namespace ink

namespace chrome_pdf {

enum class PageRotation;

// Generates the transform for converting a screen-based event input position
// into a page-based CSS pixels position. The CSS pixels position serves as a
// canonical format, where it is relative to the upper-left corner of a page for
// its original orientation at a scale factor of 100%.
//
// - `orientation`:
//     Current orientation of the page.
// - `page_content_rect`:
//     Scaled and rotated CSS coordinates of the page content area.  The amount
//     of scale and rotation match that of `orientation` and `scale_factor`.
//     The area's origin has the same offset from a viewport origin as the event
//     positions the transform will operate on.  Must not be empty.
// - `scale_factor`:
//     The current zoom factor, with 1.0 representing identity.  Must be greater
//     than zero.  This is used to ensure the resulting point is relative to a
//     scale factor of 100%.
//
// The returned transform should be used with event positions from Blink, which
// are screen-based coordinates that already have had any offset from a viewport
// origin to the page origin applied to it.
gfx::Transform GetEventToCanonicalTransform(PageOrientation orientation,
                                            const gfx::Rect& page_content_rect,
                                            float scale_factor);

// Generates the affine transformation for rendering a page's strokes to the
// screen, based on the page and its position within the viewport.
// - `viewport_origin_offset`:
//     The offset within the rendering viewport to where the page images will
//     be drawn.  Since the offset is a location within the viewport, it must
//     always contain non-negative values.  Values are scaled CSS coordinates,
//     where the amount of scaling matches that of `scale_factor`.
//
//     The X value in the offset repesents an unused area in the viewport to
//     the left of the pages, where no page pixels will be drawn.  This can
//     happen when the viewport is wider than the width of the rendered pages
//     and the pages are centered within the viewport.
//     The Y value in the offset similarly represents an unused area at the
//     top of the viewport where no page pixels would be rendered.
//
//     If the document scrolls vertically, then centering pages horizontally
//     within the viewport would lead to an offset whose X value is between
//     zero and less than half the viewport width.  The Y-offset value is
//     likely zero or a very small number for any viewport boundary padding.
//     If the document scrolls horizontally, then the reasoning of expected X
//     and Y values for the offset would be reversed.
//
//     Conceptually, the viewport origin offset is at X in this diagram, for a
//     document whose pages scroll vertically and a viewport that doesn't
//     bother with any vertical padding:
//
//                       +-------------+ +------------+         ^   scroll
//                       | page N      | | page N+1   |        /|\  direction
//                       |             | |            |         |
//                       |             | |            |         |
//                       |             | |            |
//     +-----------------X-------------+-+------------+-----------------+
//     | viewport        |             | |            |                 |
//     |                 |             | |            |                 |
//     |                 +------------ + +------------+                 |
//     |                                                                |
//     |                 +------------ + +------------+                 |
//     |                 | page N+2    | | page N+3   |                 |
//     |                 |             | |            |                 |
//     |                 |             | |            |                 |
//     |                 |             | |            |                 |
//     |                 |             | |            |                 |
//     +-----------------+-------------+-+------------+-----------------+
//                       |             | |            +
//                       +-------------+ +------------+
//
// - `orientation`:
//     Same as for `GetEventToCanonicalTransform()`.
// - `page_content_rect`:
//     Same as for `GetEventToCanonicalTransform()`.
// - `page_size_in_points`:
//     The size of the page in points for the PDF document.  I.e., no scaling
//     or orientation changes are applied to this size.
//
ink::AffineTransform GetInkRenderTransform(
    const gfx::Vector2dF& viewport_origin_offset,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    const gfx::SizeF& page_size_in_points);

// Returns the transform used when rendering a thumbnail on a canvas of
// `canvas_size`, given the other parameters. Compared to
// GetInkRenderTransform(), the transformation is simpler because there is no
// origin offset, and the thumbnail canvas is never rotated. Note that the
// thumbnail content may be rotated.
ink::AffineTransform GetInkThumbnailTransform(
    const gfx::Size& canvas_size,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    float scale_factor);

// Converts `ink::Envelope` to screen coordinates as needed for invalidation
// using `transform`.
// - The caller must provide a non-empty `envelope`.
// - `transform` should be the inverse of GetEventToCanonicalTransform().
gfx::Rect CanonicalInkEnvelopeToInvalidationScreenRect(
    const ink::Envelope& envelope,
    const gfx::Transform& transform);

// Returns a transform that converts from canonical coordinates (which has a
// top-left origin and a different DPI), to PDF coordinates (which has a
// bottom-left origin).  The translation accounts for any difference from the
// defined physical page size to the cropped, visible portion of the PDF page.
//
// - `page_size` is in points. It must not contain negative values.
// - `page_rotation` is the rotation of the page, as specified in the PDF.
//   Note that this is different from the user-chosen orientation in the viewer.
// - `translate` is in points.
gfx::Transform GetCanonicalToPdfTransform(const gfx::SizeF& page_size,
                                          PageRotation page_rotation,
                                          const gfx::Vector2dF& translate);

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_TRANSFORM_H_
