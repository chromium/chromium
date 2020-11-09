// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resolve_proxy_helper.h"

#include <tuple>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestResolveProxyHelper : public ResolveProxyHelper {
 public:
  // Incoming mojo::Remote<ProxyLookupClient>s are written to
  // |proxy_lookup_client|.
  explicit TestResolveProxyHelper(
      mojo::Remote<network::mojom::ProxyLookupClient>* proxy_lookup_client)
      : ResolveProxyHelper(0 /* renderer_process_host_id */),
        proxy_lookup_client_(proxy_lookup_client) {}

  bool SendRequestToNetworkService(
      const GURL& url,
      mojo::PendingRemote<network::mojom::ProxyLookupClient>
          proxy_lookup_client) override {
    // Only one request should be send at a time.
    EXPECT_FALSE(*proxy_lookup_client_);

    if (fail_to_send_request_)
      return false;

    pending_url_ = url;
    proxy_lookup_client_->Bind(std::move(proxy_lookup_client));
    return true;
  }

  const GURL& pending_url() const { return pending_url_; }

  void set_fail_to_send_request(bool fail_to_send_request) {
    fail_to_send_request_ = fail_to_send_request;
  }

 protected:
  ~TestResolveProxyHelper() override = default;

  bool fail_to_send_request_ = false;

  mojo::Remote<network::mojom::ProxyLookupClient>* proxy_lookup_client_;
  GURL pending_url_;

  DISALLOW_COPY_AND_ASSIGN(TestResolveProxyHelper);
};

class ResolveProxyHelperTest : public testing::Test {
 public:
  struct PendingResult {
    PendingResult(bool result, const std::string& proxy_list)
        : result(result), proxy_list(proxy_list) {}

    bool result;
    std::string proxy_list;
  };

  ResolveProxyHelperTest()
      : helper_(base::MakeRefCounted<TestResolveProxyHelper>(
            &proxy_lookup_client_)) {}

 protected:
  BrowserTaskEnvironment task_environment_;
  scoped_refptr<TestResolveProxyHelper> helper_;
  mojo::Remote<network::mojom::ProxyLookupClient> proxy_lookup_client_;
};

// Issue three sequential requests -- each should succeed.
TEST_F(ResolveProxyHelperTest, Sequential) {
  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  // Execute each request sequentially (so there are never 2 requests
  // outstanding at the same time).
  base::Optional<std::string> result_proxy_list;
  helper_->ResolveProxy(url1,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  // There should be a pending proxy lookup request. Respond to it.
  EXPECT_EQ(url1, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("result1:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);
  proxy_lookup_client_.reset();
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_TRUE(result_proxy_list.has_value());
  EXPECT_EQ("PROXY result1:80", result_proxy_list.value());
  result_proxy_list.reset();

  helper_->ResolveProxy(url2,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  EXPECT_EQ(url2, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  proxy_info.UseNamedProxy("result2:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);
  proxy_lookup_client_.reset();
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_TRUE(result_proxy_list.has_value());
  EXPECT_EQ("PROXY result2:80", result_proxy_list.value());
  result_proxy_list.reset();

  helper_->ResolveProxy(url3,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  EXPECT_EQ(url3, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  proxy_info.UseNamedProxy("result3:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_TRUE(result_proxy_list.has_value());
  EXPECT_EQ("PROXY result3:80", result_proxy_list.value());
}

// Issue a request while one is already in progress -- should be queued.
TEST_F(ResolveProxyHelperTest, QueueRequests) {
  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  // Start three requests. All the requests will be pending.
  base::Optional<std::string> result_proxy_list;
  helper_->ResolveProxy(url1,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));
  helper_->ResolveProxy(url2,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));
  helper_->ResolveProxy(url3,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  // Complete first request.
  EXPECT_EQ(url1, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("result1:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);
  proxy_lookup_client_.reset();
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_TRUE(result_proxy_list.has_value());
  EXPECT_EQ("PROXY result1:80", result_proxy_list.value());
  result_proxy_list.reset();

  // Complete second request.
  EXPECT_EQ(url2, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  proxy_info.UseNamedProxy("result2:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);
  proxy_lookup_client_.reset();
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_TRUE(result_proxy_list.has_value());
  EXPECT_EQ("PROXY result2:80", result_proxy_list.value());
  result_proxy_list.reset();

  // Complete third request.
  EXPECT_EQ(url3, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  proxy_info.UseNamedProxy("result3:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_TRUE(result_proxy_list.has_value());
  EXPECT_EQ("PROXY result3:80", result_proxy_list.value());
}

// Delete the helper while a request is in progress and others are pending.
TEST_F(ResolveProxyHelperTest, CancelPendingRequests) {
  GURL url1("http://www.google1.com/");
  GURL url2("http://www.google2.com/");
  GURL url3("http://www.google3.com/");

  // Start three requests. Since the proxy resolver is async, all the
  // requests will be pending.

  base::Optional<std::string> result_proxy_list;
  helper_->ResolveProxy(url1,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));
  helper_->ResolveProxy(url2,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));
  helper_->ResolveProxy(url3,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  // Check the first request is pending.
  EXPECT_EQ(url1, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);

  // Release a reference. The |helper_| will not be deleted, since there's a
  // pending resolution.
  helper_ = nullptr;
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(proxy_lookup_client_.is_bound());
  EXPECT_FALSE(!proxy_lookup_client_.is_connected());

  // Send Mojo message on the pipe.
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("result1:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);

  // Spinning the message loop results in the helper being destroyed and closing
  // the pipe.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(!proxy_lookup_client_.is_bound() ||
              !proxy_lookup_client_.is_connected());
  // The result should not have been sent.
  EXPECT_FALSE(result_proxy_list.has_value());

  // It should also be the case that msg1, msg2, msg3 were deleted by the
  // cancellation. (Else will show up as a leak).
}

// Issue a request that fails.
TEST_F(ResolveProxyHelperTest, RequestFails) {
  GURL url("http://www.google.com/");

  base::Optional<std::string> result_proxy_list;
  helper_->ResolveProxy(url,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  // There should be a pending proxy lookup request. Respond to it.
  EXPECT_EQ(url, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  proxy_lookup_client_->OnProxyLookupComplete(net::ERR_FAILED, base::nullopt);
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_FALSE(result_proxy_list.has_value());
}

// Issue a request, only to have the Mojo pipe closed.
TEST_F(ResolveProxyHelperTest, PipeClosed) {
  GURL url("http://www.google.com/");

  base::Optional<std::string> result_proxy_list;
  helper_->ResolveProxy(url,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  // There should be a pending proxy lookup request. Respond to it by closing
  // the pipe.
  EXPECT_EQ(url, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);
  proxy_lookup_client_.reset();
  base::RunLoop().RunUntilIdle();

  // Check result.
  EXPECT_FALSE(result_proxy_list.has_value());
}

// Fail to send a request to the network service.
TEST_F(ResolveProxyHelperTest, FailToSendRequest) {
  GURL url("http://www.google.com/");

  helper_->set_fail_to_send_request(true);

  base::Optional<std::string> result_proxy_list;
  helper_->ResolveProxy(url,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));
  // No request should be pending.
  EXPECT_TRUE(helper_->pending_url().is_empty());

  // Check result.
  EXPECT_FALSE(result_proxy_list.has_value());
}

// Make sure if mojo callback is invoked after last externally owned reference
// is released, there is no crash.
// Regression test for https://crbug.com/870675
TEST_F(ResolveProxyHelperTest, Lifetime) {
  GURL url("http://www.google1.com/");

  base::Optional<std::string> result_proxy_list;
  helper_->ResolveProxy(url,
                        base::BindLambdaForTesting(
                            [&](const base::Optional<std::string>& proxy_list) {
                              result_proxy_list = proxy_list;
                            }));

  // There should be a pending proxy lookup request. Respond to it.
  EXPECT_EQ(url, helper_->pending_url());
  ASSERT_TRUE(proxy_lookup_client_);

  // Release the |helper_| pointer. The object should keep a reference to
  // itself, so should not be deleted.
  helper_ = nullptr;
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(proxy_lookup_client_.is_bound());
  EXPECT_FALSE(!proxy_lookup_client_.is_connected());

  // Send Mojo message on the pipe.
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("result1:80");
  proxy_lookup_client_->OnProxyLookupComplete(net::OK, proxy_info);

  // Spinning the message loop results in the helper being destroyed and closing
  // the pipe.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(!proxy_lookup_client_.is_bound() ||
              !proxy_lookup_client_.is_connected());
  // The result should not have been sent.
  EXPECT_FALSE(result_proxy_list.has_value());
}

}  // namespace content
