// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for an object that receives cursor shape events.

#ifndef REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_
#define REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_

namespace remoting::protocol {

class CursorShapeInfo;

class CursorShapeStub {
 public:
  CursorShapeStub() = default;

  CursorShapeStub(const CursorShapeStub&) = delete;
  CursorShapeStub& operator=(const CursorShapeStub&) = delete;

  virtual ~CursorShapeStub() = default;

  virtual void SetCursorShape(const CursorShapeInfo& cursor_shape) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_
