// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DIALOG_OVERLAY_IMPL_H_
#define CONTENT_BROWSER_ANDROID_DIALOG_OVERLAY_IMPL_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;

// Native counterpart to DialogOverlayImpl java class.  This is created by the
// java side.  When the WebContents for the provided token is attached or
// detached from a WindowAndroid, we get the Android window token and notify the
// java side.
class DialogOverlayImpl : public ui::ViewAndroidObserver,
                          public ui::WindowAndroidObserver,
                          public WebContentsObserver {
 public:
  // This may not call back into |obj| directly, but must post.  This is because
  // |obj| is still being initialized.
  DialogOverlayImpl(const base::android::JavaParamRef<jobject>& obj,
                    RenderFrameHostImpl* rfhi,
                    WebContents* web_contents,
                    bool power_efficient,
                    bool observe_container_view);
  ~DialogOverlayImpl() override;

  // Called when the java side is ready for token / dismissed callbacks.  May
  // call back before returning.  Must guarantee that a token is eventually sent
  // if we have one.
  void CompleteInit(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);

  // Clean up and post to delete |this| later.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Calls ReceiveCompositorOffset() (java) with the compositor screen offset
  // before returning, in physical pixels.  We send |rect| as a convenience.
  void GetCompositorOffset(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           const base::android::JavaParamRef<jobject>& rect);

  // ui::ViewAndroidObserver
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;

  // WebContentsObserver
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;

  // Unregister callbacks if previously registered.
  void UnregisterCallbacksIfNeeded();

  // WindowAndroidObserver
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override {}
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

 private:
  // Signals the overlay should be cleaned up and no longer used.
  void Stop();
  void RegisterWindowObserverIfNeeded(ui::WindowAndroid* window);

  void StartObservingContainerView();
  void StopObservingContainerView();
  void ObserveContainerViewIfNeeded(
      const base::android::ScopedJavaLocalRef<jobject>& container_view);

  // Java object that owns us.
  JavaObjectWeakGlobalRef obj_;

  // RenderFrameHostImpl* associated with the given overlay routing token.
  raw_ptr<RenderFrameHostImpl> rfhi_;

  // Do we care about power efficiency?
  const bool power_efficient_;

  // Whether we added ourselves as an observer through WindowAndroid.
  bool observed_window_android_;

  // Whether we should observe the container view for location changes.
  const bool observe_container_view_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DIALOG_OVERLAY_IMPL_H_
