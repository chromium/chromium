/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_CURSOR_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef CopyCursor

namespace blink {

class PLATFORM_EXPORT Cursor {
  USING_FAST_MALLOC(Cursor);

 public:
  Cursor()
      // This is an invalid Cursor and should never actually get used.
      : type_(ui::CursorType::kNull) {}

  Cursor(Image*, bool hot_spot_specified, const IntPoint& hot_spot);

  // Hot spot is in image pixels.
  Cursor(Image*,
         bool hot_spot_specified,
         const IntPoint& hot_spot,
         float image_scale_factor);

  Cursor(const Cursor&);
  ~Cursor();
  Cursor& operator=(const Cursor&);

  explicit Cursor(ui::CursorType);
  ui::CursorType GetType() const {
    DCHECK_GE(type_, static_cast<ui::CursorType>(0));
    DCHECK_LE(type_, ui::CursorType::kCustom);
    return type_;
  }
  Image* GetImage() const { return image_.get(); }
  const IntPoint& HotSpot() const { return hot_spot_; }
  // Image scale in image pixels per logical (UI) pixel.
  float ImageScaleFactor() const { return image_scale_factor_; }

 private:
  ui::CursorType type_;
  scoped_refptr<Image> image_;
  IntPoint hot_spot_;
  float image_scale_factor_;
};

PLATFORM_EXPORT IntPoint DetermineHotSpot(Image*,
                                          bool hot_spot_specified,
                                          const IntPoint& specified_hot_spot);

PLATFORM_EXPORT const Cursor& PointerCursor();
PLATFORM_EXPORT const Cursor& CrossCursor();
PLATFORM_EXPORT const Cursor& HandCursor();
PLATFORM_EXPORT const Cursor& MoveCursor();
PLATFORM_EXPORT const Cursor& IBeamCursor();
PLATFORM_EXPORT const Cursor& WaitCursor();
PLATFORM_EXPORT const Cursor& HelpCursor();
PLATFORM_EXPORT const Cursor& EastResizeCursor();
PLATFORM_EXPORT const Cursor& NorthResizeCursor();
PLATFORM_EXPORT const Cursor& NorthEastResizeCursor();
PLATFORM_EXPORT const Cursor& NorthWestResizeCursor();
PLATFORM_EXPORT const Cursor& SouthResizeCursor();
PLATFORM_EXPORT const Cursor& SouthEastResizeCursor();
PLATFORM_EXPORT const Cursor& SouthWestResizeCursor();
PLATFORM_EXPORT const Cursor& WestResizeCursor();
PLATFORM_EXPORT const Cursor& NorthSouthResizeCursor();
PLATFORM_EXPORT const Cursor& EastWestResizeCursor();
PLATFORM_EXPORT const Cursor& NorthEastSouthWestResizeCursor();
PLATFORM_EXPORT const Cursor& NorthWestSouthEastResizeCursor();
PLATFORM_EXPORT const Cursor& ColumnResizeCursor();
PLATFORM_EXPORT const Cursor& RowResizeCursor();
PLATFORM_EXPORT const Cursor& MiddlePanningCursor();
PLATFORM_EXPORT const Cursor& MiddlePanningVerticalCursor();
PLATFORM_EXPORT const Cursor& MiddlePanningHorizontalCursor();
PLATFORM_EXPORT const Cursor& EastPanningCursor();
PLATFORM_EXPORT const Cursor& NorthPanningCursor();
PLATFORM_EXPORT const Cursor& NorthEastPanningCursor();
PLATFORM_EXPORT const Cursor& NorthWestPanningCursor();
PLATFORM_EXPORT const Cursor& SouthPanningCursor();
PLATFORM_EXPORT const Cursor& SouthEastPanningCursor();
PLATFORM_EXPORT const Cursor& SouthWestPanningCursor();
PLATFORM_EXPORT const Cursor& WestPanningCursor();
PLATFORM_EXPORT const Cursor& VerticalTextCursor();
PLATFORM_EXPORT const Cursor& CellCursor();
PLATFORM_EXPORT const Cursor& ContextMenuCursor();
PLATFORM_EXPORT const Cursor& NoDropCursor();
PLATFORM_EXPORT const Cursor& NotAllowedCursor();
PLATFORM_EXPORT const Cursor& ProgressCursor();
PLATFORM_EXPORT const Cursor& AliasCursor();
PLATFORM_EXPORT const Cursor& ZoomInCursor();
PLATFORM_EXPORT const Cursor& ZoomOutCursor();
PLATFORM_EXPORT const Cursor& CopyCursor();
PLATFORM_EXPORT const Cursor& NoneCursor();
PLATFORM_EXPORT const Cursor& GrabCursor();
PLATFORM_EXPORT const Cursor& GrabbingCursor();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_CURSOR_H_
