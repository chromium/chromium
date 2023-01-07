// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_CORE_H_
#define PPAPI_CPP_CORE_H_

#include <stdint.h>

#include "ppapi/c/ppb_core.h"

/// @file
/// This file defines APIs related to memory management, time, and threads.

namespace pp {

class CompletionCallback;
class Module;

/// APIs related to memory management, time, and threads.
class Core {
 public:
  // Note that we explicitly don't expose Resource& versions of this function
  // since Resource will normally manage the refcount properly. These should
  // be called only when doing manual management on raw PP_Resource handles,
  // which should be fairly rare.

  /// AddRefResource() increments the reference count for the provided
  /// <code>resource</code>.
  ///
  /// @param[in] resource A <code>PP_Resource</code> corresponding to a
  /// resource.
  void AddRefResource(PP_Resource resource) {
    interface_->AddRefResource(resource);
  }

  /// ReleaseResource() decrements the reference count for the provided
  /// <code>resource</code>. The resource will be deallocated if the
  /// reference count reaches zero.
  ///
  /// @param[in] resource A <code>PP_Resource</code> corresponding to a
  /// resource.
  void ReleaseResource(PP_Resource resource) {
    interface_->ReleaseResource(resource);
  }

  /// GetTime() returns the "wall clock time" according to the
  /// browser.
  ///
  /// @return A <code>PP_Time</code> containing the "wall clock time" according
  /// to the browser.
  PP_Time GetTime() {
    return interface_->GetTime();
  }

  /// GetTimeTicks() returns the "tick time" according to the browser.
  /// This clock is used by the browser when passing some event times to the
  /// module (for example, using the
  /// <code>PP_InputEvent::time_stamp_seconds</code> field). It is not
  /// correlated to any actual wall clock time (like GetTime()). Because
  /// of this, it will not change if the user changes their computer clock.
  ///
  /// @return A <code>PP_TimeTicks</code> containing the "tick time" according
  /// to the browser.
  PP_TimeTicks GetTimeTicks() {
    return interface_->GetTimeTicks();
  }

  /// CallOnMainThread() schedules work to be executed on the main pepper
  /// thread after the specified delay. The delay may be 0 to specify a call
  /// back as soon as possible.
  ///
  /// The |result| parameter will just be passed as the second argument to the
  /// callback. Many applications won't need this, but it allows a module to
  /// emulate calls of some callbacks which do use this value.
  ///
  /// <strong>Note:</strong> CallOnMainThread(), even when used from the main
  /// thread with a delay of 0 milliseconds, will never directly invoke the
  /// callback.  Even in this case, the callback will be scheduled
  /// asynchronously.
  ///
  /// <strong>Note:</strong> If the browser is shutting down or if the module
  /// has no instances, then the callback function may not be called.
  ///
  /// @param[in] delay_in_milliseconds An int32_t delay in milliseconds.
  /// @param[in] callback A <code>CompletionCallback</code> callback function
  /// that the browser will call after the specified delay.
  /// @param[in] result An int32_t that the browser will pass to the given
  /// <code>CompletionCallback</code>.
  void CallOnMainThread(int32_t delay_in_milliseconds,
                        const CompletionCallback& callback,
                        int32_t result = 0);


  /// IsMainThread() returns true if the current thread is the main pepper
  /// thread.
  ///
  /// This function is useful for implementing sanity checks, and deciding if
  /// dispatching using CallOnMainThread() is required.
  ///
  /// @return true if the current thread is the main pepper thread, otherwise
  /// false.
  bool IsMainThread();

 private:
  // Allow Module to construct.
  friend class Module;

  // Only module should make this class so this constructor is private.
  Core(const PPB_Core* inter) : interface_(inter) {}

  // Copy and assignment are disallowed.
  Core(const Core& other);
  Core& operator=(const Core& other);

  const PPB_Core* interface_;
};

}  // namespace pp

#endif  // PPAPI_CPP_CORE_H_
