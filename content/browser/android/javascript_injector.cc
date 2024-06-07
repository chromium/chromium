// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/javascript_injector.h"

#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/JavascriptInjectorImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

JavascriptInjector::JavascriptInjector(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& retained_objects,
    WebContents* web_contents)
    : WebContentsUserData<JavascriptInjector>(*web_contents),
      java_ref_(env, obj) {
  java_bridge_dispatcher_host_ =
      new GinJavaBridgeDispatcherHost(web_contents, retained_objects);
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
}

JavascriptInjector::~JavascriptInjector() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_JavascriptInjectorImpl_onDestroy(env, j_obj);
}

void JavascriptInjector::SetAllowInspection(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jboolean allow) {
  DCHECK(java_bridge_dispatcher_host_);
  java_bridge_dispatcher_host_->SetAllowObjectContentsInspection(allow);
}

void JavascriptInjector::AddInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& /* obj */,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jstring>& name,
    const JavaParamRef<jclass>& safe_annotation_clazz) {
  DCHECK(java_bridge_dispatcher_host_);

  // If a new js object is added or removed when a page is in BFCache or
  // prerendered, the change won't apply after activating the page. To avoid
  // this behavior difference when these features are involved vs not,
  // evict all BFCached and prerendered pages so that we won't activate any
  // pages that don't have this object modified.
  // Same for RemoveInterface below.
  GetWebContents().GetController().GetBackForwardCache().Flush(
      content::BackForwardCache::NotRestoredReason::
          kWebViewJavaScriptObjectChanged);
  GetWebContentsImpl().GetPrerenderHostRegistry()->CancelAllHosts(
      PrerenderFinalStatus::kJavaScriptInterfaceAdded);

  java_bridge_dispatcher_host_->AddNamedObject(
      ConvertJavaStringToUTF8(env, name), object, safe_annotation_clazz);
}

void JavascriptInjector::RemoveInterface(JNIEnv* env,
                                         const JavaParamRef<jobject>& /* obj */,
                                         const JavaParamRef<jstring>& name) {
  DCHECK(java_bridge_dispatcher_host_);

  GetWebContents().GetController().GetBackForwardCache().Flush(
      content::BackForwardCache::NotRestoredReason::
          kWebViewJavaScriptObjectChanged);
  GetWebContentsImpl().GetPrerenderHostRegistry()->CancelAllHosts(
      PrerenderFinalStatus::kJavaScriptInterfaceRemoved);

  java_bridge_dispatcher_host_->RemoveNamedObject(
      ConvertJavaStringToUTF8(env, name));
}

WebContentsImpl& JavascriptInjector::GetWebContentsImpl() {
  return static_cast<WebContentsImpl&>(GetWebContents());
}

jlong JNI_JavascriptInjectorImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& retained_objects) {
  auto* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents) << "Should be created with a valid WebContents.";
  DCHECK(!JavascriptInjector::FromWebContents(web_contents));

  // Owned by |web_contents|.
  auto* injector =
      new JavascriptInjector(env, obj, retained_objects, web_contents);
  return reinterpret_cast<intptr_t>(injector);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(JavascriptInjector);

}  // namespace content
