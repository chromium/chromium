// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_http_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const uint16_t kDummyPort = 4321;
const base::FilePath::CharType kDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("DevToolsActivePort");

class DummyServerSocket : public net::ServerSocket {
 public:
  DummyServerSocket() {}

  // net::ServerSocket "implementation"
  int Listen(const net::IPEndPoint& address, int backlog) override {
    return net::OK;
  }

  int GetLocalAddress(net::IPEndPoint* address) const override {
    *address = net::IPEndPoint(net::IPAddress::IPv4Localhost(), kDummyPort);
    return net::OK;
  }

  int Accept(std::unique_ptr<net::StreamSocket>* socket,
             net::CompletionOnceCallback callback) override {
    return net::ERR_IO_PENDING;
  }
};

void QuitFromHandlerThread(const base::Closure& quit_closure) {
  base::PostTask(FROM_HERE, {BrowserThread::UI}, quit_closure);
}

class DummyServerSocketFactory : public DevToolsSocketFactory {
 public:
  DummyServerSocketFactory(base::Closure quit_closure_1,
                           base::Closure quit_closure_2)
      : quit_closure_1_(quit_closure_1),
        quit_closure_2_(quit_closure_2) {}

  ~DummyServerSocketFactory() override {
    base::PostTask(FROM_HERE, {BrowserThread::UI}, quit_closure_2_);
  }

 protected:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&QuitFromHandlerThread, quit_closure_1_));
    return base::WrapUnique(new DummyServerSocket());
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  base::Closure quit_closure_1_;
  base::Closure quit_closure_2_;
};

class FailingServerSocketFactory : public DummyServerSocketFactory {
 public:
  FailingServerSocketFactory(const base::Closure& quit_closure_1,
                             const base::Closure& quit_closure_2)
      : DummyServerSocketFactory(quit_closure_1, quit_closure_2) {
  }

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&QuitFromHandlerThread, quit_closure_1_));
    return nullptr;
  }
};

class BrowserClient : public ContentBrowserClient {
 public:
  BrowserClient() {}
  ~BrowserClient() override {}
  DevToolsManagerDelegate* GetDevToolsManagerDelegate() override {
    return new DevToolsManagerDelegate();
  }
};

}

class DevToolsHttpHandlerTest : public testing::Test {
 public:
  DevToolsHttpHandlerTest() : testing::Test() { }

  void SetUp() override {
    content_client_.reset(new ContentClient());
    browser_content_client_.reset(new BrowserClient());
    SetBrowserClientForTesting(browser_content_client_.get());
  }

 private:
  std::unique_ptr<ContentClient> content_client_;
  std::unique_ptr<ContentBrowserClient> browser_content_client_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DevToolsHttpHandlerTest, TestStartStop) {
  base::RunLoop run_loop, run_loop_2;
  std::unique_ptr<DevToolsSocketFactory> factory(
      new DummyServerSocketFactory(run_loop.QuitClosure(),
                                   run_loop_2.QuitClosure()));
  DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(factory), base::FilePath(), base::FilePath());
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  DevToolsAgentHost::StopRemoteDebuggingServer();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}

TEST_F(DevToolsHttpHandlerTest, TestServerSocketFailed) {
  base::RunLoop run_loop, run_loop_2;
  std::unique_ptr<DevToolsSocketFactory> factory(
      new FailingServerSocketFactory(run_loop.QuitClosure(),
                                     run_loop_2.QuitClosure()));
  LOG(INFO) << "Following error message is expected:";
  DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(factory), base::FilePath(), base::FilePath());
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  for (int i = 0; i < 5; i++)
    RunAllPendingInMessageLoop(BrowserThread::UI);
  DevToolsAgentHost::StopRemoteDebuggingServer();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}


TEST_F(DevToolsHttpHandlerTest, TestDevToolsActivePort) {
  base::RunLoop run_loop, run_loop_2;
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  std::unique_ptr<DevToolsSocketFactory> factory(
      new DummyServerSocketFactory(run_loop.QuitClosure(),
                                   run_loop_2.QuitClosure()));

  DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(factory), temp_dir.GetPath(), base::FilePath());
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  DevToolsAgentHost::StopRemoteDebuggingServer();
  // Make sure the handler actually stops.
  run_loop_2.Run();

  // Now make sure the DevToolsActivePort was written into the
  // temporary directory and its contents are as expected.
  base::FilePath active_port_file =
      temp_dir.GetPath().Append(kDevToolsActivePortFileName);
  EXPECT_TRUE(base::PathExists(active_port_file));
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(active_port_file, &file_contents));
  std::vector<std::string> tokens = base::SplitString(
      file_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  int port = 0;
  EXPECT_TRUE(base::StringToInt(tokens[0], &port));
  EXPECT_EQ(static_cast<int>(kDummyPort), port);
}

}  // namespace content
