// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/proxy_resolution/multi_threaded_proxy_resolver.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker_impl.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;

using base::ASCIIToUTF16;

namespace net {

namespace {

// A synchronous mock ProxyResolver implementation, which can be used in
// conjunction with MultiThreadedProxyResolver.
//       - returns a single-item proxy list with the query's host.
class MockProxyResolver : public ProxyResolver {
 public:
  MockProxyResolver() = default;

  // ProxyResolver implementation.
  int GetProxyForURL(const GURL& query_url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override {
    last_query_url_ = query_url;
    last_network_anonymization_key_ = network_anonymization_key;

    if (!resolve_latency_.is_zero())
      base::PlatformThread::Sleep(resolve_latency_);

    EXPECT_TRUE(worker_thread_checker_.CalledOnValidThread());

    EXPECT_TRUE(callback.is_null());
    EXPECT_TRUE(request == nullptr);

    // Write something into |net_log| (doesn't really have any meaning.)
    net_log.BeginEvent(NetLogEventType::PAC_JAVASCRIPT_ALERT);

    results->UseNamedProxy(query_url.host());

    // Return a success code which represents the request's order.
    return request_count_++;
  }

  int request_count() const { return request_count_; }

  void SetResolveLatency(base::TimeDelta latency) {
    resolve_latency_ = latency;
  }

  // Return the most recent values passed to GetProxyForURL(), if any.
  const GURL& last_query_url() const { return last_query_url_; }
  const NetworkAnonymizationKey& last_network_anonymization_key() const {
    return last_network_anonymization_key_;
  }

 private:
  base::ThreadCheckerImpl worker_thread_checker_;
  int request_count_ = 0;
  base::TimeDelta resolve_latency_;

  GURL last_query_url_;
  NetworkAnonymizationKey last_network_anonymization_key_;
};


// A mock synchronous ProxyResolver which can be set to block upon reaching
// GetProxyForURL().
class BlockableProxyResolver : public MockProxyResolver {
 public:
  enum class State {
    NONE,
    BLOCKED,
    WILL_BLOCK,
  };

  BlockableProxyResolver() : condition_(&lock_) {}

  BlockableProxyResolver(const BlockableProxyResolver&) = delete;
  BlockableProxyResolver& operator=(const BlockableProxyResolver&) = delete;

  ~BlockableProxyResolver() override {
    base::AutoLock lock(lock_);
    EXPECT_NE(State::BLOCKED, state_);
  }

  // Causes the next call into GetProxyForURL() to block. Must be followed by
  // a call to Unblock().
  void Block() {
    base::AutoLock lock(lock_);
    EXPECT_EQ(State::NONE, state_);
    state_ = State::WILL_BLOCK;
    condition_.Broadcast();
  }

  // Unblocks the ProxyResolver. The ProxyResolver must already be in a
  // blocked state prior to calling.
  void Unblock() {
    base::AutoLock lock(lock_);
    EXPECT_EQ(State::BLOCKED, state_);
    state_ = State::NONE;
    condition_.Broadcast();
  }

  // Waits until the proxy resolver is blocked within GetProxyForURL().
  void WaitUntilBlocked() {
    base::AutoLock lock(lock_);
    while (state_ != State::BLOCKED)
      condition_.Wait();
  }

  int GetProxyForURL(const GURL& query_url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override {
    {
      base::AutoLock lock(lock_);

      EXPECT_NE(State::BLOCKED, state_);

      if (state_ == State::WILL_BLOCK) {
        state_ = State::BLOCKED;
        condition_.Broadcast();

        while (state_ == State::BLOCKED)
          condition_.Wait();
      }
    }

    return MockProxyResolver::GetProxyForURL(
        query_url, network_anonymization_key, results, std::move(callback),
        request, net_log);
  }

 private:
  State state_ = State::NONE;
  base::Lock lock_;
  base::ConditionVariable condition_;
};

// This factory returns new instances of BlockableProxyResolver.
class BlockableProxyResolverFactory : public ProxyResolverFactory {
 public:
  BlockableProxyResolverFactory() : ProxyResolverFactory(false) {}

  ~BlockableProxyResolverFactory() override = default;

  int CreateProxyResolver(const scoped_refptr<PacFileData>& script_data,
                          std::unique_ptr<ProxyResolver>* result,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    auto resolver = std::make_unique<BlockableProxyResolver>();
    BlockableProxyResolver* resolver_ptr = resolver.get();
    *result = std::move(resolver);
    base::AutoLock lock(lock_);
    resolvers_.push_back(resolver_ptr);
    script_data_.push_back(script_data);
    return OK;
  }

  std::vector<raw_ptr<BlockableProxyResolver, VectorExperimental>> resolvers() {
    base::AutoLock lock(lock_);
    return resolvers_;
  }

  const std::vector<scoped_refptr<PacFileData>> script_data() {
    base::AutoLock lock(lock_);
    return script_data_;
  }

 private:
  std::vector<raw_ptr<BlockableProxyResolver, VectorExperimental>> resolvers_;
  std::vector<scoped_refptr<PacFileData>> script_data_;
  base::Lock lock_;
};

class SingleShotMultiThreadedProxyResolverFactory
    : public MultiThreadedProxyResolverFactory {
 public:
  SingleShotMultiThreadedProxyResolverFactory(
      size_t max_num_threads,
      std::unique_ptr<ProxyResolverFactory> factory)
      : MultiThreadedProxyResolverFactory(max_num_threads, false),
        factory_(std::move(factory)) {}

  std::unique_ptr<ProxyResolverFactory> CreateProxyResolverFactory() override {
    DCHECK(factory_);
    return std::move(factory_);
  }

 private:
  std::unique_ptr<ProxyResolverFactory> factory_;
};

class MultiThreadedProxyResolverTest : public TestWithTaskEnvironment {
 public:
  void Init(size_t num_threads) {
    auto factory_owner = std::make_unique<BlockableProxyResolverFactory>();
    factory_ = factory_owner.get();
    resolver_factory_ =
        std::make_unique<SingleShotMultiThreadedProxyResolverFactory>(
            num_threads, std::move(factory_owner));
    TestCompletionCallback ready_callback;
    std::unique_ptr<ProxyResolverFactory::Request> request;
    resolver_factory_->CreateProxyResolver(
        PacFileData::FromUTF8("pac script bytes"), &resolver_,
        ready_callback.callback(), &request);
    EXPECT_TRUE(request);
    ASSERT_THAT(ready_callback.WaitForResult(), IsOk());

    // Verify that the script data reaches the synchronous resolver factory.
    ASSERT_EQ(1u, factory_->script_data().size());
    EXPECT_EQ(u"pac script bytes", factory_->script_data()[0]->utf16());
  }

  void ClearResolver() { resolver_.reset(); }

  BlockableProxyResolverFactory& factory() {
    DCHECK(factory_);
    return *factory_;
  }
  ProxyResolver& resolver() {
    DCHECK(resolver_);
    return *resolver_;
  }

 private:
  raw_ptr<BlockableProxyResolverFactory, DanglingUntriaged> factory_ = nullptr;
  std::unique_ptr<ProxyResolverFactory> factory_owner_;
  std::unique_ptr<MultiThreadedProxyResolverFactory> resolver_factory_;
  std::unique_ptr<ProxyResolver> resolver_;
};

TEST_F(MultiThreadedProxyResolverTest, SingleThread_Basic) {
  const size_t kNumThreads = 1u;
  ASSERT_NO_FATAL_FAILURE(Init(kNumThreads));

  // Start request 0.
  int rv;
  TestCompletionCallback callback0;
  RecordingNetLogObserver net_log_observer;
  ProxyInfo results0;
  rv = resolver().GetProxyForURL(
      GURL("http://request0"), NetworkAnonymizationKey(), &results0,
      callback0.callback(), nullptr,
      NetLogWithSource::Make(NetLogSourceType::NONE));
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Wait for request 0 to finish.
  rv = callback0.WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ("PROXY request0:80", results0.ToDebugString());

  // The mock proxy resolver should have written 1 log entry. And
  // on completion, this should have been copied into |log0|.
  // We also have 1 log entry that was emitted by the
  // MultiThreadedProxyResolver.
  auto entries0 = net_log_observer.GetEntries();

  ASSERT_EQ(2u, entries0.size());
  EXPECT_EQ(NetLogEventType::SUBMITTED_TO_RESOLVER_THREAD, entries0[0].type);

  // Start 3 more requests (request1 to request3).

  TestCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver().GetProxyForURL(
      GURL("http://request1"), NetworkAnonymizationKey(), &results1,
      callback1.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver().GetProxyForURL(
      GURL("http://request2"), NetworkAnonymizationKey(), &results2,
      callback2.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback3;
  ProxyInfo results3;
  rv = resolver().GetProxyForURL(
      GURL("http://request3"), NetworkAnonymizationKey(), &results3,
      callback3.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Wait for the requests to finish (they must finish in the order they were
  // started, which is what we check for from their magic return value)

  rv = callback1.WaitForResult();
  EXPECT_EQ(1, rv);
  EXPECT_EQ("PROXY request1:80", results1.ToDebugString());

  rv = callback2.WaitForResult();
  EXPECT_EQ(2, rv);
  EXPECT_EQ("PROXY request2:80", results2.ToDebugString());

  rv = callback3.WaitForResult();
  EXPECT_EQ(3, rv);
  EXPECT_EQ("PROXY request3:80", results3.ToDebugString());
}

// Tests that the NetLog is updated to include the time the request was waiting
// to be scheduled to a thread.
TEST_F(MultiThreadedProxyResolverTest,
       SingleThread_UpdatesNetLogWithThreadWait) {
  const size_t kNumThreads = 1u;
  ASSERT_NO_FATAL_FAILURE(Init(kNumThreads));

  int rv;

  // Block the proxy resolver, so no request can complete.
  factory().resolvers()[0]->Block();

  // Start request 0.
  std::unique_ptr<ProxyResolver::Request> request0;
  TestCompletionCallback callback0;
  ProxyInfo results0;
  RecordingNetLogObserver net_log_observer;
  NetLogWithSource log_with_source0 =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  rv = resolver().GetProxyForURL(
      GURL("http://request0"), NetworkAnonymizationKey(), &results0,
      callback0.callback(), &request0, log_with_source0);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Start 2 more requests (request1 and request2).

  TestCompletionCallback callback1;
  ProxyInfo results1;
  NetLogWithSource log_with_source1 =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  rv = resolver().GetProxyForURL(
      GURL("http://request1"), NetworkAnonymizationKey(), &results1,
      callback1.callback(), nullptr, log_with_source1);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  std::unique_ptr<ProxyResolver::Request> request2;
  TestCompletionCallback callback2;
  ProxyInfo results2;
  NetLogWithSource log_with_source2 =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  rv = resolver().GetProxyForURL(
      GURL("http://request2"), NetworkAnonymizationKey(), &results2,
      callback2.callback(), &request2, log_with_source2);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Unblock the worker thread so the requests can continue running.
  factory().resolvers()[0]->WaitUntilBlocked();
  factory().resolvers()[0]->Unblock();

  // Check that request 0 completed as expected.
  // The NetLog has 1 entry that came from the MultiThreadedProxyResolver, and
  // 1 entry from the mock proxy resolver.
  EXPECT_EQ(0, callback0.WaitForResult());
  EXPECT_EQ("PROXY request0:80", results0.ToDebugString());

  auto entries0 =
      net_log_observer.GetEntriesForSource(log_with_source0.source());

  ASSERT_EQ(2u, entries0.size());
  EXPECT_EQ(NetLogEventType::SUBMITTED_TO_RESOLVER_THREAD, entries0[0].type);

  // Check that request 1 completed as expected.
  EXPECT_EQ(1, callback1.WaitForResult());
  EXPECT_EQ("PROXY request1:80", results1.ToDebugString());

  auto entries1 =
      net_log_observer.GetEntriesForSource(log_with_source1.source());

  ASSERT_EQ(4u, entries1.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries1, 0, NetLogEventType::WAITING_FOR_PROXY_RESOLVER_THREAD));
  EXPECT_TRUE(LogContainsEndEvent(
      entries1, 1, NetLogEventType::WAITING_FOR_PROXY_RESOLVER_THREAD));

  // Check that request 2 completed as expected.
  EXPECT_EQ(2, callback2.WaitForResult());
  EXPECT_EQ("PROXY request2:80", results2.ToDebugString());

  auto entries2 =
      net_log_observer.GetEntriesForSource(log_with_source2.source());

  ASSERT_EQ(4u, entries2.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries2, 0, NetLogEventType::WAITING_FOR_PROXY_RESOLVER_THREAD));
  EXPECT_TRUE(LogContainsEndEvent(
      entries2, 1, NetLogEventType::WAITING_FOR_PROXY_RESOLVER_THREAD));
}

// Cancel a request which is in progress, and then cancel a request which
// is pending.
TEST_F(MultiThreadedProxyResolverTest, SingleThread_CancelRequest) {
  const size_t kNumThreads = 1u;
  ASSERT_NO_FATAL_FAILURE(Init(kNumThreads));

  int rv;

  // Block the proxy resolver, so no request can complete.
  factory().resolvers()[0]->Block();

  // Start request 0.
  std::unique_ptr<ProxyResolver::Request> request0;
  TestCompletionCallback callback0;
  ProxyInfo results0;
  rv = resolver().GetProxyForURL(
      GURL("http://request0"), NetworkAnonymizationKey(), &results0,
      callback0.callback(), &request0, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Wait until requests 0 reaches the worker thread.
  factory().resolvers()[0]->WaitUntilBlocked();

  // Start 3 more requests (request1 : request3).

  TestCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver().GetProxyForURL(
      GURL("http://request1"), NetworkAnonymizationKey(), &results1,
      callback1.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  std::unique_ptr<ProxyResolver::Request> request2;
  TestCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver().GetProxyForURL(
      GURL("http://request2"), NetworkAnonymizationKey(), &results2,
      callback2.callback(), &request2, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback3;
  ProxyInfo results3;
  rv = resolver().GetProxyForURL(
      GURL("http://request3"), NetworkAnonymizationKey(), &results3,
      callback3.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Cancel request0 (inprogress) and request2 (pending).
  request0.reset();
  request2.reset();

  // Unblock the worker thread so the requests can continue running.
  factory().resolvers()[0]->Unblock();

  // Wait for requests 1 and 3 to finish.

  rv = callback1.WaitForResult();
  EXPECT_EQ(1, rv);
  EXPECT_EQ("PROXY request1:80", results1.ToDebugString());

  rv = callback3.WaitForResult();
  // Note that since request2 was cancelled before reaching the resolver,
  // the request count is 2 and not 3 here.
  EXPECT_EQ(2, rv);
  EXPECT_EQ("PROXY request3:80", results3.ToDebugString());

  // Requests 0 and 2 which were cancelled, hence their completion callbacks
  // were never summoned.
  EXPECT_FALSE(callback0.have_result());
  EXPECT_FALSE(callback2.have_result());
}

// Make sure the NetworkAnonymizationKey makes it to the resolver.
TEST_F(MultiThreadedProxyResolverTest,
       SingleThread_WithNetworkAnonymizationKey) {
  const SchemefulSite kSite(GURL("https://origin.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const GURL kUrl("https://url.test/");

  const size_t kNumThreads = 1u;
  ASSERT_NO_FATAL_FAILURE(Init(kNumThreads));

  int rv;

  // Block the proxy resolver, so no request can complete.
  factory().resolvers()[0]->Block();

  // Start request.
  std::unique_ptr<ProxyResolver::Request> request;
  TestCompletionCallback callback;
  ProxyInfo results;
  rv = resolver().GetProxyForURL(kUrl, kNetworkAnonymizationKey, &results,
                                 callback.callback(), &request,
                                 NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Wait until request reaches the worker thread.
  factory().resolvers()[0]->WaitUntilBlocked();

  factory().resolvers()[0]->Unblock();
  EXPECT_EQ(0, callback.WaitForResult());

  EXPECT_EQ(kUrl, factory().resolvers()[0]->last_query_url());
  EXPECT_EQ(kNetworkAnonymizationKey,
            factory().resolvers()[0]->last_network_anonymization_key());
}

// Test that deleting MultiThreadedProxyResolver while requests are
// outstanding cancels them (and doesn't leak anything).
TEST_F(MultiThreadedProxyResolverTest, SingleThread_CancelRequestByDeleting) {
  const size_t kNumThreads = 1u;
  ASSERT_NO_FATAL_FAILURE(Init(kNumThreads));

  ASSERT_EQ(1u, factory().resolvers().size());

  // Block the proxy resolver, so no request can complete.
  factory().resolvers()[0]->Block();

  int rv;
  // Start 3 requests.

  TestCompletionCallback callback0;
  ProxyInfo results0;
  rv = resolver().GetProxyForURL(
      GURL("http://request0"), NetworkAnonymizationKey(), &results0,
      callback0.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver().GetProxyForURL(
      GURL("http://request1"), NetworkAnonymizationKey(), &results1,
      callback1.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver().GetProxyForURL(
      GURL("http://request2"), NetworkAnonymizationKey(), &results2,
      callback2.callback(), nullptr, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Wait until request 0 reaches the worker thread.
  factory().resolvers()[0]->WaitUntilBlocked();

  // Add some latency, to improve the chance that when
  // MultiThreadedProxyResolver is deleted below we are still running inside
  // of the worker thread. The test will pass regardless, so this race doesn't
  // cause flakiness. However the destruction during execution is a more
  // interesting case to test.
  factory().resolvers()[0]->SetResolveLatency(base::Milliseconds(100));

  // Unblock the worker thread and delete the underlying
  // MultiThreadedProxyResolver immediately.
  factory().resolvers()[0]->Unblock();
  ClearResolver();

  // Give any posted tasks a chance to run (in case there is badness).
  base::RunLoop().RunUntilIdle();

  // Check that none of the outstanding requests were completed.
  EXPECT_FALSE(callback0.have_result());
  EXPECT_FALSE(callback1.have_result());
  EXPECT_FALSE(callback2.have_result());
}

// Tests setting the PAC script once, lazily creating new threads, and
// cancelling requests.
TEST_F(MultiThreadedProxyResolverTest, ThreeThreads_Basic) {
  const size_t kNumThreads = 3u;
  ASSERT_NO_FATAL_FAILURE(Init(kNumThreads));

  // Verify that it reaches the synchronous resolver.
  // One thread has been provisioned (i.e. one ProxyResolver was created).
  ASSERT_EQ(1u, factory().resolvers().size());

  const int kNumRequests = 8;
  int rv;
  TestCompletionCallback callback[kNumRequests];
  ProxyInfo results[kNumRequests];
  std::unique_ptr<ProxyResolver::Request> request[kNumRequests];

  // Start request 0 -- this should run on thread 0 as there is nothing else
  // going on right now.
  rv = resolver().GetProxyForURL(
      GURL("http://request0"), NetworkAnonymizationKey(), &results[0],
      callback[0].callback(), &request[0], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Wait for request 0 to finish.
  rv = callback[0].WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ("PROXY request0:80", results[0].ToDebugString());
  ASSERT_EQ(1u, factory().resolvers().size());
  EXPECT_EQ(1, factory().resolvers()[0]->request_count());

  base::RunLoop().RunUntilIdle();

  // We now block the first resolver to ensure a request is sent to the second
  // thread.
  factory().resolvers()[0]->Block();
  rv = resolver().GetProxyForURL(
      GURL("http://request1"), NetworkAnonymizationKey(), &results[1],
      callback[1].callback(), &request[1], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  factory().resolvers()[0]->WaitUntilBlocked();
  rv = resolver().GetProxyForURL(
      GURL("http://request2"), NetworkAnonymizationKey(), &results[2],
      callback[2].callback(), &request[2], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(0, callback[2].WaitForResult());
  ASSERT_EQ(2u, factory().resolvers().size());

  // We now block the second resolver as well to ensure a request is sent to the
  // third thread.
  factory().resolvers()[1]->Block();
  rv = resolver().GetProxyForURL(
      GURL("http://request3"), NetworkAnonymizationKey(), &results[3],
      callback[3].callback(), &request[3], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  factory().resolvers()[1]->WaitUntilBlocked();
  rv = resolver().GetProxyForURL(
      GURL("http://request4"), NetworkAnonymizationKey(), &results[4],
      callback[4].callback(), &request[4], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(0, callback[4].WaitForResult());

  // We should now have a total of 3 threads, each with its own ProxyResolver
  // that will get initialized with the same data.
  ASSERT_EQ(3u, factory().resolvers().size());

  ASSERT_EQ(3u, factory().script_data().size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(u"pac script bytes", factory().script_data()[i]->utf16())
        << "i=" << i;
  }

  // Start and cancel two requests. Since the first two threads are still
  // blocked, they'll both be serviced by the third thread. The first request
  // will reach the resolver, but the second will still be queued when canceled.
  // Start a third request so we can be sure the resolver has completed running
  // the first request.
  rv = resolver().GetProxyForURL(
      GURL("http://request5"), NetworkAnonymizationKey(), &results[5],
      callback[5].callback(), &request[5], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = resolver().GetProxyForURL(
      GURL("http://request6"), NetworkAnonymizationKey(), &results[6],
      callback[6].callback(), &request[6], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = resolver().GetProxyForURL(
      GURL("http://request7"), NetworkAnonymizationKey(), &results[7],
      callback[7].callback(), &request[7], NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  request[5].reset();
  request[6].reset();

  EXPECT_EQ(2, callback[7].WaitForResult());

  // Check that the cancelled requests never invoked their callback.
  EXPECT_FALSE(callback[5].have_result());
  EXPECT_FALSE(callback[6].have_result());

  // Unblock the first two threads and wait for their requests to complete.
  factory().resolvers()[0]->Unblock();
  factory().resolvers()[1]->Unblock();
  EXPECT_EQ(1, callback[1].WaitForResult());
  EXPECT_EQ(1, callback[3].WaitForResult());

  EXPECT_EQ(2, factory().resolvers()[0]->request_count());
  EXPECT_EQ(2, factory().resolvers()[1]->request_count());
  EXPECT_EQ(3, factory().resolvers()[2]->request_count());
}

// Tests using two threads. The first request hangs the first thread. Checks
// that other requests are able to complete while this first request remains
// stalled.
TEST_F(MultiThreadedProxyResolverTest, OneThreadBlocked) {
  const size_t kNumThreads = 2u;
  ASSERT_NO_FATAL_FAILURE(Init(kNumThreads));

  int rv;

  // One thread has been provisioned (i.e. one ProxyResolver was created).
  ASSERT_EQ(1u, factory().resolvers().size());
  EXPECT_EQ(u"pac script bytes", factory().script_data()[0]->utf16());

  const int kNumRequests = 4;
  TestCompletionCallback callback[kNumRequests];
  ProxyInfo results[kNumRequests];
  std::unique_ptr<ProxyResolver::Request> request[kNumRequests];

  // Start a request that will block the first thread.

  factory().resolvers()[0]->Block();

  rv = resolver().GetProxyForURL(
      GURL("http://request0"), NetworkAnonymizationKey(), &results[0],
      callback[0].callback(), &request[0], NetLogWithSource());

  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  factory().resolvers()[0]->WaitUntilBlocked();

  // Start 3 more requests -- they should all be serviced by thread #2
  // since thread #1 is blocked.

  for (int i = 1; i < kNumRequests; ++i) {
    rv = resolver().GetProxyForURL(
        GURL(base::StringPrintf("http://request%d", i)),
        NetworkAnonymizationKey(), &results[i], callback[i].callback(),
        &request[i], NetLogWithSource());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  }

  // Wait for the three requests to complete (they should complete in FIFO
  // order).
  for (int i = 1; i < kNumRequests; ++i) {
    EXPECT_EQ(i - 1, callback[i].WaitForResult());
  }

  // Unblock the first thread.
  factory().resolvers()[0]->Unblock();
  EXPECT_EQ(0, callback[0].WaitForResult());

  // All in all, the first thread should have seen just 1 request. And the
  // second thread 3 requests.
  ASSERT_EQ(2u, factory().resolvers().size());
  EXPECT_EQ(1, factory().resolvers()[0]->request_count());
  EXPECT_EQ(3, factory().resolvers()[1]->request_count());
}

class FailingProxyResolverFactory : public ProxyResolverFactory {
 public:
  FailingProxyResolverFactory() : ProxyResolverFactory(false) {}

  // ProxyResolverFactory override.
  int CreateProxyResolver(const scoped_refptr<PacFileData>& script_data,
                          std::unique_ptr<ProxyResolver>* result,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    return ERR_PAC_SCRIPT_FAILED;
  }
};

// Test that an error when creating the synchronous resolver causes the
// MultiThreadedProxyResolverFactory create request to fail with that error.
TEST_F(MultiThreadedProxyResolverTest, ProxyResolverFactoryError) {
  const size_t kNumThreads = 1u;
  SingleShotMultiThreadedProxyResolverFactory resolver_factory(
      kNumThreads, std::make_unique<FailingProxyResolverFactory>());
  TestCompletionCallback ready_callback;
  std::unique_ptr<ProxyResolverFactory::Request> request;
  std::unique_ptr<ProxyResolver> resolver;
  EXPECT_EQ(ERR_IO_PENDING,
            resolver_factory.CreateProxyResolver(
                PacFileData::FromUTF8("pac script bytes"), &resolver,
                ready_callback.callback(), &request));
  EXPECT_TRUE(request);
  EXPECT_THAT(ready_callback.WaitForResult(), IsError(ERR_PAC_SCRIPT_FAILED));
  EXPECT_FALSE(resolver);
}

void Fail(int error) {
  FAIL() << "Unexpected callback with error " << error;
}

// Test that cancelling an in-progress create request works correctly.
TEST_F(MultiThreadedProxyResolverTest, CancelCreate) {
  const size_t kNumThreads = 1u;
  {
    SingleShotMultiThreadedProxyResolverFactory resolver_factory(
        kNumThreads, std::make_unique<BlockableProxyResolverFactory>());
    std::unique_ptr<ProxyResolverFactory::Request> request;
    std::unique_ptr<ProxyResolver> resolver;
    EXPECT_EQ(ERR_IO_PENDING, resolver_factory.CreateProxyResolver(
                                  PacFileData::FromUTF8("pac script bytes"),
                                  &resolver, base::BindOnce(&Fail), &request));
    EXPECT_TRUE(request);
    request.reset();
  }
  // The factory destructor will block until the worker thread stops, but it may
  // post tasks to the origin message loop which are still pending. Run them
  // now to ensure it works as expected.
  base::RunLoop().RunUntilIdle();
}

void DeleteRequest(CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolverFactory::Request>* request,
                   int result) {
  std::move(callback).Run(result);
  request->reset();
}

// Test that delete the Request during the factory callback works correctly.
TEST_F(MultiThreadedProxyResolverTest, DeleteRequestInFactoryCallback) {
  const size_t kNumThreads = 1u;
  SingleShotMultiThreadedProxyResolverFactory resolver_factory(
      kNumThreads, std::make_unique<BlockableProxyResolverFactory>());
  std::unique_ptr<ProxyResolverFactory::Request> request;
  std::unique_ptr<ProxyResolver> resolver;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            resolver_factory.CreateProxyResolver(
                PacFileData::FromUTF8("pac script bytes"), &resolver,
                base::BindOnce(&DeleteRequest, callback.callback(),
                               base::Unretained(&request)),
                &request));
  EXPECT_TRUE(request);
  EXPECT_THAT(callback.WaitForResult(), IsOk());
}

// Test that deleting the factory with a request in-progress works correctly.
TEST_F(MultiThreadedProxyResolverTest, DestroyFactoryWithRequestsInProgress) {
  const size_t kNumThreads = 1u;
  std::unique_ptr<ProxyResolverFactory::Request> request;
  std::unique_ptr<ProxyResolver> resolver;
  {
    SingleShotMultiThreadedProxyResolverFactory resolver_factory(
        kNumThreads, std::make_unique<BlockableProxyResolverFactory>());
    EXPECT_EQ(ERR_IO_PENDING, resolver_factory.CreateProxyResolver(
                                  PacFileData::FromUTF8("pac script bytes"),
                                  &resolver, base::BindOnce(&Fail), &request));
    EXPECT_TRUE(request);
  }
  // The factory destructor will block until the worker thread stops, but it may
  // post tasks to the origin message loop which are still pending. Run them
  // now to ensure it works as expected.
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace net
