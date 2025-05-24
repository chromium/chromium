// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_WIN_TEST_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_WIN_TEST_HELPER_H_

#include <ShellHandwriting.h>
#include <msctf.h>
#include <wrl/client.h>

#include "base/functional/callback_helpers.h"

namespace content {

class MockTfImpl;

// A test helper that configures and manages mock ShellHandwriting.h interfaces
// and initializes StylusHandwritingControllerWin for testing.
//
// Note that all of the SetUp and configuration methods must be called from the
// main thread. Tests that implement `BrowserTestBase` must call configuration
// methods from `SetUpOnMainThread` while unit tests can be called from `SetUp`.
//
// Test frameworks that only need a generic configuration may call
// SetUpDefaultMockInfrastructure from the Main thread.
//
// Tests that need gmock actions/behaviors like `ON_CALL` or `WillByDefault`,
// for example to  exercise code paths where a Windows ShellHandwriting API
// fails for code coverage or to verify an API is called a certain number of
// times, must call the following:
// 1. Call `SetUpMockTfImpl()` to initialize a new `MockTfImpl` object.
// 2. Call any desired `DefaultMock*Method` helpers for behaviors that are
//    required but where a generic implementation is fine.
// 3. Set up custom `ON_CALL` methods for `mock_tf_impl()`.
// 4. Call `SetUpStylusHandwritingControllerWin()` to complete setup and
//    initialize `StylusHandwritingControllerWin` for testing with the prepared
//    `MockTfImpl`.
//
// Example use cases:
// 1. StylusHandwritingWin test that only needs `StylusHandwritingControllerWin`
//    to be available during the test and a generic implementation is fine.
//
//   class SomeTest : public SomeTestBase {
//     void SetUp() override {
//       SomeTestBase::SetUp();
//       helper_.SetUpDefaultMockInfrastructure();
//     }
//     StylusHandwritingWinTestHelper helper_;
//   };
//
// 2. StylusHandwritingWin test that needs to conditionally exercise when the
//    `ITfHandwriting::SetHandwritingState` API succeeds or fails, but a generic
//     QueryInterface and AdviseSink implementations are fine.
//
//   class SomeTest : public SomeTestBase
//                  , public testing::WithParamInterface<HRESULT>  {
//     void SetUp() override {
//       SomeTestBase::SetUp();
//       helper_.SetUpMockTfImpl();
//       helper_.DefaultMockQueryInterfaceMethod();
//       helper_.DefaultMockAdviseSinkMethod();
//       ON_CALL(*helper_.mock_tf_impl(), SetHandwritingState(_))
//           .WillByDefault(Return(GetParam()));
//       helper_.SetUpStylusHandwritingControllerWin();
//     }
//     StylusHandwritingWinTestHelper helper_;
//   };
class StylusHandwritingWinTestHelper {
 public:
  StylusHandwritingWinTestHelper();
  StylusHandwritingWinTestHelper(const StylusHandwritingWinTestHelper&) =
      delete;
  StylusHandwritingWinTestHelper& operator=(
      const StylusHandwritingWinTestHelper&) = delete;
  virtual ~StylusHandwritingWinTestHelper();

  MockTfImpl* mock_tf_impl() { return mock_tf_impl_.Get(); }
  ITfThreadMgr* GetThreadManager();
  ITfHandwriting* GetTfHandwriting();
  ITfSource* GetTfSource();

  // Initializes MockTfImpl and StylusHandwritingControllerWin for a generic
  // StylusHandwritingWin test, where special considerations are not needed for
  // StylusHandwritingWin or Windows ShellHandwriting API.
  // Tests that call this should not call any of the other exposed `SetUp*` or
  // `DefaultMock*Method` calls.
  // Must be called on the Main thread.
  void SetUpDefaultMockInfrastructure();

  // Initializes MockTfImpl which allows tests to override the default behaviors
  // of Windows ShellHandwriting API and enable testing StylusHandwritingWin.
  // This is the first step for initialization.
  // Must be called on the Main thread.
  void SetUpMockTfImpl();

  // Initializes StylusHandwritingControllerWin using the MockTfImpl which has
  // been configured on the Main thread.
  // This is the final step for initialization, and no other exposed `SetUp*` or
  // `DefaultMock*Method` calls should be made following this.
  // Must be called on the Main thread.
  void SetUpStylusHandwritingControllerWin();

  // Configures default QueryInterface matching for ITfHandwriting and
  // ITfSource.
  void DefaultMockQueryInterfaceMethod();

  // Configures a default implementation of ITfHandwriting::SetHandwritingState.
  void DefaultMockSetHandwritingStateMethod();

  // Configures a default implementation of ITfSource::AdviseSink.
  void DefaultMockAdviseSinkMethod();

 private:
  Microsoft::WRL::ComPtr<MockTfImpl> mock_tf_impl_;
  base::ScopedClosureRunner controller_resetter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_WIN_TEST_HELPER_H_
