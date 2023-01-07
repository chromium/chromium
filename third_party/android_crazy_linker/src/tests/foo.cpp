// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/log.h>
#include <stdio.h>

extern "C" void Foo() {
  printf("%s: Entering\n", __FUNCTION__);
  __android_log_write(ANDROID_LOG_INFO, "foo", "Hello World!");
  fprintf(stderr, "Hello World from Foo!\n");
  printf("%s: Exiting\n", __FUNCTION__);
}
