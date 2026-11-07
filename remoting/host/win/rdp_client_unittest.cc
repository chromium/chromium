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

  MOCK_METHOD(void, OnRdpConnected, (), (override));
  MOCK_METHOD(void, OnRdpClosed, (), (override));
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

// Standard Windows DPI values corresponding to 100%, 125%, 150%, and 200%
// display scale factors.
constexpr int kDpi100Percent = 96;
constexpr int kDpi125Percent = 120;
constexpr int kDpi150Percent = 144;
constexpr int kDpi200Percent = 192;

const webrtc::DesktopSize kMinRdpSize(800, 600);
const webrtc::DesktopSize kVgaLandscape(640, 480);
const webrtc::DesktopSize kFullHdLandscape(1920, 1080);
const webrtc::DesktopSize k4kLandscape(3840, 2160);
const webrtc::DesktopSize k4kPortrait(2160, 3840);
const webrtc::DesktopSize k5kLandscape(5120, 2880);
const webrtc::DesktopSize k5kPortrait(2880, 5120);

ScreenResolution MakeResolution(const webrtc::DesktopSize& size, int dpi) {
  return ScreenResolution(size, webrtc::DesktopVector(dpi, dpi));
}

ScreenResolution MakeResolution(int width, int height, int dpi) {
  return MakeResolution(webrtc::DesktopSize(width, height), dpi);
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
  terminal_id_ = WtsTerminalMonitor::GenerateVirtualTerminalId();

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

TEST(GetBoundedRdpResolutionTest, WithinBoundsKeepsOriginalDpi) {
  const ScreenResolution kFullHdAt100 =
      MakeResolution(kFullHdLandscape, kDpi100Percent);
  const ScreenResolution kQhdWindowAt150 =
      MakeResolution(2560, 1372, kDpi150Percent);
  const ScreenResolution k4kLandscapeAt200 =
      MakeResolution(k4kLandscape, kDpi200Percent);
  const ScreenResolution k4kPortraitAt200 =
      MakeResolution(k4kPortrait, kDpi200Percent);

  EXPECT_EQ(GetBoundedRdpResolution(kFullHdAt100), kFullHdAt100);
  EXPECT_EQ(GetBoundedRdpResolution(kQhdWindowAt150), kQhdWindowAt150);
  EXPECT_EQ(GetBoundedRdpResolution(k4kLandscapeAt200), k4kLandscapeAt200);
  EXPECT_EQ(GetBoundedRdpResolution(k4kPortraitAt200), k4kPortraitAt200);
}

TEST(GetBoundedRdpResolutionTest, MinimumBoundsAndEvenKeepsDpi) {
  const ScreenResolution kMinAt100 =
      MakeResolution(kMinRdpSize, kDpi100Percent);
  const ScreenResolution kMinAt150 =
      MakeResolution(kMinRdpSize, kDpi150Percent);

  EXPECT_EQ(GetBoundedRdpResolution(ScreenResolution()), kMinAt100);
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(0, 0, kDpi150Percent)),
            kMinAt150);
  EXPECT_EQ(
      GetBoundedRdpResolution(MakeResolution(kVgaLandscape, kDpi100Percent)),
      kMinAt100);
  EXPECT_EQ(
      GetBoundedRdpResolution(MakeResolution(kVgaLandscape, kDpi150Percent)),
      kMinAt150);
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(kMinRdpSize, 72)),
            kMinAt100);
  EXPECT_EQ(GetBoundedRdpResolution(ScreenResolution(
                kFullHdLandscape, webrtc::DesktopVector(kDpi150Percent, 0))),
            MakeResolution(kFullHdLandscape, kDpi150Percent));
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(1921, 1081, kDpi200Percent)),
            MakeResolution(kFullHdLandscape, kDpi200Percent));
}

TEST(GetBoundedRdpResolutionTest, ScalesDownLandscapeTo4KAndScalesDpi) {
  // 16" Retina MacBook window at 2x: 5120x2744 @ 192 -> 3840x2058 @ 144.
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(5120, 2744, kDpi200Percent)),
            MakeResolution(3840, 2058, kDpi150Percent));

  // 6K Pro Display XDR window at 2x: 6016x3258 @ 192 -> 3840x2080 @ 123.
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(6016, 3258, kDpi200Percent)),
            MakeResolution(3840, 2080, 123));

  // 5K 16:9 Studio Display at 2x: 5120x2880 @ 192 -> 3840x2160 @ 144.
  EXPECT_EQ(
      GetBoundedRdpResolution(MakeResolution(k5kLandscape, kDpi200Percent)),
      MakeResolution(k4kLandscape, kDpi150Percent));

  // 16:10 UHD+ laptop at 2x: 3840x2400 @ 192 -> 3456x2160 @ 173
  // (height-limited).
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(3840, 2400, kDpi200Percent)),
            MakeResolution(3456, 2160, 173));

  // Aspect ratio that rounds to an odd width (3283) aligns to 3282 (even), and
  // scales DPI proportionally (192 * 2160 / 2500 -> 166).
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(3800, 2500, kDpi200Percent)),
            MakeResolution(3282, 2160, 166));
}

TEST(GetBoundedRdpResolutionTest, ScalesDownPortraitTo4KAndScalesDpi) {
  // 5K 9:16 portrait display at 2x: 2880x5120 @ 192 -> 2160x3840 @ 144.
  EXPECT_EQ(
      GetBoundedRdpResolution(MakeResolution(k5kPortrait, kDpi200Percent)),
      MakeResolution(k4kPortrait, kDpi150Percent));

  // 10:16 portrait UHD+ display at 2x: 2400x3840 @ 192 -> 2160x3456 @ 173.
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(2400, 3840, kDpi200Percent)),
            MakeResolution(2160, 3456, 173));
}

TEST(GetBoundedRdpResolutionTest, ClampsScaledDpiToMinimum96) {
  const ScreenResolution k4kAt100 =
      MakeResolution(k4kLandscape, kDpi100Percent);

  // 5K at 100% (96 DPI) would scale to 72 DPI, which clamps to 96 DPI.
  EXPECT_EQ(
      GetBoundedRdpResolution(MakeResolution(k5kLandscape, kDpi100Percent)),
      k4kAt100);

  // 5K at 125% (120 DPI) would scale to 90 DPI, which clamps to 96 DPI.
  EXPECT_EQ(
      GetBoundedRdpResolution(MakeResolution(k5kLandscape, kDpi125Percent)),
      k4kAt100);
}

TEST(GetBoundedRdpResolutionTest, ExtremeAspectRatio) {
  // Scaling down to 3840 width yields height < 600 (clamped up to 600) and
  // DPI < 96 (clamped up to 96).
  EXPECT_EQ(GetBoundedRdpResolution(MakeResolution(10000, 500, kDpi200Percent)),
            MakeResolution(k4kLandscape.width(), kMinRdpSize.height(),
                           kDpi100Percent));
}

TEST(GetBoundedRdpDesktopSizeTest, BoundsDimensionsAndHandlesInvalidInputs) {
  EXPECT_TRUE(GetBoundedRdpDesktopSize(100, 0).equals(kMinRdpSize));
  EXPECT_TRUE(GetBoundedRdpDesktopSize(-800, 600).equals(kMinRdpSize));
  EXPECT_TRUE(
      GetBoundedRdpDesktopSize(k5kLandscape.width(), k5kLandscape.height())
          .equals(k4kLandscape));
}

}  // namespace remoting
