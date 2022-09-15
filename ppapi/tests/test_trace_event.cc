// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_trace_event.h"

#include <stdint.h>

#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(TraceEvent);

TestTraceEvent::TestTraceEvent(TestingInstance* instance)
    : TestCase(instance),
      interface_(NULL) {
}

bool TestTraceEvent::Init() {
  interface_ = static_cast<const PPB_Trace_Event_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TRACE_EVENT_DEV_INTERFACE));
  return !!interface_;
}

void TestTraceEvent::RunTests(const std::string& filter) {
  RUN_TEST(Smoke, filter);
  RUN_TEST(SmokeWithTimestamps, filter);
  RUN_TEST(Clock, filter);
}

std::string TestTraceEvent::TestSmoke() {
  // This test does not verify the log message actually reaches dev tracing, but
  // it does test that the interface exists and that it can be called without
  // crashing.
  const void* cat_enabled = interface_->GetCategoryEnabled("ppapi");
  interface_->AddTraceEvent('B', cat_enabled, "foo", 0, 0, NULL, NULL, NULL, 0);
  interface_->AddTraceEvent('E', cat_enabled, "foo", 0, 0, NULL, NULL, NULL, 0);
  PASS();
}

std::string TestTraceEvent::TestSmokeWithTimestamps() {
  // This test does not verify the log message actually reaches dev tracing, but
  // it does test that the interface exists and that it can be called without
  // crashing.
  const void* cat_enabled = interface_->GetCategoryEnabled("ppapi");
  interface_->AddTraceEventWithThreadIdAndTimestamp(
    'B', cat_enabled, "foo", 0, 0, 42, 0, NULL, NULL, NULL, 0);
  interface_->AddTraceEventWithThreadIdAndTimestamp(
    'B', cat_enabled, "foo", 0, 1, 43, 0, NULL, NULL, NULL, 0);
  interface_->AddTraceEventWithThreadIdAndTimestamp(
    'E', cat_enabled, "foo", 0, 0, 44, 0, NULL, NULL, NULL, 0);
  interface_->AddTraceEventWithThreadIdAndTimestamp(
    'E', cat_enabled, "foo", 0, 1, 45, 0, NULL, NULL, NULL, 0);
  PASS();
}

std::string TestTraceEvent::TestClock() {
  int64_t last = interface_->Now();

  for(int i=0; i<5; ++i){
    int64_t next = interface_->Now();
    ASSERT_LE(last, next);
    last = next;
  }

  PASS();
}
