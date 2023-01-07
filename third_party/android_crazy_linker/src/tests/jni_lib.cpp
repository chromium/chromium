// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple library that provides JNI_OnLoad() and JNI_OnUnload() hooks.
// Used by test_java_vm.cpp

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VARNAME "TEST_VAR"

extern "C" int JNI_OnLoad(JavaVM* vm, void* reserved) {
  printf("%s: Entering\n", __FUNCTION__);
  const char* env = getenv(VARNAME);
  if (!env || strcmp(env, "INIT")) {
    fprintf(stderr,
            "%s: Env variable %s has invalid value: %s (expected INIT)\n",
            __FUNCTION__,
            VARNAME,
            env);
    exit(1);
  }
  setenv(VARNAME, "LOADED", 1);
  printf("%s: Exiting\n", __FUNCTION__);
  return JNI_VERSION_1_4;
}

extern "C" void JNI_OnUnload(JavaVM* vm, void* reserved) {
  printf("%s: Entering\n", __FUNCTION__);
  const char* env = getenv(VARNAME);
  if (!env || strcmp(env, "LOADED")) {
    fprintf(stderr,
            "%s: Env variable %s has invalid value: %s (expected LOADED)\n",
            __FUNCTION__,
            VARNAME,
            env);
    exit(1);
  }
  setenv(VARNAME, "UNLOADED", 1);
  printf("%s: Exiting\n", __FUNCTION__);
}
