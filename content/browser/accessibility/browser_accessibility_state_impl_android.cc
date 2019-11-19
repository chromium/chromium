// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/android/jni_android.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/content_jni_headers/BrowserAccessibilityState_jni.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/animation/animation.h"

using base::android::AttachCurrentThread;

namespace content {

void BrowserAccessibilityStateImpl::PlatformInitialize() {
  // Setup the listener for the prefers reduced motion setting changing, so we
  // can inform the renderer about changes.
  JNIEnv* env = AttachCurrentThread();
  Java_BrowserAccessibilityState_registerAnimatorDurationScaleObserver(env);
}

void BrowserAccessibilityStateImpl::
    UpdatePlatformSpecificHistogramsOnUIThread() {}

void BrowserAccessibilityStateImpl::
    UpdatePlatformSpecificHistogramsOnOtherThread() {
  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly. Be careful
  // not to add any code that isn't safe to run from a non-main thread!
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  JNIEnv* env = AttachCurrentThread();
  Java_BrowserAccessibilityState_recordAccessibilityHistograms(env);

  // Screen reader metric.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Android.ScreenReader",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

void BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms() {
  ui::AXMode mode = GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Android.ScreenReader.EveryReport",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

// static
void JNI_BrowserAccessibilityState_OnAnimatorDurationScaleChanged(JNIEnv* env) {
  // We need to call into gfx::Animation and WebContentsImpl on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  gfx::Animation::UpdatePrefersReducedMotion();
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    wc->GetRenderViewHost()->OnWebkitPreferencesChanged();
  }
}

}  // namespace content
