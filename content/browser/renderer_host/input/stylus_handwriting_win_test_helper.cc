// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_win_test_helper.h"

#include <winerror.h>

#include "content/browser/renderer_host/input/mock_tfhandwriting.h"
#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

StylusHandwritingWinTestHelper::StylusHandwritingWinTestHelper() = default;

StylusHandwritingWinTestHelper::~StylusHandwritingWinTestHelper() = default;

ITfThreadMgr* StylusHandwritingWinTestHelper::GetThreadManager() {
  return static_cast<ITfThreadMgr*>(mock_tf_impl());
}

ITfHandwriting* StylusHandwritingWinTestHelper::GetTfHandwriting() {
  return static_cast<ITfHandwriting*>(mock_tf_impl());
}

ITfSource* StylusHandwritingWinTestHelper::GetTfSource() {
  return static_cast<ITfSource*>(mock_tf_impl());
}

void StylusHandwritingWinTestHelper::SetUpDefaultMockInfrastructure() {
  SetUpMockTfImpl();
  DefaultMockQueryInterfaceMethod();
  DefaultMockSetHandwritingStateMethod();
  DefaultMockAdviseSinkMethod();
  SetUpStylusHandwritingControllerWin();
}

void StylusHandwritingWinTestHelper::SetUpMockTfImpl() {
  mock_tf_impl_ = Microsoft::WRL::Make<NiceMock<MockTfImpl>>();
}

void StylusHandwritingWinTestHelper::SetUpStylusHandwritingControllerWin() {
  controller_resetter_ =
      StylusHandwritingControllerWin::InitializeForTesting(GetThreadManager());
}

void StylusHandwritingWinTestHelper::DefaultMockQueryInterfaceMethod() {
  ON_CALL(*mock_tf_impl(), QueryInterface(Eq(__uuidof(ITfHandwriting)), _))
      .WillByDefault(SetComPointeeAndReturnResult<1>(GetTfHandwriting(), S_OK));
  ON_CALL(*mock_tf_impl(), QueryInterface(Eq(__uuidof(ITfSource)), _))
      .WillByDefault(SetComPointeeAndReturnResult<1>(GetTfSource(), S_OK));
}

void StylusHandwritingWinTestHelper::DefaultMockSetHandwritingStateMethod() {
  ON_CALL(*mock_tf_impl(), SetHandwritingState(_)).WillByDefault(Return(S_OK));
}

void StylusHandwritingWinTestHelper::DefaultMockAdviseSinkMethod() {
  ON_CALL(*mock_tf_impl(), AdviseSink(_, _, _))
      .WillByDefault(SetValueParamAndReturnResult<2>(/*value=*/0, S_OK));
}

}  // namespace content
