// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a signal handler that is used to safely handle SIGHUP
// and trigger the specified callback. It is used on Linux and Mac in order to
// reload the me2me host configuration.

#ifndef REMOTING_HOST_POSIX_SIGNAL_HANDLER_H_
#define REMOTING_HOST_POSIX_SIGNAL_HANDLER_H_

#include "base/functional/callback_forward.h"

namespace remoting {

typedef base::RepeatingCallback<void(int)> SignalHandler;

// Register for signal notifications on the current thread, which must have
// an associated MessageLoopForIO.  Multiple calls to RegisterSignalHandler
// must all be made on the same thread.
bool RegisterSignalHandler(int signal_number, const SignalHandler& handler);

}  // namespace remoting

#endif  // REMOTING_HOST_POSIX_SIGNAL_HANDLER_H_
