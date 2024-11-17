// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

#include "base/test/scoped_feature_list.h"
#include "components/stylus_handwriting/win/features.h"
#include "content/browser/renderer_host/input/mock_tfhandwriting.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

enum class APIError {
  kNone,
  kShellHandwritingDisabled,
  kQueryITfHandwriting,
  kSetHandwritingState,
};

std::vector<APIError> GetAPIErrors() {
  return {APIError::kNone, APIError::kShellHandwritingDisabled,
          APIError::kQueryITfHandwriting, APIError::kSetHandwritingState};
}

class StylusHandwritingControllerWinTestBase : public ContentBrowserTest {
 protected:
  StylusHandwritingControllerWinTestBase() = default;
  StylusHandwritingControllerWinTestBase(
      const StylusHandwritingControllerWinTestBase&) = delete;
  StylusHandwritingControllerWinTestBase& operator=(
      const StylusHandwritingControllerWinTestBase&) = delete;
  ~StylusHandwritingControllerWinTestBase() override = default;

  void SetUp() override {
    // We cannot init the features in the constructor because GetAPIError()
    // might rely on the test parameters (from GetParams()), e.g, in
    // StylusHandwritingControllerWinCreationTest.
    if (GetAPIError() == APIError::kShellHandwritingDisabled) {
      scoped_feature_list_.InitAndDisableFeature(
          stylus_handwriting::win::kStylusHandwritingWin);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          stylus_handwriting::win::kStylusHandwritingWin);
    }
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    scoped_feature_list_.Reset();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    SetUpMockTfHandwritingInstances();
    SetUpMockTfImplMethods();
    SetUpControllerInstance();
  }

  virtual APIError GetAPIError() const { return APIError::kNone; }

  virtual void SetUpMockTfHandwritingInstances() {
    mock_tf_impl_ = Microsoft::WRL::Make<NiceMock<MockTfImpl>>();
  }

  virtual void SetUpMockTfImplMethods() {
    MockQueryInterfaceMethod();
    MockSetHandwritingStateMethod();
  }

  void SetUpControllerInstance() {
    controller_resetter_ = StylusHandwritingControllerWin::InitializeForTesting(
        static_cast<ITfThreadMgr*>(mock_tf_impl()));
  }

  MockTfImpl* mock_tf_impl() { return mock_tf_impl_.Get(); }

  StylusHandwritingControllerWin* controller() {
    auto* instance = StylusHandwritingControllerWin::GetInstance();
    EXPECT_TRUE(instance);
    return instance;
  }

  void MockQueryInterfaceMethod() {
    ON_CALL(*mock_tf_impl(), QueryInterface(Eq(__uuidof(::ITfHandwriting)), _))
        .WillByDefault(SetComPointeeAndReturnResult<1>(
            static_cast<::ITfHandwriting*>(mock_tf_impl()),
            GetAPIError() == APIError::kQueryITfHandwriting ? E_NOINTERFACE
                                                            : S_OK));
  }

  void MockSetHandwritingStateMethod() {
    ON_CALL(*mock_tf_impl(), SetHandwritingState(_))
        .WillByDefault(Return(
            GetAPIError() == APIError::kSetHandwritingState ? E_FAIL : S_OK));
  }

 private:
  Microsoft::WRL::ComPtr<MockTfImpl> mock_tf_impl_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedClosureRunner controller_resetter_;
};

class StylusHandwritingControllerWinCreationTest
    : public StylusHandwritingControllerWinTestBase,
      public ::testing::WithParamInterface<APIError> {
 protected:
  StylusHandwritingControllerWinCreationTest() = default;
  StylusHandwritingControllerWinCreationTest(
      const StylusHandwritingControllerWinCreationTest&) = delete;
  StylusHandwritingControllerWinCreationTest& operator=(
      const StylusHandwritingControllerWinCreationTest&) = delete;
  ~StylusHandwritingControllerWinCreationTest() override = default;

  APIError GetAPIError() const final { return GetParam(); }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         StylusHandwritingControllerWinCreationTest,
                         ::testing::ValuesIn(GetAPIErrors()));

IN_PROC_BROWSER_TEST_P(StylusHandwritingControllerWinCreationTest,
                       APIAvailability) {
  EXPECT_EQ(StylusHandwritingControllerWin::IsHandwritingAPIAvailable(),
            GetParam() == APIError::kNone);
}

}  // namespace content
