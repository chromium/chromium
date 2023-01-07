// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to test crazy_context_set_java_vm().

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <crazy_linker.h>

#include "test_util.h"

#define VARNAME "TEST_VAR"

static const char kJniLibName[] = "libcrazy_linker_tests_libjni_lib.so";
static void* kJavaVM = (void*)0xdeadcafe;

int main() {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // Expect to find the library in the same directory than this executable.
  crazy_add_search_path_for_address((void*)&main);

  crazy_set_java_vm(kJavaVM, JNI_VERSION_1_2);

  // Load libjni_lib.so, this should invoke its JNI_OnLoad() function
  // automatically.
  setenv(VARNAME, "INIT", 1);
  if (!crazy_library_open(&library, kJniLibName, context))
    Panic("Could not open library: %s\n", crazy_context_get_error(context));

  const char* env = getenv(VARNAME);
  if (strcmp(env, "LOADED"))
    Panic("JNI_OnLoad() hook was not called! %s is %s\n", VARNAME, env);

  crazy_library_close(library);
  env = getenv(VARNAME);
  if (strcmp(env, "UNLOADED"))
    Panic("JNI_OnUnload() hook was not called! %s is %s\n", VARNAME, env);

  // Now, change the minimum JNI version to JNI_VERSION_1_6, which should
  // prevent loading the library properly, since it only supports 1.2.
  crazy_set_java_vm(kJavaVM, JNI_VERSION_1_6);

  setenv(VARNAME, "INIT", 1);
  if (crazy_library_open(&library, kJniLibName, context))
    Panic("Could load the library with JNI_VERSION_1_6 > JNI_VERSION_1_2.");

  // Disable the feature, this shall load the library, but not call the
  // JNI_OnLoad() hook.
  crazy_set_java_vm(nullptr, 0);

  setenv(VARNAME, "INIT", 1);
  if (!crazy_library_open(&library, kJniLibName, context))
    Panic("Could not load the library without a JavaVM handle !?\n");

  env = getenv(VARNAME);
  if (strcmp(env, "INIT"))
    Panic("JNI_OnLoad() was called, %s is %s (expected INIT)\n", VARNAME, env);

  crazy_library_close(library);
  env = getenv(VARNAME);
  if (strcmp(env, "INIT"))
    Panic(
        "JNI_OnUnload() was called, %s is %s (expected INIT)\n", VARNAME, env);

  crazy_context_destroy(context);

  return 0;
}
