// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_XR_INSTALL_HELPER_H_
#define CONTENT_PUBLIC_BROWSER_XR_INSTALL_HELPER_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

// Interface class to provide the opportunity for runtimes to ensure that any
// necessary installation steps that need to occur from within the browser
// process are kicked off. This is acquired via the |XrInstallHelperFactory|.
// Generally, these steps are specific per runtime, so likely this should be
// implemented for each runtime that has browser-specific installation steps.
// This should be implemented by embedders.
class CONTENT_EXPORT XrInstallHelper {
 public:
  virtual ~XrInstallHelper() = default;
  XrInstallHelper(const XrInstallHelper&) = delete;
  XrInstallHelper& operator=(const XrInstallHelper&) = delete;

  // Triggers checks and appropriate installation requests for any runtime
  // dependencies that need to be installed from the browser process.
  // render_process_id and render_frame_id are passed in case any tab specific
  // UI needs to be shown.
  // The callback should be guaranteed to run in the event that that the object
  // is destroyed, and should return whether or not the runtime was able to be
  // successfully installed (or verified to already be installed).
  virtual void EnsureInstalled(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool installed)> install_callback) = 0;

 protected:
  XrInstallHelper() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_XR_INSTALL_HELPER_H_
