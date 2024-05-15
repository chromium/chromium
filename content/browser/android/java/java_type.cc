// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/java_type.h"

#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace content {
namespace {

// Array component type names are similar to JNI type names, except for using
// dots as namespace separators in class names.
std::unique_ptr<JavaType> CreateFromArrayComponentTypeName(
    const std::string& type_name) {
  std::unique_ptr<JavaType> result(new JavaType());
  DCHECK(!type_name.empty());
  switch (type_name[0]) {
    case 'Z':
      result->type = JavaType::TypeBoolean;
      break;
    case 'B':
      result->type = JavaType::TypeByte;
      break;
    case 'C':
      result->type = JavaType::TypeChar;
      break;
    case 'S':
      result->type = JavaType::TypeShort;
      break;
    case 'I':
      result->type = JavaType::TypeInt;
      break;
    case 'J':
      result->type = JavaType::TypeLong;
      break;
    case 'F':
      result->type = JavaType::TypeFloat;
      break;
    case 'D':
      result->type = JavaType::TypeDouble;
      break;
    case '[':
      result->type = JavaType::TypeArray;
      result->inner_type =
          CreateFromArrayComponentTypeName(type_name.substr(1));
      break;
    case 'L':
      if (type_name == "Ljava.lang.String;") {
        result->type = JavaType::TypeString;
        result->class_jni_name = "java/lang/String";
      } else {
        result->type = JavaType::TypeObject;
        result->class_jni_name = type_name.substr(1, type_name.length() - 2);
        base::ReplaceSubstringsAfterOffset(
            &result->class_jni_name, 0, ".", "/");
      }
      break;
    default:
      // Includes void (V).
      NOTREACHED_IN_MIGRATION();
  }
  return result;
}

}  // namespace

JavaType::JavaType() {
}

JavaType::JavaType(const JavaType& other) {
  *this = other;
}

JavaType::~JavaType() {
}

JavaType& JavaType::operator=(const JavaType& other) {
  if (this == &other)
    return *this;
  type = other.type;
  if (other.inner_type) {
    DCHECK_EQ(JavaType::TypeArray, type);
    inner_type = std::make_unique<JavaType>(*other.inner_type);
  } else {
    inner_type.reset();
  }
  class_jni_name = other.class_jni_name;
  return *this;
}

// static
JavaType JavaType::CreateFromBinaryName(const std::string& binary_name) {
  JavaType result;
  DCHECK(!binary_name.empty());
  if (binary_name == "boolean") {
    result.type = JavaType::TypeBoolean;
  } else if (binary_name == "byte") {
    result.type = JavaType::TypeByte;
  } else if (binary_name == "char") {
    result.type = JavaType::TypeChar;
  } else if (binary_name == "short") {
    result.type = JavaType::TypeShort;
  } else if (binary_name == "int") {
    result.type = JavaType::TypeInt;
  } else if (binary_name == "long") {
    result.type = JavaType::TypeLong;
  } else if (binary_name == "float") {
    result.type = JavaType::TypeFloat;
  } else if (binary_name == "double") {
    result.type = JavaType::TypeDouble;
  } else if (binary_name == "void") {
    result.type = JavaType::TypeVoid;
  } else if (binary_name[0] == '[') {
    result.type = JavaType::TypeArray;
    result.inner_type = CreateFromArrayComponentTypeName(binary_name.substr(1));
  } else if (binary_name == "java.lang.String") {
    result.type = JavaType::TypeString;
    result.class_jni_name = "java/lang/String";
  } else {
    result.type = JavaType::TypeObject;
    result.class_jni_name = binary_name;
    base::ReplaceSubstringsAfterOffset(&result.class_jni_name, 0, ".", "/");
  }
  return result;
}

std::string JavaType::JNIName() const {
  switch (type) {
    case JavaType::TypeBoolean:
      return "Z";
    case JavaType::TypeByte:
      return "B";
    case JavaType::TypeChar:
      return "C";
    case JavaType::TypeShort:
      return "S";
    case JavaType::TypeInt:
      return "I";
    case JavaType::TypeLong:
      return "J";
    case JavaType::TypeFloat:
      return "F";
    case JavaType::TypeDouble:
      return "D";
    case JavaType::TypeVoid:
      return "V";
    case JavaType::TypeArray:
      return "[" + inner_type->JNISignature();
    case JavaType::TypeString:
    case JavaType::TypeObject:
      return class_jni_name;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string JavaType::JNISignature() const {
  if (type == JavaType::TypeString || type == JavaType::TypeObject)
    return "L" + JNIName() + ";";
  else
    return JNIName();
}

}  // namespace content
