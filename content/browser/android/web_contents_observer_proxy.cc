// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/web_contents_observer_proxy.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/media_metadata_android.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/media_metadata.h"
#include "jni/WebContentsObserverProxy_jni.h"

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

void WebContentsObserverProxy::DocumentAvailableInMainFrame() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_documentAvailableInMainFrame(env,
                                                             java_observer_);
}

void WebContentsObserverProxy::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, navigation_handle->GetURL().spec()));
  Java_WebContentsObserverProxy_didStartNavigation(
      env, java_observer_, jstring_url, navigation_handle->IsInMainFrame(),
      navigation_handle->IsSameDocument(), navigation_handle->IsErrorPage());
}

void WebContentsObserverProxy::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  JNIEnv* env = AttachCurrentThread();
  // Matches logic in
  // components/navigation_interception/navigation_params_android.cc
  ScopedJavaLocalRef<jstring> jstring_url(ConvertUTF8ToJavaString(
      env,
      navigation_handle->GetBaseURLForDataURL().is_empty()
          ? navigation_handle->GetURL().spec()
          : navigation_handle->GetBaseURLForDataURL().possibly_invalid_spec()));

  bool is_fragment_navigation = navigation_handle->IsSameDocument();

  if (navigation_handle->HasCommitted()) {
    // See http://crbug.com/251330 for why it's determined this way.
    url::Replacements<char> replacements;
    replacements.ClearRef();
    bool urls_same_ignoring_fragment =
        navigation_handle->GetURL().ReplaceComponents(replacements) ==
        navigation_handle->GetPreviousURL().ReplaceComponents(replacements);
    is_fragment_navigation &= urls_same_ignoring_fragment;
  }

  // TODO(shaktisahu): Provide appropriate error description (crbug/690784).
  ScopedJavaLocalRef<jstring> jerror_description =
      ConvertUTF8ToJavaString(env, "");

  Java_WebContentsObserverProxy_didFinishNavigation(
      env, java_observer_, jstring_url, navigation_handle->IsInMainFrame(),
      navigation_handle->IsErrorPage(), navigation_handle->HasCommitted(),
      navigation_handle->IsSameDocument(), is_fragment_navigation,
      navigation_handle->IsRendererInitiated(), navigation_handle->IsDownload(),
      navigation_handle->HasCommitted() ? navigation_handle->GetPageTransition()
                                        : -1,
      navigation_handle->GetNetErrorCode(), jerror_description,
      // TODO(shaktisahu): Change default status to -1 after fixing
      // crbug/690041.
      navigation_handle->GetResponseHeaders()
          ? navigation_handle->GetResponseHeaders()->response_code()
          : 200);
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

void WebContentsObserverProxy::DocumentLoadedInFrame(
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

void WebContentsObserverProxy::DidAttachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didAttachInterstitialPage(env, java_observer_);
}

void WebContentsObserverProxy::DidDetachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didDetachInterstitialPage(env, java_observer_);
}

void WebContentsObserverProxy::DidChangeThemeColor(SkColor color) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didChangeThemeColor(env, java_observer_, color);
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

void WebContentsObserverProxy::DidReloadLoFiImages() {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsObserverProxy_didReloadLoFiImages(env, java_observer_);
}

}  // namespace content
