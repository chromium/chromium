// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/android/net_test_jni_onload.h"

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "net/test/embedded_test_server/android/embedded_test_server_android.h"

namespace net::test {

bool OnJNIOnLoadInit() {
  return base::android::OnJNIOnLoadInit();
}

}  // namespace net::test
