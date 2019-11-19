// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_SETUP_H_
#define PRINTING_PAGE_SETUP_H_

#include "printing/printing_export.h"
#include "ui/gfx/geometry/rect.h"

namespace printing {

// Margins for a page setup.
class PRINTING_EXPORT PageMargins {
 public:
  PageMargins();

  void Clear();

  // Equality operator.
  bool Equals(const PageMargins& rhs) const;

  // Vertical space for the overlay from the top of the sheet.
  int header;
  // Vertical space for the overlay from the bottom of the sheet.
  int footer;
  // Margin on each side of the sheet.
  int left;
  int right;
  int top;
  int bottom;
};

// Settings that define the size and printable areas of a page. Unit is
// unspecified.
class PRINTING_EXPORT PageSetup {
 public:
  PageSetup();
  PageSetup(const PageSetup& other);
  ~PageSetup();

  // Gets a symmetrical printable area.
  static gfx::Rect GetSymmetricalPrintableArea(const gfx::Size& page_size,
                                               const gfx::Rect& printable_area);

  void Clear();

  // Equality operator.
  bool Equals(const PageSetup& rhs) const;

  void Init(const gfx::Size& physical_size,
            const gfx::Rect& printable_area,
            int text_height);

  // Use |requested_margins| as long as they fall inside the printable area.
  void SetRequestedMargins(const PageMargins& requested_margins);

  // Ignore the printable area, and set the margins to |requested_margins|.
  void ForceRequestedMargins(const PageMargins& requested_margins);

  // Flips the orientation of the page and recalculates all page areas.
  void FlipOrientation();

  const gfx::Size& physical_size() const { return physical_size_; }
  const gfx::Rect& overlay_area() const { return overlay_area_; }
  const gfx::Rect& content_area() const { return content_area_; }
  const gfx::Rect& printable_area() const { return printable_area_; }
  const PageMargins& effective_margins() const { return effective_margins_; }

 private:
  // Store |requested_margins_| and update page setup values.
  void SetRequestedMarginsAndCalculateSizes(
      const PageMargins& requested_margins);

  // Calculate overlay_area_, effective_margins_, and content_area_, based on
  // a constraint of |bounds| and |text_height|.
  void CalculateSizesWithinRect(const gfx::Rect& bounds, int text_height);

  // Physical size of the page, including non-printable margins.
  gfx::Size physical_size_;

  // The printable area as specified by the printer driver. We can't get
  // larger than this.
  gfx::Rect printable_area_;

  // The printable area for headers and footers.
  gfx::Rect overlay_area_;

  // The printable area as selected by the user's margins.
  gfx::Rect content_area_;

  // Effective margins.
  PageMargins effective_margins_;

  // Requested margins.
  PageMargins requested_margins_;

  // True when |effective_margins_| respects |printable_area_| else false.
  bool forced_margins_;

  // Space that must be kept free for the overlays.
  int text_height_;
};

}  // namespace printing

#endif  // PRINTING_PAGE_SETUP_H_
