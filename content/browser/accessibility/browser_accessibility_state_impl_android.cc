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

class BrowserAccessibilityStateImplAndroid
    : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplAndroid();
  ~BrowserAccessibilityStateImplAndroid() override {}

 protected:
  void UpdateHistogramsOnOtherThread() override;
  void UpdateUniqueUserHistograms() override;
  void SetImageLabelsModeForProfile(bool enabled,
                                    BrowserContext* profile) override;
};

BrowserAccessibilityStateImplAndroid::BrowserAccessibilityStateImplAndroid() {
  // Setup the listener for the prefers reduced motion setting changing, so we
  // can inform the renderer about changes.
  JNIEnv* env = AttachCurrentThread();
  Java_BrowserAccessibilityState_registerAnimatorDurationScaleObserver(env);
}

void BrowserAccessibilityStateImplAndroid::UpdateHistogramsOnOtherThread() {
  BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread();

  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly. Be careful
  // not to add any code that isn't safe to run from a non-main thread!
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Screen reader metric.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Android.ScreenReader",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

void BrowserAccessibilityStateImplAndroid::UpdateUniqueUserHistograms() {
  BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms();

  ui::AXMode mode = GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Android.ScreenReader.EveryReport",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

void BrowserAccessibilityStateImplAndroid::SetImageLabelsModeForProfile(
    bool enabled,
    BrowserContext* profile) {
  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i) {
    if (web_contents_vector[i]->GetBrowserContext() != profile)
      continue;

    ui::AXMode ax_mode = web_contents_vector[i]->GetAccessibilityMode();
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
    web_contents_vector[i]->SetAccessibilityMode(ax_mode);
  }
}

// static
void JNI_BrowserAccessibilityState_OnAnimatorDurationScaleChanged(JNIEnv* env) {
  // We need to call into gfx::Animation and WebContentsImpl on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  gfx::Animation::UpdatePrefersReducedMotion();
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    wc->OnWebPreferencesChanged();
  }
}

//
// BrowserAccessibilityStateImpl::GetInstance implementation that constructs
// this class instead of the base class.
//

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  static base::NoDestructor<BrowserAccessibilityStateImplAndroid> instance;
  return &*instance;
}

}  // namespace content
