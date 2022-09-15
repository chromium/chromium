// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Optional;

namespace content {

namespace {
const char kTestHeaders[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
}  // namespace

using TopFramePopulationBrowsertest = ContentBrowserTest;

// Test that the top frame origin field is populated on subresource requests
// from a top frame.
IN_PROC_BROWSER_TEST_F(TopFramePopulationBrowsertest, FromTopFrame) {
  int number_of_frame_loaders = 0;
  bool attempted_to_load_image = false;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& unused_origin,
              bool unused_is_for_isolated_world) {
            ASSERT_TRUE(params);

            // Ignore URLLoaderFactoryParams for the initial empty document.
            if (params->isolation_info.top_frame_origin()->opaque())
              return;

            ASSERT_THAT(params->isolation_info.top_frame_origin(),
                        Optional(url::Origin::Create(GURL("http://main.com"))));
            ++number_of_frame_loaders;
          }));

  // Serve a page from which the renderer will make a subresource
  // request, in order to observe this request's top frame origin and verify
  // that it is correct.
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&attempted_to_load_image](URLLoaderInterceptor::RequestParams* params) {
        std::string spec = params->url_request.url.spec();

        if (spec.find("main") != std::string::npos) {
          URLLoaderInterceptor::WriteResponse(
              kTestHeaders,
              R"(<html><img src="http://www.image.com/image.png"></html>)",
              params->client.get());

          return true;
        }
        if (spec.find("image")) {
          attempted_to_load_image = true;
        }
        return false;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://main.com/")));

  // As a sanity check, make sure the test did actually try to load the
  // subresource.
  ASSERT_TRUE(attempted_to_load_image);
  ASSERT_EQ(number_of_frame_loaders, 1);
}

// Test that the top frame origin field is populated on subresource requests
// from a nested frame.
IN_PROC_BROWSER_TEST_F(TopFramePopulationBrowsertest, FromNestedFrame) {
  int number_of_frame_loaders = 0;
  bool attempted_to_load_image = false;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& unused_origin,
              bool unused_is_for_isolated_world) {
            ASSERT_TRUE(params);

            // Ignore URLLoaderFactoryParams for the initial empty document.
            if (params->isolation_info.top_frame_origin()->opaque())
              return;

            ASSERT_THAT(params->isolation_info.top_frame_origin(),
                        Optional(url::Origin::Create(GURL("http://main.com"))));
            ++number_of_frame_loaders;
          }));

  // Serve a page with a nested cross-origin frame in order to, when the
  // renderer makes a subresource request from this nested frame, verify that
  // the request's top frame origin correctly equals the top frame's origin
  // (instead of, say, the nested frame's origin).
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&attempted_to_load_image](URLLoaderInterceptor::RequestParams* params) {
        std::string spec = params->url_request.url.spec();

        if (spec.find("main") != std::string::npos) {
          URLLoaderInterceptor::WriteResponse(kTestHeaders, R"(<html><iframe
              src="http://frame.com/"></html>)",
                                              params->client.get());

          return true;
        }
        if (spec.find("frame") != std::string::npos) {
          URLLoaderInterceptor::WriteResponse(
              kTestHeaders,
              R"(<html><img src="http://www.image.com/image.png"></html>)",
              params->client.get());

          return true;
        }
        if (spec.find("image")) {
          attempted_to_load_image = true;
        }
        return false;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://main.com/")));

  // As a sanity check, make sure the test did actually try to load the
  // subresource.
  ASSERT_TRUE(attempted_to_load_image);
  ASSERT_EQ(number_of_frame_loaders, 2);
}

}  // namespace content
