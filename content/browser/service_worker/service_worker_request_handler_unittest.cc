// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <utility>

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"

namespace content {
namespace service_worker_request_handler_unittest {

int kMockProviderId = 1;

class ServiceWorkerRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

 protected:
  void InitializeProviderHostForWindow() {
    // An empty host.
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(helper_->mock_render_process_id(),
                                    kMockProviderId,
                                    true /* is_parent_frame_secure */,
                                    context()->AsWeakPtr(), &remote_endpoint_);
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(std::move(host));
  }

  static std::unique_ptr<ServiceWorkerNavigationHandleCore>
  CreateNavigationHandleCore(ServiceWorkerContextWrapper* context_wrapper) {
    std::unique_ptr<ServiceWorkerNavigationHandleCore> navigation_handle_core;
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            [](ServiceWorkerContextWrapper* wrapper) {
              return std::make_unique<ServiceWorkerNavigationHandleCore>(
                  nullptr, wrapper);
            },
            base::RetainedRef(context_wrapper)),
        base::BindOnce(
            [](std::unique_ptr<ServiceWorkerNavigationHandleCore>* dest,
               std::unique_ptr<ServiceWorkerNavigationHandleCore> src) {
              *dest = std::move(src);
            },
            &navigation_handle_core));
    base::RunLoop().RunUntilIdle();
    return navigation_handle_core;
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const std::string& url,
                                                 const std::string& method) {
    std::unique_ptr<net::URLRequest> request =
        url_request_context_.CreateRequest(GURL(url), net::DEFAULT_PRIORITY,
                                           &url_request_delegate_,
                                           TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_method(method);
    return request;
  }

  void InitializeHandler(net::URLRequest* request,
                         bool skip_service_worker,
                         ResourceType resource_type) {
    ServiceWorkerRequestHandler::InitializeHandler(
        request, context_wrapper(), &blob_storage_context_,
        helper_->mock_render_process_id(), kMockProviderId, skip_service_worker,
        network::mojom::FetchRequestMode::kNoCORS,
        network::mojom::FetchCredentialsMode::kOmit,
        network::mojom::FetchRedirectMode::kFollow,
        std::string() /* integrity */, false /* keepalive */, resource_type,
        blink::mojom::RequestContextType::HYPERLINK,
        network::mojom::RequestContextFrameType::kTopLevel, nullptr);
  }

  static ServiceWorkerRequestHandler* GetHandler(net::URLRequest* request) {
    return ServiceWorkerRequestHandler::GetHandler(request);
  }

  std::unique_ptr<net::URLRequestJob> MaybeCreateJob(net::URLRequest* request) {
    return std::unique_ptr<net::URLRequestJob>(
        GetHandler(request)->MaybeCreateJob(
            request, url_request_context_.network_delegate(),
            context_wrapper()->resource_context()));
  }

  void InitializeHandlerSimpleTest(const std::string& url,
                                   const std::string& method,
                                   bool skip_service_worker,
                                   ResourceType resource_type) {
    // Skip handler initialization tests when S13nServiceWorker is enabled
    // because we don't use this path. See also comments in
    // ServiceWorkerRequestHandler::InitializeHandler().
    if (blink::ServiceWorkerUtils::IsServicificationEnabled())
      return;
    InitializeProviderHostForWindow();
    std::unique_ptr<net::URLRequest> request = CreateRequest(url, method);
    InitializeHandler(request.get(), skip_service_worker, resource_type);
    ASSERT_TRUE(GetHandler(request.get()));
    MaybeCreateJob(request.get());
    EXPECT_EQ(url, provider_host_->document_url().spec());
  }

  void InitializeHandlerForNavigationSimpleTest(const std::string& url,
                                                bool expected_handler_created) {
    bool handler_created = false;
    if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
      handler_created = InitializeHandlerForNavigationNetworkService(
          url, expected_handler_created);
    } else {
      handler_created = InitializeHandlerForNavigationNonNetworkService(
          url, expected_handler_created);
    }
    EXPECT_EQ(expected_handler_created, handler_created);
  }

  bool InitializeHandlerForNavigationNonNetworkService(
      const std::string& url,
      bool expected_handler_created) {
    std::unique_ptr<ServiceWorkerNavigationHandleCore> navigation_handle_core =
        CreateNavigationHandleCore(helper_->context_wrapper());
    std::unique_ptr<net::URLRequest> request = CreateRequest(url, "GET");
    ServiceWorkerRequestHandler::InitializeForNavigation(
        request.get(), navigation_handle_core.get(), &blob_storage_context_,
        false /* skip_service_worker */, RESOURCE_TYPE_MAIN_FRAME,
        blink::mojom::RequestContextType::HYPERLINK,
        network::mojom::RequestContextFrameType::kTopLevel,
        true /* is_parent_frame_secure */, nullptr /* body */,
        base::RepeatingCallback<WebContents*(void)>());
    return !!GetHandler(request.get());
  }

  bool InitializeHandlerForNavigationNetworkService(
      const std::string& url,
      bool expected_handler_created) {
    std::unique_ptr<ServiceWorkerNavigationHandleCore> navigation_handle_core =
        CreateNavigationHandleCore(helper_->context_wrapper());
    std::unique_ptr<NavigationLoaderInterceptor> interceptor =
        ServiceWorkerRequestHandler::InitializeForNavigationNetworkService(
            GURL(url), nullptr /* resource_context */,
            navigation_handle_core.get(), &blob_storage_context_,
            false /* skip_service_worker */, RESOURCE_TYPE_MAIN_FRAME,
            blink::mojom::RequestContextType::HYPERLINK,
            network::mojom::RequestContextFrameType::kTopLevel,
            true /* is_parent_frame_secure */, nullptr /* body */,
            base::RepeatingCallback<WebContents*(void)>());
    return !!interceptor.get();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  net::TestDelegate url_request_delegate_;
  storage::BlobStorageContext blob_storage_context_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;
};

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_FTP) {
  InitializeProviderHostForWindow();
  std::unique_ptr<net::URLRequest> request =
      CreateRequest("ftp://host/scope/doc", "GET");
  InitializeHandler(request.get(), false, RESOURCE_TYPE_MAIN_FRAME);
  // Cannot initialize a handler for non-secure origins.
  EXPECT_FALSE(GetHandler(request.get()));
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTP_MAIN_FRAME) {
  // HTTP should have the handler because the response is possible to be a
  // redirect to HTTPS.
  InitializeHandlerSimpleTest("http://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_MAIN_FRAME) {
  InitializeHandlerSimpleTest("https://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTP_SUB_FRAME) {
  // HTTP should have the handler because the response is possible to be a
  // redirect to HTTPS.
  InitializeHandlerSimpleTest("http://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_SUB_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_SUB_FRAME) {
  InitializeHandlerSimpleTest("https://host/scope/doc", "GET", false,
                              RESOURCE_TYPE_SUB_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_OPTIONS) {
  // OPTIONS is also supported. See crbug.com/434660.
  InitializeHandlerSimpleTest("https://host/scope/doc", "OPTIONS", false,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_HTTPS_SKIP) {
  InitializeHandlerSimpleTest("https://host/scope/doc", "GET", true,
                              RESOURCE_TYPE_MAIN_FRAME);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeHandler_IMAGE) {
  InitializeProviderHostForWindow();
  // Check provider host's URL after initializing a handler for an image.
  provider_host_->SetDocumentUrl(GURL("https://host/scope/doc"));
  std::unique_ptr<net::URLRequest> request =
      CreateRequest("https://host/scope/image", "GET");
  InitializeHandler(request.get(), true, RESOURCE_TYPE_IMAGE);
  ASSERT_FALSE(GetHandler(request.get()));
  EXPECT_EQ(GURL("https://host/scope/doc"), provider_host_->document_url());
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeForNavigation_HTTP) {
  InitializeHandlerForNavigationSimpleTest("http://host/scope/doc", true);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeForNavigation_HTTPS) {
  InitializeHandlerForNavigationSimpleTest("https://host/scope/doc", true);
}

TEST_F(ServiceWorkerRequestHandlerTest, InitializeForNavigation_FTP) {
  InitializeHandlerForNavigationSimpleTest("ftp://host/scope/doc", false);
}

TEST_F(ServiceWorkerRequestHandlerTest,
       InitializeForNavigation_ExternalFileScheme) {
  bool expected_handler_created = false;
#if defined(OS_CHROMEOS)
  expected_handler_created = true;
#endif  // OS_CHROMEOS
  InitializeHandlerForNavigationSimpleTest("externalfile:drive/doc",
                                           expected_handler_created);
}

}  // namespace service_worker_request_handler_unittest
}  // namespace content
