// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/dhcp_pac_file_fetcher_win.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "net/proxy_resolution/dhcp_pac_file_adapter_fetcher_win.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

TEST(DhcpPacFileFetcherWin, AdapterNamesAndPacURLFromDhcp) {
  // This tests our core Win32 implementation without any of the wrappers
  // we layer on top to achieve asynchronous and parallel operations.
  //
  // We don't make assumptions about the environment this unit test is
  // running in, so it just exercises the code to make sure there
  // is no crash and no error returned, but does not assert on the number
  // of interfaces or the information returned via DHCP.
  std::set<std::string> adapter_names;
  DhcpPacFileFetcherWin::GetCandidateAdapterNames(&adapter_names, nullptr);
  for (const std::string& adapter_name : adapter_names) {
    DhcpPacFileAdapterFetcher::GetPacURLFromDhcp(adapter_name);
  }
}

// Helper for RealFetch* tests below.
class RealFetchTester {
 public:
  RealFetchTester()
      : context_(new TestURLRequestContext),
        fetcher_(new DhcpPacFileFetcherWin(context_.get())),
        finished_(false),
        on_completion_is_error_(false) {
    // Make sure the test ends.
    timeout_.Start(FROM_HERE,
        base::TimeDelta::FromSeconds(5), this, &RealFetchTester::OnTimeout);
  }

  void RunTest() {
    int result = fetcher_->Fetch(
        &pac_text_,
        base::Bind(&RealFetchTester::OnCompletion, base::Unretained(this)),
        NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != ERR_IO_PENDING)
      finished_ = true;
  }

  void RunTestWithCancel() {
    RunTest();
    fetcher_->Cancel();
  }

  void RunTestWithDeferredCancel() {
    // Put the cancellation into the queue before even running the
    // test to avoid the chance of one of the adapter fetcher worker
    // threads completing before cancellation.  See http://crbug.com/86756.
    cancel_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(0),
                        this, &RealFetchTester::OnCancelTimer);
    RunTest();
  }

  void OnCompletion(int result) {
    if (on_completion_is_error_) {
      FAIL() << "Received completion for test in which this is error.";
    }
    finished_ = true;
  }

  void OnTimeout() {
    OnCompletion(0);
  }

  void OnCancelTimer() {
    fetcher_->Cancel();
    finished_ = true;
  }

  void WaitUntilDone() {
    while (!finished_) {
      base::RunLoop().RunUntilIdle();
    }
    base::RunLoop().RunUntilIdle();
  }

  // Attempts to give worker threads time to finish.  This is currently
  // very simplistic as completion (via completion callback or cancellation)
  // immediately "detaches" any worker threads, so the best we can do is give
  // them a little time.  If we start running into memory leaks, we can
  // do something a bit more clever to track worker threads even when the
  // DhcpPacFileFetcherWin state machine has finished.
  void FinishTestAllowCleanup() {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(30));
  }

  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<DhcpPacFileFetcherWin> fetcher_;
  bool finished_;
  base::string16 pac_text_;
  base::OneShotTimer timeout_;
  base::OneShotTimer cancel_timer_;
  bool on_completion_is_error_;
};

TEST(DhcpPacFileFetcherWin, RealFetch) {
  base::test::TaskEnvironment task_environment;

  // This tests a call to Fetch() with no stubbing out of dependencies.
  //
  // We don't make assumptions about the environment this unit test is
  // running in, so it just exercises the code to make sure there
  // is no crash and no unexpected error returned, but does not assert on
  // results beyond that.
  RealFetchTester fetcher;
  fetcher.RunTest();

  fetcher.WaitUntilDone();
  fetcher.fetcher_->GetPacURL().possibly_invalid_spec();

  fetcher.FinishTestAllowCleanup();
}

TEST(DhcpPacFileFetcherWin, RealFetchWithCancel) {
  base::test::TaskEnvironment task_environment;

  // Does a Fetch() with an immediate cancel.  As before, just
  // exercises the code without stubbing out dependencies.
  RealFetchTester fetcher;
  fetcher.RunTestWithCancel();
  base::RunLoop().RunUntilIdle();

  // Attempt to avoid memory leak reports in case worker thread is
  // still running.
  fetcher.FinishTestAllowCleanup();
}

// For RealFetchWithDeferredCancel, below.
class DelayingDhcpPacFileAdapterFetcher : public DhcpPacFileAdapterFetcher {
 public:
  DelayingDhcpPacFileAdapterFetcher(URLRequestContext* url_request_context,
                                    scoped_refptr<base::TaskRunner> task_runner)
      : DhcpPacFileAdapterFetcher(url_request_context, task_runner) {}

  class DelayingDhcpQuery : public DhcpQuery {
   public:
    explicit DelayingDhcpQuery() : DhcpQuery() {}

    std::string ImplGetPacURLFromDhcp(
        const std::string& adapter_name) override {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));
      return DhcpQuery::ImplGetPacURLFromDhcp(adapter_name);
    }

   private:
    ~DelayingDhcpQuery() override {}
  };

  DhcpQuery* ImplCreateDhcpQuery() override {
    return new DelayingDhcpQuery();
  }
};

// For RealFetchWithDeferredCancel, below.
class DelayingDhcpPacFileFetcherWin : public DhcpPacFileFetcherWin {
 public:
  explicit DelayingDhcpPacFileFetcherWin(URLRequestContext* context)
      : DhcpPacFileFetcherWin(context) {}

  DhcpPacFileAdapterFetcher* ImplCreateAdapterFetcher() override {
    return new DelayingDhcpPacFileAdapterFetcher(url_request_context(),
                                                 GetTaskRunner());
  }
};

TEST(DhcpPacFileFetcherWin, RealFetchWithDeferredCancel) {
  base::test::TaskEnvironment task_environment;

  // Does a Fetch() with a slightly delayed cancel.  As before, just
  // exercises the code without stubbing out dependencies, but
  // introduces a guaranteed 20 ms delay on the worker threads so that
  // the cancel is called before they complete.
  RealFetchTester fetcher;
  fetcher.fetcher_.reset(
      new DelayingDhcpPacFileFetcherWin(fetcher.context_.get()));
  fetcher.on_completion_is_error_ = true;
  fetcher.RunTestWithDeferredCancel();
  fetcher.WaitUntilDone();
}

// The remaining tests are to exercise our state machine in various
// situations, with actual network access fully stubbed out.

class DummyDhcpPacFileAdapterFetcher : public DhcpPacFileAdapterFetcher {
 public:
  DummyDhcpPacFileAdapterFetcher(URLRequestContext* context,
                                 scoped_refptr<base::TaskRunner> runner)
      : DhcpPacFileAdapterFetcher(context, runner),
        did_finish_(false),
        result_(OK),
        pac_script_(STRING16_LITERAL("bingo")),
        fetch_delay_ms_(1) {}

  void Fetch(const std::string& adapter_name,
             CompletionOnceCallback callback,
             const NetworkTrafficAnnotationTag traffic_annotation) override {
    callback_ = std::move(callback);
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(fetch_delay_ms_),
                 this, &DummyDhcpPacFileAdapterFetcher::OnTimer);
  }

  void Cancel() override {
    timer_.Stop();
  }

  bool DidFinish() const override {
    return did_finish_;
  }

  int GetResult() const override {
    return result_;
  }

  base::string16 GetPacScript() const override {
    return pac_script_;
  }

  void OnTimer() { std::move(callback_).Run(result_); }

  void Configure(bool did_finish,
                 int result,
                 base::string16 pac_script,
                 int fetch_delay_ms) {
    did_finish_ = did_finish;
    result_ = result;
    pac_script_ = pac_script;
    fetch_delay_ms_ = fetch_delay_ms;
  }

 private:
  bool did_finish_;
  int result_;
  base::string16 pac_script_;
  int fetch_delay_ms_;
  CompletionOnceCallback callback_;
  base::OneShotTimer timer_;
};

class MockDhcpPacFileFetcherWin : public DhcpPacFileFetcherWin {
 public:
  class MockAdapterQuery : public AdapterQuery {
   public:
    MockAdapterQuery() {
    }

    bool ImplGetCandidateAdapterNames(
        std::set<std::string>* adapter_names,
        DhcpAdapterNamesLoggingInfo* logging) override {
      adapter_names->insert(mock_adapter_names_.begin(),
                            mock_adapter_names_.end());
      return true;
    }

    std::vector<std::string> mock_adapter_names_;

   private:
    ~MockAdapterQuery() override {}
  };

  MockDhcpPacFileFetcherWin(URLRequestContext* context)
      : DhcpPacFileFetcherWin(context),
        num_fetchers_created_(0),
        worker_finished_event_(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
    ResetTestState();
  }

  ~MockDhcpPacFileFetcherWin() override { ResetTestState(); }

  using DhcpPacFileFetcherWin::GetTaskRunner;

  // Adds a fetcher object to the queue of fetchers used by
  // |ImplCreateAdapterFetcher()|, and its name to the list of adapters
  // returned by ImplGetCandidateAdapterNames.
  void PushBackAdapter(const std::string& adapter_name,
                       DhcpPacFileAdapterFetcher* fetcher) {
    adapter_query_->mock_adapter_names_.push_back(adapter_name);
    adapter_fetchers_.push_back(fetcher);
  }

  void ConfigureAndPushBackAdapter(const std::string& adapter_name,
                                   bool did_finish,
                                   int result,
                                   base::string16 pac_script,
                                   base::TimeDelta fetch_delay) {
    std::unique_ptr<DummyDhcpPacFileAdapterFetcher> adapter_fetcher(
        new DummyDhcpPacFileAdapterFetcher(url_request_context(),
                                           GetTaskRunner()));
    adapter_fetcher->Configure(
        did_finish, result, pac_script, fetch_delay.InMilliseconds());
    PushBackAdapter(adapter_name, adapter_fetcher.release());
  }

  DhcpPacFileAdapterFetcher* ImplCreateAdapterFetcher() override {
    ++num_fetchers_created_;
    return adapter_fetchers_[next_adapter_fetcher_index_++];
  }

  AdapterQuery* ImplCreateAdapterQuery() override {
    DCHECK(adapter_query_.get());
    return adapter_query_.get();
  }

  base::TimeDelta ImplGetMaxWait() override {
    return max_wait_;
  }

  void ImplOnGetCandidateAdapterNamesDone() override {
    worker_finished_event_.Signal();
  }

  void ResetTestState() {
    // Delete any adapter fetcher objects we didn't hand out.
    std::vector<DhcpPacFileAdapterFetcher*>::const_iterator it =
        adapter_fetchers_.begin();
    for (; it != adapter_fetchers_.end(); ++it) {
      if (num_fetchers_created_-- <= 0) {
        delete (*it);
      }
    }

    next_adapter_fetcher_index_ = 0;
    num_fetchers_created_ = 0;
    adapter_fetchers_.clear();
    adapter_query_ = new MockAdapterQuery();
    max_wait_ = TestTimeouts::tiny_timeout();
  }

  bool HasPendingFetchers() {
    return num_pending_fetchers() > 0;
  }

  int next_adapter_fetcher_index_;

  // Ownership gets transferred to the implementation class via
  // ImplCreateAdapterFetcher, but any objects not handed out are
  // deleted on destruction.
  std::vector<DhcpPacFileAdapterFetcher*> adapter_fetchers_;

  scoped_refptr<MockAdapterQuery> adapter_query_;

  base::TimeDelta max_wait_;
  int num_fetchers_created_;
  base::WaitableEvent worker_finished_event_;
};

class FetcherClient {
 public:
  FetcherClient()
      : context_(new TestURLRequestContext),
        fetcher_(context_.get()),
        finished_(false),
        result_(ERR_UNEXPECTED) {
  }

  void RunTest() {
    int result = fetcher_.Fetch(
        &pac_text_,
        base::Bind(&FetcherClient::OnCompletion, base::Unretained(this)),
        NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS);
    ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  }

  int RunTestThatMayFailSync() {
    int result = fetcher_.Fetch(
        &pac_text_,
        base::Bind(&FetcherClient::OnCompletion, base::Unretained(this)),
        NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != ERR_IO_PENDING)
      result_ = result;
    return result;
  }

  void RunMessageLoopUntilComplete() {
    while (!finished_) {
      base::RunLoop().RunUntilIdle();
    }
    base::RunLoop().RunUntilIdle();
  }

  void RunMessageLoopUntilWorkerDone() {
    DCHECK(fetcher_.adapter_query_.get());
    while (!fetcher_.worker_finished_event_.TimedWait(
        base::TimeDelta::FromMilliseconds(10))) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void OnCompletion(int result) {
    finished_ = true;
    result_ = result;
  }

  void ResetTestState() {
    finished_ = false;
    result_ = ERR_UNEXPECTED;
    pac_text_.clear();
    fetcher_.ResetTestState();
  }

  scoped_refptr<base::TaskRunner> GetTaskRunner() {
    return fetcher_.GetTaskRunner();
  }

  std::unique_ptr<URLRequestContext> context_;
  MockDhcpPacFileFetcherWin fetcher_;
  bool finished_;
  int result_;
  base::string16 pac_text_;
};

// We separate out each test's logic so that we can easily implement
// the ReuseFetcher test at the bottom.
void TestNormalCaseURLConfiguredOneAdapter(FetcherClient* client) {
  TestURLRequestContext context;
  std::unique_ptr<DummyDhcpPacFileAdapterFetcher> adapter_fetcher(
      new DummyDhcpPacFileAdapterFetcher(&context, client->GetTaskRunner()));
  adapter_fetcher->Configure(true, OK, STRING16_LITERAL("bingo"), 1);
  client->fetcher_.PushBackAdapter("a", adapter_fetcher.release());
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_THAT(client->result_, IsOk());
  ASSERT_EQ(STRING16_LITERAL("bingo"), client->pac_text_);
}

TEST(DhcpPacFileFetcherWin, NormalCaseURLConfiguredOneAdapter) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestNormalCaseURLConfiguredOneAdapter(&client);
}

void TestNormalCaseURLConfiguredMultipleAdapters(FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  client->fetcher_.ConfigureAndPushBackAdapter(
      "second", true, OK, STRING16_LITERAL("bingo"),
      base::TimeDelta::FromMilliseconds(50));
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, OK, STRING16_LITERAL("rocko"),
      base::TimeDelta::FromMilliseconds(1));
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_THAT(client->result_, IsOk());
  ASSERT_EQ(STRING16_LITERAL("bingo"), client->pac_text_);
}

TEST(DhcpPacFileFetcherWin, NormalCaseURLConfiguredMultipleAdapters) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestNormalCaseURLConfiguredMultipleAdapters(&client);
}

void TestNormalCaseURLConfiguredMultipleAdaptersWithTimeout(
    FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  // This will time out.
  client->fetcher_.ConfigureAndPushBackAdapter("second", false, ERR_IO_PENDING,
                                               STRING16_LITERAL("bingo"),
                                               TestTimeouts::action_timeout());
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, OK, STRING16_LITERAL("rocko"),
      base::TimeDelta::FromMilliseconds(1));
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_THAT(client->result_, IsOk());
  ASSERT_EQ(STRING16_LITERAL("rocko"), client->pac_text_);
}

TEST(DhcpPacFileFetcherWin,
     NormalCaseURLConfiguredMultipleAdaptersWithTimeout) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestNormalCaseURLConfiguredMultipleAdaptersWithTimeout(&client);
}

void TestFailureCaseURLConfiguredMultipleAdaptersWithTimeout(
    FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  // This will time out.
  client->fetcher_.ConfigureAndPushBackAdapter("second", false, ERR_IO_PENDING,
                                               STRING16_LITERAL("bingo"),
                                               TestTimeouts::action_timeout());
  // This is the first non-ERR_PAC_NOT_IN_DHCP error and as such
  // should be chosen.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, ERR_HTTP_RESPONSE_CODE_FAILURE, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  client->fetcher_.ConfigureAndPushBackAdapter(
      "fourth", true, ERR_NOT_IMPLEMENTED, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_THAT(client->result_, IsError(ERR_HTTP_RESPONSE_CODE_FAILURE));
  ASSERT_EQ(base::string16(), client->pac_text_);
}

TEST(DhcpPacFileFetcherWin,
     FailureCaseURLConfiguredMultipleAdaptersWithTimeout) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestFailureCaseURLConfiguredMultipleAdaptersWithTimeout(&client);
}

void TestFailureCaseNoURLConfigured(FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  // This will time out.
  client->fetcher_.ConfigureAndPushBackAdapter("second", false, ERR_IO_PENDING,
                                               STRING16_LITERAL("bingo"),
                                               TestTimeouts::action_timeout());
  // This is the first non-ERR_PAC_NOT_IN_DHCP error and as such
  // should be chosen.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, ERR_PAC_NOT_IN_DHCP, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_THAT(client->result_, IsError(ERR_PAC_NOT_IN_DHCP));
  ASSERT_EQ(base::string16(), client->pac_text_);
}

TEST(DhcpPacFileFetcherWin, FailureCaseNoURLConfigured) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestFailureCaseNoURLConfigured(&client);
}

void TestFailureCaseNoDhcpAdapters(FetcherClient* client) {
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_THAT(client->result_, IsError(ERR_PAC_NOT_IN_DHCP));
  ASSERT_EQ(base::string16(), client->pac_text_);
  ASSERT_EQ(0, client->fetcher_.num_fetchers_created_);
}

TEST(DhcpPacFileFetcherWin, FailureCaseNoDhcpAdapters) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestFailureCaseNoDhcpAdapters(&client);
}

void TestShortCircuitLessPreferredAdapters(FetcherClient* client) {
  // Here we have a bunch of adapters; the first reports no PAC in DHCP,
  // the second responds quickly with a PAC file, the rest take a long
  // time.  Verify that we complete quickly and do not wait for the slow
  // adapters, i.e. we finish before timeout.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "1", true, ERR_PAC_NOT_IN_DHCP, base::string16(),
      base::TimeDelta::FromMilliseconds(1));
  client->fetcher_.ConfigureAndPushBackAdapter(
      "2", true, OK, STRING16_LITERAL("bingo"),
      base::TimeDelta::FromMilliseconds(1));
  client->fetcher_.ConfigureAndPushBackAdapter(
      "3", true, OK, STRING16_LITERAL("wrongo"),
      TestTimeouts::action_max_timeout());

  // Increase the timeout to ensure the short circuit mechanism has
  // time to kick in before the timeout waiting for more adapters kicks in.
  client->fetcher_.max_wait_ = TestTimeouts::action_timeout();

  base::ElapsedTimer timer;
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_TRUE(client->fetcher_.HasPendingFetchers());
  // Assert that the time passed is definitely less than the wait timer
  // timeout, to get a second signal that it was the shortcut mechanism
  // (in OnFetcherDone) that kicked in, and not the timeout waiting for
  // more adapters.
  ASSERT_GT(client->fetcher_.max_wait_ - (client->fetcher_.max_wait_ / 10),
            timer.Elapsed());
}

TEST(DhcpPacFileFetcherWin, ShortCircuitLessPreferredAdapters) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestShortCircuitLessPreferredAdapters(&client);
}

void TestImmediateCancel(FetcherClient* client) {
  TestURLRequestContext context;
  std::unique_ptr<DummyDhcpPacFileAdapterFetcher> adapter_fetcher(
      new DummyDhcpPacFileAdapterFetcher(&context, client->GetTaskRunner()));
  adapter_fetcher->Configure(true, OK, STRING16_LITERAL("bingo"), 1);
  client->fetcher_.PushBackAdapter("a", adapter_fetcher.release());
  client->RunTest();
  client->fetcher_.Cancel();
  client->RunMessageLoopUntilWorkerDone();
  ASSERT_EQ(0, client->fetcher_.num_fetchers_created_);
}

// Regression test to check that when we cancel immediately, no
// adapter fetchers get created.
TEST(DhcpPacFileFetcherWin, ImmediateCancel) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestImmediateCancel(&client);
}

TEST(DhcpPacFileFetcherWin, ReuseFetcher) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;

  // The PacFileFetcher interface stipulates that only a single
  // |Fetch()| may be in flight at once, but allows reuse, so test
  // that the state transitions correctly from done to start in all
  // cases we're testing.

  typedef void (*FetcherClientTestFunction)(FetcherClient*);
  typedef std::vector<FetcherClientTestFunction> TestVector;
  TestVector test_functions;
  test_functions.push_back(TestNormalCaseURLConfiguredOneAdapter);
  test_functions.push_back(TestNormalCaseURLConfiguredMultipleAdapters);
  test_functions.push_back(
      TestNormalCaseURLConfiguredMultipleAdaptersWithTimeout);
  test_functions.push_back(
      TestFailureCaseURLConfiguredMultipleAdaptersWithTimeout);
  test_functions.push_back(TestFailureCaseNoURLConfigured);
  test_functions.push_back(TestFailureCaseNoDhcpAdapters);
  test_functions.push_back(TestShortCircuitLessPreferredAdapters);
  test_functions.push_back(TestImmediateCancel);

  base::RandomShuffle(test_functions.begin(), test_functions.end());
  for (TestVector::const_iterator it = test_functions.begin();
       it != test_functions.end();
       ++it) {
    (*it)(&client);
    client.ResetTestState();
  }

  // Re-do the first test to make sure the last test that was run did
  // not leave things in a bad state.
  (*test_functions.begin())(&client);
}

TEST(DhcpPacFileFetcherWin, OnShutdown) {
  base::test::TaskEnvironment task_environment;

  FetcherClient client;
  TestURLRequestContext context;
  std::unique_ptr<DummyDhcpPacFileAdapterFetcher> adapter_fetcher(
      new DummyDhcpPacFileAdapterFetcher(&context, client.GetTaskRunner()));
  adapter_fetcher->Configure(true, OK, STRING16_LITERAL("bingo"), 1);
  client.fetcher_.PushBackAdapter("a", adapter_fetcher.release());
  client.RunTest();

  client.fetcher_.OnShutdown();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client.finished_);

  client.ResetTestState();
  EXPECT_THAT(client.RunTestThatMayFailSync(), IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_EQ(0u, context.url_requests()->size());
}

}  // namespace

}  // namespace net
