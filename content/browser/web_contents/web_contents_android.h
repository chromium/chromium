// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/frame_host/navigation_controller_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class WebContentsImpl;

// Android wrapper around WebContents that provides safer passage from java and
// back to native and provides java with a means of communicating with its
// native counterpart.
class CONTENT_EXPORT WebContentsAndroid {
 public:
  explicit WebContentsAndroid(WebContentsImpl* web_contents);
  ~WebContentsAndroid();

  WebContentsImpl* web_contents() const { return web_contents_; }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Methods called from Java
  void ClearNativeReference(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetTopLevelNativeWindow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetTopLevelNativeWindow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jwindow_android);
  void SetViewAndroidDelegate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jview_delegate);
  base::android::ScopedJavaLocalRef<jobject> GetMainFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;
  base::android::ScopedJavaLocalRef<jobject> GetFocusedFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetTitle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetVisibleURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  bool IsLoading(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj) const;
  bool IsLoadingToDifferentDocument(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  void Stop(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Cut(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Copy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Paste(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void PasteAsPlainText(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void Replace(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               const base::android::JavaParamRef<jstring>& jstr);
  void SelectAll(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void CollapseSelection(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  jint GetBackgroundColor(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetLastCommittedURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;
  jboolean IsIncognito(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  void ResumeLoadingCreatedWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void OnHide(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnShow(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetImportance(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jint importance);
  void SuspendAllMediaPlayers(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& jobj);
  void SetAudioMuted(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jobj,
                     jboolean mute);

  jboolean IsShowingInterstitialPage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean FocusLocationBarByDefault(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ExitFullscreen(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void ScrollFocusedEditableNodeIntoView(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SelectWordAroundCaret(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void AdjustSelectionByCharacterOffset(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint start_adjust,
      jint end_adjust,
      jboolean show_selection_menu);
  void EvaluateJavaScript(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const base::android::JavaParamRef<jstring>& script,
                          const base::android::JavaParamRef<jobject>& callback);
  void EvaluateJavaScriptForTests(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& script,
      const base::android::JavaParamRef<jobject>& callback);

  void AddMessageToDevToolsConsole(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint level,
      const base::android::JavaParamRef<jstring>& message);

  void PostMessageToMainFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jmessage,
      const base::android::JavaParamRef<jstring>& jsource_origin,
      const base::android::JavaParamRef<jstring>& jtarget_origin,
      const base::android::JavaParamRef<jobjectArray>& jports);

  jboolean HasAccessedInitialDocument(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

  // No theme color is represented by SK_ColorTRANSPARENT.
  jint GetThemeColor(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  jfloat GetLoadProgress(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void RequestSmartClipExtract(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& callback,
      jint x,
      jint y,
      jint width,
      jint height);

  void RequestAccessibilitySnapshot(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& callback);

  base::android::ScopedJavaLocalRef<jstring> GetEncoding(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  void SetOverscrollRefreshHandler(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& overscroll_refresh_handler);

  void SetSpatialNavigationDisabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      bool disabled);

  int DownloadImage(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& url,
                    jboolean is_fav_icon,
                    jint max_bitmap_size,
                    jboolean bypass_cache,
                    const base::android::JavaParamRef<jobject>& jcallback);
  void SetHasPersistentVideo(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jboolean value);
  bool HasActiveEffectivelyFullscreenVideo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  bool IsPictureInPictureAllowedForFullscreenVideo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetFullscreenVideoSize(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetSize(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jint width,
               jint height);
  int GetWidth(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  int GetHeight(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetOrCreateEventForwarder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void SetMediaSession(
      const base::android::ScopedJavaLocalRef<jobject>& j_media_session);

  void SendOrientationChangeEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint orientation);

  void OnScaleFactorChanged(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  void SetFocus(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jboolean focused);
  bool IsBeingDestroyed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

  void SetDisplayCutoutSafeArea(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                int top,
                                int left,
                                int bottom,
                                int right);
  void NotifyRendererPreferenceUpdate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetRenderWidgetHostView(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid();

  class DestructionObserver : public base::CheckedObserver {
   public:
    // Invoked when the Java reference to the WebContents is being destroyed.
    virtual void WebContentsAndroidDestroyed(
        WebContentsAndroid* web_contents_android) = 0;
  };

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

 private:
  void OnFinishDownloadImage(const base::android::JavaRef<jobject>& obj,
                             const base::android::JavaRef<jobject>& callback,
                             int id,
                             int http_status_code,
                             const GURL& url,
                             const std::vector<SkBitmap>& bitmaps,
                             const std::vector<gfx::Size>& sizes);
  void SelectWordAroundCaretAck(bool did_select,
                                int start_adjust,
                                int end_adjust);

  WebContentsImpl* web_contents_;

  NavigationControllerAndroid navigation_controller_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  base::WeakPtrFactory<WebContentsAndroid> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebContentsAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
