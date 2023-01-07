// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/log.h>
#include <stdio.h>

extern "C" void Foo();

extern "C" void Bar() {
  printf("%s: Entering\n", __FUNCTION__);
  __android_log_print(ANDROID_LOG_INFO, "bar", "Hi There!");
  fprintf(stderr, "Hi There! from Bar\n");

  printf("%s: Calling Foo()\n", __FUNCTION__);
  Foo();

  printf("%s: Exiting\n", __FUNCTION__);
}
