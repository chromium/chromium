// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bound_object.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/contains.h"
#include "content/browser/android/java/jni_reflect.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/browser/reflection_jni_headers/Object_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaObjectArrayReader;
using base::android::ScopedJavaLocalRef;

namespace content {

// static
GinJavaBoundObject* GinJavaBoundObject::CreateNamed(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz) {
  return new GinJavaBoundObject(ref, safe_annotation_clazz);
}

// static
GinJavaBoundObject* GinJavaBoundObject::CreateTransient(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    const GlobalRenderFrameHostId& holder) {
  std::set<GlobalRenderFrameHostId> holders;
  holders.insert(holder);
  return new GinJavaBoundObject(ref, safe_annotation_clazz, holders);
}

GinJavaBoundObject::GinJavaBoundObject(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz)
    : ref_(ref),
      names_count_(1),
      are_methods_set_up_(false),
      safe_annotation_clazz_(safe_annotation_clazz) {
}

GinJavaBoundObject::GinJavaBoundObject(
    const JavaObjectWeakGlobalRef& ref,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    const std::set<GlobalRenderFrameHostId>& holders)
    : ref_(ref),
      names_count_(0),
      holders_(holders),
      are_methods_set_up_(false),
      safe_annotation_clazz_(safe_annotation_clazz) {}

GinJavaBoundObject::~GinJavaBoundObject() {
}

std::set<std::string> GinJavaBoundObject::GetMethodNames() {
  EnsureMethodsAreSetUp();
  std::set<std::string> result;
  for (JavaMethodMap::const_iterator it = methods_.begin();
       it != methods_.end();
       ++it) {
    result.insert(it->first);
  }
  return result;
}

bool GinJavaBoundObject::HasMethod(const std::string& method_name) {
  EnsureMethodsAreSetUp();
  return base::Contains(methods_, method_name);
}

const JavaMethod* GinJavaBoundObject::FindMethod(
    const std::string& method_name,
    size_t num_parameters) {
  EnsureMethodsAreSetUp();

  // Get all methods with the correct name.
  std::pair<JavaMethodMap::const_iterator, JavaMethodMap::const_iterator>
      iters = methods_.equal_range(method_name);
  if (iters.first == iters.second) {
    return NULL;
  }

  // LIVECONNECT_COMPLIANCE: We just take the first method with the correct
  // number of arguments, while the spec proposes using cost-based algorithm:
  // https://jdk6.java.net/plugin2/liveconnect/#OVERLOADED_METHODS
  for (JavaMethodMap::const_iterator iter = iters.first; iter != iters.second;
       ++iter) {
    if (iter->second->num_parameters() == num_parameters) {
      return iter->second.get();
    }
  }

  return NULL;
}

bool GinJavaBoundObject::IsObjectGetClassMethod(const JavaMethod* method) {
  static std::atomic<jmethodID> cached_method_id(nullptr);
  EnsureMethodsAreSetUp();
  // As java.lang.Object.getClass is declared to be final, it is sufficient to
  // compare methodIDs.
  JNIEnv* env = AttachCurrentThread();
  jmethodID get_class_method_id =
      base::android::MethodID::LazyGet<base::android::MethodID::TYPE_INSTANCE>(
          env, jni_zero::g_object_class, "getClass", "()Ljava/lang/Class;",
          &cached_method_id);
  return method->id() == get_class_method_id;
}

const base::android::JavaRef<jclass>&
GinJavaBoundObject::GetSafeAnnotationClass() {
  return safe_annotation_clazz_;
}

base::android::ScopedJavaLocalRef<jclass> GinJavaBoundObject::GetLocalClassRef(
    JNIEnv* env) {
  ScopedJavaLocalRef<jobject> obj = GetLocalRef(env);
  if (obj.obj()) {
    return JNI_Object::Java_Object_getClass(env, obj);
  } else {
    return base::android::ScopedJavaLocalRef<jclass>();
  }
}

void GinJavaBoundObject::EnsureMethodsAreSetUp() {
  if (are_methods_set_up_)
    return;
  are_methods_set_up_ = true;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jclass> clazz = GetLocalClassRef(env);
  if (clazz.is_null()) {
    return;
  }

  JavaObjectArrayReader<jobject> methods(GetClassMethods(env, clazz));
  // Java objects always have public methods.
  DCHECK_GT(methods.size(), 0);

  for (auto java_method : methods) {
    if (!safe_annotation_clazz_.is_null()) {
      if (!IsAnnotationPresent(env, java_method, safe_annotation_clazz_))
        continue;
    }

    std::unique_ptr<JavaMethod> method(new JavaMethod(java_method));
    methods_.insert(std::make_pair(method->name(), std::move(method)));
  }
}

}  // namespace content
