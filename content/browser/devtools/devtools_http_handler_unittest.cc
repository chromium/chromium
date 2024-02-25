// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_http_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_devtools_agent_host.h"
#include "content/public/test/test_utils.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/server_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;

const uint16_t kDummyPort = 4321;
const base::FilePath::CharType kDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("DevToolsActivePort");

class DummyServerSocket : public net::ServerSocket {
 public:
  // net::ServerSocket "implementation"
  int Listen(const net::IPEndPoint& address,
             int backlog,
             std::optional<bool> ipv6_only) override {
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

class DummyServerSocketFactory : public DevToolsSocketFactory {
 public:
  DummyServerSocketFactory(base::OnceClosure create_socket_callback,
                           base::OnceClosure shutdown_callback)
      : create_socket_callback_(std::move(create_socket_callback)),
        shutdown_callback_(std::move(shutdown_callback)) {}

  ~DummyServerSocketFactory() override {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(shutdown_callback_));
  }

 protected:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(create_socket_callback_));
    return std::make_unique<DummyServerSocket>();
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  base::OnceClosure create_socket_callback_;
  base::OnceClosure shutdown_callback_;
};

class FailingServerSocketFactory : public DummyServerSocketFactory {
 public:
  FailingServerSocketFactory(base::OnceClosure create_socket_callback,
                             base::OnceClosure shutdown_callback)
      : DummyServerSocketFactory(std::move(create_socket_callback),
                                 std::move(shutdown_callback)) {}

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(create_socket_callback_));
    return nullptr;
  }
};

class TCPServerSocketFactory : public DummyServerSocketFactory {
 public:
  TCPServerSocketFactory(base::OnceClosure create_socket_callback,
                         base::OnceClosure shutdown_callback)
      : DummyServerSocketFactory(std::move(create_socket_callback),
                                 std::move(shutdown_callback)) {}

  int port() { return port_; }

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    while (socket->ListenWithAddressAndPort("127.0.0.1", port_, 10) !=
           net::OK) {
      if (++port_ > kDummyPort + 100)
        return nullptr;
    }
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(create_socket_callback_));
    return socket;
  }

  int port_ = kDummyPort;
};

class MockDevToolsManagerDelegate : public DevToolsManagerDelegate {
 public:
  static MockDevToolsManagerDelegate* last_instance;
  MockDevToolsManagerDelegate() { last_instance = this; }
  MOCK_METHOD(scoped_refptr<DevToolsAgentHost>,
              CreateNewTarget,
              (const GURL& url, TargetType target_type),
              (override));
  MOCK_METHOD(DevToolsAgentHost::List,
              RemoteDebuggingTargets,
              (TargetType target_type),
              (override));
};

MockDevToolsManagerDelegate* MockDevToolsManagerDelegate::last_instance =
    nullptr;

class MockDevToolsAgentHostWithType : public MockDevToolsAgentHost {
 public:
  explicit MockDevToolsAgentHostWithType(const std::string& type)
      : type_(type) {}

  std::string GetType() override { return type_; }

 private:
  ~MockDevToolsAgentHostWithType() override = default;

  std::string type_;
};

class BrowserClient : public ContentBrowserClient {
 public:
  BrowserClient() {}
  ~BrowserClient() override {}
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override {
    return std::make_unique<MockDevToolsManagerDelegate>();
  }
};

}

class DevToolsHttpHandlerTest : public testing::Test {
 public:
  DevToolsHttpHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    content_client_ = std::make_unique<ContentClient>();
    browser_content_client_ = std::make_unique<BrowserClient>();
    original_client_ =
        SetBrowserClientForTesting(browser_content_client_.get());
    DevToolsManager::ShutdownForTests();
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);
    DevToolsManager::ShutdownForTests();
  }

 private:
  std::unique_ptr<ContentClient> content_client_;
  std::unique_ptr<ContentBrowserClient> browser_content_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DevToolsHttpHandlerTest, TestStartStop) {
  base::RunLoop run_loop, run_loop_2;
  auto factory = std::make_unique<DummyServerSocketFactory>(
      run_loop.QuitClosure(), run_loop_2.QuitClosure());
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
  auto factory = std::make_unique<FailingServerSocketFactory>(
      run_loop.QuitClosure(), run_loop_2.QuitClosure());
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
  auto factory = std::make_unique<DummyServerSocketFactory>(
      run_loop.QuitClosure(), run_loop_2.QuitClosure());

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

TEST_F(DevToolsHttpHandlerTest, MutatingActionsiRequireSafeVerb) {
  base::RunLoop run_loop, run_loop_2;
  auto* factory = new TCPServerSocketFactory(run_loop.QuitClosure(),
                                             run_loop_2.QuitClosure());
  DevToolsAgentHost::StartRemoteDebuggingServer(
      base::WrapUnique(factory), base::FilePath(), base::FilePath());
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  int port = factory->port();

  net::TestDelegate delegate;
  GURL url(base::StringPrintf("http://127.0.0.1:%d/json/new", port));
  auto request_context = net::CreateTestURLRequestContextBuilder()->Build();
  auto request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_GE(delegate.request_status(), 0);
  EXPECT_EQ(405, request->response_info().headers->response_code());

  request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("POST");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_GE(delegate.request_status(), 0);
  EXPECT_EQ(405, request->response_info().headers->response_code());

  EXPECT_CALL(
      *MockDevToolsManagerDelegate::last_instance,
      CreateNewTarget(GURL("about:blank"), DevToolsManagerDelegate::kFrame))
      .WillOnce(Return(base::MakeRefCounted<MockDevToolsAgentHost>()));

  request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("PUT");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_GE(delegate.request_status(), 0);
  EXPECT_EQ(200, request->response_info().headers->response_code());

  DevToolsAgentHost::StopRemoteDebuggingServer();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}

TEST_F(DevToolsHttpHandlerTest, TestJsonNew) {
  base::RunLoop run_loop, run_loop_2;
  auto* factory = new TCPServerSocketFactory(run_loop.QuitClosure(),
                                             run_loop_2.QuitClosure());
  DevToolsAgentHost::StartRemoteDebuggingServer(
      base::WrapUnique(factory), base::FilePath(), base::FilePath());
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  int port = factory->port();
  net::TestDelegate delegate;

  EXPECT_CALL(
      *MockDevToolsManagerDelegate::last_instance,
      CreateNewTarget(GURL("about:blank"), DevToolsManagerDelegate::kFrame));
  GURL url(base::StringPrintf("http://127.0.0.1:%d/json/new", port));
  auto request_context = net::CreateTestURLRequestContextBuilder()->Build();
  auto request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("PUT");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_GE(delegate.request_status(), 0);

  EXPECT_CALL(*MockDevToolsManagerDelegate::last_instance,
              CreateNewTarget(GURL("http://example.com"),
                              DevToolsManagerDelegate::kFrame));
  url = GURL(base::StringPrintf(
      "http://127.0.0.1:%d/json/new?%s", port,
      base::EscapeQueryParamValue("http://example.com", true).c_str()));
  request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("PUT");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_GE(delegate.request_status(), 0);

  EXPECT_CALL(*MockDevToolsManagerDelegate::last_instance,
              CreateNewTarget(GURL("http://example.com"),
                              DevToolsManagerDelegate::kTab));
  url = GURL(base::StringPrintf(
      "http://127.0.0.1:%d/json/new?%s&for_tab", port,
      base::EscapeQueryParamValue("http://example.com", true).c_str()));
  request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("PUT");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_GE(delegate.request_status(), 0);

  DevToolsAgentHost::StopRemoteDebuggingServer();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}

TEST_F(DevToolsHttpHandlerTest, TestJsonList) {
  base::RunLoop run_loop, run_loop_2;
  auto* factory = new TCPServerSocketFactory(run_loop.QuitClosure(),
                                             run_loop_2.QuitClosure());
  DevToolsAgentHost::StartRemoteDebuggingServer(
      base::WrapUnique(factory), base::FilePath(), base::FilePath());
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  int port = factory->port();
  net::TestDelegate delegate;

  EXPECT_CALL(*MockDevToolsManagerDelegate::last_instance,
              RemoteDebuggingTargets(DevToolsManagerDelegate::kFrame));
  GURL url(base::StringPrintf("http://127.0.0.1:%d/json", port));
  auto request_context = net::CreateTestURLRequestContextBuilder()->Build();
  auto request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("PUT");
  request->Start();
  delegate.RunUntilComplete();

  EXPECT_CALL(*MockDevToolsManagerDelegate::last_instance,
              RemoteDebuggingTargets(DevToolsManagerDelegate::kFrame))
      .WillOnce(Return(std::vector<scoped_refptr<DevToolsAgentHost>>{
          base::MakeRefCounted<MockDevToolsAgentHostWithType>("service_worker"),
          base::MakeRefCounted<MockDevToolsAgentHostWithType>("tab")}));
  url = GURL(base::StringPrintf("http://127.0.0.1:%d/json/list", port));
  request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("PUT");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.data_received(),
              HasSubstr(R"("type": "service_worker")"));
  EXPECT_THAT(delegate.data_received(), Not(HasSubstr(R"("type": "tab")")));

  EXPECT_CALL(*MockDevToolsManagerDelegate::last_instance,
              RemoteDebuggingTargets(DevToolsManagerDelegate::kTab))
      .WillOnce(Return(std::vector<scoped_refptr<DevToolsAgentHost>>{
          base::MakeRefCounted<MockDevToolsAgentHostWithType>("service_worker"),
          base::MakeRefCounted<MockDevToolsAgentHostWithType>("tab")}));
  url = GURL(base::StringPrintf("http://127.0.0.1:%d/json/list?for_tab", port));
  request = request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_method("PUT");
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.data_received(),
              HasSubstr(R"("type": "service_worker")"));
  EXPECT_THAT(delegate.data_received(), HasSubstr(R"("type": "tab")"));

  DevToolsAgentHost::StopRemoteDebuggingServer();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}

class DevToolsWebSocketHandlerTest : public DevToolsHttpHandlerTest {
 public:
  DevToolsWebSocketHandlerTest() = default;

  int StartServer() {
    std::unique_ptr<TCPServerSocketFactory> factory =
        std::make_unique<TCPServerSocketFactory>(run_loop_.QuitClosure(),
                                                 run_loop_2_.QuitClosure());
    TCPServerSocketFactory* factory_raw = factory.get();

    DevToolsAgentHost::StartRemoteDebuggingServer(
        std::move(factory), base::FilePath(), base::FilePath());
    // Our dummy socket factory will post a quit message once the server will
    // become ready.
    run_loop_.Run();
    return factory_raw->port();
  }

  void StopServer() {
    DevToolsAgentHost::StopRemoteDebuggingServer();
    // Make sure the handler actually stops.
    run_loop_2_.Run();
  }

  std::string GetWebSocketDebuggingURL(int port) {
    GURL url(base::StringPrintf("http://127.0.0.1:%d/json/version", port));
    net::TestDelegate delegate;
    auto request = request_context_->CreateRequest(
        url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_GE(delegate.request_status(), 0);
    std::optional<base::Value> response =
        base::JSONReader::Read(delegate.data_received());
    base::Value::Dict* dict = response->GetIfDict();
    // Compute HTTP upgrade request URL.
    std::string debugging_url = *dict->FindString("webSocketDebuggerUrl");
    std::string prefix = "ws://";
    return "http://" + debugging_url.substr(prefix.length());
  }

  std::unique_ptr<net::URLRequest> RunRequestUntilCompletion(
      std::string url,
      std::map<std::string, std::string> headers) {
    net::TestDelegate delegate;
    auto request = request_context_->CreateRequest(
        GURL(url), net::DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    for (auto const& [key, value] : headers) {
      request->SetExtraRequestHeaderByName(key, value, true);
    }

    request->Start();
    delegate.RunUntilComplete();
    EXPECT_GE(delegate.request_status(), 0);
    return request;
  }

 private:
  std::unique_ptr<net::URLRequestContext> request_context_ =
      net::CreateTestURLRequestContextBuilder()->Build();
  base::RunLoop run_loop_;
  base::RunLoop run_loop_2_;
};

TEST_F(DevToolsWebSocketHandlerTest,
       TestRejectsWebSocketConnectionsWithOrigin) {
  int port = StartServer();

  std::string debugging_url = GetWebSocketDebuggingURL(port);

  // Accepts an upgrade request without an Origin header.
  auto request =
      RunRequestUntilCompletion(debugging_url, {
                                                   {"connection", "upgrade"},
                                                   {"upgrade", "websocket"},
                                               });
  // This error is expected because it's not a well-formed WS request.
  // It means that the request is accepted by the server though.
  EXPECT_EQ(request->GetResponseCode(), 500);

  // Denies an upgrade request with an Origin header.
  request = RunRequestUntilCompletion(debugging_url,
                                      {{"connection", "upgrade"},
                                       {"upgrade", "websocket"},
                                       {"origin", "http://localhost"}});
  EXPECT_EQ(request->GetResponseCode(), 403);

  StopServer();
}

TEST_F(DevToolsWebSocketHandlerTest, TestAllowsCLIOverrideAllowsOriginsStar) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kRemoteAllowOrigins, "*");
  int port = StartServer();

  std::string debugging_url = GetWebSocketDebuggingURL(port);

  auto request = RunRequestUntilCompletion(debugging_url,
                                           {
                                               {"connection", "upgrade"},
                                               {"upgrade", "websocket"},
                                               {"origin", "http://localhost"},
                                           });
  // This error is expected because it's not a well-formed WS request.
  // It means that the request is accepted by the server though.
  EXPECT_EQ(request->GetResponseCode(), 500);

  StopServer();
}

TEST_F(DevToolsWebSocketHandlerTest, TestAllowsCLIOverrideAllowsOrigins) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kRemoteAllowOrigins, "http://localhost");
  int port = StartServer();

  std::string debugging_url = GetWebSocketDebuggingURL(port);

  auto request = RunRequestUntilCompletion(debugging_url,
                                           {
                                               {"connection", "upgrade"},
                                               {"upgrade", "websocket"},
                                               {"origin", "http://localhost"},
                                           });
  // This error is expected because it's not a well-formed WS request.
  // It means that the request is accepted by the server though.
  EXPECT_EQ(request->GetResponseCode(), 500);

  request = RunRequestUntilCompletion(debugging_url,
                                      {
                                          {"connection", "upgrade"},
                                          {"upgrade", "websocket"},
                                          {"origin", "http://127.0.0.1"},
                                      });
  EXPECT_EQ(request->GetResponseCode(), 403);

  StopServer();
}

}  // namespace content
