// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_CAPTIONING_CONTROLLER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_CAPTIONING_CONTROLLER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class WebContents;

// System captioning bridge for Android. Owns itself, and gets destroyed
// together with WebContents.
class CaptioningController : public WebContentsObserver {
 public:
  CaptioningController(JNIEnv* env,
                       const base::android::JavaRef<jobject>& obj,
                       WebContents* web_contents);

  ~CaptioningController() override;

  CaptioningController(const CaptioningController&) = delete;
  CaptioningController& operator=(const CaptioningController&) = delete;

  void SetTextTrackSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean textTracksEnabled,
      const base::android::JavaParamRef<jstring>& textTrackBackgroundColor,
      const base::android::JavaParamRef<jstring>& textTrackFontFamily,
      const base::android::JavaParamRef<jstring>& textTrackFontStyle,
      const base::android::JavaParamRef<jstring>& textTrackFontVariant,
      const base::android::JavaParamRef<jstring>& textTrackTextColor,
      const base::android::JavaParamRef<jstring>& textTrackTextShadow,
      const base::android::JavaParamRef<jstring>& textTrackTextSize);

 private:
  // WebContentsObserver implementation.
  void PrimaryPageChanged(Page& page) override;
  void RenderViewReady() override;
  void WebContentsDestroyed() override;

  // A weak reference to the Java CaptioningController object.
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_CAPTIONING_CONTROLLER_H_
