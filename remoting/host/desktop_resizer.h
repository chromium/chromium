// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_H_
#define REMOTING_HOST_DESKTOP_RESIZER_H_

#include <list>
#include <memory>

#include "remoting/host/base/screen_resolution.h"

namespace remoting {

class DesktopResizer {
 public:
  virtual ~DesktopResizer() {}

  // Create a platform-specific DesktopResizer instance.
  static std::unique_ptr<DesktopResizer> Create();

  // Return the current resolution of the desktop.
  virtual ScreenResolution GetCurrentResolution() = 0;

  // Get the list of supported resolutions, which should ideally include
  // |preferred|. Implementations will generally do one of the following:
  //   1. Return the list of resolutions supported by the underlying video
  //      driver, regardless of |preferred|.
  //   2. Return a list containing just |preferred|, perhaps after imposing
  //      some minimum size constraint. This will typically be the case if
  //      there are no constraints imposed by the underlying video driver.
  //   3. Return an empty list if resize is not supported.
  virtual std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) = 0;

  // Set the resolution of the desktop. |resolution| must be one of the
  // resolutions previously returned by |GetSupportedResolutions|. Note that
  // implementations should fail gracefully if the specified resolution is no
  // longer supported, since monitor configurations may change on the fly.
  virtual void SetResolution(const ScreenResolution& resolution) = 0;

  // Restore the original desktop resolution. The caller must provide the
  // original resolution of the desktop, as returned by |GetCurrentResolution|,
  // as a hint. However, implementations are free to ignore this. For example,
  // virtual hosts will typically ignore it to avoid unnecessary resizes.
  virtual void RestoreResolution(const ScreenResolution& original) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_H_
