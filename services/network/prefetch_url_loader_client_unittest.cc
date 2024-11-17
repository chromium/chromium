// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_url_loader_client.h"

#include <stdint.h>

#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

// An implementation of URLLoader that does nothing.
class StubURLLoader final : public network::mojom::URLLoader {
 public:
  explicit StubURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
      : receiver_(this, std::move(url_loader_receiver)) {}

  // network::mojom::URLLoader overrides.
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  mojo::Receiver<network::mojom::URLLoader> receiver_;
};

class PrefetchURLLoaderClientTest : public ::testing::Test {
 protected:
  PrefetchURLLoaderClientTest()
      : client_(cache()->Emplace(TestRequest())),
        stub_url_loader_(client_->GetURLLoaderPendingReceiver()),
        client_pending_remote_(client_->BindNewPipeAndPassRemote()),
        mock_client_receiver_(&mock_client_) {}

  ~PrefetchURLLoaderClientTest() override {
    // Avoid calls to a disconnect handler on the stack.
    mock_client_receiver_.reset();
  }

  // The cache that owns the object under test.
  PrefetchCache* cache() { return &cache_; }

  // The object under test.
  PrefetchURLLoaderClient* client() { return client_; }

  // A mock URLLoaderClient that will stand-in for the URLLoaderClient inside
  // the render process.
  StrictMock<MockURLLoaderClient>& mock_client() { return mock_client_; }

  // Arranges for `disconnect_handler` to be called when `mock_client_receiver_`
  // detects a disconnection. Must be called after `Consume()`.
  void set_disconnect_handler(MockFunction<void()>* disconnect_handler) {
    // This use of base::Unretained is potentially unsafe as
    // `disconnect_handler` is on the stack. It is mitigated by resetting
    // `mock_client_receiver_` in the destructor.
    mock_client_receiver_.set_disconnect_handler(base::BindOnce(
        &MockFunction<void()>::Call, base::Unretained(disconnect_handler)));
  }

  // Pretend the datapipe that passed mojo messages from the "real" URLLoader
  // disconnected.
  void ResetClientPendingRemote() { client_pending_remote_.reset(); }

  // Remove the reference to the client. This needs to be called by tests which
  // arrange for the client to be deleted before the test ends, in order to
  // prevent triggering the dangling pointer detection.
  void ClearClientPointer() { client_ = nullptr; }

  // Sets `client_` to nullptr and returns the old value. This is needed for
  // tests that need a call a method on `client_` that will delete it.
  PrefetchURLLoaderClient* TakeClientPointer() {
    return std::exchange(client_, nullptr);
  }

  // Calls Consume() on `client_`, hooking it up to the mock client.
  void Consume() {
    client_->Consume(loader_.BindNewPipeAndPassReceiver(),
                     mock_client_receiver_.BindNewPipeAndPassRemote());
    EXPECT_TRUE(loader_.is_bound());
  }

  // Calls all the mojo methods on `client_` in order with verifiable
  // parameters. This doesn't in any way correspond to the real behaviour of a
  // URLLoader.
  void CallAllMojoMethods() {
    client_->OnReceiveEarlyHints(TestEarlyHints());
    client_->OnReceiveResponse(TestURLResponseHead(), TestDataPipeConsumer(),
                               TestBigBuffer());
    client_->OnReceiveRedirect(TestRedirectInfo(), TestURLResponseHead());
    client_->OnUploadProgress(kTestCurrentPosition, kTestTotalSize,
                              base::DoNothing());
    client_->OnTransferSizeUpdated(kTransferSizeDiff);
    client_->OnComplete(TestURLLoaderCompletionStatus());
  }

  // This adds expectations that all the methods on `client` will be called with
  // arguments matching those in `CallAllMojoMethods()`.
  void ExpectCallMojoMethods() {
    EXPECT_CALL(mock_client_, OnReceiveEarlyHints(IsTrue()));
    EXPECT_CALL(mock_client_,
                OnReceiveResponse(URLResponseHeadIsOk(), _,
                                  Optional(BigBufferHasExpectedContents())))
        .WillOnce(WithArg<1>(Invoke(CheckDataPipeContents)));
    EXPECT_CALL(mock_client_, OnReceiveRedirect(EqualsTestRedirectInfo(),
                                                URLResponseHeadIsOk()));
    EXPECT_CALL(mock_client_, OnUploadProgress(Eq(kTestCurrentPosition),
                                               Eq(kTestTotalSize), _))
        .WillOnce(
            WithArg<2>([](auto&& callback) { std::move(callback).Run(); }));
    EXPECT_CALL(mock_client_, OnTransferSizeUpdated(Eq(kTransferSizeDiff)));
    EXPECT_CALL(mock_client_, OnComplete(URLLoaderCompletionStatusIsOk()));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  // A dummy URLLoader that adopts `client_`'s url_loader_ when Consume() is
  // called.
  mojo::Remote<mojom::URLLoader> loader_;

  // Owner for `client_`.
  PrefetchCache cache_;

  // The client under test. Owned by `cache_`.
  raw_ptr<PrefetchURLLoaderClient> client_;

  // The "real" URLLoader that `client_` connects to.
  StubURLLoader stub_url_loader_;

  // Connected to `client_`. Used to detect when `client_` is destroyed. In
  // normal operation this would be passed to a real URLLoader.
  mojo::PendingRemote<mojom::URLLoaderClient> client_pending_remote_;

  // Represents the renderer-side client that will eventually consume this
  // object.
  StrictMock<MockURLLoaderClient> mock_client_;

  // Forwards mojo calls to `mock_client_`.
  mojo::Receiver<mojom::URLLoaderClient> mock_client_receiver_;
};

// Tests that call Consume() leak memory and hit an assert(), so they are
// disabled on address sanitizer and debug builds.
// TODO(crbug.com/375425822): Fix the leak before running this code in
// production.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)

#define MAYBE_RecordAndReplay DISABLED_RecordAndReplay
#define MAYBE_DelegateDirectly DISABLED_DelegateDirectly
#define MAYBE_ReplayAfterResponse DISABLED_ReplayAfterResponse
#define MAYBE_ConsumeRemovesFromCache DISABLED_ConsumeRemovesFromCache

#else

#define MAYBE_RecordAndReplay RecordAndReplay
#define MAYBE_DelegateDirectly DelegateDirectly
#define MAYBE_ReplayAfterResponse ReplayAfterResponse
#define MAYBE_ConsumeRemovesFromCache ConsumeRemovesFromCache

#endif

TEST_F(PrefetchURLLoaderClientTest, Construct) {
  EXPECT_EQ(client()->url(), TestURL());
  EXPECT_EQ(client()->network_isolation_key(), TestNIK());
  EXPECT_GT(client()->expiry_time(), base::TimeTicks::Now());
  EXPECT_LT(client()->expiry_time(), base::TimeTicks::Now() + base::Days(1));
}

TEST_F(PrefetchURLLoaderClientTest, MAYBE_RecordAndReplay) {
  MockFunction<void()> checkpoint;
  MockFunction<void()> disconnect;

  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call());
    ExpectCallMojoMethods();
    EXPECT_CALL(disconnect, Call());
  }

  CallAllMojoMethods();
  ResetClientPendingRemote();
  checkpoint.Call();
  Consume();
  set_disconnect_handler(&disconnect);
  ClearClientPointer();
  RunUntilIdle();
}

// The difference from the previous test is that now Consume() is called
// before any of the delegating methods, so there's no need to record them.
TEST_F(PrefetchURLLoaderClientTest, MAYBE_DelegateDirectly) {
  MockFunction<void()> checkpoint;
  MockFunction<void()> disconnect;

  {
    InSequence s;
    ExpectCallMojoMethods();
    EXPECT_CALL(disconnect, Call());
    EXPECT_CALL(checkpoint, Call());
  }

  Consume();
  set_disconnect_handler(&disconnect);
  CallAllMojoMethods();
  ClearClientPointer();
  ResetClientPendingRemote();
  RunUntilIdle();
  checkpoint.Call();
}

// This test just verifies that all the recorded callbacks can be destroyed
// without leaks.
TEST_F(PrefetchURLLoaderClientTest, RecordAndDiscard) {
  CallAllMojoMethods();
  RunUntilIdle();
}

// Verifies that setting the client after the response comes but before it
// completes works.
TEST_F(PrefetchURLLoaderClientTest, MAYBE_ReplayAfterResponse) {
  MockFunction<void(int)> checkpoint;
  MockFunction<void()> disconnect;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(mock_client(),
                OnReceiveResponse(URLResponseHeadIsOk(), _,
                                  Optional(BigBufferHasExpectedContents())))
        .WillOnce(WithArg<1>(Invoke(CheckDataPipeContents)));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_client(), OnComplete(URLLoaderCompletionStatusIsOk()));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(disconnect, Call());
    EXPECT_CALL(checkpoint, Call(3));
  }

  client()->OnReceiveResponse(TestURLResponseHead(), TestDataPipeConsumer(),
                              TestBigBuffer());
  RunUntilIdle();
  checkpoint.Call(0);

  Consume();
  set_disconnect_handler(&disconnect);
  RunUntilIdle();
  checkpoint.Call(1);

  client()->OnComplete(TestURLLoaderCompletionStatus());
  RunUntilIdle();
  checkpoint.Call(2);

  ClearClientPointer();
  ResetClientPendingRemote();
  RunUntilIdle();
  checkpoint.Call(3);
}

TEST_F(PrefetchURLLoaderClientTest, MAYBE_ConsumeRemovesFromCache) {
  Consume();

  EXPECT_FALSE(
      cache()->Lookup(client()->network_isolation_key(), client()->url()));
}

TEST_F(PrefetchURLLoaderClientTest, BadResponseCode) {
  constexpr auto kBadResponseCode = net::HTTP_NOT_FOUND;

  TakeClientPointer()->OnReceiveResponse(
      CreateURLResponseHead(kBadResponseCode), TestDataPipeConsumer(),
      TestBigBuffer());

  // It should have been deleted from the cache.
  EXPECT_FALSE(cache()->Lookup(TestNIK(), TestURL()));
}

TEST_F(PrefetchURLLoaderClientTest, BadHeader) {
  auto url_response_head = TestURLResponseHead();
  url_response_head->headers->AddHeader("Vary", "Sec-Purpose, Set-Cookie");
  TakeClientPointer()->OnReceiveResponse(
      std::move(url_response_head), TestDataPipeConsumer(), TestBigBuffer());

  // It should have been deleted from the cache.
  EXPECT_FALSE(cache()->Lookup(TestNIK(), TestURL()));
}

TEST_F(PrefetchURLLoaderClientTest, NoStore) {
  auto url_response_head = TestURLResponseHead();
  url_response_head->headers->AddHeader("Cache-Control", "no-cache, no-store");
  TakeClientPointer()->OnReceiveResponse(
      std::move(url_response_head), TestDataPipeConsumer(), TestBigBuffer());

  // It should have been deleted from the cache.
  EXPECT_FALSE(cache()->Lookup(TestNIK(), TestURL()));
}

}  // namespace

}  // namespace network
