// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/dhcp_pac_file_fetcher_mojo.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy_resolution/mock_pac_file_fetcher.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/dhcp_pac_file_fetcher_mojo.h"
#include "services/network/mock_mojo_dhcp_wpad_url_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using net::test::IsError;
using net::test::IsOk;
}  // namespace

namespace network {

class DhcpPacFileFetcherMojoTest : public testing::Test {
 public:
  DhcpPacFileFetcherMojoTest() = default;
  ~DhcpPacFileFetcherMojoTest() override {}

 protected:
  void CreateFetcher(const std::string& pac_url) {
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    auto context = context_builder->Build();
    dhcp_pac_file_fetcher_mojo_ = std::make_unique<DhcpPacFileFetcherMojo>(
        context.get(),
        network::MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
            pac_url));
    mock_pac_file_fetcher_ = new net::MockPacFileFetcher();
    dhcp_pac_file_fetcher_mojo_->SetPacFileFetcherForTesting(
        base::WrapUnique(mock_pac_file_fetcher_.get()));
  }

  std::unique_ptr<DhcpPacFileFetcherMojo> dhcp_pac_file_fetcher_mojo_;
  raw_ptr<net::MockPacFileFetcher> mock_pac_file_fetcher_;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Test that the PAC URL set by the client is used.
TEST_F(DhcpPacFileFetcherMojoTest, UsePacSctipt) {
  GURL pac_url("http://wpad.test.com/wpad.dat");
  CreateFetcher(pac_url.spec());

  net::TestCompletionCallback callback;
  std::u16string pac_text;
  dhcp_pac_file_fetcher_mojo_->Fetch(&pac_text, callback.callback(),
                                     net::NetLogWithSource(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
  mock_pac_file_fetcher_->WaitUntilFetch();
  EXPECT_EQ(pac_url, mock_pac_file_fetcher_->pending_request_url());
  mock_pac_file_fetcher_->NotifyFetchCompletion(net::OK, "script");

  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

// Test that error is returned when PAC URL is missing.
TEST_F(DhcpPacFileFetcherMojoTest, PacScriptMissing) {
  CreateFetcher(std::string());

  net::TestCompletionCallback callback;
  std::u16string pac_text;
  dhcp_pac_file_fetcher_mojo_->Fetch(&pac_text, callback.callback(),
                                     net::NetLogWithSource(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_THAT(callback.WaitForResult(), net::ERR_PAC_NOT_IN_DHCP);
}

}  // namespace network
