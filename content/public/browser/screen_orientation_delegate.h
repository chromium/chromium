// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DELEGATE_H_

#include "content/common/content_export.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"

namespace content {

class WebContents;

// Can be implemented to provide platform specific functionality for
// ScreenOrientationProvider.
class CONTENT_EXPORT ScreenOrientationDelegate {
 public:
  ScreenOrientationDelegate() {}

  ScreenOrientationDelegate(const ScreenOrientationDelegate&) = delete;
  ScreenOrientationDelegate& operator=(const ScreenOrientationDelegate&) =
      delete;

  virtual ~ScreenOrientationDelegate() {}

  // Returns true if the provided `web_contents` must be fullscreen in order for
  // ScreenOrientationProvider to respond to requests.
  virtual bool FullScreenRequired(WebContents* web_contents) = 0;

  // Lock the display with the provided `web_contents` to the given orientation.
  virtual void Lock(
      WebContents* web_contents,
      device::mojom::ScreenOrientationLockType lock_orientation) = 0;

  // Returns true if `Lock()` above can be called for the specified
  // `web_contents`.
  virtual bool ScreenOrientationProviderSupported(
      WebContents* web_contents) = 0;

  // Unlocks the display with the provided `web_contents`, allowing hardware
  // rotation to resume.
  virtual void Unlock(WebContents* web_contents) = 0;
};

} // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DELEGATE_H_
