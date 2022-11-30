// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TRACE_EVENT_H_
#define PPAPI_TESTS_TEST_TRACE_EVENT_H_

#include <string>

#include "ppapi/c/dev/ppb_trace_event_dev.h"
#include "ppapi/tests/test_case.h"

class TestTraceEvent : public TestCase {
 public:
  explicit TestTraceEvent(TestingInstance* instance);

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestSmoke();
  std::string TestSmokeWithTimestamps();
  std::string TestClock();

  const PPB_Trace_Event_Dev* interface_;
};

#endif  // PPAPI_TESTS_TEST_TRACE_EVENT_H_
