// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_ANDROID_OVERLAY_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_ANDROID_ANDROID_OVERLAY_PROVIDER_IMPL_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/android/android_overlay_provider.h"

namespace content {

// Native counterpart to AndroidOverlayProviderImpl java class.
class AndroidOverlayProviderImpl : public AndroidOverlayProvider {
 public:
  AndroidOverlayProviderImpl();

  bool AreOverlaysSupported() override;

 private:
  ~AndroidOverlayProviderImpl() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_ANDROID_OVERLAY_PROVIDER_IMPL_H_
