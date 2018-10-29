// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_impl.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/mojo_async_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/test/url_request/url_request_slow_download_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_filter.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr int kChildId = 99;

// The test parameter is the number of bytes allocated for the buffer in the
// data pipe, for testing the case where the allocated size is smaller than the
// size the mime sniffer *implicitly* requires.
class URLLoaderFactoryImplTest : public ::testing::TestWithParam<size_t> {
 public:
  URLLoaderFactoryImplTest()
      : browser_context_(new TestBrowserContext()),
        resource_message_filter_(new ResourceMessageFilter(
            kChildId,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            BrowserContext::GetSharedCorsOriginAccessList(
                browser_context_.get()),
            base::Bind(&URLLoaderFactoryImplTest::GetContexts,
                       base::Unretained(this)),
            base::CreateSingleThreadTaskRunnerWithTraits(
                {BrowserThread::IO}))) {
    // Some tests specify request.report_raw_headers, but the RDH checks the
    // CanReadRawCookies permission before enabling it.
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(kChildId);
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
        kChildId);

    resource_message_filter_->InitializeForTest();
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(GetParam());
    rdh_.SetLoaderDelegate(&loader_deleate_);

    mojo::StrongBinding<network::mojom::URLLoaderFactory>::Create(
        std::make_unique<URLLoaderFactoryImpl>(
            resource_message_filter_->requester_info_for_test()),
        mojo::MakeRequest(&factory_));

    // Calling this function creates a request context.
    browser_context_->GetResourceContext()->GetRequestContext();
    base::RunLoop().RunUntilIdle();
  }

  ~URLLoaderFactoryImplTest() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(kChildId);
    rdh_.SetDelegate(nullptr);
    net::URLRequestFilter::GetInstance()->ClearHandlers();

    resource_message_filter_->OnChannelClosing();
    rdh_.CancelRequestsForProcess(resource_message_filter_->child_id());
    base::RunLoop().RunUntilIdle();
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(
        MojoAsyncResourceHandler::kDefaultAllocationSize);
  }

  void GetContexts(ResourceType resource_type,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context) {
    *resource_context = browser_context_->GetResourceContext();
    *request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
  }

  // Must outlive all members below.
  TestBrowserThreadBundle thread_bundle_{TestBrowserThreadBundle::IO_MAINLOOP};

  LoaderDelegateImpl loader_deleate_;
  ResourceDispatcherHostImpl rdh_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<ResourceMessageFilter> resource_message_filter_;
  network::mojom::URLLoaderFactoryPtr factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImplTest);
};

TEST_P(URLLoaderFactoryImplTest, GetResponse) {
  constexpr int32_t kRoutingId = 81;
  constexpr int32_t kRequestId = 28;
  network::mojom::URLLoaderPtr loader;
  base::FilePath root;
  base::PathService::Get(DIR_TEST_DATA, &root);
  net::URLRequestMockHTTPJob::AddUrlHandlers(root);
  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  // Assume the file contents is small enough to be stored in the data pipe.
  request.url = net::URLRequestMockHTTPJob::GetMockUrl("hello.html");
  request.method = "GET";
  // |resource_type| can't be a frame type. It is because when PlzNavigate is
  // enabled, the url scheme of frame type requests from the renderer process
  // must be blob scheme.
  request.resource_type = RESOURCE_TYPE_XHR;
  // Need to set same-site |request_initiator| for non main frame type request.
  request.request_initiator = url::Origin::Create(request.url);
  factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), kRoutingId, kRequestId,
      network::mojom::kURLLoadOptionSniffMimeType, request,
      client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());
  ASSERT_FALSE(client.has_received_completion());

  client.RunUntilResponseReceived();

  net::URLRequest* url_request =
      rdh_.GetURLRequest(GlobalRequestID(kChildId, kRequestId));
  ASSERT_TRUE(url_request);
  ResourceRequestInfoImpl* request_info =
      ResourceRequestInfoImpl::ForRequest(url_request);
  ASSERT_TRUE(request_info);
  EXPECT_EQ(kChildId, request_info->GetChildID());
  EXPECT_EQ(kRoutingId, request_info->GetRouteID());
  EXPECT_EQ(kRequestId, request_info->GetRequestID());

  ASSERT_FALSE(client.has_received_completion());

  client.RunUntilComplete();
  ASSERT_TRUE(client.response_body().is_valid());
  ASSERT_TRUE(client.has_received_completion());

  EXPECT_EQ(200, client.response_head().headers->response_code());
  std::string content_type;
  client.response_head().headers->GetNormalizedHeader("content-type",
                                                      &content_type);
  EXPECT_EQ("text/html", content_type);
  EXPECT_EQ(0, client.completion_status().error_code);

  std::string contents;
  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult r = client.response_body().ReadData(buffer, &read_size,
                                                   MOJO_READ_DATA_FLAG_NONE);
    if (r == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (r == MOJO_RESULT_SHOULD_WAIT)
      continue;
    ASSERT_EQ(MOJO_RESULT_OK, r);
    contents += std::string(buffer, read_size);
  }
  std::string expected;
  base::ReadFileToString(
      root.Append(base::FilePath(FILE_PATH_LITERAL("hello.html"))), &expected);
  EXPECT_EQ(expected, contents);
  EXPECT_EQ(static_cast<int64_t>(expected.size()) +
                client.response_head().encoded_data_length,
            client.completion_status().encoded_data_length);
  EXPECT_EQ(static_cast<int64_t>(expected.size()),
            client.completion_status().encoded_body_length);
  EXPECT_EQ(static_cast<int64_t>(expected.size()), client.body_transfer_size());
  EXPECT_GT(client.body_transfer_size(), 0);
  EXPECT_GT(client.response_head().encoded_data_length, 0);
  EXPECT_GT(client.completion_status().encoded_data_length, 0);
}

TEST_P(URLLoaderFactoryImplTest, GetFailedResponse) {
  network::mojom::URLLoaderPtr loader;
  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  net::URLRequestFailedJob::AddUrlHandler();
  request.url = net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      net::URLRequestFailedJob::START, net::ERR_TIMED_OUT);
  request.method = "GET";
  // |resource_type| can't be a frame type. It is because when PlzNavigate is
  // enabled, the url scheme of frame type requests from the renderer process
  // must be blob scheme.
  request.resource_type = RESOURCE_TYPE_XHR;
  // Need to set same-site |request_initiator| for non main frame type request.
  request.request_initiator = url::Origin::Create(request.url);
  factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1,
      network::mojom::kURLLoadOptionSniffMimeType, request,
      client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());

  EXPECT_EQ(net::ERR_TIMED_OUT, client.completion_status().error_code);
  EXPECT_EQ(0, client.completion_status().encoded_data_length);
  EXPECT_EQ(0, client.completion_status().encoded_body_length);
}

// In this case, the loading fails after receiving a response.
TEST_P(URLLoaderFactoryImplTest, GetFailedResponse2) {
  network::mojom::URLLoaderPtr loader;
  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  net::URLRequestFailedJob::AddUrlHandler();
  request.url = net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      net::URLRequestFailedJob::READ_ASYNC, net::ERR_TIMED_OUT);
  request.method = "GET";
  // |resource_type| can't be a frame type. It is because when PlzNavigate is
  // enabled, the url scheme of frame type requests from the renderer process
  // must be blob scheme.
  request.resource_type = RESOURCE_TYPE_XHR;
  // Need to set same-site |request_initiator| for non main frame type request.
  request.request_initiator = url::Origin::Create(request.url);
  factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1,
      network::mojom::kURLLoadOptionSniffMimeType, request,
      client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());

  EXPECT_EQ(net::ERR_TIMED_OUT, client.completion_status().error_code);
  EXPECT_GT(client.completion_status().encoded_data_length, 0);
  EXPECT_EQ(0, client.completion_status().encoded_body_length);
}

// This test tests a case where resource loading is cancelled before started.
TEST_P(URLLoaderFactoryImplTest, InvalidURL) {
  network::mojom::URLLoaderPtr loader;
  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  request.url = GURL();
  request.method = "GET";
  // |resource_type| can't be a frame type. It is because when PlzNavigate is
  // enabled, the url scheme of frame type requests from the renderer process
  // must be blob scheme.
  request.resource_type = RESOURCE_TYPE_XHR;
  // Need to set same-site |request_initiator| for non main frame type request.
  request.request_initiator = url::Origin::Create(request.url);
  ASSERT_FALSE(request.url.is_valid());
  factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1,
      network::mojom::kURLLoadOptionSniffMimeType, request,
      client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();
  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());

  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

// This test tests a case where resource loading is cancelled before started.
TEST_P(URLLoaderFactoryImplTest, ShouldNotRequestURL) {
  network::mojom::URLLoaderPtr loader;
  network::ResourceRequest request;
  network::TestURLLoaderClient client;

  // Child processes cannot request URLs with pseudo schemes like "about",
  // except for about:blank. See ChildProcessSecurityPolicyImpl::CanRequestURL
  // for details.
  request.url = GURL("about:version");
  request.method = "GET";
  // |resource_type| can't be a frame type. It is because when PlzNavigate is
  // enabled, the url scheme of frame type requests from the renderer process
  // must be blob scheme.
  request.resource_type = RESOURCE_TYPE_XHR;
  // Need to set same-site |request_initiator| for non main frame type request.
  request.request_initiator = url::Origin::Create(request.url);
  factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 2, 1,
      network::mojom::kURLLoadOptionSniffMimeType, request,
      client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();

  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());

  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

TEST_P(URLLoaderFactoryImplTest, OnTransferSizeUpdated) {
  constexpr int32_t kRoutingId = 81;
  constexpr int32_t kRequestId = 28;
  network::mojom::URLLoaderPtr loader;
  base::FilePath root;
  base::PathService::Get(DIR_TEST_DATA, &root);
  net::URLRequestMockHTTPJob::AddUrlHandlers(root);
  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  // Assume the file contents is small enough to be stored in the data pipe.
  request.url = net::URLRequestMockHTTPJob::GetMockUrl("gzip-content.svgz");
  request.method = "GET";
  // |resource_type| can't be a frame type. It is because when PlzNavigate is
  // enabled, the url scheme of frame type requests from the renderer process
  // must be blob scheme.
  request.resource_type = RESOURCE_TYPE_XHR;
  // Need to set same-site |request_initiator| for non main frame type request.
  request.request_initiator = url::Origin::Create(request.url);
  request.report_raw_headers = true;
  factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), kRoutingId, kRequestId,
      network::mojom::kURLLoadOptionSniffMimeType, request,
      client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  client.RunUntilComplete();

  std::string contents;
  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult r = client.response_body().ReadData(buffer, &read_size,
                                                   MOJO_READ_DATA_FLAG_NONE);
    if (r == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (r == MOJO_RESULT_SHOULD_WAIT)
      continue;
    ASSERT_EQ(MOJO_RESULT_OK, r);
    contents.append(buffer, read_size);
  }

  std::string expected_encoded_body;
  base::ReadFileToString(
      root.Append(base::FilePath(FILE_PATH_LITERAL("gzip-content.svgz"))),
      &expected_encoded_body);

  EXPECT_GT(client.response_head().encoded_data_length, 0);
  EXPECT_GT(client.completion_status().encoded_data_length, 0);
  EXPECT_EQ(static_cast<int64_t>(expected_encoded_body.size()),
            client.body_transfer_size());
  EXPECT_EQ(200, client.response_head().headers->response_code());
  EXPECT_EQ(
      client.response_head().encoded_data_length + client.body_transfer_size(),
      client.completion_status().encoded_data_length);
  EXPECT_NE(client.body_transfer_size(), static_cast<int64_t>(contents.size()));
  EXPECT_EQ(client.body_transfer_size(),
            client.completion_status().encoded_body_length);
  EXPECT_EQ(contents, "Hello World!\n");
}

// Removing the loader in the remote side will cancel the request.
TEST_P(URLLoaderFactoryImplTest, CancelFromRenderer) {
  constexpr int32_t kRoutingId = 81;
  constexpr int32_t kRequestId = 28;
  network::mojom::URLLoaderPtr loader;
  base::FilePath root;
  base::PathService::Get(DIR_TEST_DATA, &root);
  net::URLRequestFailedJob::AddUrlHandler();
  network::ResourceRequest request;
  network::TestURLLoaderClient client;
  // Assume the file contents is small enough to be stored in the data pipe.
  request.url = net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_IO_PENDING);
  request.method = "GET";
  request.is_main_frame = true;
  // |resource_type| can't be a frame type. It is because when PlzNavigate is
  // enabled, the url scheme of frame type requests from the renderer process
  // must be blob scheme.
  request.resource_type = RESOURCE_TYPE_XHR;
  // Need to set same-site |request_initiator| for non main frame type request.
  request.request_initiator = url::Origin::Create(request.url);
  factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), kRoutingId, kRequestId,
      network::mojom::kURLLoadOptionSniffMimeType, request,
      client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(rdh_.GetURLRequest(GlobalRequestID(kChildId, kRequestId)));
  ASSERT_FALSE(client.has_received_response());
  ASSERT_FALSE(client.response_body().is_valid());
  ASSERT_FALSE(client.has_received_completion());

  loader = nullptr;
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(rdh_.GetURLRequest(GlobalRequestID(kChildId, kRequestId)));
}

INSTANTIATE_TEST_CASE_P(URLLoaderFactoryImplTest,
                        URLLoaderFactoryImplTest,
                        ::testing::Values(128, 32 * 1024));

}  // namespace

}  // namespace content
