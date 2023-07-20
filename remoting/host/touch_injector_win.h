// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TOUCH_INJECTOR_WIN_H_
#define REMOTING_HOST_TOUCH_INJECTOR_WIN_H_

#include <windows.h>

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/scoped_native_library.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace remoting {

namespace protocol {

class TouchEvent;

}  // namespace protocol

// This class calls InitializeTouchInjection() and InjectTouchInput() functions.
// The methods are virtual for mocking.
class TouchInjectorWinDelegate {
 public:
  TouchInjectorWinDelegate(const TouchInjectorWinDelegate&) = delete;
  TouchInjectorWinDelegate& operator=(const TouchInjectorWinDelegate&) = delete;

  virtual ~TouchInjectorWinDelegate();

  // Determines whether Windows touch injection functions can be used.
  // Returns a non-null TouchInjectorWinDelegate on success.
  static std::unique_ptr<TouchInjectorWinDelegate> Create();

  // These match the functions in MSDN.
  virtual BOOL InitializeTouchInjection(UINT32 max_count, DWORD dw_mode);
  virtual DWORD InjectTouchInput(UINT32 count,
                                 const POINTER_TOUCH_INFO* contacts);

 protected:
  // Ctor in protected scope for mocking.
  // This object takes ownership of the |library|.
  TouchInjectorWinDelegate(
      base::NativeLibrary library,
      BOOL(NTAPI* initialize_touch_injection_func)(UINT32, DWORD),
      BOOL(NTAPI* inject_touch_input_func)(UINT32, const POINTER_TOUCH_INFO*));

 private:
  base::ScopedNativeLibrary library_module_;

  // Pointers to Windows touch injection functions.
  BOOL(NTAPI* initialize_touch_injection_func_)(UINT32, DWORD);
  BOOL(NTAPI* inject_touch_input_func_)(UINT32, const POINTER_TOUCH_INFO*);
};

// This class converts TouchEvent objects to POINTER_TOUCH_INFO so that it can
// be injected using the Windows touch injection API, and calls the injection
// functions.
// This class expects good inputs and does not sanity check the inputs.
// This class just converts the object and hands it off to the Windows API.
class TouchInjectorWin {
 public:
  // Interval that we attempt to reinject currently active touch points to keep
  // them alive. The actual interval might be somewhere within
  // [kKeepAliveInterval, 2 * kKeepAliveInterval - 1]. The value is chosen
  // somewhat arbitrarily, but it works well based on observations (timeout on
  // Windows is about a second).
  static constexpr base::TimeDelta kKeepAliveInterval = base::Milliseconds(100);

  TouchInjectorWin();

  TouchInjectorWin(const TouchInjectorWin&) = delete;
  TouchInjectorWin& operator=(const TouchInjectorWin&) = delete;

  ~TouchInjectorWin();

  // Returns false if initialization of touch injection APIs fails.
  bool Init();

  // Deinitializes the object so that it can be reinitialized.
  void Deinitialize();

  // Inject touch events.
  void InjectTouchEvent(const protocol::TouchEvent& event);

  void SetInjectorDelegateForTest(
      std::unique_ptr<TouchInjectorWinDelegate> functions);

 private:
  // Helper methods called from InjectTouchEvent().
  // These helpers adapt Chromoting touch events, which convey changes to touch
  // points, to Windows touch descriptions, which must include descriptions for
  // all currently-active touch points, not just the changed ones.
  void AddNewTouchPoints(const protocol::TouchEvent& event);
  void MoveTouchPoints(const protocol::TouchEvent& event);
  void EndTouchPoints(const protocol::TouchEvent& event);
  void CancelTouchPoints(const protocol::TouchEvent& event);

  bool InjectTouchInput(const std::vector<POINTER_TOUCH_INFO>& touches);

  void UpdateKeepAliveTimer();

  // Periodically reinjects active touch points to keep them "alive". Some
  // clients won't send touch move events for press-and-hold gestures. If
  // Windows doesn't see a touch point within ~1s, it will end the touch point.
  void OnKeepAlive();

  // Set to null if touch injection is not available from the OS.
  std::unique_ptr<TouchInjectorWinDelegate> delegate_;

  // TODO(rkuroiwa): crbug.com/470203
  // This is a naive implementation. Check if we can achieve
  // better performance by reducing the number of copies.
  // To reduce the number of copies, we can have a vector of
  // POINTER_TOUCH_INFO and a map from touch ID to index in the vector.
  // When removing points from the vector, just swap it with the last element
  // and resize the vector.
  // All the POINTER_TOUCH_INFOs are stored as "move" points.
  std::map<uint32_t, POINTER_TOUCH_INFO> touches_in_contact_;

  // Since all active touches are re-injected, we don't need to store the
  // timestamp per touch point.
  base::TimeTicks last_injected_time_;

  base::RepeatingTimer keep_alive_timer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_TOUCH_INJECTOR_WIN_H_
