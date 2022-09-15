// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_NORMALIZING_INPUT_FILTER_MAC_H_
#define REMOTING_CLIENT_INPUT_NORMALIZING_INPUT_FILTER_MAC_H_

#include <map>

#include "remoting/protocol/input_filter.h"

namespace remoting {

// NormalizingInputFilterMac is designed to solve the problem of missing keyup
// events on Mac.
//
// PROBLEM
//
// On Mac if user presses CMD and then C key there is no keyup event generated
// for C when user releases the C key before the CMD key.
// The cause is that CMD + C triggers a system action and Chrome injects only a
// keydown event for the C key. Safari shares the same behavior.
//
// SOLUTION
//
// When a keyup event for CMD key happens we will check all prior keydown
// events received and inject corresponding keyup events artificially, with
// the exception of:
//
// SHIFT, CONTROL, OPTION, LEFT CMD, RIGHT CMD and CAPS LOCK
//
// because they are reported by Chrome correctly.
//
// There are a couple cases that this solution doesn't work perfectly, one
// of them leads to duplicated keyup events.
//
// User performs this sequence of actions:
//
// CMD DOWN, C DOWN, CMD UP, C UP
//
// In this case the algorithm will generate:
//
// CMD DOWN, C DOWN, C UP, CMD UP, C UP
//
// Because we artificially generate keyup events the C UP event is duplicated
// as user releases the key after CMD key. This would not be a problem as the
// receiver end will drop this duplicated keyup event.
class NormalizingInputFilterMac : public protocol::InputFilter {
 public:
  explicit NormalizingInputFilterMac(protocol::InputStub* input_stub);

  NormalizingInputFilterMac(const NormalizingInputFilterMac&) = delete;
  NormalizingInputFilterMac& operator=(const NormalizingInputFilterMac&) =
      delete;

  ~NormalizingInputFilterMac() override;

  // InputFilter overrides.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;

 private:
  typedef std::map<int, protocol::KeyEvent> KeyPressedMap;

  // Generate keyup events for any keys pressed with CMD.
  void GenerateKeyupEvents();

  // A map that stores pressed keycodes and the corresponding key event.
  KeyPressedMap key_pressed_map_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_INPUT_NORMALIZING_INPUT_FILTER_MAC_H_
