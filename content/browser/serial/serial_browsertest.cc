// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/serial/serial_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/serial_chooser.h"
#include "content/public/browser/serial_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Exactly;
using testing::Return;

namespace content {

namespace {

class SerialTestShellContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  MockSerialDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  SerialDelegate* GetSerialDelegate() override { return &delegate_; }

 private:
  MockSerialDelegate delegate_;
};

class SerialTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    test_client_ = std::make_unique<SerialTestShellContentBrowserClient>();
    ON_CALL(delegate(), GetPortManager).WillByDefault(Return(&port_manager_));
  }

  void TearDownOnMainThread() override { test_client_.reset(); }

  MockSerialDelegate& delegate() { return test_client_->delegate(); }
  device::FakeSerialPortManager* port_manager() { return &port_manager_; }

 private:
  std::unique_ptr<SerialTestShellContentBrowserClient> test_client_;
  device::FakeSerialPortManager port_manager_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SerialTest, GetPorts) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  // Three ports are added but only two will have permission granted.
  for (size_t i = 0; i < 3; i++) {
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    port_manager()->AddPort(std::move(port));
  }

  EXPECT_CALL(delegate(), HasPortPermission(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_EQ(
      2, EvalJs(shell(),
                R"(navigator.serial.getPorts().then(ports => ports.length))"));
}

IN_PROC_BROWSER_TEST_F(SerialTest, RequestPort) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  EXPECT_CALL(delegate(), CanRequestPortPermission).WillOnce(Return(true));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  EXPECT_CALL(delegate(), RunChooserInternal)
      .WillOnce(Return(ByMove(std::move(port))));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
                           let port = await navigator.serial.requestPort({});
                           return port instanceof SerialPort;
                         })())"));
}

IN_PROC_BROWSER_TEST_F(SerialTest, DisallowRequestPort) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  EXPECT_CALL(delegate(), CanRequestPortPermission(_)).WillOnce(Return(false));
  EXPECT_CALL(delegate(), RunChooserInternal).Times(Exactly(0));

  EXPECT_EQ(false, EvalJs(shell(),
                          R"((async () => {
                            try {
                              await navigator.serial.requestPort({});
                              return true;
                            } catch (e) {
                              return false;
                            }
                          })())"));
}

}  // namespace content
