// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POINTER_LOCK_DETECTOR_H_
#define REMOTING_HOST_POINTER_LOCK_DETECTOR_H_

#include "remoting/protocol/input_filter.h"

namespace remoting {

// Non-filtering InputStub implementation which detects changes into or out of
// relative pointer mode, which corresponds to client-side pointer lock.
class PointerLockDetector : public protocol::InputFilter {
 public:
  class EventHandler {
   public:
    virtual void OnPointerLockChanged(bool active) = 0;
  };

  PointerLockDetector(InputStub* input_stub, EventHandler* event_handler_);
  ~PointerLockDetector() override;

  // InputStub overrides.
  void InjectMouseEvent(const protocol::MouseEvent& event) override;

 private:
  EventHandler* event_handler_;
  bool has_triggered_ = false;
  bool is_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(PointerLockDetector);
};

}  // namespace remoting

#endif  // REMOTING_HOST_POINTER_LOCK_DETECTOR_H_
