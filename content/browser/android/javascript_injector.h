// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVASCRIPT_INJECTOR_H_
#define CONTENT_BROWSER_ANDROID_JAVASCRIPT_INJECTOR_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/common/gin_java_bridge.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class GinJavaBridgeDispatcherHost;
class WebContentsImpl;

class JavascriptInjector : public WebContentsUserData<JavascriptInjector> {
 public:
  JavascriptInjector(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& retained_objects,
      WebContents* web_contents);

  JavascriptInjector(const JavascriptInjector&) = delete;
  JavascriptInjector& operator=(const JavascriptInjector&) = delete;

  ~JavascriptInjector() override;

  void SetAllowInspection(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean allow);

  void AddInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& /* obj */,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jstring>& name,
      const base::android::JavaParamRef<jclass>& safe_annotation_clazz);

  void RemoveInterface(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& /* obj */,
                       const base::android::JavaParamRef<jstring>& name);
 private:
  friend class content::WebContentsUserData<JavascriptInjector>;

  WebContentsImpl& GetWebContentsImpl();

  // A weak reference to the Java JavascriptInjectorImpl object.
  JavaObjectWeakGlobalRef java_ref_;

  // Manages injecting Java objects.
  scoped_refptr<GinJavaBridgeDispatcherHost> java_bridge_dispatcher_host_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVASCRIPT_INJECTOR_H_
