// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/browser/navigation_transitions/back_forward_transition_animator.h"
#include "content/browser/renderer_host/navigation_controller_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"

class GURL;

namespace ui {
struct AXTreeUpdate;
}

namespace content {

class WebContents;
class WebContentsImpl;

// Android wrapper around WebContents that provides safer passage from java and
// back to native and provides java with a means of communicating with its
// native counterpart.
class CONTENT_EXPORT WebContentsAndroid {
 public:
  explicit WebContentsAndroid(WebContentsImpl* web_contents);

  WebContentsAndroid(const WebContentsAndroid&) = delete;
  WebContentsAndroid& operator=(const WebContentsAndroid&) = delete;

  ~WebContentsAndroid();

  void Init();

  WebContentsImpl* web_contents() const { return web_contents_; }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Ensure that the RenderFrameHost etc are ready to handle JS eval
  // (e.g. recover from a crashed state).
  bool InitializeRenderFrameForJavaScript();

  // Methods called from Java
  void ClearNativeReference(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetTopLevelNativeWindow(
      JNIEnv* env);
  void SetTopLevelNativeWindow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jwindow_android);
  void SetViewAndroidDelegate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jview_delegate);
  base::android::ScopedJavaLocalRef<jobject> GetMainFrame(JNIEnv* env) const;
  base::android::ScopedJavaLocalRef<jobject> GetFocusedFrame(JNIEnv* env) const;
  bool IsFocusedElementEditable(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetRenderFrameHostFromId(
      JNIEnv* env,
      jint render_process_id,
      jint render_frame_id) const;
  base::android::ScopedJavaLocalRef<jobjectArray> GetAllRenderFrameHosts(
      JNIEnv* env) const;
  base::android::ScopedJavaLocalRef<jstring> GetTitle(JNIEnv* env) const;
  base::android::ScopedJavaLocalRef<jobject> GetVisibleURL(JNIEnv* env) const;
  jint GetVirtualKeyboardMode(JNIEnv* env) const;

  bool IsLoading(JNIEnv* env) const;
  bool ShouldShowLoadingUI(JNIEnv* env) const;
  bool HasUncommittedNavigationInPrimaryMainFrame(JNIEnv* env) const;

  void DispatchBeforeUnload(JNIEnv* env, bool auto_cancel);

  void Stop(JNIEnv* env);
  void Cut(JNIEnv* env);
  void Copy(JNIEnv* env);
  void Paste(JNIEnv* env);
  void PasteAsPlainText(JNIEnv* env);
  void Replace(JNIEnv* env, const base::android::JavaParamRef<jstring>& jstr);
  void SelectAll(JNIEnv* env);
  void CollapseSelection(JNIEnv* env);
  jint GetBackgroundColor(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetLastCommittedURL(
      JNIEnv* env) const;
  jboolean IsIncognito(JNIEnv* env);

  void ResumeLoadingCreatedWebContents(JNIEnv* env);

  void SetImportance(JNIEnv* env, jint importance);
  void SuspendAllMediaPlayers(JNIEnv* env);
  void SetAudioMuted(JNIEnv* env, jboolean mute);
  jboolean IsAudioMuted(JNIEnv* env);

  jboolean FocusLocationBarByDefault(JNIEnv* env);
  bool IsFullscreenForCurrentTab(JNIEnv* env);
  void ExitFullscreen(JNIEnv* env);
  void ScrollFocusedEditableNodeIntoView(JNIEnv* env);
  void SelectAroundCaret(JNIEnv* env,
                         jint granularity,
                         jboolean should_show_handle,
                         jboolean should_show_context_menu,
                         jint startOffset,
                         jint endOffset,
                         jint surroundingTextLength);
  void AdjustSelectionByCharacterOffset(JNIEnv* env,
                                        jint start_adjust,
                                        jint end_adjust,
                                        jboolean show_selection_menu);
  void EvaluateJavaScript(JNIEnv* env,
                          const base::android::JavaParamRef<jstring>& script,
                          const base::android::JavaParamRef<jobject>& callback);
  void EvaluateJavaScriptForTests(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& script,
      const base::android::JavaParamRef<jobject>& callback);

  void AddMessageToDevToolsConsole(
      JNIEnv* env,
      jint level,
      const base::android::JavaParamRef<jstring>& message);

  void PostMessageToMainFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jmessage,
      const base::android::JavaParamRef<jstring>& jsource_origin,
      const base::android::JavaParamRef<jstring>& jtarget_origin,
      const base::android::JavaParamRef<jobjectArray>& jports);

  jboolean HasAccessedInitialDocument(JNIEnv* env);

  jboolean HasViewTransitionOptIn(JNIEnv* env);

  // No theme color is represented by SK_ColorTRANSPARENT.
  jint GetThemeColor(JNIEnv* env);

  jfloat GetLoadProgress(JNIEnv* env);

  void RequestSmartClipExtract(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& callback,
      jint x,
      jint y,
      jint width,
      jint height);

  void RequestAccessibilitySnapshot(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& view_structure_root,
      const base::android::JavaParamRef<jobject>& view_structure_builder,
      const base::android::JavaParamRef<jobject>& callback);

  base::android::ScopedJavaLocalRef<jstring> GetEncoding(JNIEnv* env) const;

  void SetOverscrollRefreshHandler(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& overscroll_refresh_handler);

  void SetSpatialNavigationDisabled(JNIEnv* env, bool disabled);

  void SetStylusHandwritingEnabled(JNIEnv* env, bool enabled);

  int DownloadImage(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& url,
                    jboolean is_fav_icon,
                    jint max_bitmap_size,
                    jboolean bypass_cache,
                    const base::android::JavaParamRef<jobject>& jcallback);
  void SetHasPersistentVideo(JNIEnv* env, jboolean value);
  bool HasActiveEffectivelyFullscreenVideo(JNIEnv* env);
  bool IsPictureInPictureAllowedForFullscreenVideo(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetFullscreenVideoSize(
      JNIEnv* env);
  void SetSize(JNIEnv* env, jint width, jint height);
  int GetWidth(JNIEnv* env);
  int GetHeight(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetOrCreateEventForwarder(
      JNIEnv* env);

  void SetMediaSession(
      const base::android::ScopedJavaLocalRef<jobject>& j_media_session);

  void SendOrientationChangeEvent(JNIEnv* env, jint orientation);

  void OnScaleFactorChanged(JNIEnv* env);
  void SetFocus(JNIEnv* env, jboolean focused);
  bool IsBeingDestroyed(JNIEnv* env);

  void SetDisplayCutoutSafeArea(JNIEnv* env,
                                int top,
                                int left,
                                int bottom,
                                int right);
  void NotifyRendererPreferenceUpdate(JNIEnv* env);

  void NotifyBrowserControlsHeightChanged(JNIEnv* env);

  bool NeedToFireBeforeUnloadOrUnloadEvents(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetRenderWidgetHostView(
      JNIEnv* env);

  jint GetVisibility(JNIEnv* env);

  void UpdateWebContentsVisibility(JNIEnv* env, jint visibility);

  void NotifyControlsConstraintsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jold_offset_tag_bundle,
      const base::android::JavaParamRef<jobject>& joffset_tag_bundle);

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid();

  class DestructionObserver : public base::CheckedObserver {
   public:
    // Invoked when the Java reference to the WebContents is being destroyed.
    virtual void WebContentsAndroidDestroyed(
        WebContentsAndroid* web_contents_android) = 0;
  };

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

  void OnContentForNavigationEntryShown(JNIEnv* env);
  jint GetCurrentBackForwardTransitionStage(JNIEnv* env);

  void CaptureContentAsBitmapForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcallback);
  void OnFinishGetContentBitmapForTesting(
      const base::android::JavaRef<jobject>& callback,
      gfx::Image snapshot);

  void SetLongPressLinkSelectText(JNIEnv* env, jboolean enabled);

  // Adds a crash report, like DumpWithoutCrashing(), including the Java stack
  // trace from which `web_contents` was created. This is meant to help debug
  // cases where BrowserContext is destroyed before its WebContents.
  static void ReportDanglingPtrToBrowserContext(JNIEnv* env,
                                                WebContents* web_contents);

 private:
  void OnFinishDownloadImage(const base::android::JavaRef<jobject>& obj,
                             const base::android::JavaRef<jobject>& callback,
                             int id,
                             int http_status_code,
                             const GURL& url,
                             const std::vector<SkBitmap>& bitmaps,
                             const std::vector<gfx::Size>& sizes);
  void SelectAroundCaretAck(int startOffset,
                            int endOffset,
                            int surroundingTextLength,
                            blink::mojom::SelectAroundCaretResultPtr result);
  // Walks over the AXTreeUpdate and creates a light weight snapshot.
  void AXTreeSnapshotCallback(
      const base::android::JavaRef<jobject>& view_structure_root,
      const base::android::JavaRef<jobject>& view_structure_builder,
      const base::android::JavaRef<jobject>& callback,
      ui::AXTreeUpdate& result);

  raw_ptr<WebContentsImpl> web_contents_;

  NavigationControllerAndroid navigation_controller_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  class BrowserControlsOffsetTagMediator : public RenderWidgetHostConnector {
   public:
    explicit BrowserControlsOffsetTagMediator(WebContents* web_contents);
    ~BrowserControlsOffsetTagMediator() override;

    void SetOffsetTagsInfo(
        const cc::BrowserControlsOffsetTagsInfo& new_offset_tags_info);

    void UpdateRenderProcessConnection(
        RenderWidgetHostViewAndroid* old_rwhva,
        RenderWidgetHostViewAndroid* new_rhwva) override;

   private:
    raw_ptr<RenderWidgetHostViewAndroid> rwhva_ = nullptr;
    cc::BrowserControlsOffsetTagsInfo offset_tags_info_;
  };

  raw_ptr<BrowserControlsOffsetTagMediator> offset_tag_mediator_ = nullptr;

  base::WeakPtrFactory<WebContentsAndroid> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
