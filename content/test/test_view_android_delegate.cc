// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_view_android_delegate.h"

#include "base/android/scoped_java_ref.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/test/content_unittests_jni_headers/TestViewAndroidDelegate_jni.h"

namespace content {

TestViewAndroidDelegate::TestViewAndroidDelegate() {}
TestViewAndroidDelegate::~TestViewAndroidDelegate() {}

void TestViewAndroidDelegate::SetupTestDelegate(ui::ViewAndroid* view_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto test_delegate = Java_TestViewAndroidDelegate_create(env);
  view_android->SetDelegate(test_delegate);
  j_delegate_.Reset(test_delegate);
}

void TestViewAndroidDelegate::InsetViewportBottom(int bottom) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestViewAndroidDelegate_insetViewportBottom(env, j_delegate_, bottom);
}

}  // namespace content
