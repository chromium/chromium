// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_MOUSE_LOCK_H_
#define PPAPI_TESTS_TEST_MOUSE_LOCK_H_

#include <string>

#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

class TestMouseLock: public TestCase, public pp::MouseLock {
 public:
  explicit TestMouseLock(TestingInstance* instance);
  virtual ~TestMouseLock();

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);
  virtual void DidChangeView(const pp::View& view);

  // pp::MouseLock implementation.
  virtual void MouseLockLost();

 private:
  std::string TestSucceedWhenAllowed();

  void SimulateUserGesture();

  pp::Rect position_;

  NestedEvent nested_event_;
};

#endif  // PPAPI_TESTS_TEST_MOUSE_LOCK_H_
