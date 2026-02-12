// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/browser/navigation_transitions/back_forward_transition_animator.h"
#include "content/browser/renderer_host/navigation_controller_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"
#include "ui/android/browser_controls_offset_tag_definitions.h"

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
      const base::android::JavaRef<jobject>& jwindow_android);
  void SetViewAndroidDelegate(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jview_delegate);
  base::android::ScopedJavaLocalRef<jobject> GetMainFrame(JNIEnv* env) const;
  base::android::ScopedJavaLocalRef<jobject> GetFocusedFrame(JNIEnv* env) const;
  bool IsFocusedElementEditable(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetRenderFrameHostFromId(
      JNIEnv* env,
      int32_t render_process_id,
      int32_t render_frame_id) const;
  base::android::ScopedJavaLocalRef<jobjectArray> GetAllRenderFrameHosts(
      JNIEnv* env) const;
  base::android::ScopedJavaLocalRef<jstring> GetTitle(JNIEnv* env) const;
  base::android::ScopedJavaLocalRef<jobject> GetVisibleURL(JNIEnv* env) const;
  int32_t GetVirtualKeyboardMode(JNIEnv* env) const;

  bool IsLoading(JNIEnv* env) const;
  bool ShouldShowLoadingUI(JNIEnv* env) const;
  bool HasUncommittedNavigationInPrimaryMainFrame(JNIEnv* env) const;

  void DispatchBeforeUnload(JNIEnv* env, bool auto_cancel);

  void Stop(JNIEnv* env);
  void Cut(JNIEnv* env);
  void Copy(JNIEnv* env);
  void Paste(JNIEnv* env);
  void PasteAsPlainText(JNIEnv* env);
  void Replace(JNIEnv* env, const base::android::JavaRef<jstring>& jstr);
  void SelectAll(JNIEnv* env);
  void CollapseSelection(JNIEnv* env);
  int32_t GetBackgroundColor(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetLastCommittedURL(
      JNIEnv* env) const;
  bool IsIncognito(JNIEnv* env);

  void ResumeLoadingCreatedWebContents(JNIEnv* env);

  void SetPrimaryPageImportance(JNIEnv* env,
                                int32_t main_frame_importance,
                                int32_t subframe_importance);
  void SuspendAllMediaPlayers(JNIEnv* env);
  void SetAudioMuted(JNIEnv* env, bool mute);
  bool IsAudioMuted(JNIEnv* env);

  bool FocusLocationBarByDefault(JNIEnv* env);
  bool IsFullscreenForCurrentTab(JNIEnv* env);
  void ExitFullscreen(JNIEnv* env);
  void ScrollFocusedEditableNodeIntoView(JNIEnv* env);
  void SelectAroundCaret(JNIEnv* env,
                         int32_t granularity,
                         bool should_show_handle,
                         bool should_show_context_menu,
                         int32_t startOffset,
                         int32_t endOffset,
                         int32_t surroundingTextLength);
  void AdjustSelectionByCharacterOffset(JNIEnv* env,
                                        int32_t start_adjust,
                                        int32_t end_adjust,
                                        bool show_selection_menu);
  void EvaluateJavaScript(JNIEnv* env,
                          const base::android::JavaRef<jstring>& script,
                          const base::android::JavaRef<jobject>& callback);
  void EvaluateJavaScriptForTests(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& script,
      const base::android::JavaRef<jobject>& callback);

  void AddMessageToDevToolsConsole(
      JNIEnv* env,
      int32_t level,
      const base::android::JavaRef<jstring>& message);

  void PostMessageToMainFrame(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jmessage,
      const base::android::JavaRef<jstring>& jsource_origin,
      const base::android::JavaRef<jstring>& jtarget_origin,
      const base::android::JavaRef<jobjectArray>& jports);

  bool HasAccessedInitialDocument(JNIEnv* env);

  bool HasViewTransitionOptIn(JNIEnv* env);

  // No theme color is represented by SK_ColorTRANSPARENT.
  int32_t GetThemeColor(JNIEnv* env);

  float GetLoadProgress(JNIEnv* env);

  void RequestSmartClipExtract(JNIEnv* env,
                               const base::android::JavaRef<jobject>& callback,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height);

  void RequestAccessibilitySnapshot(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& view_structure_root,
      const base::android::JavaRef<jobject>& view_structure_builder,
      base::OnceClosure&& callback);

  base::android::ScopedJavaLocalRef<jstring> GetEncoding(JNIEnv* env) const;

  void Discard(base::OnceClosure&& on_discarded);

  void SetOverscrollRefreshHandler(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& overscroll_refresh_handler);

  void SetSpatialNavigationDisabled(JNIEnv* env, bool disabled);

  void SetStylusHandwritingEnabled(JNIEnv* env, bool enabled);

  int DownloadImage(JNIEnv* env,
                    const base::android::JavaRef<jobject>& url,
                    bool is_fav_icon,
                    int32_t max_bitmap_size,
                    bool bypass_cache,
                    const base::android::JavaRef<jobject>& jcallback);
  void SetHasPersistentVideo(JNIEnv* env, bool value);
  bool HasActiveEffectivelyFullscreenVideo(JNIEnv* env);
  bool IsPictureInPictureAllowedForFullscreenVideo(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetFullscreenVideoSize(
      JNIEnv* env);
  void SetSize(JNIEnv* env, int32_t width, int32_t height);
  int GetWidth(JNIEnv* env);
  int GetHeight(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetOrCreateEventForwarder(
      JNIEnv* env);

  void SendOrientationChangeEvent(JNIEnv* env, int32_t orientation);

  void OnScaleFactorChanged(JNIEnv* env);
  void SetFocus(JNIEnv* env, bool focused);
  bool IsBeingDestroyed(JNIEnv* env);

  void SetDisplayCutoutSafeArea(JNIEnv* env,
                                int top,
                                int left,
                                int bottom,
                                int right);

  void ShowInterestInElement(JNIEnv* env, int nodeID);

  void NotifyRendererPreferenceUpdate(JNIEnv* env);

  void NotifyBrowserControlsHeightChanged(JNIEnv* env);

  bool NeedToFireBeforeUnloadOrUnloadEvents(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetRenderWidgetHostView(
      JNIEnv* env);

  int32_t GetVisibility(JNIEnv* env);

  void UpdateWebContentsVisibility(JNIEnv* env, int32_t visibility);

  void UpdateOffsetTagDefinitions(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& joffset_tag_definitions);

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
  int32_t GetCurrentBackForwardTransitionStage(JNIEnv* env);

  void CaptureContentAsBitmapForTesting(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcallback);
  void OnFinishGetContentBitmapForTesting(
      const base::android::JavaRef<jobject>& callback,
      gfx::Image snapshot);

  void SetLongPressLinkSelectText(JNIEnv* env, bool enabled);

  void SetCanAcceptLoadDrops(JNIEnv* env, bool enabled);

  bool GetCanAcceptLoadDropsForTesting(JNIEnv* env);

  void SetSupportsForwardTransitionAnimation(JNIEnv* env, bool enabled);

  bool HasOpener(JNIEnv* env);

  int32_t GetOriginalWindowOpenDisposition(JNIEnv* env);

  void UpdateWindowControlsOverlay(JNIEnv* env,
                                   int32_t left,
                                   int32_t top,
                                   int32_t right,
                                   int32_t bottom);

  void SetSupportsDraggableRegions(JNIEnv* env,
                                   bool supports_draggable_regions);

  // Adds a crash report, like DumpWithoutCrashing(), including the Java stack
  // trace from which `web_contents` was created. This is meant to help debug
  // cases where BrowserContext is destroyed before its WebContents.
  static void ReportDanglingPtrToBrowserContext(JNIEnv* env,
                                                WebContents* web_contents);

  base::android::ScopedJavaLocalRef<jobject> GetDocumentPictureInPictureOpener(
      JNIEnv* env);

 private:
  void OnFinishDownloadImage(const base::android::JavaRef<jobject>& callback,
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
      base::OnceClosure&& callback,
      ui::AXTreeUpdate& result);

  raw_ptr<WebContentsImpl> web_contents_;

  NavigationControllerAndroid navigation_controller_;
  // A weak reference to the Java object. The Java object will be kept alive by
  // a static map in the Java code. ScopedJavaGlobalRef would scale poorly with
  // a large number of WebContents as each entry would consume a slot in the
  // finite global ref table.
  JavaObjectWeakGlobalRef obj_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  class BrowserControlsOffsetTagMediator : public RenderWidgetHostConnector {
   public:
    explicit BrowserControlsOffsetTagMediator(WebContents* web_contents);
    ~BrowserControlsOffsetTagMediator() override;

    void SetOffsetTagDefinitions(const ui::BrowserControlsOffsetTagDefinitions&
                                     new_offset_tag_definitions);

    void UpdateRenderProcessConnection(
        RenderWidgetHostViewAndroid* old_rwhva,
        RenderWidgetHostViewAndroid* new_rhwva) override;

   private:
    raw_ptr<RenderWidgetHostViewAndroid> rwhva_ = nullptr;
    ui::BrowserControlsOffsetTagDefinitions offset_tag_definitions_;
  };

  raw_ptr<BrowserControlsOffsetTagMediator> offset_tag_mediator_ = nullptr;

  base::WeakPtrFactory<WebContentsAndroid> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
