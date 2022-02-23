// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for an object that receives cursor shape events.

#ifndef REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_
#define REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_

namespace remoting {
namespace protocol {

class CursorShapeInfo;

class CursorShapeStub {
 public:
  CursorShapeStub() {}

  CursorShapeStub(const CursorShapeStub&) = delete;
  CursorShapeStub& operator=(const CursorShapeStub&) = delete;

  virtual ~CursorShapeStub() {}

  virtual void SetCursorShape(const CursorShapeInfo& cursor_shape) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_
