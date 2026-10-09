// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_INJECTOR_MAC_H_
#define REMOTING_HOST_INPUT_INJECTOR_MAC_H_

#include <stdint.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "remoting/host/input_injector.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class Clipboard;

// A class to generate events on Mac.
//
// All InputInjector methods run on the caller sequence (`sequence_checker_`).
// CoreGraphics input calls are posted to `ui_thread_task_runner_`, and
// `clipboard_` is bound to `input_thread_task_runner`.
class InputInjectorMac : public InputInjector {
 public:
  InputInjectorMac(
      scoped_refptr<base::SingleThreadTaskRunner> input_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

  InputInjectorMac(const InputInjectorMac&) = delete;
  InputInjectorMac& operator=(const InputInjectorMac&) = delete;

  ~InputInjectorMac() override;

  // ClipboardStub interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // InputStub interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

  // InputInjector interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

 private:
  void WakeUpDisplay();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;
  base::SequenceBound<std::unique_ptr<Clipboard>> clipboard_
      GUARDED_BY_CONTEXT(sequence_checker_);
  webrtc::DesktopVector mouse_pos_ GUARDED_BY_CONTEXT(sequence_checker_);
  uint32_t mouse_button_state_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  uint64_t left_modifiers_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  uint64_t right_modifiers_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::TimeTicks last_time_display_woken_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_INJECTOR_MAC_H_
