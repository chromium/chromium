// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_VIEW_ANDROID_DELEGATE_H_
#define CONTENT_TEST_TEST_VIEW_ANDROID_DELEGATE_H_

#include "base/android/jni_android.h"

namespace ui {
class ViewAndroid;
}

namespace content {

// Provides a test version of the ViewAndroidDelegate with a native interface
// that connects to the Java TestViewAndroidDelegate, used to test insetting on
// an RWHVA.
class TestViewAndroidDelegate {
 public:
  TestViewAndroidDelegate();
  ~TestViewAndroidDelegate();
  // Sets up the test delegate.
  // |view_android| is the ViewAndroid to use.
  // This setup must be called before calling |InsetViewportBottom|.
  void SetupTestDelegate(ui::ViewAndroid* view_android);
  // Insets the Visual Viewport bottom.  |SetupTestDelegate| must be called
  // first.
  void InsetViewportBottom(int bottom);

 private:
  base::android::ScopedJavaLocalRef<jobject> j_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestViewAndroidDelegate);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_VIEW_ANDROID_DELEGATE_H_
