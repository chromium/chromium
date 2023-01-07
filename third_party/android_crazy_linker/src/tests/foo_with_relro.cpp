// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This is a large table that contains pointers to ensure that it
// gets put inside the RELRO section.
#define LINE "some example string",
#define LINE8 LINE LINE LINE LINE LINE LINE LINE LINE
#define LINE64 LINE8 LINE8 LINE8 LINE8 LINE8 LINE8 LINE8 LINE8
#define LINE512 LINE64 LINE64 LINE64 LINE64 LINE64 LINE64 LINE64 LINE64
#define LINE4096 LINE512 LINE512 LINE512 LINE512 LINE512 LINE512 LINE512 LINE512

const char* const kStrings[] = {LINE4096 LINE4096 LINE4096 LINE4096};

extern "C" void Foo() {
  printf("%s: Entering\n", __FUNCTION__);
  for (size_t n = 0; n < sizeof(kStrings) / sizeof(kStrings[0]); ++n) {
    const char* ptr = kStrings[n];
    if (strcmp(ptr, "some example string")) {
      printf("%s: Bad string at offset=%zu\n", __FUNCTION__, n);
      exit(1);
    }
  }
  printf("%s: Exiting\n", __FUNCTION__);
}
