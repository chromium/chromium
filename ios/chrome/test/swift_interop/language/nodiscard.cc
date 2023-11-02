// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nodiscard.h"

[[nodiscard]] int NoDiscardAdd(int x, int y) {
  return x + y;
}

NoDiscardError NoDiscardReturnError(int x, int y) {
  auto z = x + y;
  NoDiscardError e(z);
  return e;
}
