// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "base/win/atl.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/win/wts_terminal_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

using testing::_;
using testing::AtMost;
using testing::InvokeWithoutArgs;

namespace remoting {

namespace {

// Default width, height, and dpi of the RDP client window.
const long kDefaultWidth = 1024;
const long kDefaultHeight = 768;
const long kDefaultDpi = 96;

const DWORD kDefaultRdpPort = 3389;

class MockRdpClientEventHandler : public RdpClient::EventHandler {
 public:
  MockRdpClientEventHandler() {}

  MockRdpClientEventHandler(const MockRdpClientEventHandler&) = delete;
  MockRdpClientEventHandler& operator=(const MockRdpClientEventHandler&) =
      delete;

  ~MockRdpClientEventHandler() override {}

  MOCK_METHOD0(OnRdpConnected, void());
  MOCK_METHOD0(OnRdpClosed, void());
};

// a14498c6-7f3b-4e42-9605-6c4a20d53c87
static GUID RdpClientModuleLibid = {
    0xa14498c6,
    0x7f3b,
    0x4e42,
    {0x96, 0x05, 0x6c, 0x4a, 0x20, 0xd5, 0x3c, 0x87}};

class RdpClientModule : public ATL::CAtlModuleT<RdpClientModule> {
 public:
  RdpClientModule();
  ~RdpClientModule() override;

  DECLARE_LIBID(RdpClientModuleLibid)

 private:
  base::win::ScopedCOMInitializer com_initializer_;
};

RdpClientModule::RdpClientModule() {
  AtlAxWinInit();
}

RdpClientModule::~RdpClientModule() {
  AtlAxWinTerm();
  ATL::_pAtlModule = nullptr;
}

}  // namespace

class RdpClientTest : public testing::Test {
 public:
  RdpClientTest();
  ~RdpClientTest() override;

  void SetUp() override;
  void TearDown() override;

  // Caaled when an RDP connection is established.
  void OnRdpConnected();

  // Tears down |rdp_client_|.
  void CloseRdpClient();

 protected:
  // The ATL module instance required by the ATL code.
  std::unique_ptr<RdpClientModule> module_;

  // Used by RdpClient. The loop is stopped once there are no more references to
  // |task_runner_|.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Mocks RdpClient::EventHandler for testing.
  MockRdpClientEventHandler event_handler_;

  // Points to the object being tested.
  std::unique_ptr<RdpClient> rdp_client_;

  // Unique terminal identifier passed to RdpClient.
  std::string terminal_id_;
};

RdpClientTest::RdpClientTest() {}

RdpClientTest::~RdpClientTest() {}

void RdpClientTest::SetUp() {
  // Arrange to run |run_loop_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(), run_loop_.QuitClosure());

  module_ = std::make_unique<RdpClientModule>();
}

void RdpClientTest::TearDown() {
  EXPECT_TRUE(!rdp_client_);

  module_.reset();
}

void RdpClientTest::OnRdpConnected() {
  uint32_t session_id = WtsTerminalMonitor::LookupSessionId(terminal_id_);

  std::string id;
  EXPECT_TRUE(WtsTerminalMonitor::LookupTerminalId(session_id, &id));
  EXPECT_EQ(id, terminal_id_);

  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RdpClientTest::CloseRdpClient, base::Unretained(this)));
}

void RdpClientTest::CloseRdpClient() {
  EXPECT_TRUE(rdp_client_);

  rdp_client_.reset();
}

// Creates a loopback RDP connection.
// TODO(crbug.com/41496654): Consistently times out on Windows 11 ARM64.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST_F(RdpClientTest, MAYBE_Basic) {
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11_23H2) {
    GTEST_SKIP() << "https://crbug.com/365126540: Skipping test for WIN11_23H2 "
                    "and greater";
  }
  terminal_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();

  // An ability to establish a loopback RDP connection depends on many factors
  // including OS SKU and having RDP enabled. Accept both successful connection
  // and a connection error as a successful outcome.
  EXPECT_CALL(event_handler_, OnRdpConnected())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &RdpClientTest::OnRdpConnected));
  EXPECT_CALL(event_handler_, OnRdpClosed())
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(this, &RdpClientTest::CloseRdpClient));

  rdp_client_ = std::make_unique<RdpClient>(
      task_runner_, task_runner_,
      ScreenResolution(webrtc::DesktopSize(kDefaultWidth, kDefaultHeight),
                       webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)),
      terminal_id_, kDefaultRdpPort, &event_handler_);
  task_runner_ = nullptr;

  run_loop_.Run();
}

}  // namespace remoting
