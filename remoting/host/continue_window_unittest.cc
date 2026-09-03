// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/base/errors.h"
#include "remoting/host/host_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class TestContinueWindow : public ContinueWindow {
 public:
  TestContinueWindow() = default;
  ~TestContinueWindow() override = default;

  MOCK_METHOD(void, ShowUi, (), (override));
  MOCK_METHOD(void, HideUi, (), (override));
  MOCK_METHOD(void, SetButtonsEnabled, (bool), (override));
};

}  // namespace

class ContinueWindowTest : public testing::Test {
 public:
  ContinueWindowTest() = default;
  ~ContinueWindowTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockClientSessionControl client_session_control_;
  base::WeakPtrFactory<MockClientSessionControl>
      client_session_control_factory_{&client_session_control_};
};

TEST_F(ContinueWindowTest, ShowUiAndDisableInputsOnSessionExpired) {
  TestContinueWindow window;
  window.Start(client_session_control_factory_.GetWeakPtr());

  EXPECT_CALL(client_session_control_, SetDisableInputs(true));
  EXPECT_CALL(window, ShowUi());
  EXPECT_CALL(window, SetButtonsEnabled(false));
  task_environment_.FastForwardBy(base::Minutes(30));
}

TEST_F(ContinueWindowTest, ContinueSessionResumesInputsAndRestartsTimer) {
  TestContinueWindow window;
  window.Start(client_session_control_factory_.GetWeakPtr());

  EXPECT_CALL(client_session_control_, SetDisableInputs(true));
  EXPECT_CALL(window, ShowUi());
  EXPECT_CALL(window, SetButtonsEnabled(false));
  task_environment_.FastForwardBy(base::Minutes(30));

  EXPECT_CALL(window, SetButtonsEnabled(true));
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_CALL(window, HideUi());
  EXPECT_CALL(client_session_control_, SetDisableInputs(false));
  window.ContinueSession();

  EXPECT_CALL(client_session_control_, SetDisableInputs(true));
  EXPECT_CALL(window, ShowUi());
  EXPECT_CALL(window, SetButtonsEnabled(false));
  task_environment_.FastForwardBy(base::Minutes(30));
}

TEST_F(ContinueWindowTest, DisconnectSessionDisconnectsClient) {
  TestContinueWindow window;
  window.Start(client_session_control_factory_.GetWeakPtr());

  EXPECT_CALL(client_session_control_, SetDisableInputs(true));
  EXPECT_CALL(window, ShowUi());
  EXPECT_CALL(window, SetButtonsEnabled(false));
  task_environment_.FastForwardBy(base::Minutes(30));

  EXPECT_CALL(window, SetButtonsEnabled(true));
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_CALL(
      client_session_control_,
      DisconnectSession(ErrorCode::MAX_SESSION_LENGTH, testing::_, testing::_));
  window.DisconnectSession();
}

TEST_F(ContinueWindowTest, DisconnectsOnTimeoutWithoutUserResponse) {
  TestContinueWindow window;
  window.Start(client_session_control_factory_.GetWeakPtr());

  EXPECT_CALL(client_session_control_, SetDisableInputs(true));
  EXPECT_CALL(window, ShowUi());
  EXPECT_CALL(window, SetButtonsEnabled(false));
  task_environment_.FastForwardBy(base::Minutes(30));

  EXPECT_CALL(window, SetButtonsEnabled(true));
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_CALL(
      client_session_control_,
      DisconnectSession(ErrorCode::MAX_SESSION_LENGTH, testing::_, testing::_));
  task_environment_.FastForwardBy(base::Minutes(5));
}

TEST_F(ContinueWindowTest, ActivationDelayBlocksEarlyUserActions) {
  TestContinueWindow window;
  window.Start(client_session_control_factory_.GetWeakPtr());

  EXPECT_CALL(client_session_control_, SetDisableInputs(true));
  EXPECT_CALL(window, ShowUi());
  EXPECT_CALL(window, SetButtonsEnabled(false));
  task_environment_.FastForwardBy(base::Minutes(30));

  // Before delay expires (1 second in), user actions should be ignored.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_CALL(window, HideUi()).Times(0);
  EXPECT_CALL(client_session_control_, SetDisableInputs(false)).Times(0);
  EXPECT_CALL(client_session_control_,
              DisconnectSession(testing::_, testing::_, testing::_))
      .Times(0);

  window.ContinueSession();
  window.DisconnectSession();

  // Once the 2-second delay expires, buttons are enabled and action succeeds.
  EXPECT_CALL(window, SetButtonsEnabled(true));
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_CALL(window, HideUi());
  EXPECT_CALL(client_session_control_, SetDisableInputs(false));
  window.ContinueSession();
}

TEST_F(ContinueWindowTest, ActivationDelayBlocksEarlyDisconnect) {
  TestContinueWindow window;
  window.Start(client_session_control_factory_.GetWeakPtr());

  EXPECT_CALL(client_session_control_, SetDisableInputs(true));
  EXPECT_CALL(window, ShowUi());
  EXPECT_CALL(window, SetButtonsEnabled(false));
  task_environment_.FastForwardBy(base::Minutes(30));

  // Early disconnect attempt during activation delay is ignored.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_CALL(client_session_control_,
              DisconnectSession(testing::_, testing::_, testing::_))
      .Times(0);
  window.DisconnectSession();

  // Once the 2-second delay expires, DisconnectSession succeeds.
  EXPECT_CALL(window, SetButtonsEnabled(true));
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_CALL(
      client_session_control_,
      DisconnectSession(ErrorCode::MAX_SESSION_LENGTH, testing::_, testing::_));
  window.DisconnectSession();
}

}  // namespace remoting
