// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"

#include "base/android/scoped_java_ref.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ScreenOrientationProviderImpl_jni.h"

namespace content {

ScreenOrientationDelegateAndroid::ScreenOrientationDelegateAndroid() {
  ScreenOrientationProvider::SetDelegate(this);
}

ScreenOrientationDelegateAndroid::~ScreenOrientationDelegateAndroid() {
  ScreenOrientationProvider::SetDelegate(nullptr);
}

bool ScreenOrientationDelegateAndroid::FullScreenRequired(
    WebContents* web_contents) {
  return true;
}

void ScreenOrientationDelegateAndroid::Lock(
    WebContents* web_contents,
    device::mojom::ScreenOrientationLockType lock_orientation) {
  base::android::ScopedJavaLocalRef<jobject> java_instance =
      Java_ScreenOrientationProviderImpl_getInstance(
          base::android::AttachCurrentThread());
  Java_ScreenOrientationProviderImpl_lockOrientationForWebContents(
      base::android::AttachCurrentThread(), java_instance,
      static_cast<WebContentsImpl*>(web_contents)->GetJavaWebContents(),
      static_cast<jbyte>(lock_orientation));
}

bool ScreenOrientationDelegateAndroid::ScreenOrientationProviderSupported(
    WebContents* web_contentss) {
  // TODO(MLamouri): Consider moving isOrientationLockEnabled to a separate
  // function, so reported error messages can differentiate between the device
  // never supporting orientation or currently not support orientation.
  base::android::ScopedJavaLocalRef<jobject> java_instance =
      Java_ScreenOrientationProviderImpl_getInstance(
          base::android::AttachCurrentThread());
  return Java_ScreenOrientationProviderImpl_isOrientationLockEnabled(
      base::android::AttachCurrentThread(), java_instance);
}

void ScreenOrientationDelegateAndroid::Unlock(WebContents* web_contents) {
  base::android::ScopedJavaLocalRef<jobject> java_instance =
      Java_ScreenOrientationProviderImpl_getInstance(
          base::android::AttachCurrentThread());
  Java_ScreenOrientationProviderImpl_unlockOrientationForWebContents(
      base::android::AttachCurrentThread(), java_instance,
      static_cast<WebContentsImpl*>(web_contents)->GetJavaWebContents());
}

} // namespace content
