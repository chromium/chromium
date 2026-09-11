// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "remoting/host/win/input_extra_info.h"
#endif

namespace remoting {

using testing::_;
using testing::AnyNumber;
using testing::ReturnRef;

namespace {

class LocalInputMonitorTest : public testing::Test {
 public:
  LocalInputMonitorTest();

  void SetUp() override;

  base::test::TaskEnvironment task_environment_ {
#if BUILDFLAG(IS_WIN)
    base::test::TaskEnvironment::MainThreadType::UI
#else   // !BUILDFLAG(IS_WIN)
    // Required to watch a file descriptor from NativeMessageProcessHost.
    base::test::TaskEnvironment::MainThreadType::IO
#endif  // !BUILDFLAG(IS_WIN)
  };

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::string client_jid_;
  MockClientSessionControl client_session_control_;
  base::WeakPtrFactory<ClientSessionControl> client_session_control_factory_;
};

LocalInputMonitorTest::LocalInputMonitorTest()
    : client_jid_("user@domain/rest-of-jid"),
      client_session_control_factory_(&client_session_control_) {}

void LocalInputMonitorTest::SetUp() {
  task_runner_ = task_environment_.GetMainThreadTaskRunner();
}

}  // namespace

// This test is really to exercise only the creation and destruction code in
// LocalInputMonitor.
TEST_F(LocalInputMonitorTest, BasicWithClientSession) {
  // Ignore all callbacks.
  EXPECT_CALL(client_session_control_, client_jid())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(client_jid_));
  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(client_session_control_, OnLocalPointerMoved(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(client_session_control_, SetDisableInputs(_)).Times(0);

  {
    std::unique_ptr<LocalInputMonitor> local_input_monitor =
        LocalInputMonitor::Create(task_runner_, task_runner_, task_runner_);
    local_input_monitor->StartMonitoringForClientSession(
        client_session_control_factory_.GetWeakPtr());
  }

  task_runner_->PostTask(FROM_HERE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();
}

TEST_F(LocalInputMonitorTest, BasicWithCallbacks) {
  // Ignore all callbacks.
  EXPECT_CALL(client_session_control_, client_jid())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(client_jid_));

  {
    std::unique_ptr<LocalInputMonitor> local_input_monitor =
        LocalInputMonitor::Create(task_runner_, task_runner_, task_runner_);
    local_input_monitor->StartMonitoring(base::DoNothing(), base::DoNothing(),
                                         base::DoNothing());
  }

  task_runner_->PostTask(FROM_HERE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();
}

#if BUILDFLAG(IS_WIN)
struct IsCrdInjectedInputTestCase {
  const char* test_name;
  DWORD dwType;
  bool has_device_handle;
  uint32_t extra_info;
  bool expected_is_crd;
};

class LocalInputMonitorWinTest
    : public testing::TestWithParam<IsCrdInjectedInputTestCase> {};

TEST_P(LocalInputMonitorWinTest, DistinguishesCrdInjectedFromSoftwareInput) {
  const auto& param = GetParam();
  RAWINPUT event = {};
  event.header.dwType = param.dwType;
  if (param.has_device_handle) {
    event.header.hDevice = reinterpret_cast<HANDLE>(0x1234);
  }
  if (param.dwType == RIM_TYPEMOUSE) {
    event.data.mouse.ulExtraInformation = param.extra_info;
  } else if (param.dwType == RIM_TYPEKEYBOARD) {
    event.data.keyboard.ExtraInformation = param.extra_info;
  }

  EXPECT_EQ(IsCrdInjectedInput(event), param.expected_is_crd);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LocalInputMonitorWinTest,
    testing::Values(
        IsCrdInjectedInputTestCase{
            .test_name = "HardwareMouseNoExtraInfo",
            .dwType = RIM_TYPEMOUSE,
            .has_device_handle = true,
            .extra_info = 0,
            .expected_is_crd = false,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "HardwareMouseWithCrdExtraInfo",
            .dwType = RIM_TYPEMOUSE,
            .has_device_handle = true,
            .extra_info = kCrdInputExtraInfo,
            .expected_is_crd = false,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "SoftwareMouseNoExtraInfo",
            .dwType = RIM_TYPEMOUSE,
            .has_device_handle = false,
            .extra_info = 0,
            .expected_is_crd = false,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "CrdInjectedMouse",
            .dwType = RIM_TYPEMOUSE,
            .has_device_handle = false,
            .extra_info = kCrdInputExtraInfo,
            .expected_is_crd = true,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "HardwareKeyboardNoExtraInfo",
            .dwType = RIM_TYPEKEYBOARD,
            .has_device_handle = true,
            .extra_info = 0,
            .expected_is_crd = false,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "HardwareKeyboardWithCrdExtraInfo",
            .dwType = RIM_TYPEKEYBOARD,
            .has_device_handle = true,
            .extra_info = kCrdInputExtraInfo,
            .expected_is_crd = false,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "SoftwareKeyboardNoExtraInfo",
            .dwType = RIM_TYPEKEYBOARD,
            .has_device_handle = false,
            .extra_info = 0,
            .expected_is_crd = false,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "CrdInjectedKeyboard",
            .dwType = RIM_TYPEKEYBOARD,
            .has_device_handle = false,
            .extra_info = kCrdInputExtraInfo,
            .expected_is_crd = true,
        },
        IsCrdInjectedInputTestCase{
            .test_name = "NonMouseOrKeyboardEvent",
            .dwType = RIM_TYPEHID,
            .has_device_handle = false,
            .extra_info = 0,
            .expected_is_crd = false,
        }),
    [](const testing::TestParamInfo<LocalInputMonitorWinTest::ParamType>&
           info) { return info.param.test_name; });
#endif  // BUILDFLAG(IS_WIN)

}  // namespace remoting
