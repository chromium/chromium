// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_NORMALIZING_INPUT_FILTER_WIN_H_
#define REMOTING_CLIENT_INPUT_NORMALIZING_INPUT_FILTER_WIN_H_

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/input_filter.h"

namespace remoting {

// NormalizingInputFilterWin works around a quirk in Windows' handling of the
// AltGr key.  When using a layout that treats RightAlt as AltGr, Windows
// generates the following for an AltGr keydown/keyup cycle:
//
//   keydown LeftControl
//   keydown RightAlt
//   keyup LeftControl
//   keyup RightAlt
//
// In a layout without AltGr the key will generate only RightAlt events.
//
// This filter captures LeftControl keydown events and defers them until
// some other event occurs; if the next event is a RightAlt keydown then
// the LeftControl is ignored and neither keydown nor keyup will be sent
// for it.
class NormalizingInputFilterWin : public protocol::InputFilter {
 public:
  explicit NormalizingInputFilterWin(protocol::InputStub* input_stub);

  NormalizingInputFilterWin(const NormalizingInputFilterWin&) = delete;
  NormalizingInputFilterWin& operator=(const NormalizingInputFilterWin&) =
      delete;

  ~NormalizingInputFilterWin() override;

  // InputFilter interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;

 private:
  void ProcessKeyDown(const protocol::KeyEvent& event);
  void ProcessKeyUp(const protocol::KeyEvent& event);

  // Sends the |deferred_control_keydown_|, if any, and clears it.
  void FlushDeferredKeydownEvent();

  // Holds the keydown event for LeftControl immediately after it has been
  // pressed, until we have determined whether it is part of AltGr.
  protocol::KeyEvent deferred_control_keydown_;

  // True if LeftControl is pressed and treated as LeftControl.
  bool left_control_is_pressed_ = false;

  // True if LeftControl is pressed as part of AltGr.
  bool altgr_is_pressed_ = false;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_INPUT_NORMALIZING_INPUT_FILTER_WIN_H_
