// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_TRANSFORM_H_
#define PDF_PDF_INK_TRANSFORM_H_

#include "pdf/buildflags.h"
#include "pdf/page_orientation.h"
#include "ui/gfx/geometry/point_f.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace gfx {
class Rect;
}  // namespace gfx

namespace chrome_pdf {

// Converts a screen-based event input position into a page-based CSS pixels
// position.  This canonical format is relative to the upper-left corner of a
// page for its original orientation at a scale factor of 100%.
// - `event_position`:
//     The input position, in screen-based coordinates.  Must already have had
//     any offset from a viewport origin to the page origin applied to it.
// - `orientation`:
//     Current orientation of the page.
// - `page_content_rect`:
//     Scaled and rotated CSS coordinates of the page content area.  The amount
//     of scale and rotation match that of `orientation` and `scale_factor`.
//     The area's origin has the same offset from a viewport origin as
//     `event_position`.  Must not be empty.
// - `scale_factor`:
//     The current zoom factor, with 1.0 representing identity.  Must be greater
//     than zero.  This is used to ensure the resulting point is relative to a
//     scale factor of 100%.
gfx::PointF EventPositionToCanonicalPosition(const gfx::PointF& event_position,
                                             PageOrientation orientation,
                                             const gfx::Rect& page_content_rect,
                                             float scale_factor);

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_TRANSFORM_H_
