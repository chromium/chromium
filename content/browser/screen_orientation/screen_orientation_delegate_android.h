// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DELEGATE_ANDROID_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DELEGATE_ANDROID_H_

#include <jni.h>

#include "content/public/browser/screen_orientation_delegate.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"

namespace content {

class WebContents;

// Android implementation of ScreenOrientationDelegate. The functionality of
// ScreenOrientationProvider is always supported.
class ScreenOrientationDelegateAndroid : public ScreenOrientationDelegate {
 public:
  ScreenOrientationDelegateAndroid();

  ScreenOrientationDelegateAndroid(const ScreenOrientationDelegateAndroid&) =
      delete;
  ScreenOrientationDelegateAndroid& operator=(
      const ScreenOrientationDelegateAndroid&) = delete;

  ~ScreenOrientationDelegateAndroid() override;

  // ScreenOrientationDelegate:
  bool FullScreenRequired(WebContents* web_contents) override;
  void Lock(WebContents* web_contents,
            device::mojom::ScreenOrientationLockType lock_orientation) override;
  bool ScreenOrientationProviderSupported(WebContents* web_contents) override;
  void Unlock(WebContents* web_contents) override;
};

} // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DELEGATE_ANDROID_H_
