/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/cursor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

IntPoint DetermineHotSpot(Image* image,
                          bool hot_spot_specified,
                          const IntPoint& specified_hot_spot) {
  if (image->IsNull())
    return IntPoint();

  IntRect image_rect = image->Rect();

  // Hot spot must be inside cursor rectangle.
  if (hot_spot_specified) {
    if (image_rect.Contains(specified_hot_spot)) {
      return specified_hot_spot;
    }

    return IntPoint(clampTo<int>(specified_hot_spot.X(), image_rect.X(),
                                 image_rect.MaxX() - 1),
                    clampTo<int>(specified_hot_spot.Y(), image_rect.Y(),
                                 image_rect.MaxY() - 1));
  }

  // If hot spot is not specified externally, it can be extracted from some
  // image formats (e.g. .cur).
  IntPoint intrinsic_hot_spot;
  bool image_has_intrinsic_hot_spot = image->GetHotSpot(intrinsic_hot_spot);
  if (image_has_intrinsic_hot_spot && image_rect.Contains(intrinsic_hot_spot))
    return intrinsic_hot_spot;

  // If neither is provided, use a default value of (0, 0).
  return IntPoint();
}

Cursor::Cursor(Image* image, bool hot_spot_specified, const IntPoint& hot_spot)
    : type_(ui::CursorType::kCustom),
      image_(image),
      hot_spot_(DetermineHotSpot(image, hot_spot_specified, hot_spot)),
      image_scale_factor_(1) {}

Cursor::Cursor(Image* image,
               bool hot_spot_specified,
               const IntPoint& hot_spot,
               float scale)
    : type_(ui::CursorType::kCustom),
      image_(image),
      hot_spot_(DetermineHotSpot(image, hot_spot_specified, hot_spot)),
      image_scale_factor_(scale) {}

Cursor::Cursor(ui::CursorType type) : type_(type), image_scale_factor_(1) {}

Cursor::Cursor(const Cursor& other) = default;

Cursor& Cursor::operator=(const Cursor& other) {
  type_ = other.type_;
  image_ = other.image_;
  hot_spot_ = other.hot_spot_;
  image_scale_factor_ = other.image_scale_factor_;
  return *this;
}

Cursor::~Cursor() = default;

const Cursor& PointerCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kPointer));
  return c;
}

const Cursor& CrossCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kCross));
  return c;
}

const Cursor& HandCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kHand));
  return c;
}

const Cursor& MoveCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kMove));
  return c;
}

const Cursor& VerticalTextCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kVerticalText));
  return c;
}

const Cursor& CellCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kCell));
  return c;
}

const Cursor& ContextMenuCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kContextMenu));
  return c;
}

const Cursor& AliasCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kAlias));
  return c;
}

const Cursor& ZoomInCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kZoomIn));
  return c;
}

const Cursor& ZoomOutCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kZoomOut));
  return c;
}

const Cursor& CopyCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kCopy));
  return c;
}

const Cursor& NoneCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNone));
  return c;
}

const Cursor& ProgressCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kProgress));
  return c;
}

const Cursor& NoDropCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNoDrop));
  return c;
}

const Cursor& NotAllowedCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNotAllowed));
  return c;
}

const Cursor& IBeamCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kIBeam));
  return c;
}

const Cursor& WaitCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kWait));
  return c;
}

const Cursor& HelpCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kHelp));
  return c;
}

const Cursor& EastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kEastResize));
  return c;
}

const Cursor& NorthResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthResize));
  return c;
}

const Cursor& NorthEastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthEastResize));
  return c;
}

const Cursor& NorthWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthWestResize));
  return c;
}

const Cursor& SouthResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kSouthResize));
  return c;
}

const Cursor& SouthEastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kSouthEastResize));
  return c;
}

const Cursor& SouthWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kSouthWestResize));
  return c;
}

const Cursor& WestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kWestResize));
  return c;
}

const Cursor& NorthSouthResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthSouthResize));
  return c;
}

const Cursor& EastWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kEastWestResize));
  return c;
}

const Cursor& NorthEastSouthWestResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthEastSouthWestResize));
  return c;
}

const Cursor& NorthWestSouthEastResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthWestSouthEastResize));
  return c;
}

const Cursor& ColumnResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kColumnResize));
  return c;
}

const Cursor& RowResizeCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kRowResize));
  return c;
}

const Cursor& MiddlePanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kMiddlePanning));
  return c;
}

const Cursor& MiddlePanningVerticalCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kMiddlePanningVertical));
  return c;
}

const Cursor& MiddlePanningHorizontalCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kMiddlePanningHorizontal));
  return c;
}

const Cursor& EastPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kEastPanning));
  return c;
}

const Cursor& NorthPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthPanning));
  return c;
}

const Cursor& NorthEastPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthEastPanning));
  return c;
}

const Cursor& NorthWestPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kNorthWestPanning));
  return c;
}

const Cursor& SouthPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kSouthPanning));
  return c;
}

const Cursor& SouthEastPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kSouthEastPanning));
  return c;
}

const Cursor& SouthWestPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kSouthWestPanning));
  return c;
}

const Cursor& WestPanningCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kWestPanning));
  return c;
}

const Cursor& GrabCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kGrab));
  return c;
}

const Cursor& GrabbingCursor() {
  DEFINE_STATIC_LOCAL(Cursor, c, (ui::CursorType::kGrabbing));
  return c;
}

}  // namespace blink
