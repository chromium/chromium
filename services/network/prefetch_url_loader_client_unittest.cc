// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_url_loader_client.h"

#include <stdint.h>

#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_status_code.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/prefetch_cache.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/mock_url_loader_client.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsTrue;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::StrictMock;
using ::testing::WithArg;

constexpr uint64_t kTestCurrentPosition = 53;
constexpr uint64_t kTestTotalSize = 103;
constexpr int32_t kTransferSizeDiff = 27;
constexpr size_t kDataPipeCapacity = 4096;
constexpr char kResponseBody[] = "Some fairly unique data";
constexpr size_t kBigBufferSize = 1 << 16;

// Returns a URL for our tests to use. The actual value isn't important.
GURL TestURL() {
  return GURL("https://origin.example/i.js");
}

// Returns an origin matching our test URL.
url::Origin TestOrigin() {
  return url::Origin::Create(TestURL());
}

// Returns an IsolationInfo object matching our test URL.
net::IsolationInfo TestIsolationInfo() {
  return net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, TestOrigin(), TestOrigin(),
      net::SiteForCookies::FromOrigin(TestOrigin()));
}

// Returns a NetworkIsolationKey object matching our test URL.
net::NetworkIsolationKey TestNIK() {
  const net::NetworkIsolationKey nik =
      TestIsolationInfo().network_isolation_key();
  return nik;
}

// Returns a ResourceRequest matching our test URL.
ResourceRequest TestRequest() {
  ResourceRequest request;
  request.url = TestURL();
  request.trusted_params.emplace();
  request.trusted_params->isolation_info = TestIsolationInfo();
  return request;
}

// Returns a dummy value for EarlyHints.
mojom::EarlyHintsPtr TestEarlyHints() {
  auto early_hints = mojom::EarlyHints::New();
  early_hints->headers = mojom::ParsedHeaders::New();
  return early_hints;
}

// Returns a successfully URLResponseHead.
mojom::URLResponseHeadPtr TestURLResponseHead() {
  return CreateURLResponseHead(net::HTTP_OK);
}

// A GMock matcher for the URLResponseHead returned by TestURLResponseHead().
MATCHER(URLResponseHeadIsOk, "URLResponseHead is ok") {
  return arg->headers->response_code() == net::HTTP_OK;
}

// Returns a ScopedDataPipeConsumerHandle for use in tests. Reading from the
// pipe will give `kResponseBody`.
mojo::ScopedDataPipeConsumerHandle TestDataPipeConsumer() {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::CreateDataPipe(kDataPipeCapacity, producer, consumer);
  // Write to the data pipe from another thread to make sure we don't block.
  base::ThreadPool::PostTask(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(
          [](mojo::ScopedDataPipeProducerHandle destination) {
            mojo::BlockingCopyFromString(kResponseBody, destination);
            // `destination` will be closed automatically on leaving scope.
          },
          std::move(producer)));
  return consumer;
}

// A function which will cause the test to fail if `consumer` is not a data pipe
// that yields kResponseBody. Blocks until the data pipe is completely read.
void CheckDataPipeContents(mojo::ScopedDataPipeConsumerHandle consumer) {
  std::string contents;
  mojo::BlockingCopyToString(std::move(consumer), &contents);
  EXPECT_EQ(contents, kResponseBody);
}

// Creates a BigBuffer suitable for use in tests. The contents are sufficiently
// unique that there shouldn't be an accidental match.
mojo_base::BigBuffer TestBigBuffer() {
  std::vector<uint8_t> contents;
  contents.reserve(kBigBufferSize);
  uint8_t value = 1;
  for (size_t i = 0; i < kBigBufferSize; ++i) {
    contents.push_back(value);
    value = (value * 7 + 1) % 256;
  }
  return mojo_base::BigBuffer(contents);
}

// Verifies that a BigBuffer object matches the one created by TestBigBuffer().
MATCHER(BigBufferHasExpectedContents,
        "does the BigBuffer have the right contents") {
  auto expected = TestBigBuffer();
  return base::ranges::equal(base::span(expected), base::span(arg));
}

// Returns a RedirectInfo object that is useful for use in tests. It is
// same-origin with the URL returned by TestURL().
net::RedirectInfo TestRedirectInfo() {
  using net::RedirectInfo;
  constexpr char kRedirectTo[] = "https://origin.example/resources/i.js";
  return RedirectInfo::ComputeRedirectInfo(
      "GET", TestURL(), TestIsolationInfo().site_for_cookies(),
      RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      TestOrigin().Serialize(), 301, GURL(kRedirectTo), std::nullopt, false);
}

// Returns true if two SiteForCookies objects match.
bool Equals(const net::SiteForCookies& lhs, const net::SiteForCookies& rhs) {
  return lhs.IsEquivalent(rhs);
}

// Returns true if a RedirectInfo object matches the one created by
// TestRedirectInfo().
MATCHER(EqualsTestRedirectInfo, "equals test RedirectInfo") {
  const net::RedirectInfo expected = TestRedirectInfo();
  return arg.status_code == expected.status_code &&
         arg.new_method == expected.new_method &&
         arg.new_url == expected.new_url &&
         Equals(arg.new_site_for_cookies, expected.new_site_for_cookies) &&
         arg.new_referrer == expected.new_referrer &&
         arg.insecure_scheme_was_upgraded ==
             expected.insecure_scheme_was_upgraded &&
         arg.is_signed_exchange_fallback_redirect ==
             expected.is_signed_exchange_fallback_redirect &&
         arg.new_referrer_policy == expected.new_referrer_policy &&
         arg.critical_ch_restart_time == expected.critical_ch_restart_time;
}

// Returns a successful URLLoaderCompletionStatus.
URLLoaderCompletionStatus TestURLLoaderCompletionStatus() {
  return URLLoaderCompletionStatus(net::OK);
}

// A GMock matcher that Verifies that a URLLoaderCompletionStatus matches the
// one returned by TestURLLoaderCompletionStatus().
MATCHER(URLLoaderCompletionStatusIsOk, "URLLoaderCompletionStatus is ok") {
  const URLLoaderCompletionStatus expected = TestURLLoaderCompletionStatus();
  const URLLoaderCompletionStatus& actual = arg;
  using S = URLLoaderCompletionStatus;
  auto equals = [&](auto member_ptr) {
    return expected.*member_ptr == actual.*member_ptr;
  };
  // `completion_time` is intentionally omitted as it is different every time.
  // `ssl_info` is omitted as it lacks an equality operator and it's not worth
  // implementing one just for this test.
  return equals(&S::error_code) && equals(&S::extended_error_code) &&
         equals(&S::exists_in_cache) && equals(&S::exists_in_memory_cache) &&
         equals(&S::encoded_data_length) && equals(&S::encoded_body_length) &&
         equals(&S::decoded_body_length) && equals(&S::cors_error_status) &&
         equals(&S::private_network_access_preflight_result) &&
         equals(&S::trust_token_operation_status) &&
         equals(&S::blocked_by_response_reason) &&
         equals(&S::should_report_orb_blocking) &&
         equals(&S::resolve_error_info) &&
         equals(&S::should_collapse_initiator);
}

// Calls all the mojo methods on `client` in order with verifiable parameters.
// This doesn't in any way correspond to the real behaviour of a URLLoader.
void CallAllMojoMethods(mojom::URLLoaderClient* client) {
  client->OnReceiveEarlyHints(TestEarlyHints());
  client->OnReceiveResponse(TestURLResponseHead(), TestDataPipeConsumer(),
                            TestBigBuffer());
  client->OnReceiveRedirect(TestRedirectInfo(), TestURLResponseHead());
  client->OnUploadProgress(kTestCurrentPosition, kTestTotalSize,
                           base::DoNothing());
  client->OnTransferSizeUpdated(kTransferSizeDiff);
  client->OnComplete(TestURLLoaderCompletionStatus());
}

// This adds expectations that all the methods on `client` will be called with
// arguments matching those in `CallAllMojoMethods()`.
void ExpectCallMojoMethods(StrictMock<MockURLLoaderClient>& mock_client) {
  EXPECT_CALL(mock_client, OnReceiveEarlyHints(IsTrue()));
  EXPECT_CALL(mock_client,
              OnReceiveResponse(URLResponseHeadIsOk(), _,
                                Optional(BigBufferHasExpectedContents())))
      .WillOnce(WithArg<1>(Invoke(CheckDataPipeContents)));
  EXPECT_CALL(mock_client, OnReceiveRedirect(EqualsTestRedirectInfo(),
                                             URLResponseHeadIsOk()));
  EXPECT_CALL(mock_client,
              OnUploadProgress(Eq(kTestCurrentPosition), Eq(kTestTotalSize), _))
      .WillOnce(WithArg<2>([](auto&& callback) { std::move(callback).Run(); }));
  EXPECT_CALL(mock_client, OnTransferSizeUpdated(Eq(kTransferSizeDiff)));
  EXPECT_CALL(mock_client, OnComplete(URLLoaderCompletionStatusIsOk()));
}

// A wrapper for a mojo::Receiver that calls Call() on `disconnect` when the
// pending remote is disconnected.
class DisconnectDetectingReceiver final {
 public:
  DisconnectDetectingReceiver(mojom::URLLoaderClient* client,
                              MockFunction<void()>* disconnect)
      : receiver_(client), disconnect_(disconnect) {}

  mojo::PendingRemote<mojom::URLLoaderClient> GetPendingRemote() {
    auto pending_remote = receiver_.BindNewPipeAndPassRemote();
    // After `receiver_` is destroyed, the callback will not be called, so this
    // use of base::Unretained() is safe.
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockFunction<void()>::Call, base::Unretained(disconnect_)));

    return pending_remote;
  }

 private:
  mojo::Receiver<mojom::URLLoaderClient> receiver_;
  raw_ptr<MockFunction<void()>> disconnect_;
};

class PrefetchURLLoaderClientTest : public ::testing::Test {
 protected:
  PrefetchCache* cache() { return &cache_; }

  // Construct a PrefetchURLLoaderClient with TestRequest() that is initially
  // owned by `cache_`. The return value is always destroyed automatically.
  PrefetchURLLoaderClient* Emplace() { return cache()->Emplace(TestRequest()); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  PrefetchCache cache_;
};

TEST_F(PrefetchURLLoaderClientTest, Construct) {
  auto* client = Emplace();
  EXPECT_EQ(client->url(), TestURL());
  EXPECT_EQ(client->network_isolation_key(), TestNIK());
  EXPECT_GT(client->expiry_time(), base::TimeTicks::Now());
  EXPECT_LT(client->expiry_time(), base::TimeTicks::Now() + base::Days(1));
}

TEST_F(PrefetchURLLoaderClientTest, RecordAndReplay) {
  StrictMock<MockURLLoaderClient> mock_client;
  MockFunction<void()> checkpoint;
  MockFunction<void()> disconnect;
  DisconnectDetectingReceiver receiver(&mock_client, &disconnect);

  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call());
    ExpectCallMojoMethods(mock_client);
    EXPECT_CALL(disconnect, Call());
  }

  auto* client = Emplace();
  auto pending_remote = client->BindNewPipeAndPassRemote();
  CallAllMojoMethods(client);
  pending_remote.reset();
  checkpoint.Call();
  client->SetClient(receiver.GetPendingRemote());
  RunUntilIdle();
}

// The difference from the previous test is that now SetClient() is called
// before any of the delegating methods, so there's no need to record them.
TEST_F(PrefetchURLLoaderClientTest, DelegateDirectly) {
  StrictMock<MockURLLoaderClient> mock_client;
  MockFunction<void()> checkpoint;
  MockFunction<void()> disconnect;
  DisconnectDetectingReceiver receiver(&mock_client, &disconnect);

  {
    InSequence s;
    ExpectCallMojoMethods(mock_client);
    EXPECT_CALL(disconnect, Call());
    EXPECT_CALL(checkpoint, Call());
  }

  auto* client = Emplace();
  auto pending_remote = client->BindNewPipeAndPassRemote();
  client->SetClient(receiver.GetPendingRemote());
  CallAllMojoMethods(client);
  pending_remote.reset();
  RunUntilIdle();
  checkpoint.Call();
}

// This test just verifies that all the recorded callbacks can be destroyed
// without leaks.
TEST_F(PrefetchURLLoaderClientTest, RecordAndDiscard) {
  auto* client = Emplace();
  CallAllMojoMethods(client);
  RunUntilIdle();
}

// Verifies that setting the client after the response comes but before it
// completes works.
TEST_F(PrefetchURLLoaderClientTest, ReplayAfterResponse) {
  StrictMock<MockURLLoaderClient> mock_client;
  MockFunction<void(int)> checkpoint;
  MockFunction<void()> disconnect;
  DisconnectDetectingReceiver receiver(&mock_client, &disconnect);
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(mock_client,
                OnReceiveResponse(URLResponseHeadIsOk(), _,
                                  Optional(BigBufferHasExpectedContents())))
        .WillOnce(WithArg<1>(Invoke(CheckDataPipeContents)));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_client, OnComplete(URLLoaderCompletionStatusIsOk()));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(disconnect, Call());
    EXPECT_CALL(checkpoint, Call(3));
  }

  auto* client = Emplace();
  auto pending_remote = client->BindNewPipeAndPassRemote();
  client->OnReceiveResponse(TestURLResponseHead(), TestDataPipeConsumer(),
                            TestBigBuffer());
  RunUntilIdle();
  checkpoint.Call(0);

  client->SetClient(receiver.GetPendingRemote());
  RunUntilIdle();
  checkpoint.Call(1);

  client->OnComplete(TestURLLoaderCompletionStatus());
  RunUntilIdle();
  checkpoint.Call(2);

  pending_remote.reset();
  RunUntilIdle();
  checkpoint.Call(3);
}

TEST_F(PrefetchURLLoaderClientTest, GetURLLoaderPendingReceiver) {
  StrictMock<MockURLLoaderClient> mock_client;
  MockFunction<void()> checkpoint;
  MockFunction<void()> disconnect;
  DisconnectDetectingReceiver receiver(&mock_client, &disconnect);

  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call());
    EXPECT_CALL(disconnect, Call());
  }

  auto* client = Emplace();
  auto pending_remote = client->BindNewPipeAndPassRemote();

  auto pending_receiver = client->GetURLLoaderPendingReceiver();
  EXPECT_TRUE(pending_receiver.is_valid());

  checkpoint.Call();
  client->SetClient(receiver.GetPendingRemote());
  pending_receiver.reset();

  RunUntilIdle();
}

TEST_F(PrefetchURLLoaderClientTest, SetClientRemovesFromCache) {
  StrictMock<MockURLLoaderClient> mock_client;
  mojo::Receiver<mojom::URLLoaderClient> receiver(&mock_client);

  auto* client = Emplace();
  auto pending_remote = client->BindNewPipeAndPassRemote();

  client->SetClient(receiver.BindNewPipeAndPassRemote());

  EXPECT_FALSE(cache()->Lookup(client->network_isolation_key(), client->url()));
}

TEST_F(PrefetchURLLoaderClientTest, BadResponseCode) {
  constexpr auto kBadResponseCode = net::HTTP_NOT_FOUND;

  auto* client = Emplace();
  client->OnReceiveResponse(CreateURLResponseHead(kBadResponseCode),
                            TestDataPipeConsumer(), TestBigBuffer());

  // It should have been deleted from the cache.
  EXPECT_FALSE(cache()->Lookup(TestNIK(), TestURL()));
}

TEST_F(PrefetchURLLoaderClientTest, BadHeader) {
  auto* client = Emplace();
  auto url_response_head = TestURLResponseHead();
  url_response_head->headers->AddHeader("Vary", "Sec-Purpose, Set-Cookie");
  client->OnReceiveResponse(std::move(url_response_head),
                            TestDataPipeConsumer(), TestBigBuffer());

  // It should have been deleted from the cache.
  EXPECT_FALSE(cache()->Lookup(TestNIK(), TestURL()));
}

TEST_F(PrefetchURLLoaderClientTest, NoStore) {
  auto* client = Emplace();
  auto url_response_head = TestURLResponseHead();
  url_response_head->headers->AddHeader("Cache-Control", "no-cache, no-store");
  client->OnReceiveResponse(std::move(url_response_head),
                            TestDataPipeConsumer(), TestBigBuffer());

  // It should have been deleted from the cache.
  EXPECT_FALSE(cache()->Lookup(TestNIK(), TestURL()));
}

}  // namespace

}  // namespace network
