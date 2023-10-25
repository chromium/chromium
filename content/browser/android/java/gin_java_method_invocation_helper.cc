// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_method_invocation_helper.h"

#include <unistd.h>
#include <cmath>
#include <memory>
#include <utility>

#include "base/android/event_log.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/browser/android/java/gin_java_script_to_java_types_coercion.h"
#include "content/browser/android/java/java_method.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

// See frameworks/base/core/java/android/webkit/EventLogTags.logtags
const int kObjectGetClassInvocationAttemptLogTag = 70151;

}  // namespace

GinJavaMethodInvocationHelper::GinJavaMethodInvocationHelper(
    std::unique_ptr<ObjectDelegate> object,
    const std::string& method_name,
    const base::Value::List& arguments)
    : object_(std::move(object)),
      method_name_(method_name),
      arguments_(arguments.Clone()),
      invocation_error_(mojom::GinJavaBridgeError::kGinJavaBridgeNoError) {}

GinJavaMethodInvocationHelper::~GinJavaMethodInvocationHelper() {}

void GinJavaMethodInvocationHelper::Init(DispatcherDelegate* dispatcher) {
  // Build on the UI thread a map of object_id -> WeakRef for Java objects from
  // |arguments_|.  Then we can use this map on the background thread without
  // accessing |dispatcher|.
  BuildObjectRefsFromListValue(dispatcher, arguments_);
}

// As V8ValueConverter has finite recursion depth when serializing
// JavaScript values, we don't bother about having a recursion threshold here.
void GinJavaMethodInvocationHelper::BuildObjectRefsFromListValue(
    DispatcherDelegate* dispatcher,
    const base::Value::List& list_value) {
  for (const auto& entry : list_value) {
    if (AppendObjectRef(dispatcher, entry))
      continue;
    if (entry.is_list()) {
      BuildObjectRefsFromListValue(dispatcher, entry.GetList());
    } else if (entry.is_dict()) {
      BuildObjectRefsFromDictionaryValue(dispatcher, entry.GetDict());
    }
  }
}

void GinJavaMethodInvocationHelper::BuildObjectRefsFromDictionaryValue(
    DispatcherDelegate* dispatcher,
    const base::Value::Dict& dict_value) {
  for (const auto item : dict_value) {
    if (AppendObjectRef(dispatcher, item.second))
      continue;
    if (item.second.is_list()) {
      BuildObjectRefsFromListValue(dispatcher, item.second.GetList());
    } else if (item.second.is_dict()) {
      BuildObjectRefsFromDictionaryValue(dispatcher, item.second.GetDict());
    }
  }
}

bool GinJavaMethodInvocationHelper::AppendObjectRef(
    DispatcherDelegate* dispatcher,
    const base::Value& raw_value) {
  if (!GinJavaBridgeValue::ContainsGinJavaBridgeValue(&raw_value))
    return false;
  std::unique_ptr<const GinJavaBridgeValue> value(
      GinJavaBridgeValue::FromValue(&raw_value));
  if (!value->IsType(GinJavaBridgeValue::TYPE_OBJECT_ID))
    return false;
  GinJavaBoundObject::ObjectID object_id;
  if (value->GetAsObjectID(&object_id)) {
    ObjectRefs::iterator iter = object_refs_.find(object_id);
    if (iter == object_refs_.end()) {
      JavaObjectWeakGlobalRef object_ref(
          dispatcher->GetObjectWeakRef(object_id));
      if (!object_ref.is_uninitialized()) {
        object_refs_.insert(std::make_pair(object_id, object_ref));
      }
    }
  }
  return true;
}

void GinJavaMethodInvocationHelper::Invoke() {
  JNIEnv* env = AttachCurrentThread();
  const JavaMethod* method =
      object_->FindMethod(method_name_, arguments_.size());
  if (!method) {
    SetInvocationError(mojom::GinJavaBridgeError::kGinJavaBridgeMethodNotFound);
    return;
  }

  if (object_->IsObjectGetClassMethod(method)) {
    base::android::EventLogWriteInt(kObjectGetClassInvocationAttemptLogTag,
                                    getuid());
    SetInvocationError(mojom::GinJavaBridgeError::
                           kGinJavaBridgeAccessToObjectGetClassIsBlocked);
    return;
  }

  ScopedJavaLocalRef<jobject> obj;
  ScopedJavaLocalRef<jclass> cls;
  if (method->is_static()) {
    cls = object_->GetLocalClassRef(env);
  } else {
    obj = object_->GetLocalRef(env);
  }
  if (obj.is_null() && cls.is_null()) {
    SetInvocationError(mojom::GinJavaBridgeError::kGinJavaBridgeObjectIsGone);
    return;
  }

  mojom::GinJavaBridgeError coercion_error =
      mojom::GinJavaBridgeError::kGinJavaBridgeNoError;
  std::vector<jvalue> parameters(method->num_parameters());
  for (size_t i = 0; i < method->num_parameters(); ++i) {
    const base::Value& argument = arguments_[i];
    parameters[i] = CoerceJavaScriptValueToJavaValue(
        env, argument, method->parameter_type(i), true, object_refs_,
        &coercion_error);
  }

  if (coercion_error == mojom::GinJavaBridgeError::kGinJavaBridgeNoError) {
    if (method->is_static()) {
      InvokeMethod(nullptr, cls.obj(), method->return_type(), method->id(),
                   parameters.data());
    } else {
      InvokeMethod(obj.obj(), nullptr, method->return_type(), method->id(),
                   parameters.data());
    }
  } else {
    SetInvocationError(coercion_error);
  }

  // Now that we're done with the jvalue, release any local references created
  // by CoerceJavaScriptValueToJavaValue().
  for (size_t i = 0; i < method->num_parameters(); ++i) {
    ReleaseJavaValueIfRequired(env, &parameters[i], method->parameter_type(i));
  }
}

void GinJavaMethodInvocationHelper::SetInvocationError(
    mojom::GinJavaBridgeError error) {
  holds_primitive_result_ = true;
  primitive_result_ = std::make_unique<base::Value::List>();
  invocation_error_ = error;
}

void GinJavaMethodInvocationHelper::SetPrimitiveResult(
    base::Value::List result_wrapper) {
  holds_primitive_result_ = true;
  primitive_result_ =
      std::make_unique<base::Value::List>(std::move(result_wrapper));
}

void GinJavaMethodInvocationHelper::SetObjectResult(
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz) {
  holds_primitive_result_ = false;
  object_result_.Reset(object);
  safe_annotation_clazz_.Reset(safe_annotation_clazz);
}

bool GinJavaMethodInvocationHelper::HoldsPrimitiveResult() {
  return holds_primitive_result_;
}

const base::Value::List& GinJavaMethodInvocationHelper::GetPrimitiveResult() {
  return *primitive_result_.get();
}

const base::android::JavaRef<jobject>&
GinJavaMethodInvocationHelper::GetObjectResult() {
  return object_result_;
}

const base::android::JavaRef<jclass>&
GinJavaMethodInvocationHelper::GetSafeAnnotationClass() {
  return safe_annotation_clazz_;
}

mojom::GinJavaBridgeError GinJavaMethodInvocationHelper::GetInvocationError() {
  return invocation_error_;
}

void GinJavaMethodInvocationHelper::InvokeMethod(jobject object,
                                                 jclass clazz,
                                                 const JavaType& return_type,
                                                 jmethodID id,
                                                 jvalue* parameters) {
  DCHECK(object || clazz);
  JNIEnv* env = AttachCurrentThread();
  base::Value::List result_wrapper;
  switch (return_type.type) {
    case JavaType::TypeBoolean:
      result_wrapper.Append(static_cast<bool>(
          object ? env->CallBooleanMethodA(object, id, parameters)
                 : env->CallStaticBooleanMethodA(clazz, id, parameters)));
      break;
    case JavaType::TypeByte:
      result_wrapper.Append(static_cast<int>(
          object ? env->CallByteMethodA(object, id, parameters)
                 : env->CallStaticByteMethodA(clazz, id, parameters)));
      break;
    case JavaType::TypeChar:
      result_wrapper.Append(static_cast<int>(
          object ? env->CallCharMethodA(object, id, parameters)
                 : env->CallStaticCharMethodA(clazz, id, parameters)));
      break;
    case JavaType::TypeShort:
      result_wrapper.Append(static_cast<int>(
          object ? env->CallShortMethodA(object, id, parameters)
                 : env->CallStaticShortMethodA(clazz, id, parameters)));
      break;
    case JavaType::TypeInt:
      result_wrapper.Append(static_cast<int>(
          object ? env->CallIntMethodA(object, id, parameters)
                 : env->CallStaticIntMethodA(clazz, id, parameters)));
      break;
    case JavaType::TypeLong:
      result_wrapper.Append(static_cast<double>(
          object ? env->CallLongMethodA(object, id, parameters)
                 : env->CallStaticLongMethodA(clazz, id, parameters)));
      break;
    case JavaType::TypeFloat: {
      float result = object
                         ? env->CallFloatMethodA(object, id, parameters)
                         : env->CallStaticFloatMethodA(clazz, id, parameters);
      if (std::isfinite(result)) {
        result_wrapper.Append(static_cast<double>(result));
      } else {
        result_wrapper.Append(base::Value::FromUniquePtrValue(
            GinJavaBridgeValue::CreateNonFiniteValue(result)));
      }
      break;
    }
    case JavaType::TypeDouble: {
      double result = object
                          ? env->CallDoubleMethodA(object, id, parameters)
                          : env->CallStaticDoubleMethodA(clazz, id, parameters);
      if (std::isfinite(result)) {
        result_wrapper.Append(result);
      } else {
        result_wrapper.Append(base::Value::FromUniquePtrValue(
            GinJavaBridgeValue::CreateNonFiniteValue(result)));
      }
      break;
    }
    case JavaType::TypeVoid:
      if (object)
        env->CallVoidMethodA(object, id, parameters);
      else
        env->CallStaticVoidMethodA(clazz, id, parameters);
      result_wrapper.Append(base::Value::FromUniquePtrValue(
          GinJavaBridgeValue::CreateUndefinedValue()));
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to not call methods that
      // return arrays. Spec requires calling the method and converting the
      // result to a JavaScript array.
      result_wrapper.Append(base::Value::FromUniquePtrValue(
          GinJavaBridgeValue::CreateUndefinedValue()));
      break;
    case JavaType::TypeString: {
      jstring java_string = static_cast<jstring>(
          object ? env->CallObjectMethodA(object, id, parameters)
                 : env->CallStaticObjectMethodA(clazz, id, parameters));
      // If an exception was raised, we must clear it before calling most JNI
      // methods. ScopedJavaLocalRef is liable to make such calls, so we test
      // first.
      if (base::android::ClearException(env)) {
        SetInvocationError(
            mojom::GinJavaBridgeError::kGinJavaBridgeJavaExceptionRaised);
        return;
      }
      ScopedJavaLocalRef<jstring> scoped_java_string(env, java_string);
      if (!scoped_java_string.obj()) {
        // LIVECONNECT_COMPLIANCE: Existing behavior is to return undefined.
        // Spec requires returning a null string.
        result_wrapper.Append(base::Value::FromUniquePtrValue(
            GinJavaBridgeValue::CreateUndefinedValue()));
        break;
      }
      result_wrapper.Append(
          base::android::ConvertJavaStringToUTF8(scoped_java_string));
      break;
    }
    case JavaType::TypeObject: {
      // If an exception was raised, we must clear it before calling most JNI
      // methods. ScopedJavaLocalRef is liable to make such calls, so we test
      // first.
      jobject java_object =
          object ? env->CallObjectMethodA(object, id, parameters)
                 : env->CallStaticObjectMethodA(clazz, id, parameters);
      if (base::android::ClearException(env)) {
        SetInvocationError(
            mojom::GinJavaBridgeError::kGinJavaBridgeJavaExceptionRaised);
        return;
      }
      ScopedJavaLocalRef<jobject> scoped_java_object(env, java_object);
      if (!scoped_java_object.obj()) {
        result_wrapper.Append(base::Value());
        break;
      }
      SetObjectResult(scoped_java_object, object_->GetSafeAnnotationClass());
      return;
    }
  }
  // This is for all cases except JavaType::TypeObject.
  if (!base::android::ClearException(env)) {
    SetPrimitiveResult(std::move(result_wrapper));
  } else {
    SetInvocationError(
        mojom::GinJavaBridgeError::kGinJavaBridgeJavaExceptionRaised);
  }
}

}  // namespace content
