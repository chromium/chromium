// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/web_contents_observer_proxy.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/android/navigation_handle_proxy.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/content_jni_headers/WebContentsObserverProxy_jni.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace content {

// TODO(dcheng): File a bug. This class incorrectly passes just a frame ID,
// which is not sufficient to identify a frame (since frame IDs are scoped per
// render process, and so may collide).
WebContentsObserverProxy::WebContentsObserverProxy(JNIEnv* env,
                                                   jobject obj,
                                                   WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(obj);
  java_observer_.Reset(env, obj);
}

WebContentsObserverProxy::~WebContentsObserverProxy() {
}

jlong JNI_WebContentsObserverProxy_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  WebContents* web_contents =
      WebContents::FromJavaWebContents(java_web_contents);
  CHECK(web_contents);

  WebContentsObserverProxy* native_observer =
      new WebContentsObserverProxy(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(native_observer);
}

void WebContentsObserverProxy::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

void WebContentsObserverProxy::WebContentsDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  // The java side will destroy |this|
  Java_WebContentsObserverProxy_destroy(env, java_observer_);
}

void WebContentsObserverProxy::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_renderViewReady(env, java_observer_);
}

void WebContentsObserverProxy::RenderProcessGone(
    base::TerminationStatus termination_status) {
  JNIEnv* env = AttachCurrentThread();
  jboolean was_oom_protected =
      termination_status == base::TERMINATION_STATUS_OOM_PROTECTED;
  Java_WebContentsObserverProxy_renderProcessGone(env, java_observer_,
                                                  was_oom_protected);
}

void WebContentsObserverProxy::DidStartLoading() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, web_contents()->GetVisibleURL().spec()));
  if (auto* entry = web_contents()->GetController().GetPendingEntry()) {
    base_url_of_last_started_data_url_ = entry->GetBaseURLForDataURL();
  }
  Java_WebContentsObserverProxy_didStartLoading(env, java_observer_,
                                                jstring_url);
}

void WebContentsObserverProxy::DidStopLoading() {
  JNIEnv* env = AttachCurrentThread();
  std::string url_string = web_contents()->GetLastCommittedURL().spec();
  SetToBaseURLForDataURLIfNeeded(&url_string);
  // DidStopLoading is the last event we should get.
  base_url_of_last_started_data_url_ = GURL::EmptyGURL();
  ScopedJavaLocalRef<jstring> jstring_url(ConvertUTF8ToJavaString(
      env, url_string));
  Java_WebContentsObserverProxy_didStopLoading(env, java_observer_,
                                               jstring_url);
}

void WebContentsObserverProxy::LoadProgressChanged(double progress) {
  Java_WebContentsObserverProxy_loadProgressChanged(
      AttachCurrentThread(), java_observer_, static_cast<jfloat>(progress));
}

void WebContentsObserverProxy::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_error_description(
      ConvertUTF16ToJavaString(env, error_description));
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, validated_url.spec()));

  Java_WebContentsObserverProxy_didFailLoad(
      env, java_observer_, !render_frame_host->GetParent(), error_code,
      jstring_error_description, jstring_url);
}

void WebContentsObserverProxy::DidChangeVisibleSecurityState() {
  Java_WebContentsObserverProxy_didChangeVisibleSecurityState(
      AttachCurrentThread(), java_observer_);
}

void WebContentsObserverProxy::DocumentAvailableInMainFrame() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_documentAvailableInMainFrame(env,
                                                             java_observer_);
}

void WebContentsObserverProxy::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  Java_WebContentsObserverProxy_didStartNavigation(
      AttachCurrentThread(), java_observer_,
      NavigationRequest::From(navigation_handle)->java_navigation_handle());
}

void WebContentsObserverProxy::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  Java_WebContentsObserverProxy_didRedirectNavigation(
      AttachCurrentThread(), java_observer_,
      NavigationRequest::From(navigation_handle)->java_navigation_handle());
}

void WebContentsObserverProxy::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Remove after fixing https://crbug/905461.
  TRACE_EVENT0("browser", "Java_WebContentsObserverProxy_didFinishNavigation");

  Java_WebContentsObserverProxy_didFinishNavigation(
      AttachCurrentThread(), java_observer_,
      NavigationRequest::From(navigation_handle)->java_navigation_handle());
}

void WebContentsObserverProxy::DidFinishLoad(RenderFrameHost* render_frame_host,
                                             const GURL& validated_url) {
  JNIEnv* env = AttachCurrentThread();

  std::string url_string = validated_url.spec();
  SetToBaseURLForDataURLIfNeeded(&url_string);

  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url_string));
  Java_WebContentsObserverProxy_didFinishLoad(
      env, java_observer_, render_frame_host->GetRoutingID(), jstring_url,
      !render_frame_host->GetParent());
}

void WebContentsObserverProxy::DOMContentLoaded(
    RenderFrameHost* render_frame_host) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_documentLoadedInFrame(
      env, java_observer_, render_frame_host->GetRoutingID(),
      !render_frame_host->GetParent());
}

void WebContentsObserverProxy::NavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_navigationEntryCommitted(env, java_observer_);
}

void WebContentsObserverProxy::NavigationEntriesDeleted() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_navigationEntriesDeleted(env, java_observer_);
}

void WebContentsObserverProxy::NavigationEntryChanged(
    const EntryChangedDetails& change_details) {
  JNIEnv* env = AttachCurrentThread();
  // TODO(jinsukkim): Convert |change_details| to Java object when needed.
  Java_WebContentsObserverProxy_navigationEntriesChanged(env, java_observer_);
}

void WebContentsObserverProxy::DidAttachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didAttachInterstitialPage(env, java_observer_);
}

void WebContentsObserverProxy::DidDetachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didDetachInterstitialPage(env, java_observer_);
}

void WebContentsObserverProxy::DidChangeThemeColor(
    base::Optional<SkColor> color) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didChangeThemeColor(
      env, java_observer_, color.value_or(SK_ColorTRANSPARENT));
}

void WebContentsObserverProxy::MediaEffectivelyFullscreenChanged(
    bool is_fullscreen) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_hasEffectivelyFullscreenVideoChange(
      env, java_observer_, is_fullscreen);
}

void WebContentsObserverProxy::DidFirstVisuallyNonEmptyPaint() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didFirstVisuallyNonEmptyPaint(env,
                                                              java_observer_);
}

void WebContentsObserverProxy::OnVisibilityChanged(
    content::Visibility visibility) {
  // Occlusion is not supported on Android.
  DCHECK_NE(visibility, content::Visibility::OCCLUDED);

  JNIEnv* env = AttachCurrentThread();

  if (visibility == content::Visibility::VISIBLE)
    Java_WebContentsObserverProxy_wasShown(env, java_observer_);
  else
    Java_WebContentsObserverProxy_wasHidden(env, java_observer_);
}

void WebContentsObserverProxy::TitleWasSet(NavigationEntry* entry) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_title = ConvertUTF8ToJavaString(
      env,
      base::UTF16ToUTF8(web_contents()->GetTitle()));
  Java_WebContentsObserverProxy_titleWasSet(env, java_observer_, jstring_title);
}

void WebContentsObserverProxy::SetToBaseURLForDataURLIfNeeded(
    std::string* url) {
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  // Note that GetBaseURLForDataURL is only used by the Android WebView.
  // FIXME: Should we only return valid specs and "about:blank" for invalid
  // ones? This may break apps.
  if (entry && !entry->GetBaseURLForDataURL().is_empty()) {
    *url = entry->GetBaseURLForDataURL().possibly_invalid_spec();
  } else if (!base_url_of_last_started_data_url_.is_empty()) {
    // NavigationController can lose the pending entry and recreate it without
    // a base URL if there has been a loadUrl("javascript:...") after
    // loadDataWithBaseUrl.
    *url = base_url_of_last_started_data_url_.possibly_invalid_spec();
  }
}

void WebContentsObserverProxy::ViewportFitChanged(
    blink::mojom::ViewportFit value) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_viewportFitChanged(
      env, java_observer_, as_jint(static_cast<int>(value)));
}

void WebContentsObserverProxy::OnWebContentsFocused(RenderWidgetHost*) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_onWebContentsFocused(env, java_observer_);
}

void WebContentsObserverProxy::OnWebContentsLostFocus(RenderWidgetHost*) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_onWebContentsLostFocus(env, java_observer_);
}

}  // namespace content
