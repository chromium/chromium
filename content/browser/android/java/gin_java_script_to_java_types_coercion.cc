// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_script_to_java_types_coercion.h"

#include <stdint.h>
#include <unistd.h>

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/android/gin_java_bridge_value.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

const char kJavaLangString[] = "java/lang/String";
const char kUndefined[] = "undefined";

double RoundDoubleTowardsZero(const double& x) {
  if (std::isnan(x)) {
    return 0.0;
  }
  return x > 0.0 ? floor(x) : ceil(x);
}

// Rounds to jlong using Java's type conversion rules.
jlong RoundDoubleToLong(const double& x) {
  double intermediate = RoundDoubleTowardsZero(x);
  // The int64_t limits can not be converted exactly to double values, so we
  // compare to custom constants. kint64max is 2^63 - 1, but the spacing
  // between double values in the the range 2^62 to 2^63 is 2^10. The cast is
  // required to silence a spurious gcc warning for integer overflow.
  const int64_t kLimit = (INT64_C(1) << 63) - static_cast<uint64_t>(1 << 10);
  DCHECK(kLimit > 0);
  const double kLargestDoubleLessThanInt64Max = kLimit;
  const double kSmallestDoubleGreaterThanInt64Min = -kLimit;
  if (intermediate > kLargestDoubleLessThanInt64Max) {
    return std::numeric_limits<int64_t>::max();
  }
  if (intermediate < kSmallestDoubleGreaterThanInt64Min) {
    return std::numeric_limits<int64_t>::min();
  }
  return static_cast<jlong>(intermediate);
}

// Rounds to jint using Java's type conversion rules.
jint RoundDoubleToInt(const double& x) {
  double intermediate = RoundDoubleTowardsZero(x);
  // The int32_t limits cast exactly to double values.
  intermediate = std::min(
      intermediate, static_cast<double>(std::numeric_limits<int32_t>::max()));
  intermediate = std::max(
      intermediate, static_cast<double>(std::numeric_limits<int32_t>::min()));
  return static_cast<jint>(intermediate);
}

jvalue CoerceJavaScriptIntegerToJavaValue(JNIEnv* env,
                                          const base::Value* value,
                                          const JavaType& target_type,
                                          bool coerce_to_string,
                                          GinJavaBridgeError* error) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_NUMBER_VALUES.

  // For conversion to numeric types, we need to replicate Java's type
  // conversion rules. This requires that for integer values, we simply discard
  // all but the lowest n buts, where n is the number of bits in the target
  // type.
  jvalue result;
  int int_value;
  value->GetAsInteger(&int_value);
  switch (target_type.type) {
    case JavaType::TypeByte:
      result.b = static_cast<jbyte>(int_value);
      break;
    case JavaType::TypeChar:
      result.c = static_cast<jchar>(int_value);
      break;
    case JavaType::TypeShort:
      result.s = static_cast<jshort>(int_value);
      break;
    case JavaType::TypeInt:
      result.i = int_value;
      break;
    case JavaType::TypeLong:
      result.j = int_value;
      break;
    case JavaType::TypeFloat:
      result.f = int_value;
      break;
    case JavaType::TypeDouble:
      result.d = int_value;
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
      // requires handling object equivalents of primitive types.
      result.l = NULL;
      break;
    case JavaType::TypeString:
      result.l = coerce_to_string ? ConvertUTF8ToJavaString(
                                        env, base::NumberToString(int_value))
                                        .Release()
                                  : NULL;
      break;
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
      // requires converting to false for 0 or NaN, true otherwise.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptDoubleToJavaValue(JNIEnv* env,
                                         double double_value,
                                         const JavaType& target_type,
                                         bool coerce_to_string,
                                         GinJavaBridgeError* error) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_NUMBER_VALUES.
  // For conversion to numeric types, we need to replicate Java's type
  // conversion rules.
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeByte:
      result.b = static_cast<jbyte>(RoundDoubleToInt(double_value));
      break;
    case JavaType::TypeChar:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert double to 0.
      // Spec requires converting doubles similarly to how we convert doubles to
      // other numeric types.
      result.c = 0;
      break;
    case JavaType::TypeShort:
      result.s = static_cast<jshort>(RoundDoubleToInt(double_value));
      break;
    case JavaType::TypeInt:
      result.i = RoundDoubleToInt(double_value);
      break;
    case JavaType::TypeLong:
      result.j = RoundDoubleToLong(double_value);
      break;
    case JavaType::TypeFloat:
      result.f = static_cast<jfloat>(double_value);
      break;
    case JavaType::TypeDouble:
      result.d = double_value;
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
      // requires handling object equivalents of primitive types.
      result.l = NULL;
      break;
    case JavaType::TypeString:
      result.l =
          coerce_to_string
              ? ConvertUTF8ToJavaString(
                    env, base::StringPrintf("%.6lg", double_value)).Release()
              : NULL;
      break;
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
      // requires converting to false for 0 or NaN, true otherwise.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to null. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptBooleanToJavaValue(JNIEnv* env,
                                          const base::Value* value,
                                          const JavaType& target_type,
                                          bool coerce_to_string,
                                          GinJavaBridgeError* error) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_BOOLEAN_VALUES.
  bool boolean_value;
  value->GetAsBoolean(&boolean_value);
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeBoolean:
      result.z = boolean_value ? JNI_TRUE : JNI_FALSE;
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires handling java.lang.Boolean and java.lang.Object.
      result.l = NULL;
      break;
    case JavaType::TypeString:
      result.l = coerce_to_string
                     ? ConvertUTF8ToJavaString(
                           env, boolean_value ? "true" : "false").Release()
                     : NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeChar:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires converting to 0 or 1.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptStringToJavaValue(JNIEnv* env,
                                         const base::Value* value,
                                         const JavaType& target_type,
                                         GinJavaBridgeError* error) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_STRING_VALUES.
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeString: {
      std::string string_result;
      value->GetAsString(&string_result);
      result.l = ConvertUTF8ToJavaString(env, string_result).Release();
      break;
    }
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires handling java.lang.Object.
      result.l = NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires using valueOf() method of corresponding object type.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeChar:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires using java.lang.Short.decode().
      result.c = 0;
      break;
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
      // requires converting the empty string to false, otherwise true.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

// Note that this only handles primitive types and strings.
jobject CreateJavaArray(JNIEnv* env, const JavaType& type, jsize length) {
  switch (type.type) {
    case JavaType::TypeBoolean:
      return env->NewBooleanArray(length);
    case JavaType::TypeByte:
      return env->NewByteArray(length);
    case JavaType::TypeChar:
      return env->NewCharArray(length);
    case JavaType::TypeShort:
      return env->NewShortArray(length);
    case JavaType::TypeInt:
      return env->NewIntArray(length);
    case JavaType::TypeLong:
      return env->NewLongArray(length);
    case JavaType::TypeFloat:
      return env->NewFloatArray(length);
    case JavaType::TypeDouble:
      return env->NewDoubleArray(length);
    case JavaType::TypeString: {
      ScopedJavaLocalRef<jclass> clazz(
          base::android::GetClass(env, kJavaLangString));
      return env->NewObjectArray(length, clazz.obj(), NULL);
    }
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
    case JavaType::TypeArray:
    case JavaType::TypeObject:
      // Not handled.
      NOTREACHED();
  }
  return NULL;
}

// Sets the specified element of the supplied array to the value of the
// supplied jvalue. Requires that the type of the array matches that of the
// jvalue. Handles only primitive types and strings. Note that in the case of a
// string, the array takes a new reference to the string object.
void SetArrayElement(JNIEnv* env,
                     jobject array,
                     const JavaType& type,
                     jsize index,
                     const jvalue& value) {
  switch (type.type) {
    case JavaType::TypeBoolean:
      env->SetBooleanArrayRegion(static_cast<jbooleanArray>(array), index, 1,
                                 &value.z);
      break;
    case JavaType::TypeByte:
      env->SetByteArrayRegion(static_cast<jbyteArray>(array), index, 1,
                              &value.b);
      break;
    case JavaType::TypeChar:
      env->SetCharArrayRegion(static_cast<jcharArray>(array), index, 1,
                              &value.c);
      break;
    case JavaType::TypeShort:
      env->SetShortArrayRegion(static_cast<jshortArray>(array), index, 1,
                               &value.s);
      break;
    case JavaType::TypeInt:
      env->SetIntArrayRegion(static_cast<jintArray>(array), index, 1,
                             &value.i);
      break;
    case JavaType::TypeLong:
      env->SetLongArrayRegion(static_cast<jlongArray>(array), index, 1,
                              &value.j);
      break;
    case JavaType::TypeFloat:
      env->SetFloatArrayRegion(static_cast<jfloatArray>(array), index, 1,
                               &value.f);
      break;
    case JavaType::TypeDouble:
      env->SetDoubleArrayRegion(static_cast<jdoubleArray>(array), index, 1,
                                &value.d);
      break;
    case JavaType::TypeString:
      env->SetObjectArrayElement(static_cast<jobjectArray>(array), index,
                                 value.l);
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
    case JavaType::TypeArray:
    case JavaType::TypeObject:
      // Not handled.
      NOTREACHED();
  }
  base::android::CheckException(env);
}

jvalue CoerceJavaScriptNullOrUndefinedToJavaValue(JNIEnv* env,
                                                  const base::Value* value,
                                                  const JavaType& target_type,
                                                  bool coerce_to_string,
                                                  GinJavaBridgeError* error) {
  bool is_undefined = false;
  std::unique_ptr<const GinJavaBridgeValue> gin_value;
  if (GinJavaBridgeValue::ContainsGinJavaBridgeValue(value)) {
    gin_value = GinJavaBridgeValue::FromValue(value);
    if (gin_value->IsType(GinJavaBridgeValue::TYPE_UNDEFINED)) {
      is_undefined = true;
    }
  }
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeObject:
      result.l = NULL;
      break;
    case JavaType::TypeString:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert undefined to
      // "undefined". Spec requires converting undefined to NULL.
      result.l = (coerce_to_string && is_undefined)
                     ? ConvertUTF8ToJavaString(env, kUndefined).Release()
                     : NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeChar:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeBoolean:
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to NULL. Spec
      // requires raising a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jobject CoerceJavaScriptListToArray(JNIEnv* env,
                                    const base::Value* value,
                                    const JavaType& target_type,
                                    const ObjectRefs& object_refs,
                                    GinJavaBridgeError* error) {
  DCHECK_EQ(JavaType::TypeArray, target_type.type);
  const JavaType& target_inner_type = *target_type.inner_type.get();
  // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for
  // multi-dimensional arrays. Spec requires handling multi-demensional arrays.
  if (target_inner_type.type == JavaType::TypeArray) {
    return NULL;
  }

  // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for object
  // arrays. Spec requires handling object arrays.
  if (target_inner_type.type == JavaType::TypeObject) {
    return NULL;
  }

  const base::ListValue* list_value;
  value->GetAsList(&list_value);
  // Create the Java array.
  jsize length = static_cast<jsize>(list_value->GetSize());
  jobject result = CreateJavaArray(env, target_inner_type, length);
  if (!result) {
    return NULL;
  }
  auto null_value = std::make_unique<base::Value>();
  for (jsize i = 0; i < length; ++i) {
    const base::Value* value_element = null_value.get();
    list_value->Get(i, &value_element);
    jvalue element = CoerceJavaScriptValueToJavaValue(
        env, value_element, target_inner_type, false, object_refs, error);
    SetArrayElement(env, result, target_inner_type, i, element);
    // CoerceJavaScriptValueToJavaValue() creates new local references to
    // strings, objects and arrays. Of these, only strings can occur here.
    // SetArrayElement() causes the array to take its own reference to the
    // string, so we can now release the local reference.
    DCHECK_NE(JavaType::TypeObject, target_inner_type.type);
    DCHECK_NE(JavaType::TypeArray, target_inner_type.type);
    ReleaseJavaValueIfRequired(env, &element, target_inner_type);
  }

  return result;
}

jobject CoerceJavaScriptDictionaryToArray(JNIEnv* env,
                                          const base::Value* value,
                                          const JavaType& target_type,
                                          const ObjectRefs& object_refs,
                                          GinJavaBridgeError* error) {
  DCHECK_EQ(JavaType::TypeArray, target_type.type);

  const JavaType& target_inner_type = *target_type.inner_type.get();
  // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for
  // multi-dimensional arrays. Spec requires handling multi-demensional arrays.
  if (target_inner_type.type == JavaType::TypeArray) {
    return NULL;
  }

  // LIVECONNECT_COMPLIANCE: Existing behavior is to return null for object
  // arrays. Spec requires handling object arrays.
  if (target_inner_type.type == JavaType::TypeObject) {
    return NULL;
  }

  const base::DictionaryValue* dictionary_value;
  value->GetAsDictionary(&dictionary_value);
  const base::Value* length_value;
  // If the object does not have a length property, return null.
  if (!dictionary_value->Get("length", &length_value)) {
    return NULL;
  }

  // If the length property does not have numeric type, or is outside the valid
  // range for a Java array length, return null.
  jsize length = -1;
  if (length_value->is_int()) {
    int int_length;
    length_value->GetAsInteger(&int_length);
    if (int_length >= 0 && int_length <= std::numeric_limits<int32_t>::max()) {
      length = static_cast<jsize>(int_length);
    }
  } else if (length_value->is_double()) {
    double double_length;
    length_value->GetAsDouble(&double_length);
    if (double_length >= 0.0 &&
        double_length <= std::numeric_limits<int32_t>::max()) {
      length = static_cast<jsize>(double_length);
    }
  }
  if (length == -1) {
    return NULL;
  }

  jobject result = CreateJavaArray(env, target_inner_type, length);
  if (!result) {
    return NULL;
  }
  auto null_value = std::make_unique<base::Value>();
  for (jsize i = 0; i < length; ++i) {
    const std::string key(base::NumberToString(i));
    const base::Value* value_element = null_value.get();
    if (dictionary_value->HasKey(key)) {
      dictionary_value->Get(key, &value_element);
    }
    jvalue element = CoerceJavaScriptValueToJavaValue(
        env, value_element, target_inner_type, false, object_refs, error);
    SetArrayElement(env, result, target_inner_type, i, element);
    // CoerceJavaScriptValueToJavaValue() creates new local references to
    // strings, objects and arrays. Of these, only strings can occur here.
    // SetArrayElement() causes the array to take its own reference to the
    // string, so we can now release the local reference.
    DCHECK_NE(JavaType::TypeObject, target_inner_type.type);
    DCHECK_NE(JavaType::TypeArray, target_inner_type.type);
    ReleaseJavaValueIfRequired(env, &element, target_inner_type);
  }

  return result;
}

jvalue CoerceJavaScriptObjectToJavaValue(JNIEnv* env,
                                         const base::Value* value,
                                         const JavaType& target_type,
                                         bool coerce_to_string,
                                         const ObjectRefs& object_refs,
                                         GinJavaBridgeError* error) {
  // This covers both JavaScript objects (including arrays) and Java objects.
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_OTHER_OBJECTS,
  // http://jdk6.java.net/plugin2/liveconnect/#JS_ARRAY_VALUES and
  // http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_OBJECTS
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeObject: {
      if (GinJavaBridgeValue::ContainsGinJavaBridgeValue(value)) {
        std::unique_ptr<const GinJavaBridgeValue> gin_value(
            GinJavaBridgeValue::FromValue(value));
        DCHECK(gin_value);
        DCHECK(gin_value->IsType(GinJavaBridgeValue::TYPE_OBJECT_ID));
        ScopedJavaLocalRef<jobject> obj;
        GinJavaBoundObject::ObjectID object_id;
        if (gin_value->GetAsObjectID(&object_id)) {
          ObjectRefs::const_iterator iter = object_refs.find(object_id);
          if (iter != object_refs.end()) {
            obj.Reset(iter->second.get(env));
          }
        }
        DCHECK(!target_type.class_ref.is_null());
        DCHECK(!obj.is_null());
        if (env->IsInstanceOf(obj.obj(), target_type.class_ref.obj()) ==
            JNI_TRUE) {
          result.l = obj.Release();
        } else {
          result.l = NULL;
          *error = kGinJavaBridgeNonAssignableTypes;
        }
      } else {
        // LIVECONNECT_COMPLIANCE: Existing behavior is to pass null. Spec
        // requires converting if the target type is
        // netscape.javascript.JSObject, otherwise raising a JavaScript
        // exception.
        result.l = NULL;
      }
      break;
    }
    case JavaType::TypeString:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to
      // "undefined". Spec requires calling toString() on the Java object.
      result.l = coerce_to_string
                     ? ConvertUTF8ToJavaString(env, kUndefined).Release()
                     : NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble:
    case JavaType::TypeChar: {
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to 0. Spec
      // requires raising a JavaScript exception.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Existing behavior is to convert to false. Spec
      // requires raising a JavaScript exception.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      if (value->is_dict()) {
        result.l = CoerceJavaScriptDictionaryToArray(
            env, value, target_type, object_refs, error);
      } else if (value->is_list()) {
        result.l = CoerceJavaScriptListToArray(
            env, value, target_type, object_refs, error);
      } else {
        result.l = NULL;
      }
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceGinJavaBridgeValueToJavaValue(JNIEnv* env,
                                           const base::Value* value,
                                           const JavaType& target_type,
                                           bool coerce_to_string,
                                           const ObjectRefs& object_refs,
                                           GinJavaBridgeError* error) {
  DCHECK(GinJavaBridgeValue::ContainsGinJavaBridgeValue(value));
  std::unique_ptr<const GinJavaBridgeValue> gin_value(
      GinJavaBridgeValue::FromValue(value));
  switch (gin_value->GetType()) {
    case GinJavaBridgeValue::TYPE_UNDEFINED:
      return CoerceJavaScriptNullOrUndefinedToJavaValue(
          env, value, target_type, coerce_to_string, error);
    case GinJavaBridgeValue::TYPE_NONFINITE: {
      float float_value;
      gin_value->GetAsNonFinite(&float_value);
      return CoerceJavaScriptDoubleToJavaValue(
          env, float_value, target_type, coerce_to_string, error);
    }
    case GinJavaBridgeValue::TYPE_OBJECT_ID:
      return CoerceJavaScriptObjectToJavaValue(
          env, value, target_type, coerce_to_string, object_refs, error);
    default:
      NOTREACHED();
  }
  return jvalue();
}

}  // namespace


void ReleaseJavaValueIfRequired(JNIEnv* env,
                                jvalue* value,
                                const JavaType& type) {
  if (type.type == JavaType::TypeString || type.type == JavaType::TypeObject ||
      type.type == JavaType::TypeArray) {
    env->DeleteLocalRef(value->l);
    value->l = NULL;
  }
}

jvalue CoerceJavaScriptValueToJavaValue(JNIEnv* env,
                                        const base::Value* value,
                                        const JavaType& target_type,
                                        bool coerce_to_string,
                                        const ObjectRefs& object_refs,
                                        GinJavaBridgeError* error) {
  // Note that in all these conversions, the relevant field of the jvalue must
  // always be explicitly set, as jvalue does not initialize its fields.

  switch (value->type()) {
    case base::Value::Type::INTEGER:
      return CoerceJavaScriptIntegerToJavaValue(
          env, value, target_type, coerce_to_string, error);
    case base::Value::Type::DOUBLE: {
      double double_value;
      value->GetAsDouble(&double_value);
      return CoerceJavaScriptDoubleToJavaValue(
          env, double_value, target_type, coerce_to_string, error);
    }
    case base::Value::Type::BOOLEAN:
      return CoerceJavaScriptBooleanToJavaValue(
          env, value, target_type, coerce_to_string, error);
    case base::Value::Type::STRING:
      return CoerceJavaScriptStringToJavaValue(env, value, target_type, error);
    case base::Value::Type::DICTIONARY:
    case base::Value::Type::LIST:
      return CoerceJavaScriptObjectToJavaValue(
          env, value, target_type, coerce_to_string, object_refs, error);
    case base::Value::Type::NONE:
      return CoerceJavaScriptNullOrUndefinedToJavaValue(
          env, value, target_type, coerce_to_string, error);
    case base::Value::Type::BINARY:
      return CoerceGinJavaBridgeValueToJavaValue(
          env, value, target_type, coerce_to_string, object_refs, error);
    // TODO(crbug.com/859477): Remove after root cause is found.
    case base::Value::Type::DEAD:
      CHECK(false);
      return jvalue();
  }

  // TODO(crbug.com/859477): Revert to NOTREACHED() after root cause is found.
  CHECK(false);
  return jvalue();
}

}  // namespace content
