// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reverse.h"
#include <stdlib.h>
#include <string.h>

extern "C" char* Reverse(const char* s) {
  size_t len = strlen(s);
  char* reversed = static_cast<char*>(malloc(len + 1));
  for (int i = len - 1; i >= 0; --i)
    reversed[len - i - 1] = s[i];
  reversed[len] = 0;
  return reversed;
}
