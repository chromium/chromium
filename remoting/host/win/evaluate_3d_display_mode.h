// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_EVALUATE_3D_DISPLAY_MODE_H_
#define REMOTING_HOST_WIN_EVALUATE_3D_DISPLAY_MODE_H_

namespace remoting {

// Evaluates the Stereoscopic 3D capability of the system.
// When this mode is enabled and we are in curtain mode on Windows, all DX
// CreateDevice calls take several seconds longer than usual.  The result is
// that the connection may time out or be unusable so we want to query this
// setting to determine if we should just use the GDI capturer instead.
// DO NOT call this method within the host process.
int Evaluate3dDisplayMode();

// Returns whether 3D Display Mode (a.k.a. Stereoscopic 3D) is enabled.
// Note: This is an expensive call as it creates a new process and blocks on it.
bool Get3dDisplayModeEnabled();

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_EVALUATE_3D_DISPLAY_MODE_H_
