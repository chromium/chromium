// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_core.h"

#include "ppapi/cpp/core.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Core);

bool TestCore::Init() {
  return true;
}

void TestCore::RunTests(const std::string& filter) {
  RUN_TEST(Time, filter);
  RUN_TEST(TimeTicks, filter);
}

std::string TestCore::TestTime() {
  pp::Core* core = pp::Module::Get()->core();
  PP_Time time1 = core->GetTime();
  ASSERT_TRUE(time1 > 0);

  PlatformSleep(100);  // 0.1 second

  PP_Time time2 = core->GetTime();
  ASSERT_TRUE(time2 > time1);

  PASS();
}

std::string TestCore::TestTimeTicks() {
  pp::Core* core = pp::Module::Get()->core();
  PP_Time time1 = core->GetTimeTicks();
  ASSERT_TRUE(time1 > 0);

  PlatformSleep(100);  // 0.1 second

  PP_Time time2 = core->GetTimeTicks();
  ASSERT_TRUE(time2 > time1);

  PASS();
}

