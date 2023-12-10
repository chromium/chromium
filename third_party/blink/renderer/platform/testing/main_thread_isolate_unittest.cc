// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/main_thread_isolate.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MainThreadIsolate, Simple) {
  base::test::TaskEnvironment task_environment;
  test::MainThreadIsolate main_thread_isolate;
}

}  // namespace blink
