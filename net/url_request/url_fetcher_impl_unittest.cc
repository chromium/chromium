// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_impl.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/network_change_notifier.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using base::Time;
using base::TimeDelta;
using net::test::IsError;
using net::test::IsOk;

// TODO(eroman): Add a regression test for http://crbug.com/40505.

namespace {

// TODO(akalin): Move all the test data to somewhere under net/.
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("net/data/url_fetcher_impl_unittest");
const char kTestServerFilePrefix[] = "/";

// Test server path and response body for the default URL used by many of the
// tests.
const char kDefaultResponsePath[] = "/defaultresponse";
const char kDefaultResponseBody[] =
    "Default response given for path: /defaultresponse";

// Request body for streams created by CreateUploadStream.
const char kCreateUploadStreamBody[] = "rosebud";

base::FilePath GetUploadFileTestPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.Append(
      FILE_PATH_LITERAL("net/data/url_request_unittest/BullRunSpeech.txt"));
}

// Simple URLRequestDelegate that waits for the specified fetcher to complete.
// Can only be used once.
class WaitingURLFetcherDelegate : public URLFetcherDelegate {
 public:
  WaitingURLFetcherDelegate() : did_complete_(false) {}

  void CreateFetcher(
      const GURL& url,
      URLFetcher::RequestType request_type,
      scoped_refptr<net::URLRequestContextGetter> context_getter) {
    if (!on_complete_or_cancel_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      on_complete_or_cancel_ = run_loop_->QuitClosure();
    }
    fetcher_.reset(new URLFetcherImpl(url, request_type, this,
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
    fetcher_->SetRequestContext(context_getter.get());
  }

  URLFetcher* fetcher() const { return fetcher_.get(); }

  // Wait until the request has completed or been canceled.
  void StartFetcherAndWait() {
    fetcher_->Start();
    WaitForComplete();
  }

  // Wait until the request has completed or been canceled. Does not start the
  // request.
  void WaitForComplete() {
    EXPECT_TRUE(task_runner_->RunsTasksInCurrentSequence());
    run_loop_->Run();
  }

  // Cancels the fetch by deleting the fetcher.
  void CancelFetch() {
    EXPECT_TRUE(fetcher_);
    fetcher_.reset();
    std::move(on_complete_or_cancel_).Run();
  }

  // URLFetcherDelegate:
  void OnURLFetchComplete(const URLFetcher* source) override {
    EXPECT_FALSE(did_complete_);
    EXPECT_TRUE(fetcher_);
    EXPECT_EQ(fetcher_.get(), source);
    did_complete_ = true;
    std::move(on_complete_or_cancel_).Run();
  }

  void OnURLFetchDownloadProgress(const URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override {
    // Note that the current progress may be greater than the previous progress,
    // in the case of retrying the request.
    EXPECT_FALSE(did_complete_);
    EXPECT_TRUE(fetcher_);
    EXPECT_EQ(source, fetcher_.get());

    EXPECT_LE(0, current);
    // If file size is not known, |total| is -1.
    if (total >= 0)
      EXPECT_LE(current, total);
  }

  void OnURLFetchUploadProgress(const URLFetcher* source,
                                int64_t current,
                                int64_t total) override {
    // Note that the current progress may be greater than the previous progress,
    // in the case of retrying the request.
    EXPECT_FALSE(did_complete_);
    EXPECT_TRUE(fetcher_);
    EXPECT_EQ(source, fetcher_.get());

    EXPECT_LE(0, current);
    // If file size is not known, |total| is -1.
    if (total >= 0)
      EXPECT_LE(current, total);
  }

  bool did_complete() const { return did_complete_; }

  void set_on_complete_or_cancel_closure(base::OnceClosure closure) {
    on_complete_or_cancel_ = std::move(closure);
  }

 private:
  bool did_complete_;

  std::unique_ptr<URLFetcherImpl> fetcher_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunnerHandle::Get();
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure on_complete_or_cancel_;

  DISALLOW_COPY_AND_ASSIGN(WaitingURLFetcherDelegate);
};

// A TestURLRequestContext with a ThrottleManager and a MockHostResolver.
class FetcherTestURLRequestContext : public TestURLRequestContext {
 public:
  // All requests for |hanging_domain| will hang on host resolution until the
  // mock_resolver()->ResolveAllPending() is called.
  FetcherTestURLRequestContext(
      const std::string& hanging_domain,
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service)
      : TestURLRequestContext(true), mock_resolver_(new MockHostResolver()) {
    mock_resolver_->set_ondemand_mode(true);
    mock_resolver_->rules()->AddRule(hanging_domain, "127.0.0.1");
    // Pass ownership to ContextStorage to ensure correct destruction order.
    context_storage_.set_host_resolver(
        std::unique_ptr<HostResolver>(mock_resolver_));
    context_storage_.set_throttler_manager(
        std::make_unique<URLRequestThrottlerManager>());
    context_storage_.set_proxy_resolution_service(
        std::move(proxy_resolution_service));
    Init();
  }

  MockHostResolver* mock_resolver() { return mock_resolver_; }

 private:
  MockHostResolver* mock_resolver_;

  DISALLOW_COPY_AND_ASSIGN(FetcherTestURLRequestContext);
};

class FetcherTestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  FetcherTestURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      const std::string& hanging_domain)
      : network_task_runner_(network_task_runner),
        hanging_domain_(hanging_domain),
        shutting_down_(false) {}

  // Sets callback to be invoked when the getter is destroyed.
  void set_on_destruction_callback(base::OnceClosure on_destruction_callback) {
    on_destruction_callback_ = std::move(on_destruction_callback);
  }

  // URLRequestContextGetter:
  FetcherTestURLRequestContext* GetURLRequestContext() override {
    // Calling this on the wrong thread may be either a bug in the test or a bug
    // in production code.
    EXPECT_TRUE(network_task_runner_->BelongsToCurrentThread());

    if (shutting_down_)
      return nullptr;

    if (!context_) {
      context_.reset(new FetcherTestURLRequestContext(
          hanging_domain_, std::move(proxy_resolution_service_)));
    }

    return context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return network_task_runner_;
  }

  // Adds a throttler entry with the specified parameters.  Does this
  // synchronously if the context lives on the current thread, or posts a task
  // to the relevant thread otherwise.
  //
  // If |reserve_sending_time_for_next_request|, will start backoff early, as
  // if there has already been a request for |url|.
  void AddThrottlerEntry(const GURL& url,
                         const std::string& url_id,
                         int sliding_window_period_ms,
                         int max_send_threshold,
                         int initial_backoff_ms,
                         double multiply_factor,
                         double jitter_factor,
                         int maximum_backoff_ms,
                         bool reserve_sending_time_for_next_request) {
    if (!network_task_runner_->RunsTasksInCurrentSequence()) {
      network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FetcherTestURLRequestContextGetter::AddThrottlerEntry,
                         this, url, url_id, sliding_window_period_ms,
                         max_send_threshold, initial_backoff_ms,
                         multiply_factor, jitter_factor, maximum_backoff_ms,
                         reserve_sending_time_for_next_request));
      return;
    }
    scoped_refptr<URLRequestThrottlerEntry> entry(new URLRequestThrottlerEntry(
        GetURLRequestContext()->throttler_manager(), url_id,
        sliding_window_period_ms, max_send_threshold, initial_backoff_ms,
        multiply_factor, jitter_factor, maximum_backoff_ms));

    GetURLRequestContext()->throttler_manager()->OverrideEntryForTests(
        url, entry.get());

    if (reserve_sending_time_for_next_request)
      entry->ReserveSendingTimeForNextRequest(base::TimeTicks());
  }

  // Tells the getter to act as if the URLRequestContext is about to be shut
  // down.
  void Shutdown() {
    if (!network_task_runner_->RunsTasksInCurrentSequence()) {
      network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FetcherTestURLRequestContextGetter::Shutdown, this));
      return;
    }

    shutting_down_ = true;
    NotifyContextShuttingDown();
    // Should now be safe to destroy the context.  Context will check it has no
    // pending requests.
    context_.reset();
  }

  // Convenience method to access the context as a FetcherTestURLRequestContext
  // without going through GetURLRequestContext.
  FetcherTestURLRequestContext* context() {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    return context_.get();
  }

  void set_proxy_resolution_service(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
    DCHECK(proxy_resolution_service);
    proxy_resolution_service_ = std::move(proxy_resolution_service);
  }

 protected:
  ~FetcherTestURLRequestContextGetter() override {
    // |context_| may only be deleted on the network thread. Fortunately,
    // the parent class already ensures it's deleted on the network thread.
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    if (!on_destruction_callback_.is_null())
      std::move(on_destruction_callback_).Run();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  const std::string hanging_domain_;

  // May be null.
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;

  std::unique_ptr<FetcherTestURLRequestContext> context_;
  bool shutting_down_;

  base::OnceClosure on_destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(FetcherTestURLRequestContextGetter);
};

}  // namespace

class URLFetcherTest : public TestWithTaskEnvironment {
 public:
  URLFetcherTest() : num_upload_streams_created_(0) {}

  static int GetNumFetcherCores() {
    return URLFetcherImpl::GetNumFetcherCores();
  }

  // Creates a URLRequestContextGetter with a URLRequestContext that lives on
  // the current thread.
  scoped_refptr<FetcherTestURLRequestContextGetter>
  CreateSameThreadContextGetter() {
    return scoped_refptr<FetcherTestURLRequestContextGetter>(
        new FetcherTestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get(), hanging_url().host()));
  }

  // Creates a URLRequestContextGetter with a URLRequestContext that lives on
  // a separate network thread.
  scoped_refptr<FetcherTestURLRequestContextGetter>
  CreateCrossThreadContextGetter() {
    if (!network_thread_) {
      network_thread_.reset(new base::Thread("network thread"));
      base::Thread::Options network_thread_options;
      network_thread_options.message_pump_type = base::MessagePumpType::IO;
      bool result = network_thread_->StartWithOptions(network_thread_options);
      CHECK(result);
    }

    return scoped_refptr<FetcherTestURLRequestContextGetter>(
        new FetcherTestURLRequestContextGetter(network_thread_->task_runner(),
                                               hanging_url().host()));
  }

  // Callback passed to URLFetcher to create upload stream by some tests.
  std::unique_ptr<UploadDataStream> CreateUploadStream() {
    ++num_upload_streams_created_;
    std::vector<char> buffer(
        kCreateUploadStreamBody,
        kCreateUploadStreamBody + strlen(kCreateUploadStreamBody));
    return ElementsUploadDataStream::CreateWithReader(
        std::unique_ptr<UploadElementReader>(
            new UploadOwnedBytesElementReader(&buffer)),
        0);
  }

  // Number of streams created by CreateUploadStream.
  size_t num_upload_streams_created() const {
    return num_upload_streams_created_;
  }

  // Downloads |file_to_fetch| and checks the contents when done.  If
  // |save_to_temporary_file| is true, saves it to a temporary file, and
  // |requested_out_path| is ignored. Otherwise, saves it to
  // |requested_out_path|. Takes ownership of the file if |take_ownership| is
  // true. Deletes file when done.
  void SaveFileTest(const char* file_to_fetch,
                    bool save_to_temporary_file,
                    const base::FilePath& requested_out_path,
                    bool take_ownership) {
    std::unique_ptr<WaitingURLFetcherDelegate> delegate(
        new WaitingURLFetcherDelegate());
    delegate->CreateFetcher(
        test_server_->GetURL(std::string(kTestServerFilePrefix) +
                             file_to_fetch),
        URLFetcher::GET, CreateSameThreadContextGetter());
    if (save_to_temporary_file) {
      delegate->fetcher()->SaveResponseToTemporaryFile(
          scoped_refptr<base::SequencedTaskRunner>(
              base::SequencedTaskRunnerHandle::Get()));
    } else {
      delegate->fetcher()->SaveResponseToFileAtPath(
          requested_out_path, scoped_refptr<base::SequencedTaskRunner>(
                                  base::SequencedTaskRunnerHandle::Get()));
    }
    delegate->StartFetcherAndWait();

    EXPECT_TRUE(delegate->fetcher()->GetStatus().is_success());
    EXPECT_EQ(200, delegate->fetcher()->GetResponseCode());

    base::FilePath out_path;
    EXPECT_TRUE(
        delegate->fetcher()->GetResponseAsFilePath(take_ownership, &out_path));
    if (!save_to_temporary_file) {
      EXPECT_EQ(requested_out_path, out_path);
    }

    base::FilePath server_root;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &server_root);

    EXPECT_TRUE(base::ContentsEqual(
        server_root.Append(kDocRoot).AppendASCII(file_to_fetch), out_path));

    // Delete the delegate and run the message loop to give the fetcher's
    // destructor a chance to delete the file.
    delegate.reset();
    base::RunLoop().RunUntilIdle();

    // File should only exist if |take_ownership| was true.
    EXPECT_EQ(take_ownership, base::PathExists(out_path));

    // Cleanup.
    if (base::PathExists(out_path))
      base::DeleteFile(out_path, false);
  }

  // Returns a URL that hangs on DNS resolution when using a context created by
  // the test fixture.
  const GURL& hanging_url() const { return hanging_url_; }

  // testing::Test:
  void SetUp() override {
    SetUpServer();
    ASSERT_TRUE(test_server_->Start());

    // URL that will hang when lookups reach the host resolver.
    hanging_url_ = GURL(base::StringPrintf(
        "http://example.com:%d%s", test_server_->host_port_pair().port(),
        kDefaultResponsePath));
    ASSERT_TRUE(hanging_url_.is_valid());
  }

  // Initializes |test_server_| without starting it.  Allows subclasses to use
  // their own server configuration.
  virtual void SetUpServer() {
    test_server_.reset(new EmbeddedTestServer);
    test_server_->AddDefaultHandlers(base::FilePath(kDocRoot));
  }

  // Network thread for cross-thread tests.  Most threads just use the main
  // thread for network activity.
  std::unique_ptr<base::Thread> network_thread_;

  std::unique_ptr<EmbeddedTestServer> test_server_;
  GURL hanging_url_;

  size_t num_upload_streams_created_;
};

namespace {

// Version of URLFetcherTest that tests bad HTTPS requests.
class URLFetcherBadHTTPSTest : public URLFetcherTest {
 public:
  URLFetcherBadHTTPSTest() = default;

  // URLFetcherTest:
  void SetUpServer() override {
    test_server_.reset(
        new EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    test_server_->ServeFilesFromSourceDirectory("net/data/ssl");
  }
};

// Verifies that the fetcher succesfully fetches resources over proxy, and
// correctly returns the value of the proxy server used.
TEST_F(URLFetcherTest, FetchedUsingProxy) {
  WaitingURLFetcherDelegate delegate;

  scoped_refptr<net::FetcherTestURLRequestContextGetter> context_getter =
      CreateSameThreadContextGetter();

  const net::ProxyServer proxy_server(ProxyServer::SCHEME_HTTP,
                                      test_server_->host_port_pair());

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          proxy_server.ToPacString(), TRAFFIC_ANNOTATION_FOR_TESTS);
  context_getter->set_proxy_resolution_service(
      std::move(proxy_resolution_service));

  delegate.CreateFetcher(
      GURL(std::string("http://does.not.resolve.test") + kDefaultResponsePath),
      URLFetcher::GET, context_getter);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kDefaultResponseBody, data);

  EXPECT_EQ(proxy_server, delegate.fetcher()->ProxyServerUsed());
}

// Create the fetcher on the main thread.  Since network IO will happen on the
// main thread, this will test URLFetcher's ability to do everything on one
// thread.
TEST_F(URLFetcherTest, SameThreadTest) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL(kDefaultResponsePath),
                         URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kDefaultResponseBody, data);

  EXPECT_EQ(static_cast<int64_t>(strlen(kDefaultResponseBody)),
            delegate.fetcher()->GetReceivedResponseContentLength());
  std::string parsed_headers;
  base::ReplaceChars(delegate.fetcher()->GetResponseHeaders()->raw_headers(),
                     std::string("\0", 1), "\n\r", &parsed_headers);
  EXPECT_EQ(static_cast<int64_t>(parsed_headers.size() +
                                 strlen(kDefaultResponseBody)),
            delegate.fetcher()->GetTotalReceivedBytes());
  EXPECT_EQ(ProxyServer::SCHEME_DIRECT,
            delegate.fetcher()->ProxyServerUsed().scheme());
}

// Create a separate thread that will create the URLFetcher.  A separate thread
// acts as the network thread.
TEST_F(URLFetcherTest, DifferentThreadsTest) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL(kDefaultResponsePath),
                         URLFetcher::GET, CreateCrossThreadContextGetter());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kDefaultResponseBody, data);
}

// Verifies that a URLFetcher works correctly on a ThreadPool Sequence.
TEST_F(URLFetcherTest, SequencedTaskTest) {
  auto sequenced_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool()});

  // Since we cannot use StartFetchAndWait(), which runs a nested RunLoop owned
  // by the Delegate, in the ThreadPool, this test is split into two Callbacks,
  // both run on |sequenced_task_runner_|. The test main thread then runs its
  // own RunLoop, which the second of the Callbacks will quit.
  base::RunLoop run_loop;

  // Actually start the test fetch, on the Sequence.
  sequenced_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<FetcherTestURLRequestContextGetter> context_getter,
             const GURL& response_path, base::OnceClosure quit_closure) {
            std::unique_ptr<WaitingURLFetcherDelegate> delegate =
                std::make_unique<WaitingURLFetcherDelegate>();
            WaitingURLFetcherDelegate* raw_delegate = delegate.get();

            // Configure the delegate to run our |on_complete_closure_| rather
            // than quitting its own |run_loop_|, on completion.
            raw_delegate->set_on_complete_or_cancel_closure(base::BindOnce(
                [](base::OnceClosure quit_closure,
                   std::unique_ptr<WaitingURLFetcherDelegate> delegate) {
                  EXPECT_TRUE(delegate->fetcher()->GetStatus().is_success());
                  EXPECT_EQ(200, delegate->fetcher()->GetResponseCode());
                  std::string data;
                  ASSERT_TRUE(delegate->fetcher()->GetResponseAsString(&data));
                  EXPECT_EQ(kDefaultResponseBody, data);
                  std::move(quit_closure).Run();
                },
                std::move(quit_closure), base::Passed(&delegate)));

            raw_delegate->CreateFetcher(response_path, URLFetcher::GET,
                                        context_getter);
            raw_delegate->fetcher()->Start();
          },
          CreateCrossThreadContextGetter(),
          test_server_->GetURL(kDefaultResponsePath), run_loop.QuitClosure()));

  run_loop.Run();
  RunUntilIdle();
}

// Tests to make sure CancelAll() will successfully cancel existing URLFetchers.
TEST_F(URLFetcherTest, CancelAll) {
  EXPECT_EQ(0, GetNumFetcherCores());

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());
  // Force context creation.
  context_getter->GetURLRequestContext();
  MockHostResolver* mock_resolver = context_getter->context()->mock_resolver();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate.fetcher()->Start();
  // Wait for the request to reach the mock resolver and hang, to ensure the
  // request has actually started.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_resolver->has_pending_requests());

  EXPECT_EQ(1, URLFetcherTest::GetNumFetcherCores());
  URLFetcherImpl::CancelAll();
  EXPECT_EQ(0, URLFetcherTest::GetNumFetcherCores());
}

TEST_F(URLFetcherTest, DontRetryOnNetworkChangedByDefault) {
  EXPECT_EQ(0, GetNumFetcherCores());

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());
  // Force context creation.
  context_getter->GetURLRequestContext();
  MockHostResolver* mock_resolver = context_getter->context()->mock_resolver();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  EXPECT_FALSE(mock_resolver->has_pending_requests());

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();
  base::RunLoop().RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(mock_resolver->has_pending_requests());
  ASSERT_FALSE(delegate.did_complete());

  // A network change notification aborts the connect job.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  delegate.WaitForComplete();
  EXPECT_FALSE(mock_resolver->has_pending_requests());

  // And the owner of the fetcher gets the ERR_NETWORK_CHANGED error.
  EXPECT_EQ(hanging_url(), delegate.fetcher()->GetOriginalURL());
  ASSERT_FALSE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate.fetcher()->GetStatus().error(),
              IsError(ERR_NETWORK_CHANGED));
}

TEST_F(URLFetcherTest, RetryOnNetworkChangedAndFail) {
  EXPECT_EQ(0, GetNumFetcherCores());

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());
  // Force context creation.
  context_getter->GetURLRequestContext();
  MockHostResolver* mock_resolver = context_getter->context()->mock_resolver();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate.fetcher()->SetAutomaticallyRetryOnNetworkChanges(3);
  EXPECT_FALSE(mock_resolver->has_pending_requests());

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();
  base::RunLoop().RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(mock_resolver->has_pending_requests());
  ASSERT_FALSE(delegate.did_complete());

  // Make it fail 3 times.
  for (int i = 0; i < 3; ++i) {
    // A network change notification aborts the connect job.
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();

    // But the fetcher retries automatically.
    EXPECT_EQ(1, GetNumFetcherCores());
    EXPECT_TRUE(mock_resolver->has_pending_requests());
    ASSERT_FALSE(delegate.did_complete());
  }

  // A 4th failure doesn't trigger another retry, and propagates the error
  // to the owner of the fetcher.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  delegate.WaitForComplete();
  EXPECT_FALSE(mock_resolver->has_pending_requests());

  // And the owner of the fetcher gets the ERR_NETWORK_CHANGED error.
  EXPECT_EQ(hanging_url(), delegate.fetcher()->GetOriginalURL());
  ASSERT_FALSE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate.fetcher()->GetStatus().error(),
              IsError(ERR_NETWORK_CHANGED));
}

TEST_F(URLFetcherTest, RetryOnNetworkChangedAndSucceed) {
  EXPECT_EQ(0, GetNumFetcherCores());

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());
  // Force context creation.
  context_getter->GetURLRequestContext();
  MockHostResolver* mock_resolver = context_getter->context()->mock_resolver();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate.fetcher()->SetAutomaticallyRetryOnNetworkChanges(3);
  EXPECT_FALSE(mock_resolver->has_pending_requests());

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();
  base::RunLoop().RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(mock_resolver->has_pending_requests());
  ASSERT_FALSE(delegate.did_complete());

  // Make it fail 3 times.
  for (int i = 0; i < 3; ++i) {
    // A network change notification aborts the connect job.
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();

    // But the fetcher retries automatically.
    EXPECT_EQ(1, GetNumFetcherCores());
    EXPECT_TRUE(mock_resolver->has_pending_requests());
    ASSERT_FALSE(delegate.did_complete());
  }

  // Now let it succeed by resolving the pending request.
  mock_resolver->ResolveAllPending();
  delegate.WaitForComplete();
  EXPECT_FALSE(mock_resolver->has_pending_requests());

  // This time the request succeeded.
  EXPECT_EQ(hanging_url(), delegate.fetcher()->GetOriginalURL());
  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());

  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kDefaultResponseBody, data);
}

TEST_F(URLFetcherTest, PostString) {
  const char kUploadData[] = "bobsyeruncle";

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateSameThreadContextGetter());
  delegate.fetcher()->SetUploadData("application/x-www-form-urlencoded",
                                    kUploadData);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kUploadData, data);
}

TEST_F(URLFetcherTest, PostEmptyString) {
  const char kUploadData[] = "";

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateSameThreadContextGetter());
  delegate.fetcher()->SetUploadData("application/x-www-form-urlencoded",
                                    kUploadData);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kUploadData, data);
}

TEST_F(URLFetcherTest, PostEntireFile) {
  base::FilePath upload_path = GetUploadFileTestPath();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateSameThreadContextGetter());
  delegate.fetcher()->SetUploadFilePath("application/x-www-form-urlencoded",
                                        upload_path, 0,
                                        std::numeric_limits<uint64_t>::max(),
                                        base::SequencedTaskRunnerHandle::Get());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());

  std::string expected;
  ASSERT_TRUE(base::ReadFileToString(upload_path, &expected));
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(expected, data);
}

TEST_F(URLFetcherTest, PostFileRange) {
  const size_t kRangeStart = 30;
  const size_t kRangeLength = 100;
  base::FilePath upload_path = GetUploadFileTestPath();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateSameThreadContextGetter());
  delegate.fetcher()->SetUploadFilePath("application/x-www-form-urlencoded",
                                        upload_path, kRangeStart, kRangeLength,
                                        base::SequencedTaskRunnerHandle::Get());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());

  std::string expected;
  ASSERT_TRUE(base::ReadFileToString(upload_path, &expected));
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(expected.substr(kRangeStart, kRangeLength), data);
}

TEST_F(URLFetcherTest, PostWithUploadStreamFactory) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateSameThreadContextGetter());
  delegate.fetcher()->SetUploadStreamFactory(
      "text/plain",
      base::Bind(&URLFetcherTest::CreateUploadStream, base::Unretained(this)));
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kCreateUploadStreamBody, data);
  EXPECT_EQ(1u, num_upload_streams_created());
}

TEST_F(URLFetcherTest, PostWithUploadStreamFactoryAndRetries) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo?status=500"),
                         URLFetcher::POST, CreateSameThreadContextGetter());
  delegate.fetcher()->SetAutomaticallyRetryOn5xx(true);
  delegate.fetcher()->SetMaxRetriesOn5xx(1);
  delegate.fetcher()->SetUploadStreamFactory(
      "text/plain",
      base::Bind(&URLFetcherTest::CreateUploadStream, base::Unretained(this)));
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(500, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kCreateUploadStreamBody, data);
  EXPECT_EQ(2u, num_upload_streams_created());
}

// Tests simple chunked POST case.
TEST_F(URLFetcherTest, PostChunked) {
  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateCrossThreadContextGetter());

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateCrossThreadContextGetter());

  delegate.fetcher()->SetChunkedUpload("text/plain");

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();

  delegate.fetcher()->AppendChunkToUpload(kCreateUploadStreamBody, false);
  delegate.fetcher()->AppendChunkToUpload(kCreateUploadStreamBody, true);

  delegate.WaitForComplete();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(std::string(kCreateUploadStreamBody) +
                std::string(kCreateUploadStreamBody),
            data);
}

// Tests that data can be appended to a request after it fails. This is needed
// because the consumer may try to append data to a request after it failed, but
// before the consumer learns that it failed.
TEST_F(URLFetcherTest, PostAppendChunkAfterError) {
  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateCrossThreadContextGetter());

  WaitingURLFetcherDelegate delegate;
  // Request that will fail almost immediately after being started, due to using
  // a reserved port.
  delegate.CreateFetcher(GURL("http://127.0.0.1:7"), URLFetcher::POST,
                         context_getter);

  delegate.fetcher()->SetChunkedUpload("text/plain");

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();

  // Give the request a chance to fail, and inform the fetcher of the failure,
  // while blocking the current thread so the error doesn't reach the delegate.
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // Try to append data.
  delegate.fetcher()->AppendChunkToUpload("kCreateUploadStreamBody", false);
  delegate.fetcher()->AppendChunkToUpload("kCreateUploadStreamBody", true);

  delegate.WaitForComplete();

  // Make sure the request failed, as expected.
  EXPECT_FALSE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate.fetcher()->GetStatus().error(),
              IsError(ERR_UNSAFE_PORT));
}

// Checks that upload progress increases over time, never exceeds what's already
// been sent, and adds a chunk whenever all previously appended chunks have
// been uploaded.
class CheckUploadProgressDelegate : public WaitingURLFetcherDelegate {
 public:
  CheckUploadProgressDelegate()
      : chunk_(1 << 16, 'a'), num_chunks_appended_(0), last_seen_progress_(0) {}
  ~CheckUploadProgressDelegate() override = default;

  void OnURLFetchUploadProgress(const URLFetcher* source,
                                int64_t current,
                                int64_t total) override {
    // Run default checks.
    WaitingURLFetcherDelegate::OnURLFetchUploadProgress(source, current, total);

    EXPECT_LE(last_seen_progress_, current);
    EXPECT_LE(current, bytes_appended());
    last_seen_progress_ = current;
    MaybeAppendChunk();
  }

  // Append the next chunk if all previously appended chunks have been sent.
  void MaybeAppendChunk() {
    const int kNumChunks = 5;
    if (last_seen_progress_ == bytes_appended() &&
        num_chunks_appended_ < kNumChunks) {
      ++num_chunks_appended_;
      fetcher()->AppendChunkToUpload(chunk_,
                                     num_chunks_appended_ == kNumChunks);
    }
  }

 private:
  int64_t bytes_appended() const {
    return num_chunks_appended_ * chunk_.size();
  }

  const std::string chunk_;

  int64_t num_chunks_appended_;
  int64_t last_seen_progress_;

  DISALLOW_COPY_AND_ASSIGN(CheckUploadProgressDelegate);
};

TEST_F(URLFetcherTest, UploadProgress) {
  CheckUploadProgressDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateSameThreadContextGetter());
  // Use a chunked upload so that the upload can be paused after uploading data.
  // Since upload progress uses a timer, the delegate may not receive any
  // notification otherwise.
  delegate.fetcher()->SetChunkedUpload("application/x-www-form-urlencoded");

  delegate.fetcher()->Start();
  // Append the first chunk.  Others will be appended automatically in response
  // to OnURLFetchUploadProgress events.
  delegate.MaybeAppendChunk();
  delegate.WaitForComplete();

  // Make sure there are no pending events that cause problems when run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  EXPECT_TRUE(delegate.did_complete());
}

// Checks that download progress never decreases, never exceeds file size, and
// that file size is correctly reported.
class CheckDownloadProgressDelegate : public WaitingURLFetcherDelegate {
 public:
  CheckDownloadProgressDelegate(int64_t file_size)
      : file_size_(file_size), last_seen_progress_(0) {}
  ~CheckDownloadProgressDelegate() override = default;

  void OnURLFetchDownloadProgress(const URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override {
    // Run default checks.
    WaitingURLFetcherDelegate::OnURLFetchDownloadProgress(
        source, current, total, current_network_bytes);

    EXPECT_LE(last_seen_progress_, current);
    EXPECT_EQ(file_size_, total);
    last_seen_progress_ = current;
  }

 private:
  int64_t file_size_;
  int64_t last_seen_progress_;

  DISALLOW_COPY_AND_ASSIGN(CheckDownloadProgressDelegate);
};

TEST_F(URLFetcherTest, DownloadProgress) {
  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  const char kFileToFetch[] = "animate1.gif";

  std::string file_contents;

  base::FilePath server_root;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &server_root);

  ASSERT_TRUE(base::ReadFileToString(
      server_root.Append(kDocRoot).AppendASCII(kFileToFetch), &file_contents));

  CheckDownloadProgressDelegate delegate(file_contents.size());
  delegate.CreateFetcher(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(file_contents, data);
}

class CancelOnUploadProgressDelegate : public WaitingURLFetcherDelegate {
 public:
  CancelOnUploadProgressDelegate() = default;
  ~CancelOnUploadProgressDelegate() override = default;

  void OnURLFetchUploadProgress(const URLFetcher* source,
                                int64_t current,
                                int64_t total) override {
    CancelFetch();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelOnUploadProgressDelegate);
};

// Check that a fetch can be safely cancelled/deleted during an upload progress
// callback.
TEST_F(URLFetcherTest, CancelInUploadProgressCallback) {
  CancelOnUploadProgressDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL("/echo"), URLFetcher::POST,
                         CreateSameThreadContextGetter());
  delegate.fetcher()->SetChunkedUpload("application/x-www-form-urlencoded");
  delegate.fetcher()->Start();
  // Use a chunked upload so that the upload can be paused after uploading data.
  // Since uploads progress uses a timer, may not receive any notification,
  // otherwise.
  std::string upload_data(1 << 16, 'a');
  delegate.fetcher()->AppendChunkToUpload(upload_data, false);
  delegate.WaitForComplete();

  // Make sure there are no pending events that cause problems when run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate.did_complete());
  EXPECT_FALSE(delegate.fetcher());
}

class CancelOnDownloadProgressDelegate : public WaitingURLFetcherDelegate {
 public:
  CancelOnDownloadProgressDelegate() = default;
  ~CancelOnDownloadProgressDelegate() override = default;

  void OnURLFetchDownloadProgress(const URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override {
    CancelFetch();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelOnDownloadProgressDelegate);
};

// Check that a fetch can be safely cancelled/deleted during a download progress
// callback.
TEST_F(URLFetcherTest, CancelInDownloadProgressCallback) {
  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  CancelOnDownloadProgressDelegate delegate;
  delegate.CreateFetcher(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.StartFetcherAndWait();

  // Make sure there are no pending events that cause problems when run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate.did_complete());
  EXPECT_FALSE(delegate.fetcher());
}

TEST_F(URLFetcherTest, Headers) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(
      test_server_->GetURL("/set-header?cache-control: private"),
      URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string header;
  ASSERT_TRUE(delegate.fetcher()->GetResponseHeaders()->GetNormalizedHeader(
      "cache-control", &header));
  EXPECT_EQ("private", header);
}

TEST_F(URLFetcherTest, SocketAddress) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL(kDefaultResponsePath),
                         URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  EXPECT_EQ(test_server_->host_port_pair().port(),
            delegate.fetcher()->GetSocketAddress().port());
  EXPECT_EQ(test_server_->host_port_pair().host(),
            delegate.fetcher()->GetSocketAddress().ToStringWithoutPort());
}

TEST_F(URLFetcherTest, StopOnRedirect) {
  const char kRedirectTarget[] = "http://redirect.target.com";

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(
      test_server_->GetURL(std::string("/server-redirect?") + kRedirectTarget),
      URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.fetcher()->SetStopOnRedirect(true);
  delegate.StartFetcherAndWait();

  EXPECT_EQ(GURL(kRedirectTarget), delegate.fetcher()->GetURL());
  EXPECT_EQ(URLRequestStatus::CANCELED,
            delegate.fetcher()->GetStatus().status());
  EXPECT_THAT(delegate.fetcher()->GetStatus().error(), IsError(ERR_ABORTED));
  EXPECT_EQ(301, delegate.fetcher()->GetResponseCode());
}

TEST_F(URLFetcherTest, ThrottleOnRepeatedFetches) {
  base::Time start_time = Time::Now();
  GURL url(test_server_->GetURL(kDefaultResponsePath));

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  context_getter->AddThrottlerEntry(
      url, std::string() /* url_id */, 200 /* sliding_window_period_ms */,
      3 /* max_send_threshold */, 1 /* initial_backoff_ms */,
      2.0 /* multiply_factor */, 0.0 /* jitter_factor */,
      256 /* maximum_backoff_ms */,
      false /* reserve_sending_time_for_next_request*/);

  for (int i = 0; i < 20; ++i) {
    WaitingURLFetcherDelegate delegate;
    delegate.CreateFetcher(url, URLFetcher::GET, context_getter);
    delegate.StartFetcherAndWait();

    EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
    EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  }

  // 20 requests were sent. Due to throttling, they should have collectively
  // taken over 1 second.
  EXPECT_GE(Time::Now() - start_time, base::TimeDelta::FromSeconds(1));
}

// If throttling kicks in for a chunked upload, there should be no crash.
TEST_F(URLFetcherTest, ThrottleChunkedUpload) {
  GURL url(test_server_->GetURL("/echo"));

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  context_getter->AddThrottlerEntry(
      url, std::string() /* url_id */, 200 /* sliding_window_period_ms */,
      3 /* max_send_threshold */, 1 /* initial_backoff_ms */,
      2.0 /* multiply_factor */, 0.0 /* jitter_factor */,
      256 /* maximum_backoff_ms */,
      false /* reserve_sending_time_for_next_request*/);

  for (int i = 0; i < 20; ++i) {
    WaitingURLFetcherDelegate delegate;
    delegate.CreateFetcher(url, URLFetcher::POST, context_getter);
    delegate.fetcher()->SetChunkedUpload("text/plain");
    delegate.fetcher()->Start();
    delegate.fetcher()->AppendChunkToUpload(kCreateUploadStreamBody, true);
    delegate.WaitForComplete();

    EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
    EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
    std::string data;
    ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
    EXPECT_EQ(kCreateUploadStreamBody, data);
  }
}

TEST_F(URLFetcherTest, ThrottleOn5xxRetries) {
  base::Time start_time = Time::Now();
  GURL url(test_server_->GetURL("/server-unavailable.html"));

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 256 milliseconds.
  // Maximum retries allowed is set to 11.
  context_getter->AddThrottlerEntry(
      url, std::string() /* url_id */, 200 /* sliding_window_period_ms */,
      3 /* max_send_threshold */, 1 /* initial_backoff_ms */,
      2.0 /* multiply_factor */, 0.0 /* jitter_factor */,
      256 /* maximum_backoff_ms */,
      false /* reserve_sending_time_for_next_request*/);

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(url, URLFetcher::GET, context_getter);
  delegate.fetcher()->SetAutomaticallyRetryOn5xx(true);
  delegate.fetcher()->SetMaxRetriesOn5xx(11);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(503, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());

  // The request should have been retried 11 times (12 times including the first
  // attempt).  Due to throttling, they should have collectively taken over 1
  // second.
  EXPECT_GE(Time::Now() - start_time, base::TimeDelta::FromSeconds(1));
}

// Tests overload protection, when responses passed through.
TEST_F(URLFetcherTest, ProtectTestPassedThrough) {
  base::Time start_time = Time::Now();
  GURL url(test_server_->GetURL("/server-unavailable.html"));

  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 150000 milliseconds.
  // Maximum retries allowed is set to 11.
  // Total time if *not* for not doing automatic backoff would be 150s.
  // In reality it should be "as soon as server responds".
  context_getter->AddThrottlerEntry(
      url, std::string() /* url_id */, 200 /* sliding_window_period_ms */,
      3 /* max_send_threshold */, 10000 /* initial_backoff_ms */,
      2.0 /* multiply_factor */, 0.0 /* jitter_factor */,
      150000 /* maximum_backoff_ms */,
      false /* reserve_sending_time_for_next_request*/);

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(url, URLFetcher::GET, context_getter);
  delegate.fetcher()->SetAutomaticallyRetryOn5xx(false);
  delegate.fetcher()->SetMaxRetriesOn5xx(11);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(503, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());
  EXPECT_GT(delegate.fetcher()->GetBackoffDelay().InMicroseconds(), 0);

  // The request should not have been retried at all.  If it had attempted all
  // 11 retries, that should have taken 2.5 minutes.
  EXPECT_TRUE(Time::Now() - start_time < TimeDelta::FromMinutes(1));
}

// Used to check if a callback has been invoked.
void SetBoolToTrue(bool* ptr) {
  *ptr = true;
}

// Make sure that the URLFetcher cancels the URLRequest and releases its context
// getter pointer synchronously when the fetcher and request context live on
// the same thread.
TEST_F(URLFetcherTest, CancelSameThread) {
  WaitingURLFetcherDelegate delegate;
  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());
  bool getter_was_destroyed = false;
  context_getter->set_on_destruction_callback(
      base::BindOnce(&SetBoolToTrue, &getter_was_destroyed));
  delegate.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);

  // The getter won't be destroyed if the test holds on to a reference to it.
  context_getter = nullptr;

  delegate.fetcher()->Start();
  // Give the fetcher a chance to start the request.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, URLFetcherTest::GetNumFetcherCores());

  // On same-thread cancel, the request should be canceled and getter destroyed
  // synchronously, for safe shutdown.
  delegate.CancelFetch();
  EXPECT_EQ(0, URLFetcherTest::GetNumFetcherCores());
  EXPECT_TRUE(getter_was_destroyed);
}

// Make sure that the URLFetcher releases its context getter pointer on
// cancellation, cross-thread case.
TEST_F(URLFetcherTest, CancelDifferentThreads) {
  base::RunLoop run_loop;

  WaitingURLFetcherDelegate delegate;
  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateCrossThreadContextGetter());
  context_getter->set_on_destruction_callback(
      base::BindOnce(base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
                     base::SequencedTaskRunnerHandle::Get(), FROM_HERE,
                     run_loop.QuitClosure()));
  delegate.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);

  // The getter won't be destroyed if the test holds on to a reference to it.
  context_getter = nullptr;

  delegate.fetcher()->Start();
  delegate.CancelFetch();
  run_loop.Run();

  EXPECT_FALSE(delegate.did_complete());
}

TEST_F(URLFetcherTest, CancelWhileDelayedByThrottleDifferentThreads) {
  GURL url = test_server_->GetURL(kDefaultResponsePath);
  base::RunLoop run_loop;

  WaitingURLFetcherDelegate delegate;
  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateCrossThreadContextGetter());
  context_getter->set_on_destruction_callback(
      base::BindOnce(base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
                     base::SequencedTaskRunnerHandle::Get(), FROM_HERE,
                     run_loop.QuitClosure()));
  delegate.CreateFetcher(url, URLFetcher::GET, context_getter);

  // Register an entry for test url using a sliding window of 400 seconds, and
  // max of 1 request.  Also simulate a request having just started, so the
  // next request will be affected by backoff of ~400 seconds.
  context_getter->AddThrottlerEntry(
      url, std::string() /* url_id */, 400000 /* sliding_window_period_ms */,
      1 /* max_send_threshold */, 200000 /* initial_backoff_ms */,
      2.0 /* multiply_factor */, 0.0 /* jitter_factor */,
      400000 /* maximum_backoff_ms */,
      true /* reserve_sending_time_for_next_request*/);

  // The getter won't be destroyed if the test holds on to a reference to it.
  context_getter = nullptr;

  delegate.fetcher()->Start();
  delegate.CancelFetch();
  run_loop.Run();

  EXPECT_FALSE(delegate.did_complete());
}

// A URLFetcherDelegate that expects to receive a response body of "request1"
// and then reuses the fetcher for the same URL, setting the "test" request
// header to "request2".
class ReuseFetcherDelegate : public WaitingURLFetcherDelegate {
 public:
  // |second_request_context_getter| is the context getter used for the second
  // request. Can't reuse the old one because fetchers release it on completion.
  ReuseFetcherDelegate(
      scoped_refptr<URLRequestContextGetter> second_request_context_getter)
      : first_request_complete_(false),
        second_request_context_getter_(second_request_context_getter) {}

  ~ReuseFetcherDelegate() override = default;

  void OnURLFetchComplete(const URLFetcher* source) override {
    EXPECT_EQ(fetcher(), source);
    if (!first_request_complete_) {
      first_request_complete_ = true;
      EXPECT_TRUE(fetcher()->GetStatus().is_success());
      EXPECT_EQ(200, fetcher()->GetResponseCode());
      std::string data;
      ASSERT_TRUE(fetcher()->GetResponseAsString(&data));
      EXPECT_EQ("request1", data);

      fetcher()->SetRequestContext(second_request_context_getter_.get());
      fetcher()->SetExtraRequestHeaders("test: request2");
      fetcher()->Start();
      return;
    }
    WaitingURLFetcherDelegate::OnURLFetchComplete(source);
  }

 private:
  bool first_request_complete_;
  scoped_refptr<URLRequestContextGetter> second_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ReuseFetcherDelegate);
};

TEST_F(URLFetcherTest, ReuseFetcherForSameURL) {
  // TODO(mmenke):  It's really weird that this is supported, particularly
  // some fields can be modified between requests, but some (Like upload body)
  // cannot be. Can we get rid of support for this?
  scoped_refptr<URLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());
  ReuseFetcherDelegate delegate(context_getter);
  delegate.CreateFetcher(test_server_->GetURL("/echoheader?test"),
                         URLFetcher::GET, context_getter);
  delegate.fetcher()->SetExtraRequestHeaders("test: request1");
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ("request2", data);
}

TEST_F(URLFetcherTest, ShutdownSameThread) {
  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateSameThreadContextGetter());

  // Create a fetcher and wait for it to create a request.
  WaitingURLFetcherDelegate delegate1;
  delegate1.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate1.fetcher()->Start();
  // Need to spin the loop to ensure the URLRequest is created and started.
  base::RunLoop().RunUntilIdle();

  // Create and start another fetcher, but don't wait for it to start. The task
  // to start the request should be in the message loop.
  WaitingURLFetcherDelegate delegate2;
  delegate2.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate2.fetcher()->Start();

  // Check that shutting down the getter cancels the request synchronously,
  // allowing the context to be destroyed.
  context_getter->Shutdown();

  // Wait for the first fetcher, make sure it failed.
  delegate1.WaitForComplete();
  EXPECT_FALSE(delegate1.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate1.fetcher()->GetStatus().error(),
              IsError(ERR_CONTEXT_SHUT_DOWN));

  // Wait for the second fetcher, make sure it failed.
  delegate2.WaitForComplete();
  EXPECT_FALSE(delegate2.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate2.fetcher()->GetStatus().error(),
              IsError(ERR_CONTEXT_SHUT_DOWN));

  // New fetchers should automatically fail without making new requests. This
  // should follow the same path as the second fetcher, but best to be safe.
  WaitingURLFetcherDelegate delegate3;
  delegate3.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate3.fetcher()->Start();
  delegate3.WaitForComplete();
  EXPECT_FALSE(delegate3.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate3.fetcher()->GetStatus().error(),
              IsError(ERR_CONTEXT_SHUT_DOWN));
}

TEST_F(URLFetcherTest, ShutdownCrossThread) {
  scoped_refptr<FetcherTestURLRequestContextGetter> context_getter(
      CreateCrossThreadContextGetter());

  WaitingURLFetcherDelegate delegate1;
  delegate1.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate1.fetcher()->Start();
  // Check that shutting the context getter lets the context be destroyed safely
  // and cancels the request.
  context_getter->Shutdown();
  delegate1.WaitForComplete();
  EXPECT_FALSE(delegate1.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate1.fetcher()->GetStatus().error(),
              IsError(ERR_CONTEXT_SHUT_DOWN));

  // New requests should automatically fail without making new requests.
  WaitingURLFetcherDelegate delegate2;
  delegate2.CreateFetcher(hanging_url(), URLFetcher::GET, context_getter);
  delegate2.StartFetcherAndWait();
  EXPECT_FALSE(delegate2.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate2.fetcher()->GetStatus().error(),
              IsError(ERR_CONTEXT_SHUT_DOWN));
}

// Get a small file.
TEST_F(URLFetcherTest, FileTestSmallGet) {
  const char kFileToFetch[] = "simple.html";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath out_path = temp_dir.GetPath().AppendASCII(kFileToFetch);
  SaveFileTest(kFileToFetch, false, out_path, false);
}

// Get a file large enough to require more than one read into URLFetcher::Core's
// IOBuffer.
TEST_F(URLFetcherTest, FileTestLargeGet) {
  const char kFileToFetch[] = "animate1.gif";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath out_path = temp_dir.GetPath().AppendASCII(kFileToFetch);
  SaveFileTest(kFileToFetch, false, out_path, false);
}

// If the caller takes the ownership of the output file, the file should persist
// even after URLFetcher is gone.
TEST_F(URLFetcherTest, FileTestTakeOwnership) {
  const char kFileToFetch[] = "simple.html";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath out_path = temp_dir.GetPath().AppendASCII(kFileToFetch);
  SaveFileTest(kFileToFetch, false, out_path, true);
}

// Test that an existing file can be overwritten be a fetcher.
TEST_F(URLFetcherTest, FileTestOverwriteExisting) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a file before trying to fetch.
  const char kFileToFetch[] = "simple.html";
  std::string data(10000, '?');  // Meant to be larger than simple.html.
  base::FilePath out_path = temp_dir.GetPath().AppendASCII(kFileToFetch);
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(out_path, data.data(), data.size()));
  ASSERT_TRUE(base::PathExists(out_path));

  SaveFileTest(kFileToFetch, false, out_path, true);
}

// Test trying to overwrite a directory with a file when using a fetcher fails.
TEST_F(URLFetcherTest, FileTestTryToOverwriteDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a directory before trying to fetch.
  static const char kFileToFetch[] = "simple.html";
  base::FilePath out_path = temp_dir.GetPath().AppendASCII(kFileToFetch);
  ASSERT_TRUE(base::CreateDirectory(out_path));
  ASSERT_TRUE(base::PathExists(out_path));

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.fetcher()->SaveResponseToFileAtPath(
      out_path, scoped_refptr<base::SequencedTaskRunner>(
                    base::SequencedTaskRunnerHandle::Get()));
  delegate.StartFetcherAndWait();

  EXPECT_FALSE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_THAT(delegate.fetcher()->GetStatus().error(),
              IsError(ERR_ACCESS_DENIED));
}

// Get a small file and save it to a temp file.
TEST_F(URLFetcherTest, TempFileTestSmallGet) {
  SaveFileTest("simple.html", true, base::FilePath(), false);
}

// Get a file large enough to require more than one read into URLFetcher::Core's
// IOBuffer and save it to a temp file.
TEST_F(URLFetcherTest, TempFileTestLargeGet) {
  SaveFileTest("animate1.gif", true, base::FilePath(), false);
}

// If the caller takes the ownership of the temp file, check that the file
// persists even after URLFetcher is gone.
TEST_F(URLFetcherTest, TempFileTestTakeOwnership) {
  SaveFileTest("simple.html", true, base::FilePath(), true);
}

TEST_F(URLFetcherBadHTTPSTest, BadHTTPS) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcher(test_server_->GetURL(kDefaultResponsePath),
                         URLFetcher::GET, CreateSameThreadContextGetter());
  delegate.StartFetcherAndWait();

  EXPECT_EQ(URLRequestStatus::CANCELED,
            delegate.fetcher()->GetStatus().status());
  EXPECT_THAT(delegate.fetcher()->GetStatus().error(), IsError(ERR_ABORTED));
  EXPECT_EQ(-1, delegate.fetcher()->GetResponseCode());
  EXPECT_FALSE(delegate.fetcher()->GetResponseHeaders());
  std::string data;
  EXPECT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_TRUE(data.empty());
}

}  // namespace

}  // namespace net
