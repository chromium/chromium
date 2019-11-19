// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client.h"

#include <cstdint>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/win/atl.h"
#include "base/win/scoped_com_initializer.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/screen_resolution.h"
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
  virtual ~MockRdpClientEventHandler() {}

  MOCK_METHOD0(OnRdpConnected, void());
  MOCK_METHOD0(OnRdpClosed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRdpClientEventHandler);
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

  module_.reset(new RdpClientModule());
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
TEST_F(RdpClientTest, Basic) {
  terminal_id_ = base::GenerateGUID();

  // An ability to establish a loopback RDP connection depends on many factors
  // including OS SKU and having RDP enabled. Accept both successful connection
  // and a connection error as a successful outcome.
  EXPECT_CALL(event_handler_, OnRdpConnected())
      .Times(AtMost(1))
      .WillOnce(Invoke(this, &RdpClientTest::OnRdpConnected));
  EXPECT_CALL(event_handler_, OnRdpClosed())
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(this, &RdpClientTest::CloseRdpClient));

  rdp_client_.reset(new RdpClient(
      task_runner_, task_runner_,
      ScreenResolution(webrtc::DesktopSize(kDefaultWidth, kDefaultHeight),
                       webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)),
      terminal_id_, kDefaultRdpPort, &event_handler_));
  task_runner_ = nullptr;

  run_loop_.Run();
}

}  // namespace remoting
