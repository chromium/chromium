// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_factory.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class TestURLLoaderFactoryTest : public testing::Test {
 public:
  TestURLLoaderFactoryTest() {}
  ~TestURLLoaderFactoryTest() override {}

  void SetUp() override {}

  void StartRequest(const std::string& url) { StartRequest(url, &client_); }

  void StartRequest(const std::string& url,
                    TestURLLoaderClient* client,
                    int load_flags = 0) {
    ResourceRequest request;
    request.url = GURL(url);
    request.load_flags = load_flags;
    loader_.reset();
    factory_.CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 0, 0, request,
        client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  std::string GetData(TestURLLoaderClient* client) {
    std::string response;
    EXPECT_TRUE(client->response_body().is_valid());
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client->response_body_release(), &response));
    EXPECT_EQ(
        static_cast<size_t>(client->completion_status().decoded_body_length),
        response.length());
    return response;
  }

  TestURLLoaderFactory* factory() { return &factory_; }
  TestURLLoaderClient* client() { return &client_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestURLLoaderFactory factory_;
  mojo::Remote<mojom::URLLoader> loader_;
  TestURLLoaderClient client_;
};

TEST_F(TestURLLoaderFactoryTest, Simple) {
  std::string url = "http://foo";
  std::string data = "bar";

  factory()->AddResponse(url, data);

  StartRequest(url);
  client()->RunUntilComplete();
  EXPECT_EQ(GetData(client()), data);

  // Data can be fetched multiple times.
  TestURLLoaderClient client2;
  StartRequest(url, &client2);
  client2.RunUntilComplete();
  EXPECT_EQ(GetData(&client2), data);
}

TEST_F(TestURLLoaderFactoryTest, AddResponseAdvanced) {
  std::string url = "http://example.com";
  std::string body = "Happy robot";

  // Test the full-featured version of AddResponse.
  factory()->AddResponse(GURL(url), CreateURLResponseHead(net::HTTP_OK), body,
                         URLLoaderCompletionStatus(net::OK));
  StartRequest(url);
  client()->RunUntilComplete();
  ASSERT_TRUE(client()->response_head()->headers != nullptr);
  EXPECT_EQ(net::HTTP_OK, client()->response_head()->headers->response_code());
  EXPECT_EQ(body, GetData(client()));
}

TEST_F(TestURLLoaderFactoryTest, AddResponse404) {
  std::string url = "http://foo";
  std::string body = "Sad robot";
  factory()->AddResponse(url, body, net::HTTP_NOT_FOUND);

  StartRequest(url);
  client()->RunUntilComplete();
  ASSERT_TRUE(client()->response_head()->headers != nullptr);
  EXPECT_EQ(net::HTTP_NOT_FOUND,
            client()->response_head()->headers->response_code());
  EXPECT_EQ(GetData(client()), body);
}

TEST_F(TestURLLoaderFactoryTest, MultipleSameURL) {
  std::string url = "http://foo";
  std::string data1 = "bar1";
  std::string data2 = "bar2";

  factory()->AddResponse(url, data1);
  factory()->AddResponse(url, data2);

  StartRequest(url);
  client()->RunUntilComplete();
  EXPECT_EQ(GetData(client()), data2);
}

TEST_F(TestURLLoaderFactoryTest, MultipleSameURL2) {
  // Tests for two requests to same URL happening before AddResponse.
  std::string url = "http://foo";
  std::string data = "bar1";

  StartRequest(url);
  TestURLLoaderClient client2;
  StartRequest(url, &client2);

  factory()->AddResponse(url, data);

  client()->RunUntilComplete();
  client2.RunUntilComplete();
  EXPECT_EQ(GetData(client()), data);
  EXPECT_EQ(GetData(&client2), data);
}

TEST_F(TestURLLoaderFactoryTest, Redirects) {
  GURL url("http://example.test/");

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL("http://example2.test/");
  TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, mojom::URLResponseHead::New());
  URLLoaderCompletionStatus status;
  std::string content = "foo";
  factory()->AddResponse(url, mojom::URLResponseHead::New(), content, status,
                         std::move(redirects));
  StartRequest(url.spec());
  client()->RunUntilComplete();

  EXPECT_EQ(GetData(client()), content);
  EXPECT_TRUE(client()->has_received_redirect());
  EXPECT_EQ(redirect_info.new_url, client()->redirect_info().new_url);
}

TEST_F(TestURLLoaderFactoryTest, IsPending) {
  std::string url = "http://foo/";

  // Normal lifecycle.
  EXPECT_FALSE(factory()->IsPending(url));
  StartRequest(url);
  EXPECT_TRUE(factory()->IsPending(url));
  factory()->AddResponse(url, "hi");
  client()->RunUntilComplete();
  EXPECT_FALSE(factory()->IsPending(url));

  // Cleanup between tests.
  client()->Unbind();
  factory()->ClearResponses();

  // Now with cancellation.
  StartRequest(url);
  EXPECT_TRUE(factory()->IsPending(url));
  client()->Unbind();
  EXPECT_FALSE(factory()->IsPending(url));

  // Cleanup between tests.
  client()->Unbind();
  factory()->ClearResponses();

  // Now with multiple requests
  StartRequest(url);
  client()->Unbind();
  EXPECT_FALSE(factory()->IsPending(url));
  StartRequest(url);
  EXPECT_TRUE(factory()->IsPending(url));
}

TEST_F(TestURLLoaderFactoryTest, IsPendingLoadFlags) {
  std::string url = "http://foo/";
  std::string url2 = "http://bar/";

  const network::ResourceRequest* pending_request;
  StartRequest(url);
  EXPECT_TRUE(factory()->IsPending(url, &pending_request));
  EXPECT_EQ(0, pending_request->load_flags);

  factory()->AddResponse(url, "hi");
  client()->RunUntilComplete();

  TestURLLoaderClient client2;
  StartRequest(url2, &client2, 42);
  EXPECT_TRUE(factory()->IsPending(url2, &pending_request));
  EXPECT_EQ(42, pending_request->load_flags);
  factory()->AddResponse(url2, "bye");
  client2.RunUntilComplete();
}

TEST_F(TestURLLoaderFactoryTest, NumPending) {
  std::string url = "http://foo/";
  std::string url2 = "http://bar/";

  EXPECT_EQ(0, factory()->NumPending());
  StartRequest(url);
  EXPECT_EQ(1, factory()->NumPending());
  client()->Unbind();
  // All cancelled.
  EXPECT_EQ(0, factory()->NumPending());

  TestURLLoaderClient client2;
  StartRequest(url2, &client2);
  EXPECT_EQ(1, factory()->NumPending());
  factory()->AddResponse(url2, "hello");
  client2.RunUntilComplete();
  EXPECT_EQ(0, factory()->NumPending());
}

TEST_F(TestURLLoaderFactoryTest, NumPending2) {
  std::string url = "http://foo/";

  EXPECT_EQ(0, factory()->NumPending());
  StartRequest(url);
  EXPECT_EQ(1, factory()->NumPending());

  TestURLLoaderClient client2;
  StartRequest(url, &client2);
  EXPECT_EQ(2, factory()->NumPending());
  factory()->AddResponse(url, "hello");
  client2.RunUntilComplete();
  EXPECT_EQ(0, factory()->NumPending());
}

TEST_F(TestURLLoaderFactoryTest, SimulateResponse) {
  std::string url = "http://foo/";
  network::URLLoaderCompletionStatus ok_status(net::OK);
  mojom::URLResponseHeadPtr response_head =
      CreateURLResponseHead(net::HTTP_NOT_FOUND);
  response_head->headers->SetHeader("Foo", "Bar");

  // By default no request is pending.
  EXPECT_FALSE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/""));

  StartRequest(url);
  EXPECT_EQ(1, factory()->NumPending());
  // Try with the wrong URL
  EXPECT_FALSE(factory()->SimulateResponseForPendingRequest(
      GURL("http://this_is_not_the_url_you_are_looking_for"), ok_status,
      response_head.Clone(), /*content=*/""));
  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/"hello"));
  EXPECT_EQ(0, factory()->NumPending());
  EXPECT_TRUE(client()->has_received_completion());
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
  ASSERT_TRUE(client()->response_head()->headers);
  EXPECT_EQ(net::HTTP_NOT_FOUND,
            client()->response_head()->headers->response_code());
  // Our header should be set.
  std::string value;
  EXPECT_TRUE(
      client()->response_head()->headers->GetNormalizedHeader("Foo", &value));
  EXPECT_EQ("Bar", value);
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client()->response_body_release(), &response));
  EXPECT_EQ("hello", response);
}

TEST_F(TestURLLoaderFactoryTest, SimulateResponseWait) {
  std::string url = "http://foo/";
  network::URLLoaderCompletionStatus ok_status(net::OK);
  mojom::URLResponseHeadPtr response_head =
      CreateURLResponseHead(net::HTTP_NOT_FOUND);
  response_head->headers->SetHeader("Foo", "Bar");

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { StartRequest(url); }));

  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/"",
      TestURLLoaderFactory::ResponseMatchFlags::kWaitForRequest));
}

TEST_F(TestURLLoaderFactoryTest, SimulateResponseMultipleRequests) {
  std::string url = "http://foo/";

  TestURLLoaderClient client1;
  StartRequest(url, &client1);
  TestURLLoaderClient client2;
  StartRequest(url, &client2);

  network::URLLoaderCompletionStatus ok_status(net::OK);
  mojom::URLResponseHeadPtr response_head = CreateURLResponseHead(net::HTTP_OK);

  EXPECT_EQ(2, factory()->NumPending());
  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/""));
  EXPECT_EQ(1, factory()->NumPending());
  EXPECT_TRUE(client1.has_received_completion());
  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/""));
  EXPECT_EQ(0, factory()->NumPending());
  EXPECT_TRUE(client2.has_received_completion());
}

TEST_F(TestURLLoaderFactoryTest, SimulateResponseUrlMatch) {
  std::string base_url = "http://foo/";
  std::string full_url = base_url + "hello?parameter=bar";
  network::URLLoaderCompletionStatus ok_status(net::OK);
  mojom::URLResponseHeadPtr response_head = CreateURLResponseHead(net::HTTP_OK);

  StartRequest(full_url);

  // Default is non URL prefix match.
  EXPECT_FALSE(factory()->SimulateResponseForPendingRequest(
      GURL(base_url), ok_status, response_head.Clone(), /*content=*/""));
  EXPECT_FALSE(factory()->SimulateResponseForPendingRequest(
      GURL(base_url), ok_status, response_head.Clone(), /*content=*/"",
      TestURLLoaderFactory::kMatchDefault));

  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(base_url), ok_status, response_head.Clone(), /*content=*/"",
      TestURLLoaderFactory::kUrlMatchPrefix));
}

TEST_F(TestURLLoaderFactoryTest, SimulateResponseMostRecentMatch) {
  std::string url = "http://foo/";

  network::URLLoaderCompletionStatus ok_status(net::OK);
  mojom::URLResponseHeadPtr response_head = CreateURLResponseHead(net::HTTP_OK);

  EXPECT_FALSE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/"",
      TestURLLoaderFactory::kMostRecentMatch));

  TestURLLoaderClient client1;
  StartRequest(url, &client1);
  TestURLLoaderClient client2;
  StartRequest(url, &client2);

  // test non matching URL when there are pending requests.
  EXPECT_FALSE(factory()->SimulateResponseForPendingRequest(
      GURL("http://bar"), ok_status, response_head.Clone(), /*content=*/"",
      TestURLLoaderFactory::kMostRecentMatch));

  EXPECT_EQ(2, factory()->NumPending());
  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/"",
      TestURLLoaderFactory::kMostRecentMatch));
  EXPECT_EQ(1, factory()->NumPending());
  // Last sent request should have been served.
  EXPECT_TRUE(client2.has_received_completion());
  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, response_head.Clone(), /*content=*/"",
      TestURLLoaderFactory::kMostRecentMatch));
  EXPECT_EQ(0, factory()->NumPending());
  EXPECT_TRUE(client1.has_received_completion());
}

TEST_F(TestURLLoaderFactoryTest, SimulateResponseNoRawHeadersByDefault) {
  std::string url = "http://foo/";
  // Raw-headers are not reported by default.
  StartRequest(url);
  network::URLLoaderCompletionStatus ok_status(net::OK);
  mojom::URLResponseHeadPtr response_head = CreateURLResponseHead(net::HTTP_OK);
  EXPECT_TRUE(factory()->SimulateResponseForPendingRequest(
      GURL(url), ok_status, std::move(response_head), /*content=*/""));
  EXPECT_TRUE(client()->has_received_completion());
}

TEST_F(TestURLLoaderFactoryTest,
       SimulateResponseWithoutRemovingFromPendingList) {
  network::URLLoaderCompletionStatus ok_status(net::OK);

  // #1
  std::string url_1 = "http://foo/1";
  TestURLLoaderClient client_1;

  // #2
  std::string url_2 = "http://foo/2";
  TestURLLoaderClient client_2;

  // #3
  std::string url_3 = "http://foo/3";
  TestURLLoaderClient client_3;

  // By default no request is pending.
  EXPECT_EQ(0, factory()->NumPending());

  StartRequest(url_1, &client_1);
  StartRequest(url_2, &client_2);
  StartRequest(url_3, &client_3);
  EXPECT_EQ(3, factory()->NumPending());

  // Try out loading the pending requests out of order.
  // #2
  auto* request = &(*factory()->pending_requests())[1];
  factory()->SimulateResponseWithoutRemovingFromPendingList(
      request, CreateURLResponseHead(net::HTTP_NOT_FOUND), /*content=*/"",
      ok_status);
  // The pending request list remains untounched and successful.
  EXPECT_EQ(3, factory()->NumPending());
  EXPECT_TRUE(client_2.has_received_completion());
  EXPECT_EQ(net::OK, client_2.completion_status().error_code);
  ASSERT_TRUE(client_2.response_head()->headers);
  EXPECT_EQ(net::HTTP_NOT_FOUND,
            client_2.response_head()->headers->response_code());

  // #1
  request = &(*factory()->pending_requests())[0];
  factory()->SimulateResponseWithoutRemovingFromPendingList(
      request, CreateURLResponseHead(net::HTTP_OK), /*content=*/"hello",
      ok_status);
  // Again, the pending request list remains untounched and successful.
  EXPECT_EQ(3, factory()->NumPending());
  EXPECT_TRUE(client_1.has_received_completion());
  EXPECT_EQ(net::OK, client_1.completion_status().error_code);
  ASSERT_TRUE(client_1.response_head()->headers);
  EXPECT_EQ(net::HTTP_OK, client_1.response_head()->headers->response_code());

  // Ensure that when the client is unbound, it is counted out
  // of the pending request list.
  client_1.Unbind();
  EXPECT_EQ(2, factory()->NumPending());
  EXPECT_EQ(3u, factory()->pending_requests()->size());

  // Add one more request and load it (remember that request #3
  // is not handled, where #1 and #2 are.
  std::string url_4 = "http://foo/4";
  TestURLLoaderClient client_4;

  StartRequest(url_4, &client_4);
  EXPECT_EQ(3, factory()->NumPending());

  // # Process 4.
  request = &(*factory()->pending_requests())[3];
  factory()->SimulateResponseWithoutRemovingFromPendingList(
      request, CreateURLResponseHead(net::HTTP_OK), /*content=*/"hello",
      ok_status);
  // Again, the pending request list remains untounched and successful.
  EXPECT_EQ(3, factory()->NumPending());
  EXPECT_TRUE(client_4.has_received_completion());
  EXPECT_EQ(net::OK, client_4.completion_status().error_code);
  ASSERT_TRUE(client_4.response_head()->headers);
  EXPECT_EQ(net::HTTP_OK, client_4.response_head()->headers->response_code());
}

}  // namespace network
