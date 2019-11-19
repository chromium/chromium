// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_rdebug.h"

#include <gtest/gtest.h>

namespace crazy {

TEST(RDebug, GetAddress) {
  RDebug rdebug;
  ASSERT_TRUE(rdebug.GetAddress()) << "Cannot find global |r_debug| address!";
}

}  // namespace crazy
