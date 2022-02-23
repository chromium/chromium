// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for a device that receives input events.
// This interface handles input event messages defined in event.proto.

#ifndef REMOTING_PROTOCOL_INPUT_STUB_H_
#define REMOTING_PROTOCOL_INPUT_STUB_H_

namespace remoting {
namespace protocol {

class KeyEvent;
class TextEvent;
class MouseEvent;
class TouchEvent;

class InputStub {
 public:
  InputStub() {}

  InputStub(const InputStub&) = delete;
  InputStub& operator=(const InputStub&) = delete;

  virtual ~InputStub() {}

  // Implementations must never assume the presence of any |event| fields,
  // nor assume that their contents are valid.
  virtual void InjectKeyEvent(const KeyEvent& event) = 0;
  virtual void InjectTextEvent(const TextEvent& event) = 0;
  virtual void InjectMouseEvent(const MouseEvent& event) = 0;
  virtual void InjectTouchEvent(const TouchEvent& event) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_INPUT_STUB_H_
