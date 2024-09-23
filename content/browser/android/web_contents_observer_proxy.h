// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_PROXY_H_
#define CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_PROXY_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/process/kill.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {

class MediaSession;
class WebContents;
class RenderFrameHost;

// Extends WebContentsObserver for providing a public Java API for some of the
// the calls it receives.
class WebContentsObserverProxy : public WebContentsObserver {
 public:
  WebContentsObserverProxy(JNIEnv* env, jobject obj, WebContents* web_contents);

  WebContentsObserverProxy(const WebContentsObserverProxy&) = delete;
  WebContentsObserverProxy& operator=(const WebContentsObserverProxy&) = delete;

  ~WebContentsObserverProxy() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus termination_status) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void LoadProgressChanged(double progress) override;
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidChangeVisibleSecurityState() override;
  void PrimaryMainDocumentElementAvailable() override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void TitleWasSet(NavigationEntry* entry) override;

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DOMContentLoaded(RenderFrameHost* render_frame_host) override;
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  void NavigationEntriesDeleted() override;
  void NavigationEntryChanged(
      const EntryChangedDetails& change_details) override;
  void FrameReceivedUserActivation(RenderFrameHost*) override;
  void WebContentsDestroyed() override;
  void DidChangeThemeColor() override;
  void OnBackgroundColorChanged() override;
  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;
  void MediaEffectivelyFullscreenChanged(bool is_fullscreen) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  bool SetToBaseURLForDataURLIfNeeded(GURL* url);
  void ViewportFitChanged(blink::mojom::ViewportFit value) override;
  void VirtualKeyboardModeChanged(ui::mojom::VirtualKeyboardMode mode) override;
  void OnWebContentsFocused(RenderWidgetHost*) override;
  void OnWebContentsLostFocus(RenderWidgetHost*) override;
  void MediaSessionCreated(MediaSession* media_session) override;

  base::android::ScopedJavaGlobalRef<jobject> java_observer_;
  GURL base_url_of_last_started_data_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_WEB_CONTENTS_OBSERVER_PROXY_H_
