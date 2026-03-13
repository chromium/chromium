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
  explicit CaptioningController(WebContents* web_contents);

  ~CaptioningController() override;

  CaptioningController(const CaptioningController&) = delete;
  CaptioningController& operator=(const CaptioningController&) = delete;

  void Destroy(JNIEnv* env);

  void SetTextTrackSettings(
      JNIEnv* env,
      bool textTracksEnabled,
      const base::android::JavaRef<jstring>& textTrackBackgroundColor,
      const base::android::JavaRef<jstring>& textTrackFontFamily,
      const base::android::JavaRef<jstring>& textTrackFontStyle,
      const base::android::JavaRef<jstring>& textTrackFontVariant,
      const base::android::JavaRef<jstring>& textTrackTextColor,
      const base::android::JavaRef<jstring>& textTrackTextShadow,
      const base::android::JavaRef<jstring>& textTrackTextSize);

 private:
  // WebContentsObserver implementation.
  void PrimaryPageChanged(Page& page) override;
  void RenderViewReady() override;

  base::android::ScopedJavaLocalRef<jobject> GetFromWebContents(
      JNIEnv* env,
      WebContents* web_contents);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_CAPTIONING_CONTROLLER_H_
