// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A variant of bar.cpp that also includes a large RELRO section.
// Used to test RELRO sharing with two different libraries at the
// same time.

// This is a large table that contains pointers to ensure that it
// gets put inside the RELRO section.
#define LINE "another example string",
#define LINE8 LINE LINE LINE LINE LINE LINE LINE LINE
#define LINE64 LINE8 LINE8 LINE8 LINE8 LINE8 LINE8 LINE8 LINE8
#define LINE512 LINE64 LINE64 LINE64 LINE64 LINE64 LINE64 LINE64 LINE64
#define LINE4096 LINE512 LINE512 LINE512 LINE512 LINE512 LINE512 LINE512 LINE512

const char* const kStrings[] = {LINE4096 LINE4096 LINE4096 LINE4096};

extern "C" void Foo();

extern "C" void Bar() {
  printf("%s: Entering\n", __FUNCTION__);
  __android_log_print(ANDROID_LOG_INFO, "bar", "Hi There!");
  fprintf(stderr, "Hi There! from Bar\n");

  for (size_t n = 0; n < sizeof(kStrings) / sizeof(kStrings[0]); ++n) {
    const char* ptr = kStrings[n];
    if (strcmp(ptr, "another example string")) {
      printf("%s: Bad string at offset=%zu\n", __FUNCTION__, n);
      exit(1);
    }
  }

  printf("%s: Calling Foo()\n", __FUNCTION__);
  Foo();

  printf("%s: Exiting\n", __FUNCTION__);
}
