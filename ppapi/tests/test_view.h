// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_VIEW_H_
#define PPAPI_TESTS_TEST_VIEW_H_

#include <stdint.h>

#include "ppapi/cpp/view.h"
#include "ppapi/tests/test_case.h"

class TestView : public TestCase {
 public:
  TestView(TestingInstance* instance);

  virtual void DidChangeView(const pp::View& view);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& test_filter);

 private:
  // Waits until we get a view changed event. Note that the browser may give
  // us any number of view changed events, so tests that use this should
  // expect that there may be spurious events and handle them accordingly.
  // Note also that view changed sequencing can change between different
  // versions of WebKit.
  //
  // Returns true if we got a view changed, false if it timed out.
  bool WaitUntilViewChanged();

  void QuitMessageLoop(int32_t result);

  std::string TestCreatedVisible();
  std::string TestCreatedInvisible();
  std::string TestPageHideShow();
  std::string TestSizeChange();
  std::string TestClipChange();
  std::string TestScrollOffsetChange();

  pp::View last_view_;

  // DidChangeView stores the page visibility in this vector on each
  // invocation so tests can check it.
  std::vector<bool> page_visibility_log_;

  // Set to true to request that the next invocation of DidChangeView should
  // post a quit to the message loop. DidChangeView will also reset the flag so
  // this will only happen once.
  bool post_quit_on_view_changed_;
};

#endif  // PPAPI_TESTS_TEST_VIEW_H_
