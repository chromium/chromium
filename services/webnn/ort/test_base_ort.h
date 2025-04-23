// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_TEST_BASE_ORT_H_
#define SERVICES_WEBNN_ORT_TEST_BASE_ORT_H_

#include "testing/gtest/include/gtest/gtest.h"

// GTEST_SKIP() will let method return directly.
#define SKIP_TEST_IF(condition)   \
  do {                            \
    if (condition)                \
      GTEST_SKIP() << #condition; \
  } while (0)

#endif  // SERVICES_WEBNN_ORT_TEST_BASE_ORT_H_
