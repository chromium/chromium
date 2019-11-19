// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/host_list_fetcher.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "remoting/test/host_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used as a HostListCallback for testing.
void OnHostlistRetrieved(
    base::Closure done_closure,
    std::vector<remoting::test::HostInfo>* hostlist,
    const std::vector<remoting::test::HostInfo>& retrieved_hostlist) {
  *hostlist = retrieved_hostlist;

  done_closure.Run();
}

const char kAccessTokenValue[] = "test_access_token_value";
const char kHostListReadyResponse[] =
"{"
"  \"data\":{"
"    \"kind\":\"chromoting#hostList\","
"    \"items\":["
"      {"
"        \"tokenUrlPatterns\":["
"          \"tokenUrlPattern_1A\","
"          \"tokenUrlPattern_1B\","
"          \"tokenUrlPattern_1C\""
"        ],"
"        \"kind\":\"chromoting#host\","
"        \"hostId\":\"test_host_id_1\","
"        \"hostName\":\"test_host_name_1\","
"        \"publicKey\":\"test_public_key_1\","
"        \"jabberId\":\"test_jabber_id_1\","
"        \"createdTime\":\"test_created_time_1\","
"        \"updatedTime\":\"test_updated_time_1\","
"        \"status\":\"ONLINE\","
"        \"hostOfflineReason\":\"\","
"        \"hostVersion\":\"test_host_version_1\""
"      },"
"      {"
"        \"kind\":\"chromoting#host\","
"        \"hostId\":\"test_host_id_2\","
"        \"hostName\":\"test_host_name_2\","
"        \"publicKey\":\"test_public_key_2\","
"        \"jabberId\":\"test_jabber_id_2\","
"        \"createdTime\":\"test_created_time_2\","
"        \"updatedTime\":\"test_updated_time_2\","
"        \"status\":\"OFFLINE\","
"        \"hostOfflineReason\":\"test_host_offline_reason_2\","
"        \"hostVersion\":\"test_host_version_2\""
"      }"
"    ]"
"  }"
"}";
const char kHostListMissingParametersResponse[] =
"{"
"  \"data\":{"
"    \"kind\":\"chromoting#hostList\","
"    \"items\":["
"      {"
"        \"tokenUrlPatterns\":["
"          \"tokenUrlPattern_1A\","
"          \"tokenUrlPattern_1B\","
"          \"tokenUrlPattern_1C\""
"        ],"
"        \"kind\":\"chromoting#host\","
"        \"hostId\":\"test_host_id_1\","
"        \"hostName\":\"test_host_name_1\","
"        \"publicKey\":\"test_public_key_1\","
"        \"createdTime\":\"test_created_time_1\","
"        \"updatedTime\":\"test_updated_time_1\","
"        \"status\":\"OFFLINE\","
"        \"hostOfflineReason\":\"\","
"        \"hostVersion\":\"test_host_version_1\""
"      },"
"      {"
"        \"kind\":\"chromoting#host\","
"        \"hostName\":\"test_host_name_2\","
"        \"publicKey\":\"test_public_key_2\","
"        \"jabberId\":\"test_jabber_id_2\","
"        \"createdTime\":\"test_created_time_2\","
"        \"updatedTime\":\"test_updated_time_2\","
"        \"status\":\"ONLINE\","
"        \"hostOfflineReason\":\"\","
"        \"hostVersion\":\"test_host_version_2\""
"      },"
"      {"
"        \"kind\":\"chromoting#host\","
"        \"hostId\":\"test_host_id_3\","
"        \"publicKey\":\"test_public_key_3\","
"        \"jabberId\":\"test_jabber_id_3\","
"        \"createdTime\":\"test_created_time_3\","
"        \"updatedTime\":\"test_updated_time_3\","
"        \"status\":\"ONLINE\","
"        \"hostOfflineReason\":\"\","
"        \"hostVersion\":\"test_host_version_3\""
"      },"
"      {"
"        \"kind\":\"chromoting#host\","
"        \"hostId\":\"test_host_id_4\","
"        \"hostName\":\"test_host_name_4\","
"        \"jabberId\":\"test_jabber_id_4\","
"        \"createdTime\":\"test_created_time_4\","
"        \"updatedTime\":\"test_updated_time_4\","
"        \"status\":\"ONLINE\","
"        \"hostOfflineReason\":\"\","
"        \"hostVersion\":\"test_host_version_4\""
"      },"
"      {"
"        \"kind\":\"chromoting#host\","
"        \"hostId\":\"test_host_id_5\","
"        \"hostName\":\"test_host_name_5\","
"        \"publicKey\":\"test_public_key_5\","
"        \"jabberId\":\"test_jabber_id_5\","
"        \"createdTime\":\"test_created_time_5\","
"        \"updatedTime\":\"test_updated_time_5\","
"        \"status\":\"OFFLINE\","
"        \"hostVersion\":\"test_host_version_5\""
"      }"
"    ]"
"  }"
"}";
const char kHostListEmptyTokenUrlPatternsResponse[] =
"{"
"  \"data\":{"
"    \"kind\":\"chromoting#hostList\","
"    \"items\":["
"      {"
"        \"tokenUrlPatterns\":["
"        ],"
"        \"kind\":\"chromoting#host\","
"        \"hostId\":\"test_host_id_1\","
"        \"hostName\":\"test_host_name_1\","
"        \"publicKey\":\"test_public_key_1\","
"        \"jabberId\":\"test_jabber_id_1\","
"        \"createdTime\":\"test_created_time_1\","
"        \"updatedTime\":\"test_updated_time_1\","
"        \"status\":\"ONLINE\","
"        \"hostOfflineReason\":\"\","
"        \"hostVersion\":\"test_host_version_1\""
"      }"
"    ]"
"  }"
"}";
const char kHostListEmptyItemsResponse[] =
"{"
"  \"data\":{"
"    \"kind\":\"chromoting#hostList\","
"    \"items\":["
"    ]"
"  }"
"}";
const char kHostListEmptyResponse[] = "{}";

const unsigned int kExpectedEmptyPatternsHostListSize = 1;
const unsigned int kExpectedHostListSize = 2;
const unsigned int kExpectedPatternsSize = 3;

}  // namespace

namespace remoting {
namespace test {

// Provides base functionality for the HostListFetcher Tests below.
// The FakeURLFetcherFactory allows us to override the response data and payload
// for specified URLs. We use this to stub out network calls made by the
// HostListFetcher. This fixture also creates an IO MessageLoop
// for use by the HostListFetcher.
class HostListFetcherTest : public ::testing::Test {
 public:
  HostListFetcherTest() : url_fetcher_factory_(nullptr) {}
  ~HostListFetcherTest() override = default;

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Sets the HTTP status and data returned for a specified URL.
  void SetFakeResponse(const GURL& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       net::URLRequestStatus::Status status);

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::FakeURLFetcherFactory url_fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostListFetcherTest);
};

void HostListFetcherTest::SetUp() {
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListEmptyResponse, net::HTTP_NOT_FOUND,
                  net::URLRequestStatus::FAILED);

  SetFakeResponse(GURL(kHostListTestRequestUrl), kHostListEmptyResponse,
                  net::HTTP_NOT_FOUND, net::URLRequestStatus::FAILED);
}

void HostListFetcherTest::SetFakeResponse(
    const GURL& url,
    const std::string& data,
    net::HttpStatusCode code,
    net::URLRequestStatus::Status status) {
  url_fetcher_factory_.SetFakeResponse(url, data, code, status);
}

TEST_F(HostListFetcherTest, RetrieveHostListFromProd) {
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListReadyResponse, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  std::vector<HostInfo> hostlist;

  base::RunLoop run_loop;
  HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&OnHostlistRetrieved, run_loop.QuitClosure(), &hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     host_list_callback);

  run_loop.Run();

  EXPECT_EQ(hostlist.size(), kExpectedHostListSize);

  HostInfo online_host_info = hostlist.at(0);
  EXPECT_EQ(online_host_info.token_url_patterns.size(), kExpectedPatternsSize);
  EXPECT_FALSE(online_host_info.host_id.empty());
  EXPECT_FALSE(online_host_info.host_jid.empty());
  EXPECT_FALSE(online_host_info.host_name.empty());
  EXPECT_EQ(online_host_info.status, HostStatus::kHostStatusOnline);
  EXPECT_TRUE(online_host_info.offline_reason.empty());
  EXPECT_FALSE(online_host_info.public_key.empty());

  HostInfo offline_host_info = hostlist.at(1);
  EXPECT_TRUE(offline_host_info.token_url_patterns.empty());
  EXPECT_FALSE(offline_host_info.host_id.empty());
  EXPECT_FALSE(offline_host_info.host_jid.empty());
  EXPECT_FALSE(offline_host_info.host_name.empty());
  EXPECT_EQ(offline_host_info.status, HostStatus::kHostStatusOffline);
  EXPECT_FALSE(offline_host_info.offline_reason.empty());
  EXPECT_FALSE(offline_host_info.public_key.empty());
}

TEST_F(HostListFetcherTest, RetrieveHostListFromTest) {
  SetFakeResponse(GURL(kHostListTestRequestUrl), kHostListReadyResponse,
                  net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  std::vector<HostInfo> hostlist;

  base::RunLoop run_loop;
  HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&OnHostlistRetrieved, run_loop.QuitClosure(), &hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListTestRequestUrl,
                                     host_list_callback);

  run_loop.Run();

  EXPECT_EQ(hostlist.size(), kExpectedHostListSize);

  HostInfo online_host_info = hostlist.at(0);
  EXPECT_EQ(online_host_info.token_url_patterns.size(), kExpectedPatternsSize);
  EXPECT_FALSE(online_host_info.host_id.empty());
  EXPECT_FALSE(online_host_info.host_jid.empty());
  EXPECT_FALSE(online_host_info.host_name.empty());
  EXPECT_EQ(online_host_info.status, HostStatus::kHostStatusOnline);
  EXPECT_TRUE(online_host_info.offline_reason.empty());
  EXPECT_FALSE(online_host_info.public_key.empty());

  HostInfo offline_host_info = hostlist.at(1);
  EXPECT_TRUE(offline_host_info.token_url_patterns.empty());
  EXPECT_FALSE(offline_host_info.host_id.empty());
  EXPECT_FALSE(offline_host_info.host_jid.empty());
  EXPECT_FALSE(offline_host_info.host_name.empty());
  EXPECT_EQ(offline_host_info.status, HostStatus::kHostStatusOffline);
  EXPECT_FALSE(offline_host_info.offline_reason.empty());
  EXPECT_FALSE(offline_host_info.public_key.empty());
}

TEST_F(HostListFetcherTest, RetrieveHostListWithEmptyPatterns) {
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListEmptyTokenUrlPatternsResponse, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  std::vector<HostInfo> hostlist;

  base::RunLoop run_loop;
  HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&OnHostlistRetrieved, run_loop.QuitClosure(), &hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     host_list_callback);

  run_loop.Run();

  EXPECT_EQ(hostlist.size(), kExpectedEmptyPatternsHostListSize);

  // While this is unlikely to happen, empty token url patterns are handled.
  HostInfo online_host_info = hostlist.at(0);
  EXPECT_TRUE(online_host_info.token_url_patterns.empty());
  EXPECT_FALSE(online_host_info.host_id.empty());
  EXPECT_FALSE(online_host_info.host_jid.empty());
  EXPECT_FALSE(online_host_info.host_name.empty());
  EXPECT_EQ(online_host_info.status, HostStatus::kHostStatusOnline);
  EXPECT_TRUE(online_host_info.offline_reason.empty());
  EXPECT_FALSE(online_host_info.public_key.empty());
}

TEST_F(HostListFetcherTest,
    RetrieveHostListMissingParametersResponse) {
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListMissingParametersResponse, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  std::vector<HostInfo> hostlist;

  base::RunLoop run_loop;
  HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&OnHostlistRetrieved, run_loop.QuitClosure(), &hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     host_list_callback);
  run_loop.Run();

  EXPECT_EQ(hostlist.size(), kExpectedHostListSize);

  HostInfo no_jid_host_info = hostlist.at(0);
  EXPECT_EQ(no_jid_host_info.token_url_patterns.size(), kExpectedPatternsSize);
  EXPECT_FALSE(no_jid_host_info.host_id.empty());
  EXPECT_TRUE(no_jid_host_info.host_jid.empty());
  EXPECT_FALSE(no_jid_host_info.host_name.empty());
  EXPECT_EQ(no_jid_host_info.status, HostStatus::kHostStatusOffline);
  EXPECT_TRUE(no_jid_host_info.offline_reason.empty());
  EXPECT_FALSE(no_jid_host_info.public_key.empty());

  HostInfo no_offline_reason_host_info = hostlist.at(1);
  EXPECT_TRUE(no_offline_reason_host_info.token_url_patterns.empty());
  EXPECT_FALSE(no_offline_reason_host_info.host_id.empty());
  EXPECT_FALSE(no_offline_reason_host_info.host_jid.empty());
  EXPECT_FALSE(no_offline_reason_host_info.host_name.empty());
  EXPECT_EQ(no_offline_reason_host_info.status, HostStatus::kHostStatusOffline);
  EXPECT_TRUE(no_offline_reason_host_info.offline_reason.empty());
  EXPECT_FALSE(no_offline_reason_host_info.public_key.empty());
}


TEST_F(HostListFetcherTest, RetrieveHostListNetworkError) {
  base::RunLoop run_loop;

  std::vector<HostInfo> hostlist;

  HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&OnHostlistRetrieved, run_loop.QuitClosure(), &hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     host_list_callback);
  run_loop.Run();

  // If there was a network error retrieving the host list, then the host list
  // should be empty.
  EXPECT_TRUE(hostlist.empty());
}

TEST_F(HostListFetcherTest, RetrieveHostListEmptyItemsResponse) {
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListEmptyItemsResponse, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  base::RunLoop run_loop;

  std::vector<HostInfo> hostlist;

  HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&OnHostlistRetrieved, run_loop.QuitClosure(), &hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     host_list_callback);
  run_loop.Run();

  // If we received an empty items response, then host list should be empty.
  EXPECT_TRUE(hostlist.empty());
}

TEST_F(HostListFetcherTest, RetrieveHostListEmptyResponse) {
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListEmptyResponse, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  base::RunLoop run_loop;

  std::vector<HostInfo> hostlist;

  HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&OnHostlistRetrieved, run_loop.QuitClosure(), &hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     host_list_callback);
  run_loop.Run();

  // If we received an empty response, then host list should be empty.
  EXPECT_TRUE(hostlist.empty());
}

TEST_F(HostListFetcherTest, MultipleRetrieveHostListRequests) {
  // First, we will retrieve a valid response from the directory service.
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListReadyResponse, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  std::vector<HostInfo> ready_hostlist;

  base::RunLoop ready_run_loop;
  HostListFetcher::HostlistCallback ready_host_list_callback =
      base::Bind(&OnHostlistRetrieved,
                 ready_run_loop.QuitClosure(),
                 &ready_hostlist);

  HostListFetcher host_list_fetcher;
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     ready_host_list_callback);

  ready_run_loop.Run();

  EXPECT_EQ(ready_hostlist.size(), kExpectedHostListSize);

  HostInfo online_host_info = ready_hostlist.at(0);
  EXPECT_EQ(online_host_info.token_url_patterns.size(), kExpectedPatternsSize);
  EXPECT_FALSE(online_host_info.host_id.empty());
  EXPECT_FALSE(online_host_info.host_jid.empty());
  EXPECT_FALSE(online_host_info.host_name.empty());
  EXPECT_EQ(online_host_info.status, HostStatus::kHostStatusOnline);
  EXPECT_TRUE(online_host_info.offline_reason.empty());
  EXPECT_FALSE(online_host_info.public_key.empty());

  HostInfo offline_host_info = ready_hostlist.at(1);
  EXPECT_TRUE(offline_host_info.token_url_patterns.empty());
  EXPECT_FALSE(offline_host_info.host_id.empty());
  EXPECT_FALSE(offline_host_info.host_jid.empty());
  EXPECT_FALSE(offline_host_info.host_name.empty());
  EXPECT_EQ(offline_host_info.status, HostStatus::kHostStatusOffline);
  EXPECT_FALSE(offline_host_info.offline_reason.empty());
  EXPECT_FALSE(offline_host_info.public_key.empty());

  // Next, we will retrieve an empty items response from the directory service.
  SetFakeResponse(GURL(kHostListProdRequestUrl),
                  kHostListEmptyItemsResponse, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);

  std::vector<HostInfo> empty_items_hostlist;

  base::RunLoop empty_items_run_loop;

  HostListFetcher::HostlistCallback empty_host_list_callback =
      base::Bind(&OnHostlistRetrieved,
                 empty_items_run_loop.QuitClosure(),
                 &empty_items_hostlist);

  // Re-use the same host_list_fetcher.
  host_list_fetcher.RetrieveHostlist(kAccessTokenValue, kHostListProdRequestUrl,
                                     empty_host_list_callback);

  empty_items_run_loop.Run();

  // If we received an empty items response, then host list should be empty.
  EXPECT_TRUE(empty_items_hostlist.empty());
}

}  // namespace test
}  // namespace remoting
