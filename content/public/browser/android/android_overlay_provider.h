// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_ANDROID_OVERLAY_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_ANDROID_OVERLAY_PROVIDER_H_

#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT AndroidOverlayProvider {
 public:
  /*
   * Return the singleton provider instance.
   */
  static AndroidOverlayProvider* GetInstance();

  /**
   * Return true if this overlays are supported on this device.
   */
  virtual bool AreOverlaysSupported() = 0;

 protected:
  virtual ~AndroidOverlayProvider() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_ANDROID_OVERLAY_PROVIDER_H_
