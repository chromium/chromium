// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_JAVA_TYPE_H_
#define CONTENT_BROWSER_ANDROID_JAVA_JAVA_TYPE_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"

namespace content {

// The type of a Java value. A light-weight enum-like structure intended for
// use by value and in STL containers.
struct CONTENT_EXPORT JavaType {
  JavaType();
  JavaType(const JavaType& other);
  ~JavaType();
  JavaType& operator=(const JavaType& other);

  // Java's reflection API represents types as a string using an extended
  // 'binary name'.
  static JavaType CreateFromBinaryName(const std::string& binary_name);

  // JNIName is used with FindClass.
  std::string JNIName() const;
  // JNISignature is used for creating method signatures.
  std::string JNISignature() const;

  enum Type {
    TypeBoolean,
    TypeByte,
    TypeChar,
    TypeShort,
    TypeInt,
    TypeLong,
    TypeFloat,
    TypeDouble,
    // This is only used as a return type, so we should never convert from
    // JavaScript with this type.
    TypeVoid,
    TypeArray,
    // We special-case strings, as they get special handling when coercing.
    TypeString,
    TypeObject,
  };

  Type type;
  std::unique_ptr<JavaType> inner_type;  // Used for TypeArray only.
  std::string class_jni_name;  // Used for TypeString and TypeObject only.
  base::android::ScopedJavaGlobalRef<jclass> class_ref;  // TypeObject only.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_JAVA_TYPE_H_
