// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_SCREEN_CONTROLS_H_
#define REMOTING_HOST_BASE_SCREEN_CONTROLS_H_

namespace remoting {

class ScreenResolution;

// Used to change the screen resolution (both dimensions and DPI).
class ScreenControls {
 public:
  virtual ~ScreenControls() = default;

  // If |resolution| is not empty, attempts to set new screen resolution in the
  // session. If |resolution| is empty, attempts to restore the original screen
  // resolution.
  virtual void SetScreenResolution(const ScreenResolution& resolution) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_SCREEN_CONTROLS_H_
