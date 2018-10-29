// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_info.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/test/test_data_pipe_getter.h"
#include "services/network/test/test_network_service_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test_chunked_data_pipe_getter.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_owner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

// Returns a URLLoader::DeleteCallback that destroys |url_loader| and quits
// |run_loop| when invoked. Tests must wait on the RunLoop to ensure nothing is
// leaked. Takes a std::unique_ptr<URLLoader>* instead of a unique_ptr because
// the callback must be created before the URLLoader is actually created.
URLLoader::DeleteCallback DeleteLoaderCallback(
    base::RunLoop* run_loop,
    std::unique_ptr<URLLoader>* url_loader) {
  return base::BindOnce(
      [](base::RunLoop* run_loop, std::unique_ptr<URLLoader>* url_loader,
         mojom::URLLoader* url_loader_ptr) {
        DCHECK_EQ(url_loader->get(), url_loader_ptr);
        url_loader->reset();
        run_loop->Quit();
      },
      run_loop, url_loader);
}

// Returns a URLLoader::DeleteCallback that does nothing, but calls NOTREACHED.
// Tests that use a URLLoader that actually tries to delete itself shouldn't use
// this method, as URLLoaders don't expect to be alive after they invoke their
// delete callback.
URLLoader::DeleteCallback NeverInvokedDeleteLoaderCallback() {
  return base::BindOnce([](mojom::URLLoader* /* loader*/) { NOTREACHED(); });
}

constexpr char kBodyReadFromNetBeforePausedHistogram[] =
    "Network.URLLoader.BodyReadFromNetBeforePaused";

constexpr char kTestAuthURL[] = "/auth-basic?password=PASS&realm=REALM";

static ResourceRequest CreateResourceRequest(const char* method,
                                             const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.site_for_cookies = url;  // bypass third-party cookie blocking
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator is set
  request.is_main_frame = true;
  request.allow_download = true;
  return request;
}

class URLRequestMultipleWritesJob : public net::URLRequestJob {
 public:
  URLRequestMultipleWritesJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate,
                              std::list<std::string> packets,
                              net::Error net_error,
                              bool async_reads)
      : URLRequestJob(request, network_delegate),
        packets_(std::move(packets)),
        net_error_(net_error),
        async_reads_(async_reads),
        weak_factory_(this) {}

  // net::URLRequestJob implementation:
  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestMultipleWritesJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    int result;
    if (packets_.empty()) {
      result = net_error_;
    } else {
      std::string packet = packets_.front();
      packets_.pop_front();
      CHECK_GE(buf_size, static_cast<int>(packet.length()));
      memcpy(buf->data(), packet.c_str(), packet.length());
      result = packet.length();
    }

    if (async_reads_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&URLRequestMultipleWritesJob::ReadRawDataComplete,
                         weak_factory_.GetWeakPtr(), result));
      return net::ERR_IO_PENDING;
    }
    return result;
  }

 private:
  ~URLRequestMultipleWritesJob() override {}

  void StartAsync() { NotifyHeadersComplete(); }

  std::list<std::string> packets_;
  net::Error net_error_;
  bool async_reads_;

  base::WeakPtrFactory<URLRequestMultipleWritesJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMultipleWritesJob);
};

class MultipleWritesInterceptor : public net::URLRequestInterceptor {
 public:
  MultipleWritesInterceptor(std::list<std::string> packets,
                            net::Error net_error,
                            bool async_reads)
      : packets_(std::move(packets)),
        net_error_(net_error),
        async_reads_(async_reads) {}
  ~MultipleWritesInterceptor() override {}

  static GURL GetURL() { return GURL("http://foo"); }

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestMultipleWritesJob(request, network_delegate,
                                           std::move(packets_), net_error_,
                                           async_reads_);
  }

 private:
  std::list<std::string> packets_;
  net::Error net_error_;
  bool async_reads_;

  DISALLOW_COPY_AND_ASSIGN(MultipleWritesInterceptor);
};

// Every read completes synchronously.
class URLRequestEternalSyncReadsJob : public net::URLRequestJob {
 public:
  // If |fill_entire_buffer| is true, each read fills the entire read buffer at
  // once. Otherwise, one byte is read at a time.
  URLRequestEternalSyncReadsJob(net::URLRequest* request,
                                net::NetworkDelegate* network_delegate,
                                bool fill_entire_buffer)
      : URLRequestJob(request, network_delegate),
        fill_entire_buffer_(fill_entire_buffer),
        weak_factory_(this) {}

  // net::URLRequestJob implementation:
  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestEternalSyncReadsJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    DCHECK_GT(buf_size, 0);
    if (fill_entire_buffer_) {
      memset(buf->data(), 'a', buf_size);
      return buf_size;
    }

    buf->data()[0] = 'a';
    return 1;
  }

 private:
  ~URLRequestEternalSyncReadsJob() override {}

  void StartAsync() { NotifyHeadersComplete(); }

  const bool fill_entire_buffer_;

  base::WeakPtrFactory<URLRequestEternalSyncReadsJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestEternalSyncReadsJob);
};

class EternalSyncReadsInterceptor : public net::URLRequestInterceptor {
 public:
  EternalSyncReadsInterceptor() {}
  ~EternalSyncReadsInterceptor() override {}

  static std::string GetHostName() { return "eternal"; }

  static GURL GetSingleByteURL() { return GURL("http://eternal/single-byte"); }
  static GURL GetFillBufferURL() { return GURL("http://eternal/fill-buffer"); }

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    if (request->url() == GetSingleByteURL()) {
      return new URLRequestEternalSyncReadsJob(request, network_delegate,
                                               false /* fill_entire_buffer */);
    }
    if (request->url() == GetFillBufferURL()) {
      return new URLRequestEternalSyncReadsJob(request, network_delegate,
                                               true /* fill_entire_buffer */);
    }
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EternalSyncReadsInterceptor);
};

// Simulates handing over things to the disk to write before returning to the
// caller.
class URLRequestSimulatedCacheJob : public net::URLRequestJob {
 public:
  // If |fill_entire_buffer| is true, each read fills the entire read buffer at
  // once. Otherwise, one byte is read at a time.
  URLRequestSimulatedCacheJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      scoped_refptr<net::IOBuffer>* simulated_cache_dest)
      : URLRequestJob(request, network_delegate),
        simulated_cache_dest_(simulated_cache_dest),
        weak_factory_(this) {}

  // net::URLRequestJob implementation:
  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestSimulatedCacheJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    DCHECK_GT(buf_size, 0);

    // Pretend this is the entire network stack, which has sent the buffer
    // to some worker thread to be written to disk.
    memset(buf->data(), 'a', buf_size);
    *simulated_cache_dest_ = buf;

    // The network stack will not report the read result until the write
    // completes.
    return net::ERR_IO_PENDING;
  }

 private:
  ~URLRequestSimulatedCacheJob() override {}
  void StartAsync() { NotifyHeadersComplete(); }

  scoped_refptr<net::IOBuffer>* simulated_cache_dest_;
  base::WeakPtrFactory<URLRequestSimulatedCacheJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestSimulatedCacheJob);
};

class SimulatedCacheInterceptor : public net::URLRequestInterceptor {
 public:
  explicit SimulatedCacheInterceptor(
      scoped_refptr<net::IOBuffer>* simulated_cache_dest)
      : simulated_cache_dest_(simulated_cache_dest) {}

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestSimulatedCacheJob(request, network_delegate,
                                           simulated_cache_dest_);
  }

 private:
  scoped_refptr<net::IOBuffer>* simulated_cache_dest_;
  DISALLOW_COPY_AND_ASSIGN(SimulatedCacheInterceptor);
};

// Returns whether monitoring was successfully set up. If yes,
// StopMonitorBodyReadFromNetBeforePausedHistogram() needs to be called later to
// stop monitoring.
//
// |*output_sample| needs to stay valid until monitoring is stopped.
WARN_UNUSED_RESULT bool StartMonitorBodyReadFromNetBeforePausedHistogram(
    const base::RepeatingClosure& quit_closure,
    base::HistogramBase::Sample* output_sample) {
  return base::StatisticsRecorder::SetCallback(
      kBodyReadFromNetBeforePausedHistogram,
      base::BindRepeating(
          [](const base::RepeatingClosure& quit_closure,
             base::HistogramBase::Sample* output,
             base::HistogramBase::Sample sample) {
            *output = sample;
            quit_closure.Run();
          },
          quit_closure, output_sample));
}

void StopMonitorBodyReadFromNetBeforePausedHistogram() {
  base::StatisticsRecorder::ClearCallback(
      kBodyReadFromNetBeforePausedHistogram);
}

}  // namespace

class URLLoaderTest : public testing::Test {
 public:
  URLLoaderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        resource_scheduler_(true) {
    net::URLRequestContextBuilder context_builder;
    context_builder.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateDirect());
    context_ = context_builder.Build();
    resource_scheduler_client_ = base::MakeRefCounted<ResourceSchedulerClient>(
        kProcessId, kRouteId, &resource_scheduler_,
        context_->network_quality_estimator());
    net::URLRequestFailedJob::AddUrlHandler();
  }
  ~URLLoaderTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void SetUp() override {
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("services/test/data")));
    // This Unretained is safe because test_server_ is owned by |this|.
    test_server_.RegisterRequestMonitor(
        base::Bind(&URLLoaderTest::Monitor, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  // Attempts to load |url| and returns the resulting error code. If |body| is
  // non-NULL, also attempts to read the response body. The advantage of using
  // |body| instead of calling ReadBody() after Load is that it will load the
  // response body before URLLoader is complete, so URLLoader completion won't
  // block on trying to write the body buffer.
  int Load(const GURL& url, std::string* body = nullptr) WARN_UNUSED_RESULT {
    DCHECK(!ran_);

    ResourceRequest request =
        CreateResourceRequest(!request_body_ ? "GET" : "POST", url);
    uint32_t options = mojom::kURLLoadOptionNone;
    if (send_ssl_with_response_)
      options |= mojom::kURLLoadOptionSendSSLInfoWithResponse;
    if (sniff_)
      options |= mojom::kURLLoadOptionSniffMimeType;
    if (send_ssl_for_cert_error_)
      options |= mojom::kURLLoadOptionSendSSLInfoForCertificateError;

    std::unique_ptr<TestNetworkServiceClient> network_service_client;
    if (allow_file_uploads_)
      network_service_client = std::make_unique<TestNetworkServiceClient>();

    if (request_body_)
      request.request_body = request_body_;

    base::RunLoop delete_run_loop;
    mojom::URLLoaderPtr loader;
    std::unique_ptr<URLLoader> url_loader;
    static mojom::URLLoaderFactoryParams params;
    params.process_id = mojom::kBrowserProcessId;
    params.is_corb_enabled = false;
    url_loader = std::make_unique<URLLoader>(
        context(), network_service_client.get(),
        DeleteLoaderCallback(&delete_run_loop, &url_loader),
        mojo::MakeRequest(&loader), options, request, false,
        client_.CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
        0 /* request_id */, resource_scheduler_client(), nullptr,
        nullptr /* network_usage_accumulator */);

    ran_ = true;

    if (expect_redirect_) {
      client_.RunUntilRedirectReceived();
      loader->FollowRedirect(base::nullopt, base::nullopt);
    }

    if (body) {
      client_.RunUntilResponseBodyArrived();
      *body = ReadBody();
    }

    client_.RunUntilComplete();
    if (body) {
      EXPECT_EQ(body->size(),
                static_cast<size_t>(
                    client()->completion_status().decoded_body_length));
    }

    delete_run_loop.Run();

    return client_.completion_status().error_code;
  }

  void LoadAndCompareFile(const std::string& path) {
    base::FilePath file = GetTestFilePath(path);

    std::string expected;
    if (!base::ReadFileToString(file, &expected)) {
      ADD_FAILURE() << "File not found: " << file.value();
      return;
    }

    std::string body;
    EXPECT_EQ(net::OK,
              Load(test_server()->GetURL(std::string("/") + path), &body));
    EXPECT_EQ(expected, body);
    // The file isn't compressed, so both encoded and decoded body lengths
    // should match the read body length.
    EXPECT_EQ(
        expected.size(),
        static_cast<size_t>(client()->completion_status().decoded_body_length));
    EXPECT_EQ(
        expected.size(),
        static_cast<size_t>(client()->completion_status().encoded_body_length));
    // Over the wire length should include headers, so should be longer.
    // TODO(mmenke): Worth adding better tests for encoded_data_length?
    EXPECT_LT(
        expected.size(),
        static_cast<size_t>(client()->completion_status().encoded_data_length));
  }

  // Adds a MultipleWritesInterceptor for MultipleWritesInterceptor::GetURL()
  // that results in seeing each element of |packets| read individually, and
  // then a final read that returns |net_error|. The URLRequestInterceptor is
  // not removed from URLFilter until the test fixture is torn down.
  // |async_reads| indicates whether all reads (including those for |packets|
  // and |net_error|) complete asynchronously or not.
  void AddMultipleWritesInterceptor(std::list<std::string> packets,
                                    net::Error net_error,
                                    bool async_reads) {
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        MultipleWritesInterceptor::GetURL(),
        std::unique_ptr<net::URLRequestInterceptor>(
            new MultipleWritesInterceptor(std::move(packets), net_error,
                                          async_reads)));
  }

  // Adds an EternalSyncReadsInterceptor for
  // EternalSyncReadsInterceptor::GetURL(), which creates URLRequestJobs where
  // all reads return a sync byte that's read synchronously.
  void AddEternalSyncReadsInterceptor() {
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", EternalSyncReadsInterceptor::GetHostName(),
        std::make_unique<EternalSyncReadsInterceptor>());
  }

  // If |second| is empty, then it's ignored.
  void LoadPacketsAndVerifyContents(const std::string& first,
                                    const std::string& second) {
    EXPECT_FALSE(first.empty());
    std::list<std::string> packets;
    packets.push_back(first);
    if (!second.empty())
      packets.push_back(second);
    AddMultipleWritesInterceptor(std::move(packets), net::OK,
                                 false /* async_reads */);

    std::string expected_body = first + second;
    std::string actual_body;
    EXPECT_EQ(net::OK, Load(MultipleWritesInterceptor::GetURL(), &actual_body));

    EXPECT_EQ(actual_body, expected_body);
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  net::URLRequestContext* context() { return context_.get(); }
  TestURLLoaderClient* client() { return &client_; }
  void DestroyContext() {
    resource_scheduler_client_ = nullptr;
    context_.reset();
  }

  // Returns the path of the requested file in the test data directory.
  base::FilePath GetTestFilePath(const std::string& file_name) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("services"));
    file_path = file_path.Append(FILE_PATH_LITERAL("test"));
    file_path = file_path.Append(FILE_PATH_LITERAL("data"));
    return file_path.AppendASCII(file_name);
  }

  base::File OpenFileForUpload(const base::FilePath& file_path) {
    int open_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
#if defined(OS_WIN)
    open_flags |= base::File::FLAG_ASYNC;
#endif  //  defined(OS_WIN)
    base::File file(file_path, open_flags);
    EXPECT_TRUE(file.IsValid());
    return file;
  }

  ResourceScheduler* resource_scheduler() { return &resource_scheduler_; }
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client() {
    return resource_scheduler_client_;
  }

  // Configure how Load() works.
  void allow_file_uploads() {
    DCHECK(!ran_);
    allow_file_uploads_ = true;
  }
  void set_sniff() {
    DCHECK(!ran_);
    sniff_ = true;
  }
  void set_send_ssl_with_response() {
    DCHECK(!ran_);
    send_ssl_with_response_ = true;
  }
  void set_send_ssl_for_cert_error() {
    DCHECK(!ran_);
    send_ssl_for_cert_error_ = true;
  }
  void set_expect_redirect() {
    DCHECK(!ran_);
    expect_redirect_ = true;
  }
  void set_request_body(scoped_refptr<ResourceRequestBody> request_body) {
    request_body_ = request_body;
  }

  // Convenience methods after calling Load();
  std::string mime_type() const {
    DCHECK(ran_);
    return client_.response_head().mime_type;
  }

  bool did_mime_sniff() const {
    DCHECK(ran_);
    return client_.response_head().did_mime_sniff;
  }

  const base::Optional<net::SSLInfo>& ssl_info() const {
    DCHECK(ran_);
    return client_.ssl_info();
  }

  // Reads the response body from client()->response_body() until the channel is
  // closed. Expects client()->response_body() to already be populated, and
  // non-NULL.
  std::string ReadBody() {
    std::string body;
    while (true) {
      MojoHandle consumer = client()->response_body().value();

      const void* buffer;
      uint32_t num_bytes;
      MojoResult rv = MojoBeginReadData(consumer, nullptr, &buffer, &num_bytes);
      // If no data has been received yet, spin the message loop until it has.
      if (rv == MOJO_RESULT_SHOULD_WAIT) {
        mojo::SimpleWatcher watcher(
            FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
            base::SequencedTaskRunnerHandle::Get());
        base::RunLoop run_loop;

        watcher.Watch(
            client()->response_body(),
            MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            MOJO_WATCH_CONDITION_SATISFIED,
            base::BindRepeating(
                [](base::Closure quit, MojoResult result,
                   const mojo::HandleSignalsState& state) { quit.Run(); },
                run_loop.QuitClosure()));
        run_loop.Run();
        continue;
      }

      // The pipe was closed.
      if (rv == MOJO_RESULT_FAILED_PRECONDITION)
        return body;

      CHECK_EQ(rv, MOJO_RESULT_OK);

      body.append(static_cast<const char*>(buffer), num_bytes);
      MojoEndReadData(consumer, num_bytes, nullptr);
    }

    return body;
  }

  std::string ReadAvailableBody() {
    MojoHandle consumer = client()->response_body().value();

    uint32_t num_bytes = 0;
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_READ_DATA_FLAG_QUERY;
    MojoResult result = MojoReadData(consumer, &options, nullptr, &num_bytes);
    CHECK_EQ(MOJO_RESULT_OK, result);
    if (num_bytes == 0)
      return std::string();

    std::vector<char> buffer(num_bytes);
    result = MojoReadData(consumer, nullptr, buffer.data(), &num_bytes);
    CHECK_EQ(MOJO_RESULT_OK, result);
    CHECK_EQ(num_bytes, buffer.size());

    return std::string(buffer.data(), buffer.size());
  }

  const net::test_server::HttpRequest& sent_request() const {
    return sent_request_;
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  static constexpr int kProcessId = 4;
  static constexpr int kRouteId = 8;

 private:
  void Monitor(const net::test_server::HttpRequest& request) {
    sent_request_ = request;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<net::URLRequestContext> context_;
  ResourceScheduler resource_scheduler_;
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;

  // Options applied to the created request in Load().
  bool allow_file_uploads_ = false;
  bool sniff_ = false;
  bool send_ssl_with_response_ = false;
  bool send_ssl_for_cert_error_ = false;
  bool expect_redirect_ = false;
  scoped_refptr<ResourceRequestBody> request_body_;

  // Used to ensure that methods are called either before or after a request is
  // made, since the test fixture is meant to be used only once.
  bool ran_ = false;
  net::test_server::HttpRequest sent_request_;
  TestURLLoaderClient client_;
};

constexpr int URLLoaderTest::kProcessId;
constexpr int URLLoaderTest::kRouteId;

TEST_F(URLLoaderTest, Basic) {
  LoadAndCompareFile("simple_page.html");
}

TEST_F(URLLoaderTest, Empty) {
  LoadAndCompareFile("empty.html");
}

TEST_F(URLLoaderTest, BasicSSL) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("/simple_page.html");
  set_send_ssl_with_response();
  EXPECT_EQ(net::OK, Load(url));
  ASSERT_TRUE(!!ssl_info());
  ASSERT_TRUE(!!ssl_info()->cert);

  ASSERT_TRUE(https_server.GetCertificate()->EqualsExcludingChain(
      ssl_info()->cert.get()));
}

TEST_F(URLLoaderTest, SSLSentOnlyWhenRequested) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("/simple_page.html");
  EXPECT_EQ(net::OK, Load(url));
  ASSERT_FALSE(!!ssl_info());
}

// Test decoded_body_length / encoded_body_length when they're different.
TEST_F(URLLoaderTest, GzipTest) {
  std::string body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/gzip-body?Body"), &body));
  EXPECT_EQ("Body", body);
  // Deflating a 4-byte string should result in a longer string - main thing to
  // check here, though, is that the two lengths are of different.
  EXPECT_LT(client()->completion_status().decoded_body_length,
            client()->completion_status().encoded_body_length);
  // Over the wire length should include headers, so should be longer.
  EXPECT_LT(client()->completion_status().encoded_body_length,
            client()->completion_status().encoded_data_length);
}

TEST_F(URLLoaderTest, ErrorBeforeHeaders) {
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            Load(test_server()->GetURL("/close-socket"), nullptr));
  EXPECT_FALSE(client()->response_body().is_valid());
}

TEST_F(URLLoaderTest, SyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(net::ERR_FAILED,
            Load(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                     net::URLRequestFailedJob::READ_SYNC, net::ERR_FAILED),
                 &body));
  EXPECT_EQ("", body);
}

TEST_F(URLLoaderTest, AsyncErrorWhileReadingBody) {
  std::string body;
  EXPECT_EQ(net::ERR_FAILED,
            Load(net::URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
                     net::URLRequestFailedJob::READ_ASYNC, net::ERR_FAILED),
                 &body));
  EXPECT_EQ("", body);
}

TEST_F(URLLoaderTest, SyncErrorWhileReadingBodyAfterBytesReceived) {
  const std::string kBody("Foo.");

  std::list<std::string> packets;
  packets.push_back(kBody);
  AddMultipleWritesInterceptor(packets, net::ERR_ACCESS_DENIED,
                               false /*async_reads*/);
  std::string body;
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            Load(MultipleWritesInterceptor::GetURL(), &body));
  EXPECT_EQ(kBody, body);
}

TEST_F(URLLoaderTest, AsyncErrorWhileReadingBodyAfterBytesReceived) {
  const std::string kBody("Foo.");

  std::list<std::string> packets;
  packets.push_back(kBody);
  AddMultipleWritesInterceptor(packets, net::ERR_ACCESS_DENIED,
                               true /*async_reads*/);
  std::string body;
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            Load(MultipleWritesInterceptor::GetURL(), &body));
  EXPECT_EQ(kBody, body);
}

TEST_F(URLLoaderTest, DoNotSniffUnlessSpecified) {
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test0.html")));
  EXPECT_FALSE(did_mime_sniff());
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderTest, SniffMimeType) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test0.html")));
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("text/html"), mime_type());
}

TEST_F(URLLoaderTest, RespectNoSniff) {
  set_sniff();
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/nosniff-test.html")));
  EXPECT_FALSE(did_mime_sniff());
  ASSERT_TRUE(mime_type().empty());
}

TEST_F(URLLoaderTest, SniffTextPlainDoesNotResultInHTML) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test1.html")));
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

TEST_F(URLLoaderTest, DoNotSniffHTMLFromImageGIF) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test2.html")));
  EXPECT_FALSE(did_mime_sniff());
  ASSERT_EQ(std::string("image/gif"), mime_type());
}

TEST_F(URLLoaderTest, EmptyHtmlIsTextPlain) {
  set_sniff();
  EXPECT_EQ(net::OK,
            Load(test_server()->GetURL("/content-sniffer-test4.html")));
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

TEST_F(URLLoaderTest, EmptyHtmlIsTextPlainWithAsyncResponse) {
  set_sniff();

  const std::string kBody;

  std::list<std::string> packets;
  packets.push_back(kBody);
  AddMultipleWritesInterceptor(packets, net::OK, true /*async_reads*/);

  std::string body;
  EXPECT_EQ(net::OK, Load(MultipleWritesInterceptor::GetURL(), &body));
  EXPECT_EQ(kBody, body);
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

// Tests the case where the first read doesn't have enough data to figure out
// the right mime type. The second read would have enough data even though the
// total bytes is still smaller than net::kMaxBytesToSniff.
TEST_F(URLLoaderTest, FirstReadNotEnoughToSniff1) {
  set_sniff();
  std::string first(500, 'a');
  std::string second(std::string(100, 'b'));
  second[10] = 0;
  EXPECT_LE(first.size() + second.size(),
            static_cast<uint32_t>(net::kMaxBytesToSniff));
  LoadPacketsAndVerifyContents(first, second);
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("application/octet-stream"), mime_type());
}

// Like above, except that the total byte count is > kMaxBytesToSniff.
TEST_F(URLLoaderTest, FirstReadNotEnoughToSniff2) {
  set_sniff();
  std::string first(500, 'a');
  std::string second(std::string(1000, 'b'));
  second[10] = 0;
  EXPECT_GE(first.size() + second.size(),
            static_cast<uint32_t>(net::kMaxBytesToSniff));
  LoadPacketsAndVerifyContents(first, second);
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("application/octet-stream"), mime_type());
}

// Tests that even if the first and only read is smaller than the minimum number
// of bytes needed to sniff, the loader works correctly and returns the data.
TEST_F(URLLoaderTest, LoneReadNotEnoughToSniff) {
  set_sniff();
  std::string first(net::kMaxBytesToSniff - 100, 'a');
  LoadPacketsAndVerifyContents(first, std::string());
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

// Tests the simple case where the first read is enough to sniff.
TEST_F(URLLoaderTest, FirstReadIsEnoughToSniff) {
  set_sniff();
  std::string first(net::kMaxBytesToSniff + 100, 'a');
  LoadPacketsAndVerifyContents(first, std::string());
  EXPECT_TRUE(did_mime_sniff());
  ASSERT_EQ(std::string("text/plain"), mime_type());
}

class NeverFinishedBodyHttpResponse : public net::test_server::HttpResponse {
 public:
  NeverFinishedBodyHttpResponse() = default;
  ~NeverFinishedBodyHttpResponse() override = default;

 private:
  // net::test_server::HttpResponse implementation.
  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override {
    send.Run(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "long long ago..." +
            std::string(1024 * 1024, 'a'),
        base::DoNothing());

    // Never call |done|, so the other side will never see the completion of the
    // response body.
  }
};

// Check that the URLLoader tears itself down when the URLLoader pipe is closed.
TEST_F(URLLoaderTest, DestroyOnURLLoaderPipeClosed) {
  net::EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        std::unique_ptr<net::test_server::HttpResponse> response =
            std::make_unique<NeverFinishedBodyHttpResponse>();
        return response;
      }));
  ASSERT_TRUE(server.Start());

  ResourceRequest request =
      CreateResourceRequest("GET", server.GetURL("/hello.html"));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  // Run until the response body pipe arrives, to make sure that a live body
  // pipe does not result in keeping the loader alive when the URLLoader pipe is
  // closed.
  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  loader.reset();

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  // client()->RunUntilConnectionError();
  // EXPECT_FALSE(client()->has_received_completion());
  client()->RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client()->completion_status().error_code);
}

// Make sure that the URLLoader is destroyed when the data pipe is closed.
// The pipe may either be closed in
// URLLoader::OnResponseBodyStreamConsumerClosed() or URLLoader::DidRead(),
// depending on whether the closed pipe is first noticed when trying to write to
// it, or when a mojo close notification is received, so if only one path
// breaks, this test may flakily fail.
TEST_F(URLLoaderTest, CloseResponseBodyConsumerBeforeProducer) {
  net::EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        std::unique_ptr<net::test_server::HttpResponse> response =
            std::make_unique<NeverFinishedBodyHttpResponse>();
        return response;
      }));
  ASSERT_TRUE(server.Start());

  ResourceRequest request =
      CreateResourceRequest("GET", server.GetURL("/hello.html"));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Wait for a little amount of time for the response body pipe to be filled.
  // (Please note that this doesn't guarantee that the pipe is filled to the
  // point that it is not writable anymore.)
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();

  auto response_body = client()->response_body_release();
  response_body.reset();

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client()->completion_status().error_code);
}

TEST_F(URLLoaderTest, PauseReadingBodyFromNetBeforeResponseHeaders) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContents = "This is the data as you requested.";

  base::HistogramBase::Sample output_sample = -1;
  base::RunLoop histogram_run_loop;
  EXPECT_TRUE(StartMonitorBodyReadFromNetBeforePausedHistogram(
      histogram_run_loop.QuitClosure(), &output_sample));

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  // Pausing reading response body from network stops future reads from the
  // underlying URLRequest. So no data should be sent using the response body
  // data pipe.
  loader->PauseReadingBodyFromNet();
  // In order to avoid flakiness, make sure PauseReadBodyFromNet() is handled by
  // the loader before the test HTTP server serves the response.
  loader.FlushForTesting();

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContents));
  response_controller.Done();

  // We will still receive the response body data pipe, although there won't be
  // any data available until ResumeReadBodyFromNet() is called.
  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Wait for a little amount of time so that if the loader mistakenly reads
  // response body from the underlying URLRequest, it is easier to find out.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  run_loop.Run();

  std::string available_data = ReadAvailableBody();
  EXPECT_TRUE(available_data.empty());

  loader->ResumeReadingBodyFromNet();
  client()->RunUntilComplete();

  available_data = ReadBody();
  EXPECT_EQ(kBodyContents, available_data);

  delete_run_loop.Run();
  client()->Unbind();
  histogram_run_loop.Run();
  EXPECT_EQ(0, output_sample);
  StopMonitorBodyReadFromNetBeforePausedHistogram();
}

TEST_F(URLLoaderTest, PauseReadingBodyFromNetWhenReadIsPending) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  base::HistogramBase::Sample output_sample = -1;
  base::RunLoop histogram_run_loop;
  EXPECT_TRUE(StartMonitorBodyReadFromNetBeforePausedHistogram(
      histogram_run_loop.QuitClosure(), &output_sample));

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContentsFirstHalf));

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  loader->PauseReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.Send(kBodyContentsSecondHalf);
  response_controller.Done();

  // It is uncertain how much data has been read before reading is actually
  // paused, because if there is a pending read when PauseReadingBodyFromNet()
  // arrives, the pending read won't be cancelled. Therefore, this test only
  // checks that after ResumeReadingBodyFromNet() we should be able to get the
  // whole response body.
  loader->ResumeReadingBodyFromNet();
  client()->RunUntilComplete();

  EXPECT_EQ(std::string(kBodyContentsFirstHalf) +
                std::string(kBodyContentsSecondHalf),
            ReadBody());

  delete_run_loop.Run();
  client()->Unbind();
  histogram_run_loop.Run();
  EXPECT_LE(0, output_sample);
  StopMonitorBodyReadFromNetBeforePausedHistogram();
}

TEST_F(URLLoaderTest, ResumeReadingBodyFromNetAfterClosingConsumer) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";

  base::HistogramBase::Sample output_sample = -1;
  base::RunLoop histogram_run_loop;
  EXPECT_TRUE(StartMonitorBodyReadFromNetBeforePausedHistogram(
      histogram_run_loop.QuitClosure(), &output_sample));

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  loader->PauseReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContentsFirstHalf));

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  auto response_body = client()->response_body_release();
  response_body.reset();

  // It shouldn't cause any issue even if a ResumeReadingBodyFromNet() call is
  // made after the response body data pipe is closed.
  loader->ResumeReadingBodyFromNet();
  loader.FlushForTesting();

  loader.reset();
  client()->Unbind();
  delete_run_loop.Run();
  histogram_run_loop.Run();
  EXPECT_EQ(0, output_sample);
  StopMonitorBodyReadFromNetBeforePausedHistogram();
}

TEST_F(URLLoaderTest, MultiplePauseResumeReadingBodyFromNet) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  base::HistogramBase::Sample output_sample = -1;
  base::RunLoop histogram_run_loop;
  EXPECT_TRUE(StartMonitorBodyReadFromNetBeforePausedHistogram(
      histogram_run_loop.QuitClosure(), &output_sample));

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  // It is okay to call ResumeReadingBodyFromNet() even if there is no prior
  // PauseReadingBodyFromNet().
  loader->ResumeReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.WaitForRequest();
  response_controller.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n" +
      std::string(kBodyContentsFirstHalf));

  loader->PauseReadingBodyFromNet();

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  loader->PauseReadingBodyFromNet();
  loader->PauseReadingBodyFromNet();
  loader.FlushForTesting();

  response_controller.Send(kBodyContentsSecondHalf);
  response_controller.Done();

  // One ResumeReadingBodyFromNet() call will resume reading even if there are
  // multiple PauseReadingBodyFromNet() calls before it.
  loader->ResumeReadingBodyFromNet();

  delete_run_loop.Run();
  client()->RunUntilComplete();

  EXPECT_EQ(std::string(kBodyContentsFirstHalf) +
                std::string(kBodyContentsSecondHalf),
            ReadBody());

  loader.reset();
  client()->Unbind();
  histogram_run_loop.Run();
  EXPECT_LE(0, output_sample);
  StopMonitorBodyReadFromNetBeforePausedHistogram();
}

TEST_F(URLLoaderTest, UploadBytes) {
  const std::string kRequestBody = "Request Body";

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendBytes(kRequestBody.c_str(), kRequestBody.length());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(kRequestBody, response_body);
}

TEST_F(URLLoaderTest, UploadFile) {
  allow_file_uploads();
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendFileRange(
      file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

TEST_F(URLLoaderTest, UploadFileWithRange) {
  allow_file_uploads();
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();
  expected_body = expected_body.substr(1, expected_body.size() - 2);

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendFileRange(file_path, 1, expected_body.size(),
                                base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

TEST_F(URLLoaderTest, UploadTwoFiles) {
  allow_file_uploads();
  base::FilePath file_path1 = GetTestFilePath("simple_page.html");
  base::FilePath file_path2 = GetTestFilePath("hello.html");

  std::string expected_body1;
  std::string expected_body2;
  ASSERT_TRUE(base::ReadFileToString(file_path1, &expected_body1))
      << "File not found: " << file_path1.value();
  ASSERT_TRUE(base::ReadFileToString(file_path2, &expected_body2))
      << "File not found: " << file_path2.value();

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendFileRange(
      file_path1, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  request_body->AppendFileRange(
      file_path2, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body1 + expected_body2, response_body);
}

TEST_F(URLLoaderTest, UploadInvalidFile) {
  // Don't call allow_file_uploads();
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendFileRange(
      file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  EXPECT_EQ(net::ERR_ACCESS_DENIED, Load(test_server()->GetURL("/echo")));
}

class CallbackSavingNetworkServiceClient : public TestNetworkServiceClient {
 public:
  void OnFileUploadRequested(uint32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override {
    file_upload_requested_callback_ = std::move(callback);
    if (quit_closure_for_on_file_upload_requested_)
      quit_closure_for_on_file_upload_requested_.Run();
  }

  void RunUntilUploadRequested(OnFileUploadRequestedCallback* callback) {
    if (!file_upload_requested_callback_) {
      base::RunLoop run_loop;
      quit_closure_for_on_file_upload_requested_ = run_loop.QuitClosure();
      run_loop.Run();
      quit_closure_for_on_file_upload_requested_.Reset();
    }
    *callback = std::move(file_upload_requested_callback_);
  }

 private:
  base::Closure quit_closure_for_on_file_upload_requested_;
  OnFileUploadRequestedCallback file_upload_requested_callback_;
};

TEST_F(URLLoaderTest, UploadFileCanceled) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");
  std::vector<base::File> opened_file;
  opened_file.emplace_back(file_path, base::File::FLAG_OPEN |
                                          base::File::FLAG_READ |
                                          base::File::FLAG_ASYNC);
  ASSERT_TRUE(opened_file.back().IsValid());

  ResourceRequest request =
      CreateResourceRequest("POST", test_server()->GetURL("/echo"));
  request.request_body = new ResourceRequestBody();
  request.request_body->AppendFileRange(
      file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  static mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  auto network_service_client =
      std::make_unique<CallbackSavingNetworkServiceClient>();
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), network_service_client.get(),
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  mojom::NetworkServiceClient::OnFileUploadRequestedCallback callback;
  network_service_client->RunUntilUploadRequested(&callback);

  // Check we can call the callback from a deleted URLLoader without crashing.
  url_loader.reset();
  base::RunLoop().RunUntilIdle();
  std::move(callback).Run(net::OK, std::move(opened_file));
  base::RunLoop().RunUntilIdle();
}

TEST_F(URLLoaderTest, UploadRawFile) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendRawFileRange(
      OpenFileForUpload(file_path), GetTestFilePath("should_be_ignored"), 0,
      std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

TEST_F(URLLoaderTest, UploadRawFileWithRange) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();
  expected_body = expected_body.substr(1, expected_body.size() - 2);

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendRawFileRange(OpenFileForUpload(file_path),
                                   GetTestFilePath("should_be_ignored"), 1,
                                   expected_body.size(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

// Tests a request body with a data pipe element.
TEST_F(URLLoaderTest, UploadDataPipe) {
  const std::string kRequestBody = "Request Body";

  mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(kRequestBody, response_body);
}

// Same as above and tests that the body is sent after a 307 redirect.
TEST_F(URLLoaderTest, UploadDataPipe_Redirect307) {
  const std::string kRequestBody = "Request Body";

  mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));
  set_expect_redirect();

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/redirect307-to-echo"),
                          &response_body));
  EXPECT_EQ(kRequestBody, response_body);
}

// Tests a large request body, which should result in multiple asynchronous
// reads.
TEST_F(URLLoaderTest, UploadDataPipeWithLotsOfData) {
  std::string request_body;
  request_body.reserve(5 * 1024 * 1024);
  // Using a repeating patter with a length that's prime is more likely to spot
  // out of order or repeated chunks of data.
  while (request_body.size() < 5 * 1024 * 1024)
    request_body.append("foppity");

  mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      request_body, mojo::MakeRequest(&data_pipe_getter_ptr));

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(request_body, response_body);
}

TEST_F(URLLoaderTest, UploadDataPipeError) {
  const std::string kRequestBody = "Request Body";

  mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));
  data_pipe_getter->set_start_error(net::ERR_ACCESS_DENIED);

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  EXPECT_EQ(net::ERR_ACCESS_DENIED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadDataPipeClosedEarly) {
  const std::string kRequestBody = "Request Body";

  mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, mojo::MakeRequest(&data_pipe_getter_ptr));
  data_pipe_getter->set_pipe_closed_early(true);

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::ERR_FAILED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadDoubleRawFile) {
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_body))
      << "File not found: " << file_path.value();

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendRawFileRange(
      OpenFileForUpload(file_path), GetTestFilePath("should_be_ignored"), 0,
      std::numeric_limits<uint64_t>::max(), base::Time());
  request_body->AppendRawFileRange(
      OpenFileForUpload(file_path), GetTestFilePath("should_be_ignored"), 0,
      std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body + expected_body, response_body);
}

// Tests a request body with a chunked data pipe element.
TEST_F(URLLoaderTest, UploadChunkedDataPipe) {
  const std::string kRequestBody = "Request Body";

  TestChunkedDataPipeGetter data_pipe_getter;

  ResourceRequest request =
      CreateResourceRequest("POST", test_server()->GetURL("/echo"));
  request.request_body = base::MakeRefCounted<ResourceRequestBody>();
  request.request_body->SetToChunkedDataPipe(
      data_pipe_getter.GetDataPipeGetterPtr());

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false /* report_raw_headers */,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, nullptr /* resource_scheduler_client */,
      nullptr /* keepalive_statistics_reporter */,
      nullptr /* network_usage_accumulator */);

  mojom::ChunkedDataPipeGetter::GetSizeCallback get_size_callback =
      data_pipe_getter.WaitForGetSize();
  mojo::BlockingCopyFromString(kRequestBody,
                               data_pipe_getter.WaitForStartReading());
  std::move(get_size_callback).Run(net::OK, kRequestBody.size());
  delete_run_loop.Run();
  client()->RunUntilComplete();

  EXPECT_EQ(kRequestBody, ReadBody());
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
}

// Tests that SSLInfo is not attached to OnComplete messages when there is no
// certificate error.
TEST_F(URLLoaderTest, NoSSLInfoWithoutCertificateError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  set_send_ssl_for_cert_error();
  EXPECT_EQ(net::OK, Load(https_server.GetURL("/")));
  EXPECT_FALSE(client()->completion_status().ssl_info.has_value());
}

// Tests that SSLInfo is not attached to OnComplete messages when the
// corresponding option is not set.
TEST_F(URLLoaderTest, NoSSLInfoOnComplete) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, Load(https_server.GetURL("/")));
  EXPECT_FALSE(client()->completion_status().ssl_info.has_value());
}

// Tests that SSLInfo is attached to OnComplete messages when the corresponding
// option is set.
TEST_F(URLLoaderTest, SSLInfoOnComplete) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());
  set_send_ssl_for_cert_error();
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, Load(https_server.GetURL("/")));
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info.value().cert);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client()->completion_status().ssl_info.value().cert_status);
}

// Make sure the client can modify headers during a redirect.
TEST_F(URLLoaderTest, RedirectModifiedHeaders) {
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server()->GetURL("/redirect307-to-echo"));
  request.headers.SetHeader("Header1", "Value1");
  request.headers.SetHeader("Header2", "Value2");

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), mojom::kURLLoadOptionNone, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilRedirectReceived();

  // Initial request should only have initial headers.
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers1.find("Header1")->second);
  EXPECT_EQ("Value2", request_headers1.find("Header2")->second);
  EXPECT_EQ(request_headers1.end(), request_headers1.find("Header3"));

  // Overwrite Header2 and add Header3.
  net::HttpRequestHeaders redirect_headers;
  redirect_headers.SetHeader("Header2", "");
  redirect_headers.SetHeader("Header3", "Value3");
  loader->FollowRedirect(base::nullopt, redirect_headers);

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // Redirected request should also have modified headers.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers2.find("Header1")->second);
  EXPECT_EQ("", request_headers2.find("Header2")->second);
  EXPECT_EQ("Value3", request_headers2.find("Header3")->second);
}

// Test the client can remove headers during a redirect.
TEST_F(URLLoaderTest, RedirectRemoveHeader) {
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server()->GetURL("/redirect307-to-echo"));
  request.headers.SetHeader("Header1", "Value1");
  request.headers.SetHeader("Header2", "Value2");

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), mojom::kURLLoadOptionNone, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilRedirectReceived();

  // Initial request should only have initial headers.
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers1.find("Header1")->second);
  EXPECT_EQ("Value2", request_headers1.find("Header2")->second);

  // Remove Header1.
  std::vector<std::string> to_be_removed_request_headers = {"Header1"};
  loader->FollowRedirect(to_be_removed_request_headers, base::nullopt);

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // Redirected request should have the updated headers.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Header1"));
  EXPECT_EQ("Value2", request_headers2.find("Header2")->second);
}

// Test the client can remove headers and add headers back during a redirect.
TEST_F(URLLoaderTest, RedirectRemoveHeaderAndAddItBack) {
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server()->GetURL("/redirect307-to-echo"));
  request.headers.SetHeader("Header1", "Value1");
  request.headers.SetHeader("Header2", "Value2");

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), mojom::kURLLoadOptionNone, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilRedirectReceived();

  // Initial request should only have initial headers.
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers1.find("Header1")->second);
  EXPECT_EQ("Value2", request_headers1.find("Header2")->second);

  // Remove Header1 and add it back using a different value.
  std::vector<std::string> to_be_removed_request_headers = {"Header1"};
  net::HttpRequestHeaders redirect_headers;
  redirect_headers.SetHeader("Header1", "NewValue1");
  loader->FollowRedirect(to_be_removed_request_headers, redirect_headers);

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // Redirected request should have the updated headers.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ("NewValue1", request_headers2.find("Header1")->second);
  EXPECT_EQ("Value2", request_headers2.find("Header2")->second);
}

// A mock URLRequestJob which simulates an HTTPS request with a certificate
// error.
class MockHTTPSURLRequestJob : public net::URLRequestTestJob {
 public:
  MockHTTPSURLRequestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}

  // net::URLRequestTestJob:
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    // Get the original response info, but override the SSL info.
    net::URLRequestJob::GetResponseInfo(info);
    info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    info->ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  }

 private:
  ~MockHTTPSURLRequestJob() override {}

  DISALLOW_COPY_AND_ASSIGN(MockHTTPSURLRequestJob);
};

class MockHTTPSJobURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  MockHTTPSJobURLRequestInterceptor() {}
  ~MockHTTPSJobURLRequestInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new MockHTTPSURLRequestJob(request, network_delegate, std::string(),
                                      "dummy response", true);
  }
};

// Tests that |cert_status| is set on the resource response.
TEST_F(URLLoaderTest, CertStatusOnResponse) {
  net::URLRequestFilter::GetInstance()->ClearHandlers();
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      "https", "example.test",
      std::unique_ptr<net::URLRequestInterceptor>(
          new MockHTTPSJobURLRequestInterceptor()));

  EXPECT_EQ(net::OK, Load(GURL("https://example.test/")));
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client()->response_head().cert_status);
}

// Verifies if URLLoader works well with ResourceScheduler.
TEST_F(URLLoaderTest, ResourceSchedulerIntegration) {
  // ResourceScheduler limits the number of connections for the same host
  // by 6.
  constexpr int kRepeat = 6;
  constexpr char kPath[] = "/hello.html";

  net::EmbeddedTestServer server;
  // This is needed to stall all requests to the server.
  net::test_server::ControllableHttpResponse response_controllers[kRepeat] = {
      {&server, kPath}, {&server, kPath}, {&server, kPath},
      {&server, kPath}, {&server, kPath}, {&server, kPath},
  };

  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));
  request.load_flags = net::LOAD_DISABLE_CACHE;
  request.priority = net::IDLE;

  // Fill up the ResourceScheduler with delayable requests.
  std::vector<std::pair<std::unique_ptr<URLLoader>, mojom::URLLoaderPtr>>
      loaders;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  for (int i = 0; i < kRepeat; ++i) {
    TestURLLoaderClient client;
    mojom::URLLoaderPtr loaderInterfacePtr;

    std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
        context(), nullptr /* network_service_client */,
        NeverInvokedDeleteLoaderCallback(),
        mojo::MakeRequest(&loaderInterfacePtr), 0, request, false,
        client.CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
        0 /* request_id */, resource_scheduler_client(), nullptr,
        nullptr /* network_usage_accumulator */);

    loaders.emplace_back(
        std::make_pair(std::move(url_loader), std::move(loaderInterfacePtr)));
  }

  base::RunLoop().RunUntilIdle();
  for (const auto& pair : loaders) {
    URLLoader* loader = pair.first.get();
    ASSERT_NE(loader, nullptr);
    EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_RESPONSE,
              loader->GetLoadStateForTesting());
  }

  mojom::URLLoaderPtr loader_interface_ptr;
  std::unique_ptr<URLLoader> loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      NeverInvokedDeleteLoaderCallback(),
      mojo::MakeRequest(&loader_interface_ptr), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);
  base::RunLoop().RunUntilIdle();

  // Make sure that the ResourceScheduler throttles this request.
  EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_DELEGATE,
            loader->GetLoadStateForTesting());

  loader->SetPriority(net::HIGHEST, 0 /* intra_priority_value */);
  base::RunLoop().RunUntilIdle();

  // Make sure that the ResourceScheduler stops throtting.
  EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET,
            loader->GetLoadStateForTesting());
}

// This tests that case where a read pipe is closed while there's a post task to
// invoke ReadMore.
TEST_F(URLLoaderTest, ReadPipeClosedWhileReadTaskPosted) {
  AddEternalSyncReadsInterceptor();

  ResourceRequest request = CreateResourceRequest(
      "GET", EternalSyncReadsInterceptor::GetSingleByteURL());

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), mojom::kURLLoadOptionNone, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilResponseBodyArrived();
  client()->response_body_release();
  delete_run_loop.Run();
}

// Test power monitor source that can simulate entering suspend mode. Can't use
// the one in base/ because it insists on bringing its own MessageLoop.
class TestPowerMonitorSource : public base::PowerMonitorSource {
 public:
  TestPowerMonitorSource() = default;
  ~TestPowerMonitorSource() override = default;

  void Shutdown() override {}

  void Suspend() { ProcessPowerEvent(SUSPEND_EVENT); }

  void Resume() { ProcessPowerEvent(RESUME_EVENT); }

  bool IsOnBatteryPowerImpl() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPowerMonitorSource);
};

// This tests the case where suspend mode is entered when there's a loader that
// has received response headers but has no pending read. URLRequestJob will
// cancel the request and call OnReadCompleted despite there being no pending
// read, which is a bit weird, and can cause crashes if not handled properly.
TEST_F(URLLoaderTest, EnterSuspendModeWhileNoPendingRead) {
  AddEternalSyncReadsInterceptor();

  std::unique_ptr<TestPowerMonitorSource> power_monitor_source =
      std::make_unique<TestPowerMonitorSource>();
  TestPowerMonitorSource* unowned_power_monitor_source =
      power_monitor_source.get();
  base::PowerMonitor power_monitor(std::move(power_monitor_source));

  ResourceRequest request = CreateResourceRequest(
      "GET", EternalSyncReadsInterceptor::GetFillBufferURL());

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), mojom::kURLLoadOptionNone, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  // This will spin the run loop until the Mojo read buffer is full. The
  // URLLoader will end up waiting for Mojo to give it more read buffer space.
  base::RunLoop().RunUntilIdle();

  unowned_power_monitor_source->Suspend();

  client()->RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client()->completion_status().error_code);
  delete_run_loop.Run();

  unowned_power_monitor_source->Resume();
}

TEST_F(URLLoaderTest, EnterSuspendDiskCacheWriteQueued) {
  // Test to make sure that fetch abort on suspend doesn't yank out the backing
  // for IOBuffer for an issued disk_cache Write.

  GURL url("http://www.example.com");
  scoped_refptr<net::IOBuffer> simulated_cache_dest;
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::make_unique<SimulatedCacheInterceptor>(&simulated_cache_dest));

  std::unique_ptr<TestPowerMonitorSource> power_monitor_source =
      std::make_unique<TestPowerMonitorSource>();
  TestPowerMonitorSource* unowned_power_monitor_source =
      power_monitor_source.get();
  base::PowerMonitor power_monitor(std::move(power_monitor_source));

  ResourceRequest request = CreateResourceRequest("GET", url);

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), mojom::kURLLoadOptionNone, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  // Spin until the job has produced a (simulated) cache write.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(simulated_cache_dest);

  unowned_power_monitor_source->Suspend();

  client()->RunUntilComplete();

  EXPECT_EQ(net::ERR_ABORTED, client()->completion_status().error_code);
  delete_run_loop.Run();

  unowned_power_monitor_source->Resume();

  // The "cache write" should still have data available.
  EXPECT_EQ('a', simulated_cache_dest->data()[0]);
}

class FakeSSLPrivateKeyImpl : public network::mojom::SSLPrivateKey {
 public:
  explicit FakeSSLPrivateKeyImpl(
      scoped_refptr<net::SSLPrivateKey> ssl_private_key)
      : ssl_private_key_(std::move(ssl_private_key)) {}
  ~FakeSSLPrivateKeyImpl() override {}

  // network::mojom::SSLPrivateKey:
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            network::mojom::SSLPrivateKey::SignCallback callback) override {
    base::span<const uint8_t> input_span(input);
    ssl_private_key_->Sign(
        algorithm, input_span,
        base::BindOnce(&FakeSSLPrivateKeyImpl::Callback, base::Unretained(this),
                       std::move(callback)));
  }

 private:
  void Callback(network::mojom::SSLPrivateKey::SignCallback callback,
                net::Error net_error,
                const std::vector<uint8_t>& signature) {
    std::move(callback).Run(static_cast<int32_t>(net_error), signature);
  }

  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;

  DISALLOW_COPY_AND_ASSIGN(FakeSSLPrivateKeyImpl);
};

// A mock NetworkServiceClient that does the following:
// 1. Responds auth challenges with previously set credentials.
// 2. Responds certificate request with previously set responses.
class MockNetworkServiceClient : public mojom::NetworkServiceClient {
 public:
  MockNetworkServiceClient() = default;
  ~MockNetworkServiceClient() override = default;

  enum class CredentialsResponse {
    NO_CREDENTIALS,
    CORRECT_CREDENTIALS,
    INCORRECT_CREDENTIALS_THEN_CORRECT_ONES,
  };

  enum class CertificateResponse {
    INVALID = -1,
    URL_LOADER_REQUEST_CANCELLED,
    CANCEL_CERTIFICATE_SELECTION,
    NULL_CERTIFICATE,
    VALID_CERTIFICATE_SIGNATURE,
    INVALID_CERTIFICATE_SIGNATURE,
  };

  // mojom::NetworkServiceClient:
  void OnAuthRequired(
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      const GURL& site_for_cookies,
      bool first_auth_attempt,
      const scoped_refptr<net::AuthChallengeInfo>& auth_info,
      int32_t resource_type,
      const base::Optional<network::ResourceResponseHead>& head,
      mojom::AuthChallengeResponderPtr auth_challenge_responder) override {
    switch (credentials_response_) {
      case CredentialsResponse::NO_CREDENTIALS:
        auth_credentials_ = base::nullopt;
        break;
      case CredentialsResponse::CORRECT_CREDENTIALS:
        auth_credentials_ = net::AuthCredentials(base::ASCIIToUTF16("USER"),
                                                 base::ASCIIToUTF16("PASS"));
        break;
      case CredentialsResponse::INCORRECT_CREDENTIALS_THEN_CORRECT_ONES:
        auth_credentials_ = net::AuthCredentials(base::ASCIIToUTF16("USER"),
                                                 base::ASCIIToUTF16("FAIL"));
        credentials_response_ = CredentialsResponse::CORRECT_CREDENTIALS;
        break;
    }
    std::move(auth_challenge_responder)->OnAuthCredentials(auth_credentials_);
    ++on_auth_required_call_counter_;
    last_seen_response_headers_ = head ? head->headers : nullptr;
  }

  void OnCertificateRequested(
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojom::NetworkServiceClient::OnCertificateRequestedCallback callback)
      override {
    switch (certificate_response_) {
      case CertificateResponse::INVALID:
        NOTREACHED();
        break;
      case CertificateResponse::URL_LOADER_REQUEST_CANCELLED:
        ASSERT_TRUE(url_loader_ptr_);
        url_loader_ptr_->reset();
        break;
      case CertificateResponse::CANCEL_CERTIFICATE_SELECTION:
        std::move(callback).Run(nullptr, std::vector<uint16_t>(), nullptr,
                                true /* cancel_certificate_selection */);
        break;
      case CertificateResponse::NULL_CERTIFICATE:
        std::move(callback).Run(nullptr, std::vector<uint16_t>(), nullptr,
                                false /* cancel_certificate_selection */);
        break;
      case CertificateResponse::VALID_CERTIFICATE_SIGNATURE:
      case CertificateResponse::INVALID_CERTIFICATE_SIGNATURE:
        std::move(callback).Run(std::move(certificate_), algorithm_preferences_,
                                std::move(ssl_private_key_ptr_),
                                false /* cancel_certificate_selection */);
        break;
    }
    ++on_certificate_requested_counter_;
  }

  void OnSSLCertificateError(uint32_t process_id,
                             uint32_t routing_id,
                             uint32_t request_id,
                             int32_t resource_type,
                             const GURL& url,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override {
    NOTREACHED();
  }
  void OnCookiesRead(int process_id,
                     int routing_id,
                     const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override {
    NOTREACHED();
  }
  void OnCookieChange(int process_id,
                      int routing_id,
                      const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override {
    NOTREACHED();
  }

#if defined(OS_CHROMEOS)
  void OnUsedTrustAnchor(const std::string& username_hash) override {
    NOTREACHED();
  }
#endif

  void OnFileUploadRequested(uint32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override {
    NOTREACHED();
  }

  void OnLoadingStateUpdate(std::vector<mojom::LoadInfoPtr> infos,
                            OnLoadingStateUpdateCallback callback) override {
    NOTREACHED();
  }

  void OnClearSiteData(int process_id,
                       int routing_id,
                       const GURL& url,
                       const std::string& header_value,
                       int load_flags,
                       OnClearSiteDataCallback callback) override {
    NOTREACHED();
  }

  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override {}

  void set_credentials_response(CredentialsResponse credentials_response) {
    credentials_response_ = credentials_response;
  }

  int on_auth_required_call_counter() { return on_auth_required_call_counter_; }

  net::HttpResponseHeaders* last_seen_response_headers() {
    return last_seen_response_headers_.get();
  }

  void set_certificate_response(CertificateResponse certificate_response) {
    certificate_response_ = certificate_response;
  }

  void set_url_loader_ptr(mojom::URLLoaderPtr* url_loader_ptr) {
    url_loader_ptr_ = url_loader_ptr;
  }

  void set_private_key(scoped_refptr<net::SSLPrivateKey> ssl_private_key) {
    ssl_private_key_ = std::move(ssl_private_key);
    algorithm_preferences_ = ssl_private_key_->GetAlgorithmPreferences();
    auto ssl_private_key_request = mojo::MakeRequest(&ssl_private_key_ptr_);
    mojo::MakeStrongBinding(
        std::make_unique<FakeSSLPrivateKeyImpl>(std::move(ssl_private_key_)),
        std::move(ssl_private_key_request));
  }

  void set_certificate(scoped_refptr<net::X509Certificate> certificate) {
    certificate_ = std::move(certificate);
  }

  int on_certificate_requested_counter() {
    return on_certificate_requested_counter_;
  }

 private:
  CredentialsResponse credentials_response_;
  base::Optional<net::AuthCredentials> auth_credentials_;
  int on_auth_required_call_counter_ = 0;
  scoped_refptr<net::HttpResponseHeaders> last_seen_response_headers_;
  CertificateResponse certificate_response_ = CertificateResponse::INVALID;
  mojom::URLLoaderPtr* url_loader_ptr_ = nullptr;
  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
  scoped_refptr<net::X509Certificate> certificate_;
  network::mojom::SSLPrivateKeyPtr ssl_private_key_ptr_;
  std::vector<uint16_t> algorithm_preferences_;
  int on_certificate_requested_counter_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockNetworkServiceClient);
};

TEST_F(URLLoaderTest, SetAuth) {
  MockNetworkServiceClient network_service_client;
  network_service_client.set_credentials_response(
      MockNetworkServiceClient::CredentialsResponse::CORRECT_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(url_loader);

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head().headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ(1, network_service_client.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
}

TEST_F(URLLoaderTest, CancelAuth) {
  MockNetworkServiceClient network_service_client;
  network_service_client.set_credentials_response(
      MockNetworkServiceClient::CredentialsResponse::NO_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(url_loader);

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head().headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(401, headers->response_code());
  EXPECT_EQ(1, network_service_client.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
}

TEST_F(URLLoaderTest, TwoChallenges) {
  MockNetworkServiceClient network_service_client;
  network_service_client.set_credentials_response(
      MockNetworkServiceClient::CredentialsResponse::
          INCORRECT_CREDENTIALS_THEN_CORRECT_ONES);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(url_loader);

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head().headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ(2, network_service_client.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
}

TEST_F(URLLoaderTest, NoAuthRequiredForFavicon) {
  constexpr char kFaviconTestPage[] = "/has_favicon.html";

  MockNetworkServiceClient network_service_client;
  network_service_client.set_credentials_response(
      MockNetworkServiceClient::CredentialsResponse::CORRECT_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kFaviconTestPage));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(url_loader);

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head().headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  // No auth required for favicon.
  EXPECT_EQ(0, network_service_client.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
}

TEST_F(URLLoaderTest, HttpAuthResponseHeadersAvailable) {
  MockNetworkServiceClient network_service_client;
  network_service_client.set_credentials_response(
      MockNetworkServiceClient::CredentialsResponse::CORRECT_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(url_loader);

  client()->RunUntilResponseBodyArrived();

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  EXPECT_EQ(1, network_service_client.on_auth_required_call_counter());

  auto* auth_required_headers =
      network_service_client.last_seen_response_headers();
  ASSERT_TRUE(auth_required_headers);
  EXPECT_EQ(auth_required_headers->response_code(), 401);
}

// This simulates plugins without universal access, like PNaCl. These make
// cross-origin fetches with CORS, and we expect CORB to block them.
TEST_F(URLLoaderTest, CorbEffectiveWithCors) {
  int kResourceType = 1;
  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/hello.html"));
  request.resource_type = kResourceType;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.request_initiator = url::Origin::Create(GURL("http://foo.com/"));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.corb_excluded_resource_type = kResourceType;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilResponseBodyArrived();
  std::string body = ReadBody();

  client()->RunUntilComplete();

  delete_run_loop.Run();

  // Blocked because this is a cross-origin request made with a CORS request
  // header, but without a valid CORS response header.
  // params.corb_excluded_resource_type does not apply in that case.
  ASSERT_EQ(std::string(), body);
}

// This simulates plugins with universal access, like Flash. These can make
// cross-origin requests that are not subject to CORB.
TEST_F(URLLoaderTest, CorbExcludedWithNoCors) {
  int kResourceType = 1;
  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/hello.html"));
  request.resource_type = kResourceType;
  request.fetch_request_mode = mojom::FetchRequestMode::kNoCORS;
  request.request_initiator = url::Origin::Create(GURL("http://foo.com/"));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.corb_excluded_resource_type = kResourceType;
  params.process_id = 123;
  CrossOriginReadBlocking::AddExceptionForPlugin(123);
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilResponseBodyArrived();
  std::string body = ReadBody();

  client()->RunUntilComplete();

  delete_run_loop.Run();

  // The request body is allowed through because CORB isn't applied.
  ASSERT_NE(std::string(), body);

  CrossOriginReadBlocking::RemoveExceptionForPlugin(123);
}

// This simulates a renderer that pretends to be proxying requests for Flash
// (when browser didn't actually confirm that Flash is hosted by the given
// process via CrossOriginReadBlocking::AddExceptionForPlugin).  We should still
// apply CORB in this case.
TEST_F(URLLoaderTest, CorbEffectiveWithNoCorsWhenNoActualPlugin) {
  int kResourceType = 1;
  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/hello.html"));
  request.resource_type = kResourceType;
  request.fetch_request_mode = mojom::FetchRequestMode::kNoCORS;
  request.request_initiator = url::Origin::Create(GURL("http://foo.com/"));

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.corb_excluded_resource_type = kResourceType;
  params.process_id = 234;
  // No call to CrossOriginReadBlocking::AddExceptionForPlugin(123) - this is
  // what we primarily want to cover in this test.
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilResponseBodyArrived();
  std::string body = ReadBody();

  client()->RunUntilComplete();

  delete_run_loop.Run();

  // The request body should be blocked by CORB.
  ASSERT_EQ(std::string(), body);
}

// Make sure the client can't call FollowRedirect if there's no pending
// redirect.
TEST_F(URLLoaderTest, FollowRedirectTwice) {
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server()->GetURL("/redirect307-to-echo"));
  request.headers.SetHeader("Header1", "Value1");
  request.headers.SetHeader("Header2", "Value2");

  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  std::unique_ptr<URLLoader> url_loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = mojom::kBrowserProcessId;
  params.is_corb_enabled = false;
  url_loader = std::make_unique<URLLoader>(
      context(), nullptr /* network_service_client */,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), mojom::kURLLoadOptionNone, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  client()->RunUntilRedirectReceived();

  url_loader->FollowRedirect(base::nullopt, base::nullopt);
  EXPECT_DCHECK_DEATH(url_loader->FollowRedirect(base::nullopt, base::nullopt));

  client()->RunUntilComplete();
  delete_run_loop.Run();
}

class TestSSLPrivateKey : public net::SSLPrivateKey {
 public:
  explicit TestSSLPrivateKey(scoped_refptr<net::SSLPrivateKey> key)
      : key_(std::move(key)) {}

  void set_fail_signing(bool fail_signing) { fail_signing_ = fail_signing; }
  int sign_count() const { return sign_count_; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return key_->GetAlgorithmPreferences();
  }
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    sign_count_++;
    if (fail_signing_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
                                    std::vector<uint8_t>()));
    } else {
      key_->Sign(algorithm, input, std::move(callback));
    }
  }

 private:
  ~TestSSLPrivateKey() override = default;

  scoped_refptr<net::SSLPrivateKey> key_;
  bool fail_signing_ = false;
  int sign_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestSSLPrivateKey);
};

#if !defined(OS_IOS)
TEST_F(URLLoaderTest, ClientAuthCancelConnection) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  MockNetworkServiceClient network_service_client;
  network_service_client.set_certificate_response(
      MockNetworkServiceClient::CertificateResponse::
          URL_LOADER_REQUEST_CANCELLED);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);
  network_service_client.set_url_loader_ptr(&loader);

  RunUntilIdle();
  ASSERT_TRUE(url_loader);

  client()->RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, client()->completion_status().error_code);
}

TEST_F(URLLoaderTest, ClientAuthCancelCertificateSelection) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  MockNetworkServiceClient network_service_client;
  network_service_client.set_certificate_response(
      MockNetworkServiceClient::CertificateResponse::
          CANCEL_CERTIFICATE_SELECTION);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  RunUntilIdle();
  ASSERT_TRUE(url_loader);

  EXPECT_EQ(0, network_service_client.on_certificate_requested_counter());

  client()->RunUntilComplete();

  EXPECT_EQ(1, network_service_client.on_certificate_requested_counter());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            client()->completion_status().error_code);
}

TEST_F(URLLoaderTest, ClientAuthNoCertificate) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  MockNetworkServiceClient network_service_client;
  network_service_client.set_certificate_response(
      MockNetworkServiceClient::CertificateResponse::NULL_CERTIFICATE);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  RunUntilIdle();
  ASSERT_TRUE(url_loader);

  EXPECT_EQ(0, network_service_client.on_certificate_requested_counter());

  client()->RunUntilComplete();

  EXPECT_EQ(1, network_service_client.on_certificate_requested_counter());
  EXPECT_EQ(net::ERR_BAD_SSL_CLIENT_AUTH_CERT,
            client()->completion_status().error_code);
}

TEST_F(URLLoaderTest, ClientAuthCertificateWithValidSignature) {
  std::unique_ptr<net::FakeClientCertIdentity> identity =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());
  TestSSLPrivateKey* private_key_ptr = private_key.get();

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  MockNetworkServiceClient network_service_client;
  network_service_client.set_certificate_response(
      MockNetworkServiceClient::CertificateResponse::
          VALID_CERTIFICATE_SIGNATURE);
  network_service_client.set_private_key(std::move(private_key));
  scoped_refptr<net::X509Certificate> certificate =
      test_server.GetCertificate();
  network_service_client.set_certificate(std::move(certificate));

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  RunUntilIdle();
  ASSERT_TRUE(url_loader);

  EXPECT_EQ(0, network_service_client.on_certificate_requested_counter());
  EXPECT_EQ(0, private_key_ptr->sign_count());

  client()->RunUntilComplete();

  EXPECT_EQ(1, network_service_client.on_certificate_requested_counter());
  // The private key should have been used.
  EXPECT_EQ(1, private_key_ptr->sign_count());
}

TEST_F(URLLoaderTest, ClientAuthCertificateWithInvalidSignature) {
  std::unique_ptr<net::FakeClientCertIdentity> identity =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());
  private_key->set_fail_signing(true);
  TestSSLPrivateKey* private_key_ptr = private_key.get();

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  MockNetworkServiceClient network_service_client;
  network_service_client.set_certificate_response(
      MockNetworkServiceClient::CertificateResponse::
          VALID_CERTIFICATE_SIGNATURE);
  network_service_client.set_private_key(std::move(private_key));
  scoped_refptr<net::X509Certificate> certificate =
      test_server.GetCertificate();
  network_service_client.set_certificate(std::move(certificate));

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojom::URLLoaderPtr loader;
  mojom::URLLoaderFactoryParams params;
  params.process_id = kProcessId;
  params.is_corb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = std::make_unique<URLLoader>(
      context(), &network_service_client,
      DeleteLoaderCallback(&delete_run_loop, &url_loader),
      mojo::MakeRequest(&loader), 0, request, false,
      client()->CreateInterfacePtr(), TRAFFIC_ANNOTATION_FOR_TESTS, &params,
      0 /* request_id */, resource_scheduler_client(), nullptr,
      nullptr /* network_usage_accumulator */);

  RunUntilIdle();
  ASSERT_TRUE(url_loader);

  EXPECT_EQ(0, network_service_client.on_certificate_requested_counter());
  EXPECT_EQ(0, private_key_ptr->sign_count());

  client()->RunUntilComplete();

  EXPECT_EQ(1, network_service_client.on_certificate_requested_counter());
  // The private key should have been used.
  EXPECT_EQ(1, private_key_ptr->sign_count());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
            client()->completion_status().error_code);
}
#endif  // !defined(OS_IOS)

}  // namespace network
