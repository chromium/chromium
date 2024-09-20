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

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/randr.h"

namespace remoting {

// Make a connection to the X Server impervious to X Server grabs. Returns
// true if successful or false if the required XTEST extension is not present.
bool IgnoreXServerGrabs(x11::Connection* connection, bool ignore);

// Returns whether the host is running under a virtual session.
bool IsVirtualSession(x11::Connection* connection);

// Returns whether the video dummy driver is being used (all outputs are
// DUMMY*).
bool IsUsingVideoDummyDriver(x11::Connection* connection);

// Returns the X11 Atom for the string, or x11::Atom::None if there was an
// error. Callers can assign the result to a static local variable to avoid
// repeated X11 round-trips.
x11::Atom GetX11Atom(x11::Connection* connection, const std::string& name);

// Sets the physical size of a RANDR Output device in mm. This is done by
// setting some RANDR properties which are supported by the xf86-video-dummy
// driver. The values are returned to the client in the X11 RRGetOutputInfo
// response.
void SetOutputPhysicalSizeInMM(x11::Connection* connection,
                               x11::RandR::Output output,
                               int width,
                               int height);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X11_UTIL_H_
