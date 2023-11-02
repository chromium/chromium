// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_OVERSCROLL_OBSERVER_H_
#define CONTENT_TEST_MOCK_OVERSCROLL_OBSERVER_H_

namespace content {

// An interface for tests to use MockOverscrollControllerDelegateAura and
// MockOverscrollRefreshHandlerAndroid.
class MockOverscrollObserver {
 public:
  MockOverscrollObserver() {}
  virtual ~MockOverscrollObserver() {}
  virtual void WaitForUpdate() = 0;
  virtual void WaitForEnd() = 0;
  virtual void Reset() = 0;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_OVERSCROLL_OBSERVER_H_
