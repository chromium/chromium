// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_EMPTY_CURSOR_FILTER_H_
#define REMOTING_CLIENT_EMPTY_CURSOR_FILTER_H_

#include "base/macros.h"
#include "remoting/protocol/cursor_shape_stub.h"

namespace remoting {

// Returns an empty cursor.
protocol::CursorShapeInfo EmptyCursorShape();

// Returns true of the supplied cursor is empty, i.e. width and/or height <= 0.
bool IsCursorShapeEmpty(const protocol::CursorShapeInfo& cursor_shape);

// Helper that checks whether a cursor has any visible pixels, and resizes
// it down to (0x0) if not.
// TODO(wez): Turn this into a general-purpose cropping filter that both
// client and host can use.
class EmptyCursorFilter : public protocol::CursorShapeStub {
 public:
  explicit EmptyCursorFilter(protocol::CursorShapeStub* cursor_stub);
  ~EmptyCursorFilter() override;

  // protocol::CursorShapeStub interface.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;

  // Replaces the stub to which cursor shapes will be passed-on.
  void set_cursor_stub(protocol::CursorShapeStub* cursor_stub) {
    cursor_stub_ = cursor_stub;
  }

 private:
  protocol::CursorShapeStub* cursor_stub_;

  DISALLOW_COPY_AND_ASSIGN(EmptyCursorFilter);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_EMPTY_CURSOR_FILTER_H_
