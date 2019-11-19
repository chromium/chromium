// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/net/oldhttp/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "fuchsia/http/http_service_impl.h"
#include "fuchsia/http/url_loader_impl.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace oldhttp = ::fuchsia::net::oldhttp;

namespace {

const base::FilePath::CharType kTestFilePath[] =
    FILE_PATH_LITERAL("fuchsia/http/testdata");

// Capacity, in bytes, for buffers used to read data off the URLResponse.
const size_t kBufferCapacity = 1024;

using ResponseHeaders = std::multimap<std::string, std::string>;

class HttpServiceTest : public ::testing::Test {
 public:
  HttpServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        binding_(&http_service_server_) {
    // Initialize the test server.
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL(kTestFilePath)));
    net::test_server::RegisterDefaultHandlers(&test_server_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  void SetUp() override {
    ASSERT_TRUE(test_server_.Start());

    // Bind the service with the client-side interface.
    binding_.Bind(http_service_interface_.NewRequest());
  }

  void TearDown() override {
    // Disconnect the client and wait for the service to shut down.
    base::RunLoop run_loop;
    binding_.set_error_handler(
        [&run_loop](zx_status_t status) { run_loop.Quit(); });
    http_service_interface_.Unbind();
    run_loop.Run();
    binding_.set_error_handler(nullptr);

    // Check there are no pending requests.
    EXPECT_EQ(URLLoaderImpl::GetNumActiveRequestsForTests(), 0);
  }

  // Helper method to start |request| on |url_loader|
  void ExecuteRequest(const oldhttp::URLLoaderPtr& url_loader,
                      oldhttp::URLRequest request) {
    base::RunLoop run_loop;

    url_loader->Start(std::move(request),
                      [this, &run_loop](oldhttp::URLResponse response) {
                        run_loop.Quit();
                        url_response_ = std::move(response);
                      });
    run_loop.Run();
  }

  net::EmbeddedTestServer* http_test_server() { return &test_server_; }
  oldhttp::HttpServicePtr& http_service() { return http_service_interface_; }
  oldhttp::URLResponse& url_response() { return url_response_; }

 private:
  net::EmbeddedTestServer test_server_;

  HttpServiceImpl http_service_server_;
  oldhttp::HttpServicePtr http_service_interface_;
  fidl::Binding<oldhttp::HttpService> binding_;
  oldhttp::URLResponse url_response_;

  DISALLOW_COPY_AND_ASSIGN(HttpServiceTest);
};

class TestZxHandleWatcher : public base::MessagePumpFuchsia::ZxHandleWatcher {
 public:
  explicit TestZxHandleWatcher(base::OnceClosure on_signaled)
      : on_signaled_(std::move(on_signaled)) {}
  ~TestZxHandleWatcher() override = default;

  // ZxHandleWatcher implementation.
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) override {
    signals_ = signals;
    std::move(on_signaled_).Run();
  }

  zx_signals_t signals() { return signals_; }

 protected:
  base::OnceClosure on_signaled_;
  zx_signals_t signals_ = 0;
};

// Runs MessageLoop until one of the specified |signals| is signaled on the
// |handle|. Return observed signals.
zx_signals_t RunLoopUntilSignal(zx_handle_t handle, zx_signals_t signals) {
  base::RunLoop run_loop;
  TestZxHandleWatcher watcher(run_loop.QuitClosure());
  base::MessagePumpForIO::ZxHandleWatchController watch_contoller(FROM_HERE);

  base::MessageLoopCurrentForIO::Get()->WatchZxHandle(
      handle, /*persistent=*/false, signals, &watch_contoller, &watcher);
  run_loop.Run();

  return watcher.signals();
}

void CheckResponseError(const oldhttp::URLResponse& response,
                        int expected_network_error) {
  // Unexpected network error.
  ASSERT_TRUE(expected_network_error != net::OK || !response.error)
      << response.error->description;

  // Unexpected success.
  ASSERT_TRUE(expected_network_error == net::OK || response.error) << "net::OK";

  // Wrong network error.
  ASSERT_TRUE(expected_network_error == net::OK ||
              response.error->code == expected_network_error)
      << response.error->description;
}

void CheckResponseStream(const oldhttp::URLResponse& response,
                         const std::string& expected_response) {
  EXPECT_TRUE(response.body->is_stream());

  zx::socket stream = std::move(response.body->stream());
  size_t offset = 0;

  while (true) {
    std::array<char, kBufferCapacity> buffer;
    size_t size = 0;
    zx_status_t result = stream.read(0, buffer.data(), kBufferCapacity, &size);

    if (result == ZX_ERR_SHOULD_WAIT) {
      zx_signals_t signals = RunLoopUntilSignal(
          stream.get(), ZX_SOCKET_READABLE | ZX_SOCKET_PEER_CLOSED);

      if (signals & ZX_SOCKET_READABLE) {
        // Attempt to read again now that the socket is readable.
        continue;
      } else if (signals & ZX_SOCKET_PEER_CLOSED) {
        // Done reading.
        break;
      } else {
        NOTREACHED();
      }
    } else if (result == ZX_ERR_PEER_CLOSED) {
      // Done reading.
      break;
    }
    EXPECT_EQ(result, ZX_OK);

    EXPECT_TRUE(std::equal(buffer.begin(), buffer.begin() + size,
                           expected_response.begin() + offset));
    offset += size;
  }

  EXPECT_EQ(offset, expected_response.length());
}

void CheckResponseBuffer(const oldhttp::URLResponse& response,
                         const std::string& expected_response) {
  EXPECT_TRUE(response.body->is_buffer());

  fuchsia::mem::Buffer mem_buffer = std::move(response.body->buffer());
  size_t response_size = mem_buffer.size;
  EXPECT_EQ(mem_buffer.size, expected_response.length());

  std::array<char, kBufferCapacity> buffer;
  size_t offset = 0;
  while (offset != mem_buffer.size) {
    size_t length = std::min(response_size - offset, kBufferCapacity);
    zx_status_t result = mem_buffer.vmo.read(buffer.data(), offset, length);
    EXPECT_EQ(result, ZX_OK);

    EXPECT_TRUE(std::equal(buffer.begin(), buffer.begin() + response_size,
                           expected_response.begin() + offset));
    offset += response_size;
  }

  EXPECT_EQ(offset, expected_response.length());
}

void CheckResponseHeaders(const oldhttp::URLResponse& response,
                          ResponseHeaders* expected_headers) {
  ASSERT_TRUE(response.headers.has_value());
  for (auto& header : response.headers.value()) {
    const std::string header_name = header.name.data();
    const std::string header_value = header.value.data();
    auto iter = std::find_if(expected_headers->begin(), expected_headers->end(),
                             [&header_name, &header_value](auto& elt) -> bool {
                               return elt.first.compare(header_name) == 0 &&
                                      elt.second.compare(header_value) == 0;
                             });
    EXPECT_NE(iter, expected_headers->end())
        << "Unexpected header: \"" << header_name << "\" with value: \""
        << header_value << "\".";
    if (iter != expected_headers->end()) {
      expected_headers->erase(iter);
    }
  }
  EXPECT_TRUE(expected_headers->empty());
}

}  // namespace

// Check a basic end-to-end request resolution with the response being streamed
// is handled properly.
TEST_F(HttpServiceTest, BasicRequestStream) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = http_test_server()->GetURL("/simple.html").spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 200u);
  CheckResponseStream(url_response(), "hello");
}

// Check a basic end-to-end request resolution with the response being
// buffered is handled properly.
TEST_F(HttpServiceTest, BasicRequestBuffer) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = http_test_server()->GetURL("/simple.html").spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::BUFFER;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 200u);
  CheckResponseBuffer(url_response(), "hello");
}

// Check network request headers are received properly.
TEST_F(HttpServiceTest, RequestWithHeaders) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = http_test_server()->GetURL("/with-headers.html").spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 200u);
  CheckResponseStream(
      url_response(),
      "This file is boring; all the action's in the .mock-http-headers.\n");
  ResponseHeaders expected_headers = {
      {"Cache-Control", "private"},
      {"Content-Type", "text/html; charset=ISO-8859-1"},
      {"X-Multiple-Entries", "a"},
      {"X-Multiple-Entries", "b"},
  };
  CheckResponseHeaders(url_response(), &expected_headers);
}

// Check duplicate network request headers are received properly.
TEST_F(HttpServiceTest, RequestWithDuplicateHeaders) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url =
      http_test_server()->GetURL("/with-duplicate-headers.html").spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 200u);
  CheckResponseStream(
      url_response(),
      "This file is boring; all the action's in the .mock-http-headers.\n");
  ResponseHeaders expected_headers = {
      {"Cache-Control", "private"},
      {"Content-Type", "text/html; charset=ISO-8859-1"},
      {"X-Multiple-Entries", "a"},
      {"X-Multiple-Entries", "a"},
      {"X-Multiple-Entries", "b"},
  };
  CheckResponseHeaders(url_response(), &expected_headers);
}

// Check a request with automatic redirect resolution is handled properly.
TEST_F(HttpServiceTest, AutoRedirect) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = http_test_server()->GetURL("/redirect-test.html").spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;
  request.auto_follow_redirects = true;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 200u);
  ASSERT_TRUE(url_response().url.has_value());
  EXPECT_EQ(url_response().url.value(),
            http_test_server()->GetURL("/with-headers.html").spec());
}

// Check a request with manual redirect resolution is handled properly.
TEST_F(HttpServiceTest, ManualRedirect) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());
  std::string request_url =
      http_test_server()->GetURL("/redirect-test.html").spec();

  oldhttp::URLRequest request;
  request.url = request_url;
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;
  request.auto_follow_redirects = false;

  ExecuteRequest(url_loader, std::move(request));
  std::string final_url =
      http_test_server()->GetURL("/with-headers.html").spec();
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 302u);
  EXPECT_EQ(url_response().url.value_or(""), request_url);
  EXPECT_EQ(url_response().redirect_url.value_or(""), final_url);

  base::RunLoop run_loop;
  url_loader->FollowRedirect(
      [&run_loop, &final_url](oldhttp::URLResponse response) {
        EXPECT_EQ(response.status_code, 200u);
        EXPECT_EQ(response.url.value_or(""), final_url);
        run_loop.Quit();
      });
  run_loop.Run();
}

// Check HTTP error codes are properly populated.
TEST_F(HttpServiceTest, HttpErrorCode) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = http_test_server()
                    ->base_url()
                    .Resolve("/non_existent_cooper.html")
                    .spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 404u);
}

// Check network error codes are properly populated.
TEST_F(HttpServiceTest, InvalidURL) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = "ht\\tp://test.test/";
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::ERR_INVALID_URL);
}

// Ensure the service can handle multiple concurrent requests.
TEST_F(HttpServiceTest, MultipleRequests) {
  const int kNumRequests = 10;
  oldhttp::URLLoaderPtr url_loaders[kNumRequests];
  for (int i = 0; i < kNumRequests; i++) {
    http_service()->CreateURLLoader(url_loaders[i].NewRequest());
  }

  base::RunLoop run_loop;
  int requests_done = 0;
  for (int i = 0; i < kNumRequests; i++) {
    oldhttp::URLRequest request;
    request.url = http_test_server()->GetURL("/simple.html").spec();
    request.method = "GET";
    request.response_body_mode = (i % 2) == 0
                                     ? oldhttp::ResponseBodyMode::STREAM
                                     : oldhttp::ResponseBodyMode::BUFFER;
    url_loaders[i]->Start(
        std::move(request),
        [&requests_done, &run_loop](oldhttp::URLResponse response) {
          requests_done++;
          if (requests_done == kNumRequests) {
            // Last request signals the run_loop to exit.
            run_loop.Quit();
          }

          CheckResponseError(response, net::OK);
          ASSERT_EQ(response.status_code, 200u);
          if (response.body->is_buffer()) {
            CheckResponseBuffer(response, "hello");
          } else {
            CheckResponseStream(response, "hello");
          }
        });
  }
  run_loop.Run();
}

// Check QueryStatus works as expected when a request is loading.
// Also checks the request is properly deleted after the binding is destroyed.
TEST_F(HttpServiceTest, QueryStatus) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = http_test_server()->GetURL("/hung-after-headers").spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;

  // In socket mode, we should still get the response headers.
  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::OK);
  EXPECT_EQ(url_response().status_code, 200u);

  base::RunLoop run_loop;
  url_loader->QueryStatus([&run_loop](oldhttp::URLLoaderStatus status) {
    EXPECT_TRUE(status.is_loading);
    run_loop.Quit();
  });
  run_loop.Run();
}

// Check the response error is properly set if the server disconnects early.
TEST_F(HttpServiceTest, CloseSocket) {
  oldhttp::URLLoaderPtr url_loader;
  http_service()->CreateURLLoader(url_loader.NewRequest());

  oldhttp::URLRequest request;
  request.url = http_test_server()->GetURL("/close-socket").spec();
  request.method = "GET";
  request.response_body_mode = oldhttp::ResponseBodyMode::STREAM;

  ExecuteRequest(url_loader, std::move(request));
  CheckResponseError(url_response(), net::ERR_EMPTY_RESPONSE);
}
