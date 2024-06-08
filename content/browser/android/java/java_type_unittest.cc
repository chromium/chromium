// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/android/java/java_type.h"

#include <stddef.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class JavaTypeTest : public testing::Test {
};

TEST_F(JavaTypeTest, ScalarTypes) {
  struct {
    const char* binary_type;
    JavaType::Type java_type;
    const char* jni_name;
    const char* jni_signature;
  } scalar_types[] = {
    {"boolean", JavaType::TypeBoolean, "Z", "Z"},
    {"byte", JavaType::TypeByte, "B", "B"},
    {"char", JavaType::TypeChar, "C", "C"},
    {"short", JavaType::TypeShort, "S", "S"},
    {"int", JavaType::TypeInt, "I", "I"},
    {"long", JavaType::TypeLong, "J", "J"},
    {"float", JavaType::TypeFloat, "F", "F"},
    {"double", JavaType::TypeDouble, "D", "D"},
    {"void", JavaType::TypeVoid, "V", "V"},
    {"java.lang.String", JavaType::TypeString, "java/lang/String",
     "Ljava/lang/String;"},
    {"java.lang.Object", JavaType::TypeObject, "java/lang/Object",
     "Ljava/lang/Object;"},
    {"my.nested.Type$Foo", JavaType::TypeObject, "my/nested/Type$Foo",
     "Lmy/nested/Type$Foo;"}};
  for (size_t i = 0; i < std::size(scalar_types); ++i) {
    JavaType jt = JavaType::CreateFromBinaryName(scalar_types[i].binary_type);
    EXPECT_EQ(scalar_types[i].java_type, jt.type);
    EXPECT_FALSE(jt.inner_type);
    EXPECT_EQ(scalar_types[i].jni_name, jt.JNIName());
    EXPECT_EQ(scalar_types[i].jni_signature, jt.JNISignature());
  }
}

TEST_F(JavaTypeTest, ArrayTypes) {
  JavaType array_of_boolean = JavaType::CreateFromBinaryName("[Z");
  EXPECT_EQ(JavaType::TypeArray, array_of_boolean.type);
  EXPECT_TRUE(array_of_boolean.inner_type);
  EXPECT_EQ(JavaType::TypeBoolean, array_of_boolean.inner_type->type);
  EXPECT_FALSE(array_of_boolean.inner_type->inner_type);
  EXPECT_EQ("[Z", array_of_boolean.JNIName());
  EXPECT_EQ("[Z", array_of_boolean.JNISignature());

  JavaType array_of_boolean_2d = JavaType::CreateFromBinaryName("[[Z");
  EXPECT_EQ(JavaType::TypeArray, array_of_boolean_2d.type);
  EXPECT_TRUE(array_of_boolean_2d.inner_type);
  EXPECT_EQ(JavaType::TypeArray, array_of_boolean_2d.inner_type->type);
  EXPECT_TRUE(array_of_boolean_2d.inner_type->inner_type);
  EXPECT_EQ(JavaType::TypeBoolean,
            array_of_boolean_2d.inner_type->inner_type->type);
  EXPECT_FALSE(array_of_boolean_2d.inner_type->inner_type->inner_type);
  EXPECT_EQ("[[Z", array_of_boolean_2d.JNIName());
  EXPECT_EQ("[[Z", array_of_boolean_2d.JNISignature());

  JavaType array_of_string =
      JavaType::CreateFromBinaryName("[Ljava.lang.String;");
  EXPECT_EQ(JavaType::TypeArray, array_of_string.type);
  EXPECT_TRUE(array_of_string.inner_type);
  EXPECT_EQ(JavaType::TypeString, array_of_string.inner_type->type);
  EXPECT_FALSE(array_of_string.inner_type->inner_type);
  EXPECT_EQ("[Ljava/lang/String;", array_of_string.JNIName());
  EXPECT_EQ("[Ljava/lang/String;", array_of_string.JNISignature());

  JavaType array_of_object =
      JavaType::CreateFromBinaryName("[Ljava.lang.Object;");
  EXPECT_EQ(JavaType::TypeArray, array_of_object.type);
  EXPECT_TRUE(array_of_object.inner_type);
  EXPECT_EQ(JavaType::TypeObject, array_of_object.inner_type->type);
  EXPECT_FALSE(array_of_object.inner_type->inner_type);
  EXPECT_EQ("[Ljava/lang/Object;", array_of_object.JNIName());
  EXPECT_EQ("[Ljava/lang/Object;", array_of_object.JNISignature());
}

}  // namespace content
