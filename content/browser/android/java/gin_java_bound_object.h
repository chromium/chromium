// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BOUND_OBJECT_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BOUND_OBJECT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "content/browser/android/java/java_method.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

class GinJavaBoundObject
    : public base::RefCountedThreadSafe<GinJavaBoundObject> {
 public:
  typedef int32_t ObjectID;

  static GinJavaBoundObject* CreateNamed(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  static GinJavaBoundObject* CreateTransient(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      const GlobalRenderFrameHostId& holder);

  // The following methods can be called on any thread.
  JavaObjectWeakGlobalRef& GetWeakRef() { return ref_; }
  base::android::ScopedJavaLocalRef<jobject> GetLocalRef(JNIEnv* env) {
    return ref_.get(env);
  }

  bool IsNamed() { return names_count_ > 0; }
  void AddName() { ++names_count_; }
  void RemoveName() { --names_count_; }

  // The following methods are called on the background thread.
  bool HasHolders() { return !holders_.empty(); }
  void AddHolder(const GlobalRenderFrameHostId& holder) {
    holders_.insert(holder);
  }
  void RemoveHolder(const GlobalRenderFrameHostId& holder) {
    holders_.erase(holder);
  }

  std::set<std::string> GetMethodNames();
  bool HasMethod(const std::string& method_name);
  const JavaMethod* FindMethod(const std::string& method_name,
                               size_t num_parameters);
  bool IsObjectGetClassMethod(const JavaMethod* method);
  const base::android::JavaRef<jclass>& GetSafeAnnotationClass();
  base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(JNIEnv* env);

 private:
  friend class base::RefCountedThreadSafe<GinJavaBoundObject>;

  GinJavaBoundObject(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  GinJavaBoundObject(
      const JavaObjectWeakGlobalRef& ref,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      const std::set<GlobalRenderFrameHostId>& holders);
  ~GinJavaBoundObject();

  // The following methods are called on the background thread.
  void EnsureMethodsAreSetUp();

  JavaObjectWeakGlobalRef ref_;

  // An object must be kept in retained_object_set_ either if it has
  // names or if it has a non-empty holders set.
  int names_count_;
  std::set<GlobalRenderFrameHostId> holders_;

  // The following fields are accessed on the background thread.
  using JavaMethodMap = std::multimap<std::string, std::unique_ptr<JavaMethod>>;
  JavaMethodMap methods_;
  bool are_methods_set_up_;
  base::android::ScopedJavaGlobalRef<jclass> safe_annotation_clazz_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BOUND_OBJECT_H_
