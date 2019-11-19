// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_setup.h"

#include <algorithm>

#include "base/logging.h"

namespace printing {

namespace {

// Checks whether |printable_area| can be used to form a valid symmetrical
// printable area, so that margin_left equals margin_right, and margin_top
// equals margin_bottom.  For example if
// printable_area.x() * 2 >= page_size.width(), then the
// content_width = page_size.width() - 2 * printable_area.x() would be zero or
// negative, which is invalid.
// |page_size| is the physical page size that includes margins.
bool IsValidPrintableArea(const gfx::Size& page_size,
                          const gfx::Rect& printable_area) {
  return !printable_area.IsEmpty() && printable_area.x() >= 0 &&
         printable_area.y() >= 0 &&
         printable_area.right() <= page_size.width() &&
         printable_area.bottom() <= page_size.height() &&
         printable_area.x() * 2 < page_size.width() &&
         printable_area.y() * 2 < page_size.height() &&
         printable_area.right() * 2 > page_size.width() &&
         printable_area.bottom() * 2 > page_size.height();
}

}  // namespace

PageMargins::PageMargins()
    : header(0), footer(0), left(0), right(0), top(0), bottom(0) {}

void PageMargins::Clear() {
  header = 0;
  footer = 0;
  left = 0;
  right = 0;
  top = 0;
  bottom = 0;
}

bool PageMargins::Equals(const PageMargins& rhs) const {
  return header == rhs.header && footer == rhs.footer && left == rhs.left &&
         top == rhs.top && right == rhs.right && bottom == rhs.bottom;
}

PageSetup::PageSetup() {
  Clear();
}

PageSetup::PageSetup(const PageSetup& other) = default;

PageSetup::~PageSetup() = default;

// static
gfx::Rect PageSetup::GetSymmetricalPrintableArea(
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  if (!IsValidPrintableArea(page_size, printable_area))
    return gfx::Rect();

  int left_right_margin =
      std::max(printable_area.x(), page_size.width() - printable_area.right());
  int top_bottom_margin = std::max(
      printable_area.y(), page_size.height() - printable_area.bottom());
  int width = page_size.width() - 2 * left_right_margin;
  int height = page_size.height() - 2 * top_bottom_margin;

  gfx::Rect symmetrical_printable_area = gfx::Rect(page_size);
  symmetrical_printable_area.ClampToCenteredSize(gfx::Size(width, height));

  return symmetrical_printable_area;
}

void PageSetup::Clear() {
  physical_size_.SetSize(0, 0);
  printable_area_.SetRect(0, 0, 0, 0);
  overlay_area_.SetRect(0, 0, 0, 0);
  content_area_.SetRect(0, 0, 0, 0);
  effective_margins_.Clear();
  text_height_ = 0;
  forced_margins_ = false;
}

bool PageSetup::Equals(const PageSetup& rhs) const {
  return physical_size_ == rhs.physical_size_ &&
         printable_area_ == rhs.printable_area_ &&
         overlay_area_ == rhs.overlay_area_ &&
         content_area_ == rhs.content_area_ &&
         effective_margins_.Equals(rhs.effective_margins_) &&
         requested_margins_.Equals(rhs.requested_margins_) &&
         text_height_ == rhs.text_height_;
}

void PageSetup::Init(const gfx::Size& physical_size,
                     const gfx::Rect& printable_area,
                     int text_height) {
  DCHECK_LE(printable_area.right(), physical_size.width());
  // I've seen this assert triggers on Canon GP160PF PCL 5e and HP LaserJet 5.
  // Since we don't know the dpi here, just disable the check.
  // DCHECK_LE(printable_area.bottom(), physical_size.height());
  DCHECK_GE(printable_area.x(), 0);
  DCHECK_GE(printable_area.y(), 0);
  DCHECK_GE(text_height, 0);
  physical_size_ = physical_size;
  printable_area_ = printable_area;
  text_height_ = text_height;

  SetRequestedMarginsAndCalculateSizes(requested_margins_);
}

void PageSetup::SetRequestedMargins(const PageMargins& requested_margins) {
  forced_margins_ = false;
  SetRequestedMarginsAndCalculateSizes(requested_margins);
}

void PageSetup::ForceRequestedMargins(const PageMargins& requested_margins) {
  forced_margins_ = true;
  SetRequestedMarginsAndCalculateSizes(requested_margins);
}

void PageSetup::FlipOrientation() {
  if (physical_size_.width() && physical_size_.height()) {
    gfx::Size new_size(physical_size_.height(), physical_size_.width());
    int new_y = physical_size_.width() -
                (printable_area_.width() + printable_area_.x());
    gfx::Rect new_printable_area(printable_area_.y(), new_y,
                                 printable_area_.height(),
                                 printable_area_.width());
    Init(new_size, new_printable_area, text_height_);
  }
}

void PageSetup::SetRequestedMarginsAndCalculateSizes(
    const PageMargins& requested_margins) {
  requested_margins_ = requested_margins;
  if (physical_size_.width() && physical_size_.height()) {
    if (forced_margins_)
      CalculateSizesWithinRect(gfx::Rect(physical_size_), 0);
    else
      CalculateSizesWithinRect(printable_area_, text_height_);
  }
}

void PageSetup::CalculateSizesWithinRect(const gfx::Rect& bounds,
                                         int text_height) {
  // Calculate the effective margins. The tricky part.
  effective_margins_.header = std::max(requested_margins_.header, bounds.y());
  effective_margins_.footer = std::max(
      requested_margins_.footer, physical_size_.height() - bounds.bottom());
  effective_margins_.left = std::max(requested_margins_.left, bounds.x());
  effective_margins_.top = std::max({requested_margins_.top, bounds.y(),
                                     effective_margins_.header + text_height});
  effective_margins_.right = std::max(requested_margins_.right,
                                      physical_size_.width() - bounds.right());
  effective_margins_.bottom = std::max(
      {requested_margins_.bottom, physical_size_.height() - bounds.bottom(),
       effective_margins_.footer + text_height});

  // Calculate the overlay area. If the margins are excessive, the overlay_area
  // size will be (0, 0).
  overlay_area_.set_x(effective_margins_.left);
  overlay_area_.set_y(effective_margins_.header);
  overlay_area_.set_width(std::max(
      0,
      physical_size_.width() - effective_margins_.right - overlay_area_.x()));
  overlay_area_.set_height(std::max(
      0,
      physical_size_.height() - effective_margins_.footer - overlay_area_.y()));

  // Calculate the content area. If the margins are excessive, the content_area
  // size will be (0, 0).
  content_area_.set_x(effective_margins_.left);
  content_area_.set_y(effective_margins_.top);
  content_area_.set_width(std::max(
      0,
      physical_size_.width() - effective_margins_.right - content_area_.x()));
  content_area_.set_height(std::max(
      0,
      physical_size_.height() - effective_margins_.bottom - content_area_.y()));
}

}  // namespace printing
