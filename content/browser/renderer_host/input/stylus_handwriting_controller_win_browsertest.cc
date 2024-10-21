// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

#include "base/test/scoped_feature_list.h"
#include "components/stylus_handwriting/win/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

enum class CreateError {
  kNone,
  kShellHandwritingDisabled,
  kBindInterfaces,
};

std::vector<CreateError> GetCreateErrors() {
  return {CreateError::kNone, CreateError::kShellHandwritingDisabled,
          CreateError::kBindInterfaces};
}

class MockStylusHandwritingControllerWin final
    : public StylusHandwritingControllerWin {
 public:
  explicit MockStylusHandwritingControllerWin(CreateError error)
      : create_error_(error) {
    MockBindInterfaces();
  }
  MockStylusHandwritingControllerWin(
      const MockStylusHandwritingControllerWin&) = delete;
  MockStylusHandwritingControllerWin& operator=(
      const MockStylusHandwritingControllerWin&) = delete;
  ~MockStylusHandwritingControllerWin() final = default;

  void MockBindInterfaces() {
    if (create_error_ != CreateError::kBindInterfaces) {
      instance_resetter_.emplace(
          StylusHandwritingControllerWin::SetInstanceForTesting(this));
    }
  }

 private:
  CreateError create_error_ = CreateError::kNone;
  std::optional<base::AutoReset<StylusHandwritingControllerWin*>>
      instance_resetter_;
};

class StylusHandwritingControllerWinTestBase : public ContentBrowserTest {
 public:
  StylusHandwritingControllerWinTestBase() = default;
  StylusHandwritingControllerWinTestBase(
      const StylusHandwritingControllerWinTestBase&) = delete;
  StylusHandwritingControllerWinTestBase& operator=(
      const StylusHandwritingControllerWinTestBase&) = delete;
  ~StylusHandwritingControllerWinTestBase() override = default;

  void SetUp() override {
    if (GetCreateError() == CreateError::kShellHandwritingDisabled) {
      scoped_feature_list_.InitAndDisableFeature(
          stylus_handwriting::win::kStylusHandwritingWin);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          stylus_handwriting::win::kStylusHandwritingWin);
    }
    mock_instance_ =
        std::make_unique<MockStylusHandwritingControllerWin>(GetCreateError());
    ContentBrowserTest::SetUp();
  }

 protected:
  virtual CreateError GetCreateError() const { return CreateError::kNone; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockStylusHandwritingControllerWin> mock_instance_;
};

class StylusHandwritingControllerWinCreationTest
    : public StylusHandwritingControllerWinTestBase,
      public ::testing::WithParamInterface<CreateError> {
 public:
  StylusHandwritingControllerWinCreationTest() = default;
  StylusHandwritingControllerWinCreationTest(
      const StylusHandwritingControllerWinCreationTest&) = delete;
  StylusHandwritingControllerWinCreationTest& operator=(
      const StylusHandwritingControllerWinCreationTest&) = delete;
  ~StylusHandwritingControllerWinCreationTest() override = default;

 protected:
  CreateError GetCreateError() const final { return GetParam(); }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         StylusHandwritingControllerWinCreationTest,
                         ::testing::ValuesIn(GetCreateErrors()));

IN_PROC_BROWSER_TEST_P(StylusHandwritingControllerWinCreationTest,
                       APIAvailability) {
  EXPECT_EQ(MockStylusHandwritingControllerWin::IsHandwritingAPIAvailable(),
            GetParam() == CreateError::kNone);
}

}  // namespace content
