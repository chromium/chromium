// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_win_test_helper.h"

#include <winerror.h>

#include "base/functional/bind.h"
#include "content/browser/renderer_host/input/mock_tfhandwriting.h"
#include "content/browser/renderer_host/input/stylus_handwriting_callback_sink_win.h"
#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/win/stylus_handwriting_properties_win.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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

void StylusHandwritingWinTestHelper::
    DefaultMockRequestHandwritingForPointerMethod() {
  mock_handwriting_request_ =
      Microsoft::WRL::Make<NiceMock<MockTfHandwritingRequest>>();
  ON_CALL(*mock_tf_impl(), RequestHandwritingForPointer(_, _, _, _))
      .WillByDefault(
          RequestHandwritingForPointerDefault(mock_handwriting_request_.Get()));
}

Microsoft::WRL::ComPtr<MockTfFocusHandwritingTargetArgsImpl>
StylusHandwritingWinTestHelper::SetUpStartedStylusWriting(
    base::WeakPtr<RenderWidgetHostViewBase> view) {
  auto mock_focus_args =
      Microsoft::WRL::Make<NiceMock<MockTfFocusHandwritingTargetArgsImpl>>();
  ON_CALL(*mock_focus_args.Get(), GetPointerTargetInfo(_, _, _))
      .WillByDefault(Return(S_OK));

  auto* controller = StylusHandwritingControllerWin::GetInstance();
  CHECK(controller);

  ui::StylusHandwritingPropertiesWin properties;
  StylusHandwritingControllerWin::OnFocusHandwritingTargetCallback callback =
      base::BindRepeating([](const gfx::Rect&, const gfx::Size&) {});
  controller->OnStartStylusWriting(view.get(), callback, properties);

  return mock_focus_args;
}

Microsoft::WRL::ComPtr<MockTfFocusHandwritingTargetArgsImpl>
StylusHandwritingWinTestHelper::SetUpWaitingForFocusResult(
    base::WeakPtr<RenderWidgetHostViewBase> view) {
  auto mock_focus_args = SetUpStartedStylusWriting(std::move(view));

  auto* controller = StylusHandwritingControllerWin::GetInstance();
  CHECK(controller);

  auto sink = controller->GetCallbackSinkForTesting();
  CHECK(sink);
  CHECK_EQ(TF_S_ASYNC, sink->FocusHandwritingTarget(mock_focus_args.Get()));
  CHECK(controller->IsWaitingForFocusResult());

  return mock_focus_args;
}

}  // namespace content
