// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "outlined_class.h"

Outlined::Outlined() : sum_(0) {}

Outlined::Outlined(int initial_value) : sum_(initial_value) {}

int Outlined::OutlinedAddition(int delta) {
  sum_ += delta;
  return sum_;
}
