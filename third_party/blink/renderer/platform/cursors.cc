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

#include "third_party/blink/renderer/platform/cursors.h"

#include "base/no_destructor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef CopyCursor

namespace blink {

const ui::Cursor& PointerCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kPointer);
  return *c;
}

const ui::Cursor& CrossCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kCross);
  return *c;
}

const ui::Cursor& HandCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kHand);
  return *c;
}

const ui::Cursor& MoveCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kMove);
  return *c;
}

const ui::Cursor& VerticalTextCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kVerticalText);
  return *c;
}

const ui::Cursor& CellCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kCell);
  return *c;
}

const ui::Cursor& ContextMenuCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kContextMenu);
  return *c;
}

const ui::Cursor& AliasCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kAlias);
  return *c;
}

const ui::Cursor& ZoomInCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kZoomIn);
  return *c;
}

const ui::Cursor& ZoomOutCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kZoomOut);
  return *c;
}

const ui::Cursor& CopyCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kCopy);
  return *c;
}

const ui::Cursor& NoneCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kNone);
  return *c;
}

const ui::Cursor& ProgressCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kProgress);
  return *c;
}

const ui::Cursor& NoDropCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNoDrop);
  return *c;
}

const ui::Cursor& NotAllowedCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNotAllowed);
  return *c;
}

const ui::Cursor& IBeamCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kIBeam);
  return *c;
}

const ui::Cursor& WaitCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kWait);
  return *c;
}

const ui::Cursor& HelpCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kHelp);
  return *c;
}

const ui::Cursor& EastResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kEastResize);
  return *c;
}

const ui::Cursor& NorthResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthResize);
  return *c;
}

const ui::Cursor& NorthEastResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthEastResize);
  return *c;
}

const ui::Cursor& NorthWestResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthWestResize);
  return *c;
}

const ui::Cursor& SouthResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kSouthResize);
  return *c;
}

const ui::Cursor& SouthEastResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kSouthEastResize);
  return *c;
}

const ui::Cursor& SouthWestResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kSouthWestResize);
  return *c;
}

const ui::Cursor& WestResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kWestResize);
  return *c;
}

const ui::Cursor& NorthSouthResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthSouthResize);
  return *c;
}

const ui::Cursor& EastWestResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kEastWestResize);
  return *c;
}

const ui::Cursor& NorthEastSouthWestResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthEastSouthWestResize);
  return *c;
}

const ui::Cursor& NorthWestSouthEastResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthWestSouthEastResize);
  return *c;
}

const ui::Cursor& ColumnResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kColumnResize);
  return *c;
}

const ui::Cursor& RowResizeCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kRowResize);
  return *c;
}

const ui::Cursor& MiddlePanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kMiddlePanning);
  return *c;
}

const ui::Cursor& MiddlePanningVerticalCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kMiddlePanningVertical);
  return *c;
}

const ui::Cursor& MiddlePanningHorizontalCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kMiddlePanningHorizontal);
  return *c;
}

const ui::Cursor& EastPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kEastPanning);
  return *c;
}

const ui::Cursor& NorthPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthPanning);
  return *c;
}

const ui::Cursor& NorthEastPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthEastPanning);
  return *c;
}

const ui::Cursor& NorthWestPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kNorthWestPanning);
  return *c;
}

const ui::Cursor& SouthPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kSouthPanning);
  return *c;
}

const ui::Cursor& SouthEastPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kSouthEastPanning);
  return *c;
}

const ui::Cursor& SouthWestPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kSouthWestPanning);
  return *c;
}

const ui::Cursor& WestPanningCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kWestPanning);
  return *c;
}

const ui::Cursor& GrabCursor() {
  static base::NoDestructor<ui::Cursor> c(ui::mojom::blink::CursorType::kGrab);
  return *c;
}

const ui::Cursor& GrabbingCursor() {
  static base::NoDestructor<ui::Cursor> c(
      ui::mojom::blink::CursorType::kGrabbing);
  return *c;
}

}  // namespace blink
