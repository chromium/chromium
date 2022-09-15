// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MOUSE_LOCK_H_
#define PPAPI_CPP_MOUSE_LOCK_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/instance_handle.h"

/// @file
/// This file defines the API for locking the target of mouse events to a
/// specific module instance.

namespace pp {

class CompletionCallback;
class Instance;

/// This class allows you to associate the <code>PPP_MouseLock</code> and
/// <code>PPB_MouseLock</code> C-based interfaces with an object. It associates
/// itself with the given instance, and registers as the global handler for
/// handling the <code>PPP_MouseLock</code> interface that the browser calls.
///
/// You would typically use this class by inheritance on your instance or by
/// composition.
///
/// <strong>Example (inheritance):</strong>
/// @code
///   class MyInstance : public pp::Instance, public pp::MouseLock {
///     class MyInstance() : pp::MouseLock(this) {
///     }
///     ...
///   };
/// @endcode
///
/// <strong>Example (composition):</strong>
/// @code
///   class MyMouseLock : public pp::MouseLock {
///     ...
///   };
///
///   class MyInstance : public pp::Instance {
///     MyInstance() : mouse_lock_(this) {
///     }
///
///     MyMouseLock mouse_lock_;
///   };
/// @endcode
class MouseLock {
 public:
  /// A constructor for creating a <code>MouseLock</code>.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit MouseLock(Instance* instance);

  /// Destructor.
  virtual ~MouseLock();

  /// PPP_MouseLock functions exposed as virtual functions for you to override.
  virtual void MouseLockLost() = 0;

  /// LockMouse() requests the mouse to be locked.
  ///
  /// While the mouse is locked, the cursor is implicitly hidden from the user.
  /// Any movement of the mouse will generate a
  /// <code>PP_INPUTEVENT_TYPE_MOUSEMOVE</code> event. The
  /// <code>GetPosition()</code> function in <code>InputEvent()</code>
  /// reports the last known mouse position just as mouse lock was
  /// entered. The <code>GetMovement()</code> function provides relative
  /// movement information indicating what the change in position of the mouse
  /// would be had it not been locked.
  ///
  /// The browser may revoke the mouse lock for reasons including (but not
  /// limited to) the user pressing the ESC key, the user activating another
  /// program using a reserved keystroke (e.g. ALT+TAB), or some other system
  /// event.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t LockMouse(const CompletionCallback& cc);

  /// UnlockMouse causes the mouse to be unlocked, allowing it to track user
  /// movement again. This is an asynchronous operation. The module instance
  /// will be notified using the <code>PPP_MouseLock</code> interface when it
  /// has lost the mouse lock.
  void UnlockMouse();

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_MOUSE_LOCK_H_
