// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"

#include "build/chromeos_buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerMainResourceLoaderInterceptorTest : public testing::Test {
 public:
  ServiceWorkerMainResourceLoaderInterceptorTest() = default;
  ServiceWorkerMainResourceLoaderInterceptorTest(
      const ServiceWorkerMainResourceLoaderInterceptorTest&) = delete;
  ServiceWorkerMainResourceLoaderInterceptorTest& operator=(
      const ServiceWorkerMainResourceLoaderInterceptorTest&) = delete;
  ~ServiceWorkerMainResourceLoaderInterceptorTest() override = default;

  void SetUp() override {
    browser_context_ = std::make_unique<TestBrowserContext>();
  }

  bool ShouldCreateForNavigation(
      const GURL& url,
      network::mojom::RequestDestination request_destination) {
    return ServiceWorkerMainResourceLoaderInterceptor::
        ShouldCreateForNavigation(url, request_destination,
                                  browser_context_.get());
  }

 private:
  BrowserTaskEnvironment task_environment_{BrowserTaskEnvironment::IO_MAINLOOP};
  std::unique_ptr<TestBrowserContext> browser_context_;
};

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_HTTP) {
  EXPECT_TRUE(
      ShouldCreateForNavigation(GURL("http://host/scope/doc"),
                                network::mojom::RequestDestination::kDocument));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("http://host/scope/doc"),
                                network::mojom::RequestDestination::kEmbed));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("http://host/scope/doc"),
                                network::mojom::RequestDestination::kObject));
  EXPECT_TRUE(ShouldCreateForNavigation(
      GURL("http://host/scope/doc"),
      network::mojom::RequestDestination::kFencedframe));
}

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_HTTPS) {
  EXPECT_TRUE(
      ShouldCreateForNavigation(GURL("https://host/scope/doc"),
                                network::mojom::RequestDestination::kDocument));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("https://host/scope/doc"),
                                network::mojom::RequestDestination::kEmbed));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("https://host/scope/doc"),
                                network::mojom::RequestDestination::kObject));
  EXPECT_TRUE(ShouldCreateForNavigation(
      GURL("https://host/scope/doc"),
      network::mojom::RequestDestination::kFencedframe));
}

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_FTP) {
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("ftp://host/scope/doc"),
                                network::mojom::RequestDestination::kDocument));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("ftp://host/scope/doc"),
                                network::mojom::RequestDestination::kEmbed));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("ftp://host/scope/doc"),
                                network::mojom::RequestDestination::kObject));
  EXPECT_FALSE(ShouldCreateForNavigation(
      GURL("ftp://host/scope/doc"),
      network::mojom::RequestDestination::kFencedframe));
}

TEST_F(ServiceWorkerMainResourceLoaderInterceptorTest,
       ShouldCreateForNavigation_ExternalFileScheme) {
  bool expected_handler_created = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  expected_handler_created = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(
      expected_handler_created,
      ShouldCreateForNavigation(GURL("externalfile:drive/doc"),
                                network::mojom::RequestDestination::kDocument));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("externalfile:drive/doc"),
                                network::mojom::RequestDestination::kEmbed));
  EXPECT_FALSE(
      ShouldCreateForNavigation(GURL("externalfile:drive/doc"),
                                network::mojom::RequestDestination::kObject));
  EXPECT_EQ(expected_handler_created,
            ShouldCreateForNavigation(
                GURL("externalfile:drive/doc"),
                network::mojom::RequestDestination::kFencedframe));
}

}  // namespace content
