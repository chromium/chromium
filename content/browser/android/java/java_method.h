// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_JAVA_METHOD_H_
#define CONTENT_BROWSER_ANDROID_JAVA_JAVA_METHOD_H_

#include <jni.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "content/browser/android/java/java_type.h"
#include "content/common/content_export.h"

namespace content {

// Wrapper around java.lang.reflect.Method. This class must be used on a single
// thread only.
class CONTENT_EXPORT JavaMethod {
 public:
  JavaMethod() = delete;

  explicit JavaMethod(const base::android::JavaRef<jobject>& method);

  JavaMethod(const JavaMethod&) = delete;
  JavaMethod& operator=(const JavaMethod&) = delete;

  ~JavaMethod();

  const std::string& name() const { return name_; }
  size_t num_parameters() const;
  bool is_static() const;
  const JavaType& parameter_type(size_t index) const;
  const JavaType& return_type() const;
  jmethodID id() const;

 private:
  void EnsureNumParametersIsSetUp() const;
  void EnsureTypesAndIDAreSetUp() const;

  std::string name_;
  mutable base::android::ScopedJavaGlobalRef<jobject> java_method_;
  mutable bool have_calculated_num_parameters_;
  mutable size_t num_parameters_;
  mutable std::vector<JavaType> parameter_types_;
  mutable JavaType return_type_;
  mutable bool is_static_;
  mutable jmethodID id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_JAVA_METHOD_H_
