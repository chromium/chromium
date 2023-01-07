// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>

#include "eightball.h"

extern "C" const char* Magic8Ball() {
  const int NSIDES = 8;
  const char* answer[NSIDES] = { "YES", "NO", "MAYBE", "MAYBE NOT",
                                 "DEFINITELY", "ASK ME TOMORROW",
                                 "PARTLY CLOUDY", "42", };
  return answer[rand() % NSIDES];
}
