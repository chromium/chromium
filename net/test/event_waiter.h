// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EVENT_WAITER_H_
#define NET_TEST_EVENT_WAITER_H_

#include "base/run_loop.h"

namespace net {

// Helper class to run a RunLoop until an expected event is reported.
template <typename Event>
class EventWaiter {
 public:
  // Runs a RunLoop until NotifyEvent() is called with |event|.
  void WaitForEvent(Event event) {
    expected_event_ = event;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Unblocks a WaitForEvent() call if it was called with |event|. Otherwise,
  // has no effect.
  void NotifyEvent(Event event) {
    if (!quit_closure_.is_null() && event == expected_event_)
      std::move(quit_closure_).Run();
  }

 private:
  Event expected_event_;
  base::OnceClosure quit_closure_;
};

}  // namespace net

#endif  // NET_TEST_EVENT_WAITER_H_
