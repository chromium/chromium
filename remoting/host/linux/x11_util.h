// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_X11_UTIL_H_
#define REMOTING_HOST_LINUX_X11_UTIL_H_

// Xlib.h (via ui/gfx/x/x11.h) defines XErrorEvent as an anonymous
// struct, so we can't forward- declare it in this header. Since
// Xlib.h is not generally something you should #include into
// arbitrary code, please refrain from #including this header in
// another header.

#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/connection.h"

namespace remoting {

// Grab/release the X server within a scope. This can help avoid race
// conditions that would otherwise lead to X errors.
class ScopedXGrabServer {
 public:
  explicit ScopedXGrabServer(x11::Connection* connection);

  ScopedXGrabServer(const ScopedXGrabServer&) = delete;
  ScopedXGrabServer& operator=(const ScopedXGrabServer&) = delete;

  ~ScopedXGrabServer();

 private:
  raw_ptr<x11::Connection> connection_;
};

// Make a connection to the X Server impervious to X Server grabs. Returns
// true if successful or false if the required XTEST extension is not present.
bool IgnoreXServerGrabs(x11::Connection* connection, bool ignore);

// Returns whether the host is running under a virtual session.
bool IsVirtualSession(x11::Connection* connection);

// Returns whether the video dummy driver is being used (all outputs are
// DUMMY*).
bool IsUsingVideoDummyDriver(x11::Connection* connection);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X11_UTIL_H_
