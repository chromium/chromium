// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "net/base/transport_info.h"
#include "net/cert/test_root_certs.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/storage_access_api/status.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/attribution/attribution_request_helper.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_access_observer.mojom-forward.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/shared_dictionary/shared_dictionary_access_checker.h"
#include "services/network/shared_storage/shared_storage_header_utils.h"
#include "services/network/shared_storage/shared_storage_request_helper.h"
#include "services/network/shared_storage/shared_storage_test_url_loader_network_observer.h"
#include "services/network/shared_storage/shared_storage_test_utils.h"
#include "services/network/test/mock_devtools_observer.h"
#include "services/network/test/test_data_pipe_getter.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_network_observer.h"
#include "services/network/test/url_loader_context_for_tests.h"
#include "services/network/test_chunked_data_pipe_getter.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/trust_tokens/trust_token_request_helper_factory.h"
#include "services/network/url_request_context_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

using ::net::test::IsError;
using ::net::test::IsOk;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::SizeIs;
using ::testing::ValuesIn;

// Returns a URLLoader::DeleteCallback that destroys |url_loader| and quits
// |run_loop| when invoked. Tests must wait on the RunLoop to ensure nothing is
// leaked. Takes a std::unique_ptr<URLLoader>* instead of a unique_ptr because
// the callback must be created before the URLLoader is actually created.
URLLoader::DeleteCallback DeleteLoaderCallback(
    base::RunLoop* run_loop,
    std::unique_ptr<URLLoader>* url_loader) {
  return base::BindOnce(
      [](base::RunLoop* run_loop, std::unique_ptr<URLLoader>* url_loader,
         URLLoader* url_loader_ptr) {
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
  return base::BindOnce(
      [](URLLoader* /* loader*/) { NOTREACHED_IN_MIGRATION(); });
}

constexpr char kTestAuthURL[] = "/auth-basic?password=PASS&realm=REALM";

constexpr char kInsecureHost[] = "othersite.test";

constexpr char kHostnameWithAliases[] = "www.example.test";

constexpr char kHostnameWithoutAliases[] = "www.other.test";

static ResourceRequest CreateResourceRequest(const char* method,
                                             const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.site_for_cookies =
      net::SiteForCookies::FromUrl(url);  // bypass third-party cookie blocking
  url::Origin origin = url::Origin::Create(url);
  request.request_initiator = origin;  // ensure initiator is set
  request.is_outermost_main_frame = true;
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);
  return request;
}

class URLRequestMultipleWritesJob : public net::URLRequestJob {
 public:
  URLRequestMultipleWritesJob(net::URLRequest* request,
                              std::list<std::string> packets,
                              net::Error net_error,
                              bool async_reads)
      : URLRequestJob(request),
        packets_(std::move(packets)),
        net_error_(net_error),
        async_reads_(async_reads) {}

  URLRequestMultipleWritesJob(const URLRequestMultipleWritesJob&) = delete;
  URLRequestMultipleWritesJob& operator=(const URLRequestMultipleWritesJob&) =
      delete;

  ~URLRequestMultipleWritesJob() override = default;

  // net::URLRequestJob implementation:
  void Start() override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&URLRequestMultipleWritesJob::ReadRawDataComplete,
                         weak_factory_.GetWeakPtr(), result));
      return net::ERR_IO_PENDING;
    }
    return result;
  }

 private:
  void StartAsync() { NotifyHeadersComplete(); }

  std::list<std::string> packets_;
  net::Error net_error_;
  bool async_reads_;

  base::WeakPtrFactory<URLRequestMultipleWritesJob> weak_factory_{this};
};

class MultipleWritesInterceptor : public net::URLRequestInterceptor {
 public:
  MultipleWritesInterceptor(std::list<std::string> packets,
                            net::Error net_error,
                            bool async_reads)
      : packets_(std::move(packets)),
        net_error_(net_error),
        async_reads_(async_reads) {}

  MultipleWritesInterceptor(const MultipleWritesInterceptor&) = delete;
  MultipleWritesInterceptor& operator=(const MultipleWritesInterceptor&) =
      delete;

  ~MultipleWritesInterceptor() override {}

  static GURL GetURL() { return GURL("http://foo"); }

  // URLRequestInterceptor implementation:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_FALSE(request->load_flags() & net::LOAD_BYPASS_CACHE);
    return std::make_unique<URLRequestMultipleWritesJob>(
        request, std::move(packets_), net_error_, async_reads_);
  }

 private:
  std::list<std::string> packets_;
  net::Error net_error_;
  bool async_reads_;
};

// Every read completes synchronously.
class URLRequestEternalSyncReadsJob : public net::URLRequestJob {
 public:
  // If |fill_entire_buffer| is true, each read fills the entire read buffer at
  // once. Otherwise, one byte is read at a time.
  URLRequestEternalSyncReadsJob(net::URLRequest* request,
                                bool fill_entire_buffer)
      : URLRequestJob(request), fill_entire_buffer_(fill_entire_buffer) {}

  URLRequestEternalSyncReadsJob(const URLRequestEternalSyncReadsJob&) = delete;
  URLRequestEternalSyncReadsJob& operator=(
      const URLRequestEternalSyncReadsJob&) = delete;

  ~URLRequestEternalSyncReadsJob() override = default;

  // net::URLRequestJob implementation:
  void Start() override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  void StartAsync() { NotifyHeadersComplete(); }

  const bool fill_entire_buffer_;

  base::WeakPtrFactory<URLRequestEternalSyncReadsJob> weak_factory_{this};
};

class EternalSyncReadsInterceptor : public net::URLRequestInterceptor {
 public:
  EternalSyncReadsInterceptor() {}

  EternalSyncReadsInterceptor(const EternalSyncReadsInterceptor&) = delete;
  EternalSyncReadsInterceptor& operator=(const EternalSyncReadsInterceptor&) =
      delete;

  ~EternalSyncReadsInterceptor() override {}

  static std::string GetHostName() { return "eternal"; }

  static GURL GetSingleByteURL() { return GURL("http://eternal/single-byte"); }
  static GURL GetFillBufferURL() { return GURL("http://eternal/fill-buffer"); }

  // URLRequestInterceptor implementation:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_FALSE(request->load_flags() & net::LOAD_BYPASS_CACHE);
    if (request->url() == GetSingleByteURL()) {
      return std::make_unique<URLRequestEternalSyncReadsJob>(
          request, false /* fill_entire_buffer */);
    }
    if (request->url() == GetFillBufferURL()) {
      return std::make_unique<URLRequestEternalSyncReadsJob>(
          request, true /* fill_entire_buffer */);
    }
    return nullptr;
  }
};

// Simulates handing over things to the disk to write before returning to the
// caller.
class URLRequestSimulatedCacheJob : public net::URLRequestJob {
 public:
  // If |fill_entire_buffer| is true, each read fills the entire read buffer at
  // once. Otherwise, one byte is read at a time.
  URLRequestSimulatedCacheJob(
      net::URLRequest* request,
      scoped_refptr<net::IOBuffer>* simulated_cache_dest,
      bool use_text_plain)
      : URLRequestJob(request),
        simulated_cache_dest_(simulated_cache_dest),
        use_text_plain_(use_text_plain) {}

  URLRequestSimulatedCacheJob(const URLRequestSimulatedCacheJob&) = delete;
  URLRequestSimulatedCacheJob& operator=(const URLRequestSimulatedCacheJob&) =
      delete;

  ~URLRequestSimulatedCacheJob() override = default;

  // net::URLRequestJob implementation:
  void Start() override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestSimulatedCacheJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    if (!use_text_plain_) {
      return URLRequestJob::GetResponseInfo(info);
    }
    if (!info->headers) {
      info->headers = net::HttpResponseHeaders::TryToCreate(
          "HTTP/1.1 200 OK\r\nContent-Type: text/plain");
    }
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
  void StartAsync() { NotifyHeadersComplete(); }

  raw_ptr<scoped_refptr<net::IOBuffer>> simulated_cache_dest_;
  bool use_text_plain_;
  base::WeakPtrFactory<URLRequestSimulatedCacheJob> weak_factory_{this};
};

class SimulatedCacheInterceptor : public net::URLRequestInterceptor {
 public:
  explicit SimulatedCacheInterceptor(
      scoped_refptr<net::IOBuffer>* simulated_cache_dest,
      bool use_text_plain)
      : simulated_cache_dest_(simulated_cache_dest),
        use_text_plain_(use_text_plain) {}

  SimulatedCacheInterceptor(const SimulatedCacheInterceptor&) = delete;
  SimulatedCacheInterceptor& operator=(const SimulatedCacheInterceptor&) =
      delete;

  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_FALSE(request->load_flags() & net::LOAD_BYPASS_CACHE);
    return std::make_unique<URLRequestSimulatedCacheJob>(
        request, simulated_cache_dest_, use_text_plain_);
  }

 private:
  raw_ptr<scoped_refptr<net::IOBuffer>> simulated_cache_dest_;
  bool use_text_plain_;
};

// Observes net error results. Thread-compatible.
class ResultObserver {
 public:
  ResultObserver() = default;

  ResultObserver(const ResultObserver&) = delete;
  ResultObserver& operator=(const ResultObserver&) = delete;

  void Observe(int result) { results_.push_back(result); }

  const std::vector<int>& results() { return results_; }

 private:
  std::vector<int> results_;
};

// Fakes the TransportInfo passed to `URLRequest::Delegate::OnConnected()`.
class URLRequestFakeTransportInfoJob : public net::URLRequestJob {
 public:
  // `transport_info` is subsequently passed to the `OnConnected()` callback
  // during `Start()`.
  // `second_transport_info`, if non-nullopt, is passed to the `OnConnected()`
  // callback during `ReadRawData()`.
  // `connected_callback_result_observer`, if non-nullptr, is notified of all
  // the results of calling `OnConnected()`.
  URLRequestFakeTransportInfoJob(
      net::URLRequest* request,
      net::TransportInfo transport_info,
      std::optional<net::TransportInfo> second_transport_info,
      ResultObserver* connected_callback_result_observer)
      : URLRequestJob(request),
        transport_info_(std::move(transport_info)),
        second_transport_info_(std::move(second_transport_info)),
        connected_callback_result_observer_(
            connected_callback_result_observer) {}

  URLRequestFakeTransportInfoJob(const URLRequestFakeTransportInfoJob&) =
      delete;
  URLRequestFakeTransportInfoJob& operator=(
      const URLRequestFakeTransportInfoJob&) = delete;

  ~URLRequestFakeTransportInfoJob() override = default;

  // net::URLRequestJob implementation:
  void Start() override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestFakeTransportInfoJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    if (!second_transport_info_) {
      return 0;
    }

    int result = NotifyConnected(
        *second_transport_info_, base::BindLambdaForTesting([](int result) {
          ADD_FAILURE() << "NotifyConnected() callback called with " << result;
        }));
    ObserveResult(result);
    return result;
  }

 private:
  void StartAsync() {
    // Simulate notifying caller of a connection. Call NotifyStartError() or
    // NotifyHeadersComplete() synchronously/asynchronously on error/success,
    // depending on return value and callback invocation.
    const int result = NotifyConnected(
        transport_info_,
        base::BindOnce(
            &URLRequestFakeTransportInfoJob::ConnectedCallbackComplete,
            base::Unretained(this)));

    // Wait for callback to be invoked in async case.
    if (result == net::ERR_IO_PENDING) {
      return;
    }

    ConnectedCallbackComplete(result);
  }

  void ConnectedCallbackComplete(int result) {
    ObserveResult(result);

    if (result != net::OK) {
      NotifyStartError(result);
      return;
    }

    NotifyHeadersComplete();
  }

  void ObserveResult(int result) {
    if (!connected_callback_result_observer_) {
      return;
    }

    connected_callback_result_observer_->Observe(result);
  }

  // The fake transport info we pass to `NotifyConnected()` during `Start()`.
  const net::TransportInfo transport_info_;

  // An optional fake transport info we pass to `NotifyConnected()` during
  // `ReadRawData()`.
  const std::optional<net::TransportInfo> second_transport_info_;

  // An observer to be called with each result of calling `NotifyConnected()`.
  raw_ptr<ResultObserver> connected_callback_result_observer_;

  base::WeakPtrFactory<URLRequestFakeTransportInfoJob> weak_factory_{this};
};

// Intercepts URLRequestJob creation to a specific URL. All requests to this
// URL will report being connected with a fake TransportInfo struct.
class FakeTransportInfoInterceptor : public net::URLRequestInterceptor {
 public:
  // All intercepted requests will claim to be connected via |transport_info|.
  explicit FakeTransportInfoInterceptor(
      const net::TransportInfo& transport_info)
      : transport_info_(transport_info) {}

  // Sets a second transport info that will be passed to `OnConnected()` when
  // the response data is read.
  void SetSecondTransportInfo(const net::TransportInfo& transport_info) {
    second_transport_info_ = transport_info;
  }

  // Sets up `observer` to be notified of the result of all future calls to
  // `NotifyConnected` from within `URLRequestFakeTransportInfoJob`.
  //
  // `observer` must outlive this instance.
  void SetConnectedCallbackResultObserver(ResultObserver* observer) {
    connected_callback_result_observer_ = observer;
  }

  ~FakeTransportInfoInterceptor() override = default;

  FakeTransportInfoInterceptor(const FakeTransportInfoInterceptor&) = delete;
  FakeTransportInfoInterceptor& operator=(const FakeTransportInfoInterceptor&) =
      delete;

  // URLRequestInterceptor implementation:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_FALSE(request->load_flags() & net::LOAD_BYPASS_CACHE);
    return std::make_unique<URLRequestFakeTransportInfoJob>(
        request, transport_info_, second_transport_info_,
        connected_callback_result_observer_);
  }

 private:
  const net::TransportInfo transport_info_;
  std::optional<net::TransportInfo> second_transport_info_;

  raw_ptr<ResultObserver> connected_callback_result_observer_ = nullptr;
};

// Returns a maximally-restrictive security state for use in tests.
mojom::ClientSecurityStatePtr NewSecurityState() {
  auto result = mojom::ClientSecurityState::New();
  result->is_web_secure_context = false;
  result->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;
  result->ip_address_space = mojom::IPAddressSpace::kUnknown;
  return result;
}

CorsErrorStatus InsecurePrivateNetworkCorsErrorStatus(
    mojom::IPAddressSpace resource_address_space) {
  return CorsErrorStatus(mojom::CorsError::kInsecurePrivateNetwork,
                         mojom::IPAddressSpace::kUnknown,
                         resource_address_space);
}

std::string CookieOrLineToString(const mojom::CookieOrLinePtr& cookie_or_line) {
  switch (cookie_or_line->which()) {
    case mojom::CookieOrLine::Tag::kCookie:
      return base::StrCat({
          cookie_or_line->get_cookie().Name(),
          "=",
          cookie_or_line->get_cookie().Value(),
      });
    case mojom::CookieOrLine::Tag::kCookieString:
      return cookie_or_line->get_cookie_string();
  }
}

MATCHER_P2(CookieOrLine, string, type, "") {
  return type == arg->which() &&
         testing::ExplainMatchResult(string, CookieOrLineToString(arg),
                                     result_listener);
}

// Permits simply constructing a URLLoader object specifying only those options
// which are non-default.
// Replace:
//   auto url_loader = std::make_unique<URLLoader>(context(), ...,
//                                                 /*some_option=*/true);
// With:
//   URLLoaderOptions options;
//   options.some_option = true;
//   auto url_loader = options.MakeURLLoader(context(), ...);
struct URLLoaderOptions {
  // Non-optional arguments are passed to MakeURLLoader(). MakeURLLoader() can
  // only be called once for each URLLoaderOptions object.
  std::unique_ptr<URLLoader> MakeURLLoader(
      URLLoaderContext& context,
      URLLoader::DeleteCallback delete_callback,
      mojo::PendingReceiver<mojom::URLLoader> url_loader_receiver,
      const ResourceRequest& request,
      mojo::PendingRemote<mojom::URLLoaderClient> url_loader_client) {
    DCHECK(!used);
    used = true;
    return std::make_unique<URLLoader>(
        context, std::move(delete_callback), std::move(url_loader_receiver),
        options, request, std::move(url_loader_client),
        std::move(sync_url_loader_client), traffic_annotation, request_id,
        keepalive_request_size, std::move(keepalive_statistics_recorder),
        std::move(trust_token_helper_factory),
        std::move(shared_dictionary_manager),
        std::move(shared_dictionary_checker), std::move(cookie_observer),
        std::move(trust_token_observer), std::move(url_loader_network_observer),
        std::move(devtools_observer), std::move(accept_ch_frame_observer),
        std::move(attribution_request_helper),
        shared_storage_writable_eligible);
  }

  int32_t options = mojom::kURLLoadOptionNone;
  base::WeakPtr<mojom::URLLoaderClient> sync_url_loader_client;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      TRAFFIC_ANNOTATION_FOR_TESTS;
  int32_t request_id = 0;
  int keepalive_request_size = 0;
  base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder;
  std::unique_ptr<TrustTokenRequestHelperFactory> trust_token_helper_factory;
  raw_ptr<SharedDictionaryManager> shared_dictionary_manager;
  std::unique_ptr<SharedDictionaryAccessChecker> shared_dictionary_checker;
  std::unique_ptr<AttributionRequestHelper> attribution_request_helper;
  mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer =
      mojo::NullRemote();
  mojo::PendingRemote<mojom::TrustTokenAccessObserver> trust_token_observer =
      mojo::NullRemote();
  mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer = mojo::NullRemote();
  mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer =
      mojo::NullRemote();
  mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer =
      mojo::NullRemote();
  bool shared_storage_writable_eligible = false;

 private:
  bool used = false;
};

}  // namespace

class MockAcceptCHFrameObserver : public mojom::AcceptCHFrameObserver {
 public:
  MockAcceptCHFrameObserver() = default;
  ~MockAcceptCHFrameObserver() override = default;

  mojo::PendingRemote<mojom::AcceptCHFrameObserver> Bind() {
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void OnAcceptCHFrameReceived(
      const url::Origin& origin,
      const std::vector<network::mojom::WebClientHintsType>& accept_ch_frame,
      OnAcceptCHFrameReceivedCallback callback) override {
    called_ = true;
    accept_ch_frame_ = accept_ch_frame;
    std::move(callback).Run(net::OK);
  }
  void Clone(mojo::PendingReceiver<network::mojom::AcceptCHFrameObserver>
                 listener) override {
    receivers_.Add(this, std::move(listener));
  }

  bool called() const { return called_; }

  const std::vector<network::mojom::WebClientHintsType>& accept_ch_frame()
      const {
    return accept_ch_frame_;
  }

 private:
  bool called_ = false;
  std::vector<network::mojom::WebClientHintsType> accept_ch_frame_;
  mojo::ReceiverSet<mojom::AcceptCHFrameObserver> receivers_;
};

class URLLoaderTest : public testing::Test {
 public:
  URLLoaderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    scoped_refptr<net::X509Certificate> quic_root = net::ImportCertFromFile(
        net::GetTestCertsDirectory().AppendASCII("quic-root.pem"));
    if (quic_root) {
      scoped_test_root_.Reset({quic_root});
    } else {
      ADD_FAILURE();
    }

    net::QuicSimpleTestServer::Start();
    net::URLRequestFailedJob::AddUrlHandler();

    scoped_feature_list_.InitAndEnableFeature(features::kAcceptCHFrame);
  }
  ~URLLoaderTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void SetUp() override {
    net::HttpNetworkSessionParams params;
    auto quic_context = std::make_unique<net::QuicContext>();
    quic_context->params()->origins_to_force_quic_on.insert(
        net::HostPortPair(net::QuicSimpleTestServer::GetHost(),
                          net::QuicSimpleTestServer::GetPort()));
    params.enable_quic = true;
    net::URLRequestContextBuilder context_builder;
    context_builder.set_http_network_session_params(params);
    context_builder.set_quic_context(std::move(quic_context));
    context_builder.set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateDirect());
    auto test_network_delegate = std::make_unique<net::TestNetworkDelegate>();
    unowned_test_network_delegate_ = test_network_delegate.get();
    context_builder.set_network_delegate(std::move(test_network_delegate));
    context_builder.set_client_socket_factory_for_testing(GetSocketFactory());
    url_request_context_ = context_builder.Build();
    context().set_url_request_context(url_request_context_.get());
    resource_scheduler_client_ = base::MakeRefCounted<ResourceSchedulerClient>(
        ResourceScheduler::ClientId::Create(), IsBrowserInitiated(false),
        &resource_scheduler_,
        url_request_context_->network_quality_estimator());
    context().set_resource_scheduler_client(resource_scheduler_client_.get());

    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("services/test/data")));
    // This Unretained is safe because test_server_ is owned by |this|.
    test_server_.RegisterRequestMonitor(
        base::BindRepeating(&URLLoaderTest::Monitor, base::Unretained(this)));
    RegisterAdditionalHandlers();
    ASSERT_TRUE(test_server_.Start());

    // Set up a scoped host resolver so that |kInsecureHost| will resolve to
    // the loopback address and will let us access |test_server_|.
    scoped_refptr<net::RuleBasedHostResolverProc> mock_resolver_proc =
        base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);
    mock_resolver_proc->AddIPLiteralRuleWithDnsAliases(
        kHostnameWithAliases, "127.0.0.1", {"alias1", "alias2", "host"});
    mock_resolver_proc->AddRule("*", "127.0.0.1");
    mock_host_resolver_ = std::make_unique<net::ScopedDefaultHostResolverProc>(
        mock_resolver_proc.get());
  }

  void TearDown() override {
    context().Detach();
    unowned_test_network_delegate_ = nullptr;
    url_request_context_.reset();
    net::QuicSimpleTestServer::Shutdown();
  }

  // For derived classes to register additional handlers for `test_server_`
  // before the server is started.
  virtual void RegisterAdditionalHandlers() {}

  // Attempts to load |url| and returns the resulting error code.
  [[nodiscard]] int Load(const GURL& url, std::string* body = nullptr) {
    DCHECK(!ran_);

    ResourceRequest request =
        CreateResourceRequest(!request_body_ ? "GET" : "POST", url);

    if (request_body_) {
      request.request_body = request_body_;
    }

    request.trusted_params->client_security_state.Swap(
        &request_client_security_state_);

    request.headers.MergeFrom(additional_headers_);

    request.target_ip_address_space = target_ip_address_space_;

    return LoadRequest(request, body);
  }

  // Attempts to load |request| and returns the resulting error code. If |body|
  // is non-NULL, also attempts to read the response body. The advantage of
  // using |body| instead of calling ReadBody() after Load is that it will load
  // the response body before URLLoader is complete, so URLLoader completion
  // won't block on trying to write the body buffer.
  [[nodiscard]] int LoadRequest(const ResourceRequest& request,
                                std::string* body = nullptr,
                                bool is_trusted = true) {
    uint32_t options = mojom::kURLLoadOptionNone;
    if (send_ssl_with_response_) {
      options |= mojom::kURLLoadOptionSendSSLInfoWithResponse;
    }
    if (sniff_) {
      options |= mojom::kURLLoadOptionSniffMimeType;
    }
    if (send_ssl_for_cert_error_) {
      options |= mojom::kURLLoadOptionSendSSLInfoForCertificateError;
    }

    std::unique_ptr<TestNetworkContextClient> network_context_client;
    std::unique_ptr<TestURLLoaderNetworkObserver> url_loader_network_observer;
    if (allow_file_uploads_) {
      network_context_client = std::make_unique<TestNetworkContextClient>();
      network_context_client->set_upload_files_invalid(upload_files_invalid_);
      network_context_client->set_ignore_last_upload_file(
          ignore_last_upload_file_);
    }
    context().set_network_context_client(network_context_client.get());
    if (ignore_certificate_errors_) {
      url_loader_network_observer =
          std::make_unique<TestURLLoaderNetworkObserver>();
      url_loader_network_observer->set_ignore_certificate_errors(true);
    }

    base::RunLoop delete_run_loop;
    mojo::Remote<mojom::URLLoader> loader;
    std::unique_ptr<URLLoader> url_loader;

    SetUpContext(request.url, is_trusted);

    URLLoaderOptions url_loader_options;
    url_loader_options.options = options;
    url_loader_options.url_loader_network_observer =
        url_loader_network_observer ? url_loader_network_observer->Bind()
                                    : mojo::NullRemote();
    url_loader_options.devtools_observer =
        devtools_observer_ ? devtools_observer_->Bind() : mojo::NullRemote();
    url_loader_options.accept_ch_frame_observer =
        accept_ch_frame_observer_ ? accept_ch_frame_observer_->Bind()
                                  : mojo::NullRemote();
    url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.BindNewPipeAndPassReceiver(), request, client_.CreateRemote());

    ran_ = true;
    devtools_observer_ = nullptr;
    accept_ch_frame_observer_ = nullptr;

    if (expect_redirect_) {
      client_.RunUntilRedirectReceived();
      loader->FollowRedirect({}, {}, {}, std::nullopt);
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

    context().set_network_context_client(nullptr);
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

  void SetUpContext(const GURL& url, bool is_trusted) {
    context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
    context().mutable_factory_params().is_orb_enabled = orb_enabled_;
    context().mutable_factory_params().client_security_state.Swap(
        &factory_client_security_state_);
    context().mutable_factory_params().isolation_info =
        net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url));
    context().mutable_factory_params().is_trusted = is_trusted;
    context().mutable_factory_params().cookie_setting_overrides =
        cookie_setting_overrides_;
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
    if (!second.empty()) {
      packets.push_back(second);
    }
    AddMultipleWritesInterceptor(std::move(packets), net::OK,
                                 false /* async_reads */);

    std::string expected_body = first + second;
    std::string actual_body;
    EXPECT_EQ(net::OK, Load(MultipleWritesInterceptor::GetURL(), &actual_body));

    EXPECT_EQ(actual_body, expected_body);
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }
  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }
  URLLoaderContextForTests& context() { return url_loader_context_for_tests_; }
  TestURLLoaderClient* client() { return &client_; }

  // Returns the path of the requested file in the test data directory.
  base::FilePath GetTestFilePath(const std::string& file_name) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("services"));
    file_path = file_path.Append(FILE_PATH_LITERAL("test"));
    file_path = file_path.Append(FILE_PATH_LITERAL("data"));
    return file_path.AppendASCII(file_name);
  }

  base::File OpenFileForUpload(const base::FilePath& file_path) {
    int open_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
#if BUILDFLAG(IS_WIN)
    open_flags |= base::File::FLAG_ASYNC;
#endif  //  BUILDFLAG(IS_WIN)
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
  void set_upload_files_invalid(bool upload_files_invalid) {
    DCHECK(!ran_);
    upload_files_invalid_ = upload_files_invalid;
  }
  void set_ignore_last_upload_file(bool ignore_last_upload_file) {
    DCHECK(!ran_);
    ignore_last_upload_file_ = ignore_last_upload_file;
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
  void set_ignore_certificate_errors() {
    DCHECK(!ran_);
    ignore_certificate_errors_ = true;
  }
  void set_expect_redirect() {
    DCHECK(!ran_);
    expect_redirect_ = true;
  }
  void set_factory_client_security_state(mojom::ClientSecurityStatePtr state) {
    factory_client_security_state_ = std::move(state);
  }
  void set_request_client_security_state(mojom::ClientSecurityStatePtr state) {
    request_client_security_state_ = std::move(state);
  }
  void set_devtools_observer_for_next_request(MockDevToolsObserver* observer) {
    devtools_observer_ = observer;
  }
  void set_request_body(scoped_refptr<ResourceRequestBody> request_body) {
    request_body_ = request_body;
  }
  void set_additional_headers(const net::HttpRequestHeaders& headers) {
    additional_headers_ = headers;
  }
  void set_target_ip_address_space(mojom::IPAddressSpace address_space) {
    target_ip_address_space_ = address_space;
  }
  void set_accept_ch_frame_observer_for_next_request(
      MockAcceptCHFrameObserver* observer) {
    accept_ch_frame_observer_ = observer;
  }
  void set_cookie_setting_overrides(
      const net::CookieSettingOverrides& overrides) {
    cookie_setting_overrides_ = overrides;
  }

  // Convenience methods after calling Load();
  std::string mime_type() const {
    DCHECK(ran_);
    return client_.response_head()->mime_type;
  }

  bool did_mime_sniff() const {
    DCHECK(ran_);
    return client_.response_head()->did_mime_sniff;
  }

  const std::optional<net::SSLInfo>& ssl_info() const {
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
            base::SequencedTaskRunner::GetCurrentDefault());
        base::RunLoop run_loop;

        watcher.Watch(
            client()->response_body(),
            MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            MOJO_WATCH_CONDITION_SATISFIED,
            base::BindRepeating(
                [](base::RepeatingClosure quit, MojoResult result,
                   const mojo::HandleSignalsState& state) { quit.Run(); },
                run_loop.QuitClosure()));
        run_loop.Run();
        continue;
      }

      // The pipe was closed.
      if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
        return body;
      }

      CHECK_EQ(rv, MOJO_RESULT_OK);

      body.append(static_cast<const char*>(buffer), num_bytes);
      MojoEndReadData(consumer, num_bytes, nullptr);
    }
  }

  std::string ReadAvailableBody() {
    MojoHandle consumer = client()->response_body().value();

    uint32_t num_bytes = 0;
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_READ_DATA_FLAG_QUERY;
    MojoResult result = MojoReadData(consumer, &options, nullptr, &num_bytes);
    CHECK_EQ(MOJO_RESULT_OK, result);
    if (num_bytes == 0) {
      return std::string();
    }

    std::vector<char> buffer(num_bytes);
    result = MojoReadData(consumer, nullptr, buffer.data(), &num_bytes);
    CHECK_EQ(MOJO_RESULT_OK, result);
    CHECK_EQ(num_bytes, buffer.size());

    return std::string(buffer.data(), buffer.size());
  }

  const net::test_server::HttpRequest& sent_request() const {
    return sent_request_;
  }

  net::TestNetworkDelegate* test_network_delegate() {
    return unowned_test_network_delegate_;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  static constexpr int kProcessId = 4;
  static constexpr int kRouteId = 8;

  // |OnServerReceivedRequest| allows subclasses to register additional logic to
  // execute once a request reaches the test server.
  virtual void OnServerReceivedRequest(const net::test_server::HttpRequest&) {}

  // Lets subclasses inject a mock ClientSocketFactory.
  virtual net::ClientSocketFactory* GetSocketFactory() { return nullptr; }

  ResourceRequest CreateCrossOriginResourceRequest() {
    ResourceRequest request =
        CreateResourceRequest("GET", test_server()->GetURL("/empty.html"));
    request.request_initiator =
        url::Origin::Create(GURL("http://other-origin.test/"));
    return request;
  }

 protected:
  void Monitor(const net::test_server::HttpRequest& request) {
    sent_request_ = request;
    OnServerReceivedRequest(request);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;
  net::ScopedTestRoot scoped_test_root_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_;
  raw_ptr<net::TestNetworkDelegate>
      unowned_test_network_delegate_;  // owned by |url_request_context_|
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  URLLoaderContextForTests url_loader_context_for_tests_;
  ResourceScheduler resource_scheduler_;
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;

  // Options applied to the created request in Load().
  bool allow_file_uploads_ = false;
  bool upload_files_invalid_ = false;
  bool ignore_last_upload_file_ = false;
  bool sniff_ = false;
  bool send_ssl_with_response_ = false;
  bool send_ssl_for_cert_error_ = false;
  bool ignore_certificate_errors_ = false;
  bool expect_redirect_ = false;
  mojom::ClientSecurityStatePtr factory_client_security_state_;
  mojom::ClientSecurityStatePtr request_client_security_state_;
  raw_ptr<MockDevToolsObserver> devtools_observer_ = nullptr;
  scoped_refptr<ResourceRequestBody> request_body_;
  net::HttpRequestHeaders additional_headers_;
  mojom::IPAddressSpace target_ip_address_space_ =
      mojom::IPAddressSpace::kUnknown;
  net::CookieSettingOverrides cookie_setting_overrides_;

  bool orb_enabled_ = false;

  // Used to ensure that methods are called either before or after a request is
  // made, since the test fixture is meant to be used only once.
  bool ran_ = false;
  net::test_server::HttpRequest sent_request_;
  TestURLLoaderClient client_;

  raw_ptr<MockAcceptCHFrameObserver> accept_ch_frame_observer_ = nullptr;
};

class URLLoaderMockSocketTest : public URLLoaderTest {
 public:
  URLLoaderMockSocketTest() = default;
  ~URLLoaderMockSocketTest() override = default;

  // Lets subclasses inject mock ClientSocketFactories.
  net::ClientSocketFactory* GetSocketFactory() override {
    return &socket_factory_;
  }

 protected:
  net::MockClientSocketFactory socket_factory_;
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

// This test verifies that when the request is same-origin and the origin is
// potentially trustworthy, the request is not blocked.
TEST_F(URLLoaderTest, PotentiallyTrustworthySameOriginIsOk) {
  mojom::ClientSecurityStatePtr client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  GURL url = test_server()->GetURL("/empty.html");
  ResourceRequest request = CreateResourceRequest("GET", url);
  request.request_initiator = url::Origin::Create(url);

  EXPECT_EQ(net::OK, LoadRequest(request));
}

// This test verifies that when the URLLoaderFactory's parameters are missing
// a client security state, requests to local network resources are authorized.
TEST_F(URLLoaderTest, MissingClientSecurityStateIsOk) {
  EXPECT_EQ(net::OK, LoadRequest(CreateCrossOriginResourceRequest()));
}

// This test verifies that when the request's `target_ip_address_space` matches
// the resource's IP address space, then the request is allowed even if it
// would otherwise be blocked by policy.
TEST_F(URLLoaderTest, MatchingTargetIPAddressSpaceIsOk) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();
  request.target_ip_address_space = mojom::IPAddressSpace::kLocal;

  EXPECT_EQ(net::OK, LoadRequest(request));
}

// This test verifies that when the request's `target_ip_address_space` does not
// match the resource's IP address space, and the policy is `kPreflightWarn`,
// then the request is not blocked.
TEST_F(URLLoaderTest, MismatchingTargetIPAddressSpaceWarnIsNotBlocked) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();
  request.target_ip_address_space = mojom::IPAddressSpace::kPrivate;

  EXPECT_EQ(net::OK, LoadRequest(request));
}

// This test verifies that when the request's `target_ip_address_space` does not
// match the resource's IP address space, and the policy is `kPreflightBlock`,
// then the request is blocked.
TEST_F(URLLoaderTest, MismatchingTargetIPAddressSpaceIsBlocked) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();
  request.target_ip_address_space = mojom::IPAddressSpace::kPrivate;

  EXPECT_EQ(net::ERR_FAILED, LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(mojom::CorsError::kInvalidPrivateNetworkAccess,
                               mojom::IPAddressSpace::kPrivate,
                               mojom::IPAddressSpace::kLocal)));
}

// This test verifies that when the request's `target_ip_address_space` does not
// match the resource's IP address space, and the policy is `kPreflightBlock`,
// then `URLLoader::OnConnected()` returns the right error code. This error code
// causes any cache entry in use to be invalidated.
TEST_F(URLLoaderTest, MismatchingTargetIPAddressSpaceErrorCode) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResultObserver connected_callback_result_observer;

  net::TransportInfo info = net::DefaultTransportInfo();
  info.endpoint = net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 80);
  auto interceptor = std::make_unique<FakeTransportInfoInterceptor>(info);

  interceptor->SetConnectedCallbackResultObserver(
      &connected_callback_result_observer);

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::move(interceptor));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.request_initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));
  request.target_ip_address_space = mojom::IPAddressSpace::kPrivate;

  EXPECT_EQ(net::ERR_FAILED, LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(mojom::CorsError::kInvalidPrivateNetworkAccess,
                               mojom::IPAddressSpace::kPrivate,
                               mojom::IPAddressSpace::kLocal)));

  EXPECT_THAT(connected_callback_result_observer.results(),
              ElementsAre(IsError(net::ERR_INCONSISTENT_IP_ADDRESS_SPACE)));
}

// This test verifies that when the request calls `URLLoader::OnConnected()`
// twice with endpoints belonging to different IP address spaces, but the
// private network request policy is `kPreflightWarn`, then the request is not
// blocked.
TEST_F(URLLoaderTest, InconsistentIPAddressSpaceWarnIsNotBlocked) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->is_web_secure_context = true;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  net::TransportInfo info = net::DefaultTransportInfo();
  info.endpoint = net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80);
  auto interceptor = std::make_unique<FakeTransportInfoInterceptor>(info);

  info.endpoint = net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 80);
  interceptor->SetSecondTransportInfo(info);

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::move(interceptor));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.request_initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  EXPECT_EQ(net::OK, LoadRequest(request));
}

// This test verifies that when the request calls `URLLoader::OnConnected()`
// twice with endpoints belonging to different IP address spaces, the request
// fails. In that case `URLLoader::OnConnected()` returns the right error code,
// which causes any cache entry in use to be invalidated.
TEST_F(URLLoaderTest, InconsistentIPAddressSpaceIsBlocked) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->is_web_secure_context = true;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResultObserver connected_callback_result_observer;

  net::TransportInfo info = net::DefaultTransportInfo();
  info.endpoint = net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80);
  auto interceptor = std::make_unique<FakeTransportInfoInterceptor>(info);

  info.endpoint = net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 80);
  interceptor->SetSecondTransportInfo(info);
  interceptor->SetConnectedCallbackResultObserver(
      &connected_callback_result_observer);

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::move(interceptor));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.request_initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  EXPECT_EQ(net::ERR_FAILED, LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(mojom::CorsError::kInvalidPrivateNetworkAccess,
                               // TODO(crbug.com/40208529): Expect
                               // `kPublic` here instead, for better debugging.
                               mojom::IPAddressSpace::kUnknown,
                               mojom::IPAddressSpace::kLocal)));

  // The first connection was fine, but the second was inconsistent.
  EXPECT_THAT(connected_callback_result_observer.results(),
              ElementsAre(IsError(net::OK),
                          IsError(net::ERR_INCONSISTENT_IP_ADDRESS_SPACE)));
}

// These tests verify that requests from both secure and non-secure contexts to
// an IP in the `kLocal` address space are only blocked when the policy is
// `kBlock` and the initiator's address space is not `kLocal`.
//
// NOTE: These tests exercise the same codepath as
// URLLoaderFakeTransportInfoTest below, except they use real URLRequestJob and
// HttpTransaction implementations for higher confidence in the correctness of
// the whole stack. OTOH, using an embedded test server prevents us from mocking
// out the endpoint IP address.

TEST_F(URLLoaderTest, SecureUnknownToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(client()->completion_status().cors_error_status,
              Optional(InsecurePrivateNetworkCorsErrorStatus(
                  mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecureUnknownToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecureUnknownToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecureUnknownToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecureUnknownToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecureUnknownToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(client()->completion_status().cors_error_status,
              Optional(InsecurePrivateNetworkCorsErrorStatus(
                  mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecureUnknownToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecureUnknownToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecureUnknownToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecureUnknownToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kUnknown;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecurePublicToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(client()->completion_status().cors_error_status,
              Optional(InsecurePrivateNetworkCorsErrorStatus(
                  mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecurePublicToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecurePublicToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecurePublicToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecurePublicToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecurePublicToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(client()->completion_status().cors_error_status,
              Optional(InsecurePrivateNetworkCorsErrorStatus(
                  mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecurePublicToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecurePublicToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecurePublicToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecurePublicToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecurePrivateToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(client()->completion_status().cors_error_status,
              Optional(InsecurePrivateNetworkCorsErrorStatus(
                  mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecurePrivateToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecurePrivateToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecurePrivateToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecurePrivateToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecurePrivateToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(client()->completion_status().cors_error_status,
              Optional(InsecurePrivateNetworkCorsErrorStatus(
                  mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecurePrivateToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecurePrivateToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecurePrivateToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, NonSecurePrivateToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));
  EXPECT_THAT(
      client()->completion_status().cors_error_status,
      Optional(CorsErrorStatus(
          mojom::CorsError::kUnexpectedPrivateNetworkAccess,
          mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal)));
}

TEST_F(URLLoaderTest, SecureLocalToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecureLocalToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecureLocalToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecureLocalToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, SecureLocalToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = true;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecureLocalToLocalBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecureLocalToLocalWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecureLocalToLocalAllow) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecureLocalToLocalPreflightBlock) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, NonSecureLocalToLocalPreflightWarn) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
  set_factory_client_security_state(std::move(client_security_state));

  ResourceRequest request = CreateCrossOriginResourceRequest();

  EXPECT_EQ(net::OK, LoadRequest(request));
}

TEST_F(URLLoaderTest, AddsNetLogEntryForPrivateNetworkAccessCheckSuccess) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kLocal;
  set_factory_client_security_state(std::move(client_security_state));

  net::RecordingNetLogObserver net_log_observer;

  ResourceRequest request = CreateCrossOriginResourceRequest();

  std::ignore = LoadRequest(request);

  std::vector<net::NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      net::NetLogEventType::PRIVATE_NETWORK_ACCESS_CHECK);

  ASSERT_THAT(entries, SizeIs(1));

  const base::Value::Dict& params = entries[0].params;

  EXPECT_THAT(params.FindString("client_address_space"), Pointee(Eq("local")));

  EXPECT_THAT(params.FindString("resource_address_space"),
              Pointee(Eq("local")));

  EXPECT_THAT(params.FindString("result"),
              Pointee(Eq("allowed-no-less-public")));
}

TEST_F(URLLoaderTest, AddsNetLogEntryForPrivateNetworkAccessCheckFailure) {
  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  net::RecordingNetLogObserver net_log_observer;

  ResourceRequest request = CreateCrossOriginResourceRequest();

  std::ignore = LoadRequest(request);

  std::vector<net::NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      net::NetLogEventType::PRIVATE_NETWORK_ACCESS_CHECK);

  ASSERT_THAT(entries, SizeIs(1));

  const base::Value::Dict params = std::move(entries[0].params);

  EXPECT_THAT(params.FindString("client_address_space"), Pointee(Eq("public")));

  EXPECT_THAT(params.FindString("resource_address_space"),
              Pointee(Eq("local")));

  EXPECT_THAT(params.FindString("result"),
              Pointee(Eq("blocked-by-policy-preflight-block")));
}

TEST_F(URLLoaderTest, AddsNetLogEntryForPrivateNetworkAccessCheckSameOrigin) {
  mojom::ClientSecurityStatePtr client_security_state = NewSecurityState();
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;
  set_factory_client_security_state(std::move(client_security_state));

  net::RecordingNetLogObserver net_log_observer;

  GURL url = test_server()->GetURL("/empty.html");
  ResourceRequest request = CreateResourceRequest("GET", url);
  request.request_initiator = url::Origin::Create(url);

  EXPECT_EQ(net::OK, LoadRequest(request));

  std::vector<net::NetLogEntry> entries = net_log_observer.GetEntriesWithType(
      net::NetLogEventType::PRIVATE_NETWORK_ACCESS_CHECK);

  ASSERT_THAT(entries, SizeIs(1));

  const base::Value::Dict& params = entries[0].params;

  EXPECT_THAT(params.FindString("client_address_space"), Pointee(Eq("public")));

  EXPECT_THAT(params.FindString("resource_address_space"),
              Pointee(Eq("local")));

  EXPECT_THAT(params.FindString("result"),
              Pointee(Eq("allowed-potentially-trustworthy-same-origin")));
}

// Bundles together the inputs to a parameterized private network request test.
struct URLLoaderFakeTransportInfoTestParams {
  // The address space of the client.
  mojom::IPAddressSpace client_address_space;

  // The address space of the endpoint serving the request.
  mojom::IPAddressSpace endpoint_address_space;

  // The type of transport to set in `TransportInfo`.
  net::TransportType transport_type;

  // The expected request result.
  int expected_result;
};

// For clarity when debugging parameterized test failures.
std::ostream& operator<<(std::ostream& out,
                         const URLLoaderFakeTransportInfoTestParams& params) {
  return out << "{ client_address_space: " << params.client_address_space
             << ", endpoint_address_space: " << params.endpoint_address_space
             << ", transport_type: "
             << net::TransportTypeToString(params.transport_type)
             << ", expected_result: "
             << net::ErrorToString(params.expected_result) << " }";
}

mojom::IPAddressSpace ResponseAddressSpace(
    const URLLoaderFakeTransportInfoTestParams& params) {
  switch (params.transport_type) {
    case net::TransportType::kDirect:
    case net::TransportType::kCached:
      return params.endpoint_address_space;
    case net::TransportType::kProxied:
    case net::TransportType::kCachedFromProxy:
      return mojom::IPAddressSpace::kUnknown;
  }
}

class URLLoaderFakeTransportInfoTest
    : public URLLoaderTest,
      public testing::WithParamInterface<URLLoaderFakeTransportInfoTestParams> {
 protected:
  // Returns an address in the given IP address space.
  static net::IPAddress FakeAddress(mojom::IPAddressSpace space) {
    switch (space) {
      case mojom::IPAddressSpace::kUnknown:
        return net::IPAddress();
      case mojom::IPAddressSpace::kPublic:
        return net::IPAddress(42, 0, 1, 2);
      case mojom::IPAddressSpace::kPrivate:
        return net::IPAddress(10, 0, 1, 2);
      case mojom::IPAddressSpace::kLocal:
        return net::IPAddress::IPv4Localhost();
    }
  }

  // Returns an endpoint in the given IP address space.
  static net::IPEndPoint FakeEndpoint(mojom::IPAddressSpace space) {
    return net::IPEndPoint(FakeAddress(space), 80);
  }

  // Returns a transport info with an endpoint in the given IP address space.
  static net::TransportInfo FakeTransportInfo(
      const URLLoaderFakeTransportInfoTestParams& params) {
    return net::TransportInfo(
        params.transport_type, FakeEndpoint(params.endpoint_address_space),
        /*accept_ch_frame_arg=*/"",
        /*cert_is_issued_by_known_root=*/false, net::kProtoUnknown);
  }
};

// This test verifies that requests made from insecure contexts are handled
// appropriately when they go from a less-private address space to a
// more-private address space or not. The test is parameterized by
// (client address space, server address space, expected result) tuple.
TEST_P(URLLoaderFakeTransportInfoTest, PrivateNetworkRequestLoadsCorrectly) {
  const auto params = GetParam();

  auto client_security_state = NewSecurityState();
  client_security_state->ip_address_space = params.client_address_space;
  set_factory_client_security_state(std::move(client_security_state));

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::make_unique<FakeTransportInfoInterceptor>(
               FakeTransportInfo(params)));

  // Despite its name, IsError(OK) asserts that the matched value is OK.
  EXPECT_THAT(Load(url), IsError(params.expected_result));
  if (params.expected_result != net::OK) {
    if (params.expected_result !=
        net::
            ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY) {
      // CORS error status shouldn't be set when the cache entry was blocked by
      // private network access policy because we'll retry fetching from the
      // network.
      EXPECT_THAT(client()->completion_status().cors_error_status,
                  Optional(InsecurePrivateNetworkCorsErrorStatus(
                      params.endpoint_address_space)));
    }
    return;
  }

  // Check that the right address spaces are reported in `URLResponseHead`.
  ASSERT_FALSE(client()->response_head().is_null());
  EXPECT_EQ(client()->response_head()->client_address_space,
            params.client_address_space);
  EXPECT_EQ(client()->response_head()->response_address_space,
            ResponseAddressSpace(params));
}

// Lists all combinations we want to test in URLLoaderFakeTransportInfoTest.
constexpr URLLoaderFakeTransportInfoTestParams
    kURLLoaderFakeTransportInfoTestParamsList[] = {
        // Client: kUnknown
        {
            mojom::IPAddressSpace::kUnknown,
            mojom::IPAddressSpace::kUnknown,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kUnknown,
            mojom::IPAddressSpace::kPublic,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kUnknown,
            mojom::IPAddressSpace::kPrivate,
            net::TransportType::kDirect,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
        },
        {
            mojom::IPAddressSpace::kUnknown,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kDirect,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
        },
        // Client: kPublic
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kUnknown,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kPublic,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kPrivate,
            net::TransportType::kDirect,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kDirect,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
        },
        // Client: kPrivate
        {
            mojom::IPAddressSpace::kPrivate,
            mojom::IPAddressSpace::kUnknown,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPrivate,
            mojom::IPAddressSpace::kPublic,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPrivate,
            mojom::IPAddressSpace::kPrivate,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPrivate,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kDirect,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
        },
        // Client: kLocal
        {
            mojom::IPAddressSpace::kLocal,
            mojom::IPAddressSpace::kUnknown,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kLocal,
            mojom::IPAddressSpace::kPublic,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kLocal,
            mojom::IPAddressSpace::kPrivate,
            net::TransportType::kDirect,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kLocal,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kDirect,
            net::OK,
        },
        // TransportType: kProxied
        {
            mojom::IPAddressSpace::kUnknown,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kProxied,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kProxied,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kPrivate,
            net::TransportType::kProxied,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPrivate,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kProxied,
            net::OK,
        },
        // TransportType: kCachedFromProxy
        {
            mojom::IPAddressSpace::kUnknown,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kCachedFromProxy,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kCachedFromProxy,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kPrivate,
            net::TransportType::kCachedFromProxy,
            net::OK,
        },
        {
            mojom::IPAddressSpace::kPrivate,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kCachedFromProxy,
            net::OK,
        },
        // TransportType: kCached. We only test a local target for brevity.
        {
            mojom::IPAddressSpace::kUnknown,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kCached,
            net::
                ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY,
        },
        {
            mojom::IPAddressSpace::kPublic,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kCached,
            net::
                ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY,
        },
        {
            mojom::IPAddressSpace::kPrivate,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kCached,
            net::
                ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_POLICY,
        },
        {
            mojom::IPAddressSpace::kLocal,
            mojom::IPAddressSpace::kLocal,
            net::TransportType::kCached,
            net::OK,
        },
};

INSTANTIATE_TEST_SUITE_P(Parameterized,
                         URLLoaderFakeTransportInfoTest,
                         ValuesIn(kURLLoaderFakeTransportInfoTestParamsList));

// Tests that auth challenge info is present on the response when a request
// receives an authentication challenge.
TEST_F(URLLoaderTest, AuthChallengeInfo) {
  GURL url = test_server()->GetURL("/auth-basic");
  EXPECT_EQ(net::OK, Load(url));
  ASSERT_TRUE(client()->response_head()->auth_challenge_info.has_value());
  EXPECT_FALSE(client()->response_head()->auth_challenge_info->is_proxy);
  EXPECT_EQ(url::SchemeHostPort(url),
            client()->response_head()->auth_challenge_info->challenger);
  EXPECT_EQ("basic", client()->response_head()->auth_challenge_info->scheme);
  EXPECT_EQ("testrealm", client()->response_head()->auth_challenge_info->realm);
  EXPECT_EQ("Basic realm=\"testrealm\"",
            client()->response_head()->auth_challenge_info->challenge);
  EXPECT_EQ("/auth-basic",
            client()->response_head()->auth_challenge_info->path);
}

// Tests that no auth challenge info is present on the response when a request
// does not receive an authentication challenge.
TEST_F(URLLoaderTest, NoAuthChallengeInfo) {
  GURL url = test_server()->GetURL("/");
  EXPECT_EQ(net::OK, Load(url));
  EXPECT_FALSE(client()->response_head()->auth_challenge_info.has_value());
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
  ASSERT_EQ(std::string("text/plain"), mime_type());
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
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    delegate->SendResponseHeaders(net::HTTP_OK, "OK",
                                  {{"Content-Type", "text/plain"}});
    delegate->SendContents(
        "long long ago..." +
        std::string(static_cast<uint64_t>(1024 * 1024), 'a'));

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
  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

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
  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Wait for a little amount of time for the response body pipe to be filled.
  // (Please note that this doesn't guarantee that the pipe is filled to the
  // point that it is not writable anymore.)
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
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

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();

  std::string available_data = ReadAvailableBody();
  EXPECT_TRUE(available_data.empty());

  loader->ResumeReadingBodyFromNet();
  client()->RunUntilComplete();

  available_data = ReadBody();
  EXPECT_EQ(kBodyContents, available_data);

  delete_run_loop.Run();
  client()->Unbind();
}

TEST_F(URLLoaderTest, PauseReadingBodyFromNetWhenReadIsPending) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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
}

TEST_F(URLLoaderTest, ResumeReadingBodyFromNetAfterClosingConsumer) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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
}

TEST_F(URLLoaderTest, MultiplePauseResumeReadingBodyFromNet) {
  const char* const kPath = "/hello.html";
  const char* const kBodyContentsFirstHalf = "This is the first half.";
  const char* const kBodyContentsSecondHalf = "This is the second half.";

  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response_controller(&server,
                                                                 kPath);
  ASSERT_TRUE(server.Start());

  ResourceRequest request = CreateResourceRequest("GET", server.GetURL(kPath));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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

TEST_F(URLLoaderTest, UploadTwoBatchesOfFiles) {
  allow_file_uploads();
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  std::string expected_body;
  size_t num_files = 2 * kMaxFileUploadRequestsPerBatch;
  for (size_t i = 0; i < num_files; ++i) {
    std::string tmp_expected_body;
    ASSERT_TRUE(base::ReadFileToString(file_path, &tmp_expected_body))
        << "File not found: " << file_path.value();
    expected_body += tmp_expected_body;
  }

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  for (size_t i = 0; i < num_files; ++i) {
    request_body->AppendFileRange(
        file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  }
  set_request_body(std::move(request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(expected_body, response_body);
}

TEST_F(URLLoaderTest, UploadTwoBatchesOfFilesWithRespondInvalidFile) {
  allow_file_uploads();
  set_upload_files_invalid(true);
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  size_t num_files = 2 * kMaxFileUploadRequestsPerBatch;
  for (size_t i = 0; i < num_files; ++i) {
    request_body->AppendFileRange(
        file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  }
  set_request_body(std::move(request_body));

  EXPECT_EQ(net::ERR_ACCESS_DENIED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadTwoBatchesOfFilesWithRespondDifferentNumOfFiles) {
  allow_file_uploads();
  set_ignore_last_upload_file(true);
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  size_t num_files = 2 * kMaxFileUploadRequestsPerBatch;
  for (size_t i = 0; i < num_files; ++i) {
    request_body->AppendFileRange(
        file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  }
  set_request_body(std::move(request_body));

  EXPECT_EQ(net::ERR_FAILED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadInvalidFile) {
  allow_file_uploads();
  set_upload_files_invalid(true);
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendFileRange(
      file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  EXPECT_EQ(net::ERR_ACCESS_DENIED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadFileWithoutNetworkServiceClient) {
  // Don't call allow_file_uploads();
  base::FilePath file_path = GetTestFilePath("simple_page.html");

  scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
  request_body->AppendFileRange(
      file_path, 0, std::numeric_limits<uint64_t>::max(), base::Time());
  set_request_body(std::move(request_body));

  EXPECT_EQ(net::ERR_ACCESS_DENIED, Load(test_server()->GetURL("/echo")));
}

class CallbackSavingNetworkContextClient : public TestNetworkContextClient {
 public:
  void OnFileUploadRequested(int32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             const GURL& destination_url,
                             OnFileUploadRequestedCallback callback) override {
    file_upload_requested_callback_ = std::move(callback);
    if (quit_closure_for_on_file_upload_requested_) {
      std::move(quit_closure_for_on_file_upload_requested_).Run();
    }
  }

  void RunUntilUploadRequested(OnFileUploadRequestedCallback* callback) {
    if (!file_upload_requested_callback_) {
      base::RunLoop run_loop;
      quit_closure_for_on_file_upload_requested_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    *callback = std::move(file_upload_requested_callback_);
  }

 private:
  base::OnceClosure quit_closure_for_on_file_upload_requested_;
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
  mojo::Remote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  auto network_context_client =
      std::make_unique<CallbackSavingNetworkContextClient>();
  context().set_network_context_client(network_context_client.get());
  std::unique_ptr<URLLoader> url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  mojom::NetworkContextClient::OnFileUploadRequestedCallback callback;
  network_context_client->RunUntilUploadRequested(&callback);

  // Check we can call the callback from a deleted URLLoader without crashing.
  url_loader.reset();
  base::RunLoop().RunUntilIdle();
  std::move(callback).Run(net::OK, std::move(opened_file));
  base::RunLoop().RunUntilIdle();
  context().set_network_context_client(nullptr);
}

// Tests a request body with a data pipe element.
TEST_F(URLLoaderTest, UploadDataPipe) {
  const std::string kRequestBody = "Request Body";

  mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter_remote;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_remote));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(kRequestBody, response_body);
}

// Same as above and tests that the body is sent after a 307 redirect.
TEST_F(URLLoaderTest, UploadDataPipe_Redirect307) {
  const std::string kRequestBody = "Request Body";

  mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter_remote;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_remote));
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
  while (request_body.size() < 5 * 1024 * 1024) {
    request_body.append("foppity");
  }

  mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter_remote;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      request_body, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_remote));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::OK, Load(test_server()->GetURL("/echo"), &response_body));
  EXPECT_EQ(request_body, response_body);
}

TEST_F(URLLoaderTest, UploadDataPipeError) {
  const std::string kRequestBody = "Request Body";

  mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter_remote;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  data_pipe_getter->set_start_error(net::ERR_ACCESS_DENIED);

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_remote));
  set_request_body(std::move(resource_request_body));

  EXPECT_EQ(net::ERR_ACCESS_DENIED, Load(test_server()->GetURL("/echo")));
}

TEST_F(URLLoaderTest, UploadDataPipeClosedEarly) {
  const std::string kRequestBody = "Request Body";

  mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter_remote;
  auto data_pipe_getter = std::make_unique<TestDataPipeGetter>(
      kRequestBody, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  data_pipe_getter->set_pipe_closed_early(true);

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->AppendDataPipe(std::move(data_pipe_getter_remote));
  set_request_body(std::move(resource_request_body));

  std::string response_body;
  EXPECT_EQ(net::ERR_FAILED, Load(test_server()->GetURL("/echo")));
}

// Tests a request body with a chunked data pipe element.
TEST_F(URLLoaderTest, UploadChunkedDataPipe) {
  const std::string kRequestBody = "Request Body";

  TestChunkedDataPipeGetter data_pipe_getter;

  ResourceRequest request =
      CreateResourceRequest("POST", test_server()->GetURL("/echo"));
  request.request_body = base::MakeRefCounted<ResourceRequestBody>();
  request.request_body->SetAllowHTTP1ForStreamingUpload(true);
  request.request_body->SetToChunkedDataPipe(
      data_pipe_getter.GetDataPipeGetterRemote(),
      ResourceRequestBody::ReadOnlyOnce(false));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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

TEST_F(URLLoaderTest, UploadChunkedDataPipeOverHTTP2) {
  const std::string kRequestBody = "Request Body";

  TestChunkedDataPipeGetter data_pipe_getter;

  ResourceRequest request = CreateResourceRequest(
      "POST", net::QuicSimpleTestServer::GetFileURL("/echo"));
  request.request_body = base::MakeRefCounted<ResourceRequestBody>();
  request.request_body->SetToChunkedDataPipe(
      data_pipe_getter.GetDataPipeGetterRemote(),
      ResourceRequestBody::ReadOnlyOnce(false));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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

TEST_F(URLLoaderTest, UploadChunkedDataPipeNotAllowHTTP1) {
  const std::string kRequestBody = "Request Body";

  TestChunkedDataPipeGetter data_pipe_getter;

  auto resource_request_body = base::MakeRefCounted<ResourceRequestBody>();
  resource_request_body->SetToChunkedDataPipe(
      data_pipe_getter.GetDataPipeGetterRemote(),
      ResourceRequestBody::ReadOnlyOnce(false));
  set_request_body(std::move(resource_request_body));

  EXPECT_EQ(net::ERR_H2_OR_QUIC_REQUIRED, Load(test_server()->GetURL("/echo")));
}

// Tests a request body with ReadOnceStream.
TEST_F(URLLoaderTest, UploadChunkedDataPipeReadOnceStream) {
  const std::string kRequestBody = "Request Body";

  TestChunkedDataPipeGetter data_pipe_getter;

  ResourceRequest request =
      CreateResourceRequest("POST", test_server()->GetURL("/echo"));
  request.request_body = base::MakeRefCounted<ResourceRequestBody>();
  request.request_body->SetAllowHTTP1ForStreamingUpload(true);
  request.request_body->SetToChunkedDataPipe(
      data_pipe_getter.GetDataPipeGetterRemote(),
      ResourceRequestBody::ReadOnlyOnce(true));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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

// Tests that SSLInfo is not attached to OnComplete messages or the
// URLResponseHead when there is no certificate error.
TEST_F(URLLoaderTest, NoSSLInfoWithoutCertificateError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_server.Start());
  set_send_ssl_for_cert_error();
  EXPECT_EQ(net::OK, Load(https_server.GetURL("/")));
  EXPECT_FALSE(client()->completion_status().ssl_info.has_value());
  EXPECT_FALSE(client()->response_head()->ssl_info.has_value());
}

// Tests that SSLInfo is not attached to OnComplete messages when the
// corresponding option is not set.
TEST_F(URLLoaderTest, NoSSLInfoOnComplete) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, Load(https_server.GetURL("/")));
  EXPECT_FALSE(client()->completion_status().ssl_info.has_value());
}

// Tests that SSLInfo is attached to OnComplete messages when the corresponding
// option is set and the certificate error causes the load to fail.
TEST_F(URLLoaderTest, SSLInfoOnComplete) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());
  set_send_ssl_for_cert_error();
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, Load(https_server.GetURL("/")));
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info.value().cert);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client()->completion_status().ssl_info.value().cert_status);
}

// Tests that SSLInfo is attached to OnComplete messages and the URLResponseHead
// when the corresponding option is set and the certificate error doesn't cause
// the load to fail.
TEST_F(URLLoaderTest, SSLInfoOnResponseWithCertificateError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());
  set_send_ssl_for_cert_error();
  set_ignore_certificate_errors();
  EXPECT_EQ(net::OK, Load(https_server.GetURL("/")));
  ASSERT_TRUE(client()->completion_status().ssl_info.has_value());
  EXPECT_TRUE(client()->completion_status().ssl_info.value().cert);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client()->completion_status().ssl_info.value().cert_status);
  ASSERT_TRUE(client()->response_head()->ssl_info.has_value());
  EXPECT_TRUE(client()->response_head()->ssl_info.value().cert);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client()->response_head()->ssl_info.value().cert_status);
}

// Tests that SSLInfo is attached to the URLResponseHead on redirects when the
// corresponding option is set and the certificate error doesn't cause the load
// to fail.
TEST_F(URLLoaderTest, SSLInfoOnRedirectWithCertificateError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(https_server.Start());

  TestURLLoaderClient client;
  ResourceRequest request = CreateResourceRequest(
      "GET", https_server.GetURL("/server-redirect?http://foo.test"));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  auto network_context_client = std::make_unique<TestNetworkContextClient>();
  context().set_network_context_client(network_context_client.get());
  TestURLLoaderNetworkObserver url_loader_network_observer;
  url_loader_network_observer.set_ignore_certificate_errors(true);
  URLLoaderOptions url_loader_options;
  url_loader_options.options =
      mojom::kURLLoadOptionSendSSLInfoWithResponse |
      mojom::kURLLoadOptionSendSSLInfoForCertificateError;
  url_loader_options.url_loader_network_observer =
      url_loader_network_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client.CreateRemote());

  client.RunUntilRedirectReceived();
  ASSERT_TRUE(client.response_head()->ssl_info.has_value());
  EXPECT_TRUE(client.response_head()->ssl_info.value().cert);
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID,
            client.response_head()->ssl_info.value().cert_status);
  context().set_network_context_client(nullptr);
}

// Make sure the client can modify headers during a redirect.
TEST_F(URLLoaderTest, RedirectModifiedHeaders) {
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server()->GetURL("/redirect307-to-echo"));
  request.headers.SetHeader("Header1", "Value1");
  request.headers.SetHeader("Header2", "Value2");

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

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
  loader->FollowRedirect({}, redirect_headers, {}, std::nullopt);

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // Redirected request should also have modified headers.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers2.find("Header1")->second);
  EXPECT_EQ("", request_headers2.find("Header2")->second);
  EXPECT_EQ("Value3", request_headers2.find("Header3")->second);
}

TEST_F(URLLoaderTest, RedirectFailsOnModifyUnsafeHeader) {
  const char* kUnsafeHeaders[] = {
      net::HttpRequestHeaders::kContentLength,
      net::HttpRequestHeaders::kHost,
      net::HttpRequestHeaders::kProxyConnection,
      net::HttpRequestHeaders::kProxyAuthorization,
      "Proxy-Foo",
  };

  for (const auto* unsafe_header : kUnsafeHeaders) {
    TestURLLoaderClient client;
    ResourceRequest request = CreateResourceRequest(
        "GET", test_server()->GetURL("/redirect307-to-echo"));

    base::RunLoop delete_run_loop;
    mojo::Remote<mojom::URLLoader> loader;
    std::unique_ptr<URLLoader> url_loader;
    context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    url_loader = URLLoaderOptions().MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.BindNewPipeAndPassReceiver(), request, client.CreateRemote());

    client.RunUntilRedirectReceived();

    net::HttpRequestHeaders redirect_headers;
    redirect_headers.SetHeader(unsafe_header, "foo");
    loader->FollowRedirect({}, redirect_headers, {}, std::nullopt);

    client.RunUntilComplete();
    delete_run_loop.Run();

    EXPECT_TRUE(client.has_received_completion());
    EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  }
}

// Test the client can remove headers during a redirect.
TEST_F(URLLoaderTest, RedirectRemoveHeader) {
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server()->GetURL("/redirect307-to-echo"));
  request.headers.SetHeader("Header1", "Value1");
  request.headers.SetHeader("Header2", "Value2");

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  // Initial request should only have initial headers.
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers1.find("Header1")->second);
  EXPECT_EQ("Value2", request_headers1.find("Header2")->second);

  // Remove Header1.
  std::vector<std::string> removed_headers = {"Header1"};
  loader->FollowRedirect(removed_headers, {}, {}, std::nullopt);

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
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  // Initial request should only have initial headers.
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers1.find("Header1")->second);
  EXPECT_EQ("Value2", request_headers1.find("Header2")->second);

  // Remove Header1 and add it back using a different value.
  std::vector<std::string> removed_headers = {"Header1"};
  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader("Header1", "NewValue1");
  loader->FollowRedirect(removed_headers, modified_headers, {}, std::nullopt);

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // Redirected request should have the updated headers.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ("NewValue1", request_headers2.find("Header1")->second);
  EXPECT_EQ("Value2", request_headers2.find("Header2")->second);
}

// Validate Sec- prefixed headers are handled properly when redirecting from
// insecure => secure urls. The Sec-Fetch-Site header should be re-added on the
// secure url.
TEST_F(URLLoaderTest, UpgradeAddsSecHeaders) {
  // Set up a redirect to signal we will go from insecure => secure.
  GURL url = test_server()->GetURL(
      kInsecureHost,
      "/server-redirect?" + test_server()->GetURL("/echo").spec());
  ResourceRequest request = CreateResourceRequest("GET", url);

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  // The initial request is received when the redirect before it has been
  // followed. It should have no added Sec- headers as it is not trustworthy.
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ(request_headers1.end(), request_headers1.find("Sec-Fetch-Site"));
  EXPECT_EQ(request_headers1.end(), request_headers1.find("Sec-Fetch-User"));

  // Now follow the redirect to the final destination and validate again.
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client()->RunUntilComplete();
  delete_run_loop.Run();

  // The Sec-Fetch-Site header should have been added again since we are now on
  // a trustworthy url again.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ("cross-site", request_headers2.find("Sec-Fetch-Site")->second);
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Sec-Fetch-User"));
}

// Validate Sec- prefixed headers are properly handled when redirecting from
// secure => insecure urls. All Sec-CH- and Sec-Fetch- prefixed
// headers should be removed.
TEST_F(URLLoaderTest, DowngradeRemovesSecHeaders) {
  // Set up a redirect to signal we will go from secure => insecure.
  GURL url = test_server()->GetURL(
      "/server-redirect?" +
      test_server()->GetURL(kInsecureHost, "/echo").spec());

  // Add some initial headers to ensure the right ones are removed and
  // everything else is left alone.
  ResourceRequest request = CreateResourceRequest("GET", url);
  request.headers.SetHeader("Sec-CH-UA", "Value1");
  request.headers.SetHeader("Sec-Other-Type", "Value2");
  request.headers.SetHeader("Other-Header", "Value3");

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  // The initial request is received when the redirect before it has been
  // followed. It should have all the Sec- headers as it is trustworthy. It
  // should also have added a Sec-Fetch-Site header
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers1.find("Sec-CH-UA")->second);
  EXPECT_EQ("Value2", request_headers1.find("Sec-Other-Type")->second);
  EXPECT_EQ("Value3", request_headers1.find("Other-Header")->second);
  EXPECT_EQ("same-origin", request_headers1.find("Sec-Fetch-Site")->second);
  EXPECT_EQ(request_headers1.end(), request_headers1.find("Sec-Fetch-User"));

  // Now follow the redirect to the final destination and validate again.
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client()->RunUntilComplete();
  delete_run_loop.Run();

  // We should have removed our special Sec-CH- and Sec-Fetch- prefixed headers
  // and left the others. We are now operating on an un-trustworthy context.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Sec-CH-UA"));
  EXPECT_EQ("Value2", request_headers2.find("Sec-Other-Type")->second);
  EXPECT_EQ("Value3", request_headers2.find("Other-Header")->second);
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Sec-Fetch-Site"));
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Sec-Fetch-User"));
}

// Validate Sec- prefixed headers are properly handled when redirecting from
// secure => insecure => secure urls.The headers on insecure
// urls should be removed and Sec-Fetch-Site should be re-added on secure ones.
TEST_F(URLLoaderTest, RedirectChainRemovesAndAddsSecHeaders) {
  // Set up a redirect to signal we will go from secure => insecure => secure.
  GURL insecure_upgrade_url = test_server()->GetURL(
      kInsecureHost,
      "/server-redirect?" + test_server()->GetURL("/echo").spec());
  GURL url =
      test_server()->GetURL("/server-redirect?" + insecure_upgrade_url.spec());

  // Add some initial headers to ensure the right ones are removed and
  // everything else is left alone.
  ResourceRequest request = CreateResourceRequest("GET", url);
  request.headers.SetHeader("Sec-CH-UA", "Value1");
  request.headers.SetHeader("Sec-Other-Type", "Value2");
  request.headers.SetHeader("Other-Header", "Value3");

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  // The initial request is received when the redirect before it has been
  // followed. It should have all the Sec- headers as it is trustworthy. It
  // should also have added a Sec-Fetch-Site header
  const auto& request_headers1 = sent_request().headers;
  EXPECT_EQ("Value1", request_headers1.find("Sec-CH-UA")->second);
  EXPECT_EQ("Value2", request_headers1.find("Sec-Other-Type")->second);
  EXPECT_EQ("Value3", request_headers1.find("Other-Header")->second);
  EXPECT_EQ("same-origin", request_headers1.find("Sec-Fetch-Site")->second);
  EXPECT_EQ(request_headers1.end(), request_headers1.find("Sec-Fetch-User"));

  // Follow our redirect and then verify again.
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client()->ClearHasReceivedRedirect();
  client()->RunUntilRedirectReceived();

  // Special Sec-CH- and Sec-Fetch- prefixed headers should have been removed
  // and the others left alone. We are now operating on an un-trustworthy
  // context.
  const auto& request_headers2 = sent_request().headers;
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Sec-CH-UA"));
  EXPECT_EQ("Value2", request_headers2.find("Sec-Other-Type")->second);
  EXPECT_EQ("Value3", request_headers2.find("Other-Header")->second);
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Sec-Fetch-Site"));
  EXPECT_EQ(request_headers2.end(), request_headers2.find("Sec-Fetch-User"));

  // Now follow the final redirect back to a trustworthy destination and
  // re-validate.
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client()->RunUntilComplete();
  delete_run_loop.Run();

  const auto& request_headers3 = sent_request().headers;
  EXPECT_EQ(request_headers3.end(), request_headers3.find("Sec-CH-UA"));
  EXPECT_EQ("Value2", request_headers3.find("Sec-Other-Type")->second);
  EXPECT_EQ("Value3", request_headers3.find("Other-Header")->second);
  EXPECT_EQ("cross-site", request_headers3.find("Sec-Fetch-Site")->second);
  EXPECT_EQ(request_headers3.end(), request_headers3.find("Sec-Fetch-User"));
}

// Validate Sec-Fetch-User header is properly handled.
TEST_F(URLLoaderTest, RedirectSecHeadersUser) {
  GURL url = test_server()->GetURL("/server-redirect?" +
                                   test_server()->GetURL("/echo").spec());

  // Add some initial headers to ensure the right ones are removed and
  // everything else is left alone.
  ResourceRequest request = CreateResourceRequest("GET", url);
  request.trusted_params->has_user_activation = true;

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  const auto& request_headers = sent_request().headers;
  EXPECT_EQ("same-origin", request_headers.find("Sec-Fetch-Site")->second);
  EXPECT_EQ("?1", request_headers.find("Sec-Fetch-User")->second);
}

// Validate Sec-Fetch-User header cannot be modified by manually set the value.
TEST_F(URLLoaderTest, RedirectDirectlyModifiedSecHeadersUser) {
  GURL url = test_server()->GetURL("/server-redirect?" +
                                   test_server()->GetURL("/echo").spec());

  // Try to modify `Sec-Fetch-User` directly.
  ResourceRequest request = CreateResourceRequest("GET", url);
  request.headers.SetHeader("Sec-Fetch-User", "?1");
  request.headers.SetHeader("Sec-Fetch-Dest", "embed");

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  const auto& request_headers = sent_request().headers;
  EXPECT_EQ(request_headers.end(), request_headers.find("Sec-Fetch-User"));
  EXPECT_EQ("empty", request_headers.find("Sec-Fetch-Dest")->second);
}

// A mock URLRequestJob which simulates an HTTPS request with a certificate
// error.
class MockHTTPSURLRequestJob : public net::URLRequestTestJob {
 public:
  MockHTTPSURLRequestJob(net::URLRequest* request,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               response_headers,
                               response_data,
                               auto_advance) {}

  MockHTTPSURLRequestJob(const MockHTTPSURLRequestJob&) = delete;
  MockHTTPSURLRequestJob& operator=(const MockHTTPSURLRequestJob&) = delete;

  ~MockHTTPSURLRequestJob() override = default;

  // net::URLRequestTestJob:
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    // Get the original response info, but override the SSL info.
    net::URLRequestJob::GetResponseInfo(info);
    info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    info->ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  }
};

class MockHTTPSJobURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  MockHTTPSJobURLRequestInterceptor() {}
  ~MockHTTPSJobURLRequestInterceptor() override {}

  // net::URLRequestInterceptor:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_FALSE(request->load_flags() & net::LOAD_BYPASS_CACHE);
    return std::make_unique<MockHTTPSURLRequestJob>(request, std::string(),
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
            client()->response_head()->cert_status);
}

// Verifies if URLLoader works well with ResourceScheduler.
// TODO(crbug.com/333723898): enable this test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ResourceSchedulerIntegration DISABLED_ResourceSchedulerIntegration
#else
#define MAYBE_ResourceSchedulerIntegration ResourceSchedulerIntegration
#endif
TEST_F(URLLoaderTest, MAYBE_ResourceSchedulerIntegration) {
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
  std::vector<
      std::pair<std::unique_ptr<URLLoader>, mojo::Remote<mojom::URLLoader>>>
      loaders;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  for (int i = 0; i < kRepeat; ++i) {
    TestURLLoaderClient client;
    mojo::PendingRemote<mojom::URLLoader> loader_remote;

    std::unique_ptr<URLLoader> url_loader = URLLoaderOptions().MakeURLLoader(
        context(), NeverInvokedDeleteLoaderCallback(),
        loader_remote.InitWithNewPipeAndPassReceiver(), request,
        client.CreateRemote());

    loaders.emplace_back(std::move(url_loader), std::move(loader_remote));
  }

  base::RunLoop().RunUntilIdle();
  for (const auto& pair : loaders) {
    URLLoader* loader = pair.first.get();
    ASSERT_NE(loader, nullptr);
    EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_RESPONSE, loader->GetLoadState());
  }

  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> loader = URLLoaderOptions().MakeURLLoader(
      context(), NeverInvokedDeleteLoaderCallback(),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());
  base::RunLoop().RunUntilIdle();

  // Make sure that the ResourceScheduler throttles this request.
  EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_DELEGATE, loader->GetLoadState());

  loader->SetPriority(net::HIGHEST, 0 /* intra_priority_value */);
  base::RunLoop().RunUntilIdle();

  // Make sure that the ResourceScheduler stops throtting.
  EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET,
            loader->GetLoadState());
}

// This tests that case where a read pipe is closed while there's a post task to
// invoke ReadMore.
TEST_F(URLLoaderTest, ReadPipeClosedWhileReadTaskPosted) {
  AddEternalSyncReadsInterceptor();

  ResourceRequest request = CreateResourceRequest(
      "GET", EternalSyncReadsInterceptor::GetSingleByteURL());

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilResponseBodyArrived();
  client()->response_body_release();
  delete_run_loop.Run();
}

class FakeSSLPrivateKeyImpl : public network::mojom::SSLPrivateKey {
 public:
  explicit FakeSSLPrivateKeyImpl(
      scoped_refptr<net::SSLPrivateKey> ssl_private_key)
      : ssl_private_key_(std::move(ssl_private_key)) {}

  FakeSSLPrivateKeyImpl(const FakeSSLPrivateKeyImpl&) = delete;
  FakeSSLPrivateKeyImpl& operator=(const FakeSSLPrivateKeyImpl&) = delete;

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
};

using CookieAccessType = mojom::CookieAccessDetails::Type;

class MockCookieObserver : public network::mojom::CookieAccessObserver {
 public:
  explicit MockCookieObserver(
      std::optional<CookieAccessType> access_type = std::nullopt)
      : access_type_(access_type) {}
  ~MockCookieObserver() override = default;

  struct CookieDetails {
    CookieDetails(const mojom::CookieAccessDetailsPtr& details,
                  const mojom::CookieOrLineWithAccessResultPtr& cookie)
        : type(details->type),
          cookie_or_line(std::move(cookie->cookie_or_line)),
          is_include(cookie->access_result.status.IsInclude()),
          url(details->url),
          status(cookie->access_result.status) {}

    CookieAccessType type;
    mojom::CookieOrLinePtr cookie_or_line;
    bool is_include;

    // The full details are available for the tests to query manually, but
    // they are not covered by operator== (and testing::ElementsAre).
    GURL url;
    net::CookieInclusionStatus status;
  };

  mojo::PendingRemote<mojom::CookieAccessObserver> GetRemote() {
    mojo::PendingRemote<mojom::CookieAccessObserver> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override {
    for (auto& details : details_vector) {
      if (access_type_ && access_type_ != details->type) {
        continue;
      }

      for (const auto& cookie_with_status : details->cookie_list) {
        observed_cookies_.emplace_back(details, cookie_with_status);
      }
    }
    if (wait_for_cookie_count_ &&
        observed_cookies().size() >= wait_for_cookie_count_) {
      std::move(wait_for_cookies_quit_closure_).Run();
    }
  }

  void WaitForCookies(size_t cookie_count) {
    if (observed_cookies_.size() < cookie_count) {
      wait_for_cookie_count_ = cookie_count;
      base::RunLoop run_loop;
      wait_for_cookies_quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    EXPECT_EQ(observed_cookies_.size(), cookie_count);
  }

  void Clone(
      mojo::PendingReceiver<mojom::CookieAccessObserver> observer) override {
    receivers_.Add(this, std::move(observer));
  }

  const std::vector<CookieDetails>& observed_cookies() {
    return observed_cookies_;
  }

 private:
  std::optional<CookieAccessType> access_type_;
  size_t wait_for_cookie_count_ = 0;
  base::OnceClosure wait_for_cookies_quit_closure_;
  std::vector<CookieDetails> observed_cookies_;
  mojo::ReceiverSet<mojom::CookieAccessObserver> receivers_;
};

MATCHER_P3(MatchesCookieDetails, type, cookie_or_line, is_include, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field(&MockCookieObserver::CookieDetails::type, type),
          testing::Field(&MockCookieObserver::CookieDetails::cookie_or_line,
                         cookie_or_line),
          testing::Field(&MockCookieObserver::CookieDetails::is_include,
                         is_include)),
      arg, result_listener);
}

class MockTrustTokenObserver : public network::mojom::TrustTokenAccessObserver {
 public:
  MockTrustTokenObserver() = default;
  ~MockTrustTokenObserver() override = default;

  struct TrustTokenDetails {
    explicit TrustTokenDetails(
        const mojom::TrustTokenAccessDetailsPtr& details) {
      switch (details->which()) {
        case mojom::TrustTokenAccessDetails::Tag::kIssuance:
          type = mojom::TrustTokenOperationType::kIssuance;
          origin = details->get_issuance()->origin;
          issuer = details->get_issuance()->issuer;
          blocked = details->get_issuance()->blocked;
          break;
        case mojom::TrustTokenAccessDetails::Tag::kRedemption:
          type = mojom::TrustTokenOperationType::kRedemption;
          origin = details->get_redemption()->origin;
          issuer = details->get_redemption()->issuer;
          blocked = details->get_redemption()->blocked;
          break;
        case mojom::TrustTokenAccessDetails::Tag::kSigning:
          type = mojom::TrustTokenOperationType::kSigning;
          origin = details->get_signing()->origin;
          blocked = details->get_signing()->blocked;
          break;
      }
    }

    url::Origin origin;
    mojom::TrustTokenOperationType type;
    std::optional<url::Origin> issuer;
    bool blocked;
  };

  mojo::PendingRemote<mojom::TrustTokenAccessObserver> GetRemote() {
    mojo::PendingRemote<mojom::TrustTokenAccessObserver> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void OnTrustTokensAccessed(
      mojom::TrustTokenAccessDetailsPtr details) override {
    observed_tokens_.emplace_back(details);
    if (wait_for_token_count_ &&
        observed_tokens().size() >= wait_for_token_count_) {
      std::move(wait_for_tokens_quit_closure_).Run();
    }
  }

  void WaitForTrustTokens(size_t token_count) {
    if (observed_tokens_.size() < token_count) {
      wait_for_token_count_ = token_count;
      base::RunLoop run_loop;
      wait_for_tokens_quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    EXPECT_EQ(observed_tokens_.size(), token_count);
  }

  void Clone(mojo::PendingReceiver<mojom::TrustTokenAccessObserver> observer)
      override {
    receivers_.Add(this, std::move(observer));
  }

  const std::vector<TrustTokenDetails>& observed_tokens() {
    return observed_tokens_;
  }

 private:
  size_t wait_for_token_count_ = 0;
  base::OnceClosure wait_for_tokens_quit_closure_;
  std::vector<TrustTokenDetails> observed_tokens_;
  mojo::ReceiverSet<mojom::TrustTokenAccessObserver> receivers_;
};

MATCHER_P3(MatchesTrustTokenDetails, origin, issuer, blocked, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field(&MockTrustTokenObserver::TrustTokenDetails::origin,
                         origin),
          testing::Field(&MockTrustTokenObserver::TrustTokenDetails::issuer,
                         issuer),
          testing::Field(&MockTrustTokenObserver::TrustTokenDetails::blocked,
                         blocked)),
      arg, result_listener);
}

// Responds certificate request with previously set responses.
class ClientCertAuthObserver : public TestURLLoaderNetworkObserver {
 public:
  ClientCertAuthObserver() = default;
  ~ClientCertAuthObserver() override = default;

  enum class CertificateResponse {
    INVALID = -1,
    URL_LOADER_REQUEST_CANCELLED,
    CANCEL_CERTIFICATE_SELECTION,
    NULL_CERTIFICATE,
    VALID_CERTIFICATE_SIGNATURE,
    INVALID_CERTIFICATE_SIGNATURE,
    DESTROY_CLIENT_CERT_RESPONDER,
  };

  enum class CredentialsResponse {
    NO_CREDENTIALS,
    CORRECT_CREDENTIALS,
    INCORRECT_CREDENTIALS_THEN_CORRECT_ONES,
  };
  void OnAuthRequired(
      const std::optional<base::UnguessableToken>& window_id,
      int32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<mojom::AuthChallengeResponder>
          auth_challenge_responder) override {
    switch (credentials_response_) {
      case CredentialsResponse::NO_CREDENTIALS:
        auth_credentials_ = std::nullopt;
        break;
      case CredentialsResponse::CORRECT_CREDENTIALS:
        auth_credentials_ = net::AuthCredentials(u"USER", u"PASS");
        break;
      case CredentialsResponse::INCORRECT_CREDENTIALS_THEN_CORRECT_ONES:
        auth_credentials_ = net::AuthCredentials(u"USER", u"FAIL");
        credentials_response_ = CredentialsResponse::CORRECT_CREDENTIALS;
        break;
    }
    mojo::Remote<mojom::AuthChallengeResponder> auth_challenge_responder_remote(
        std::move(auth_challenge_responder));
    auth_challenge_responder_remote->OnAuthCredentials(auth_credentials_);
    ++on_auth_required_call_counter_;
    last_seen_response_headers_ = head_headers;
  }

  void OnCertificateRequested(
      const std::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<mojom::ClientCertificateResponder>
          client_cert_responder_remote) override {
    mojo::Remote<mojom::ClientCertificateResponder> client_cert_responder(
        std::move(client_cert_responder_remote));
    switch (certificate_response_) {
      case CertificateResponse::INVALID:
        NOTREACHED_IN_MIGRATION();
        break;
      case CertificateResponse::URL_LOADER_REQUEST_CANCELLED:
        ASSERT_TRUE(url_loader_remote_);
        url_loader_remote_->reset();
        break;
      case CertificateResponse::CANCEL_CERTIFICATE_SELECTION:
        client_cert_responder->CancelRequest();
        break;
      case CertificateResponse::NULL_CERTIFICATE:
        client_cert_responder->ContinueWithoutCertificate();
        break;
      case CertificateResponse::VALID_CERTIFICATE_SIGNATURE:
      case CertificateResponse::INVALID_CERTIFICATE_SIGNATURE:
        client_cert_responder->ContinueWithCertificate(
            std::move(certificate_), provider_name_, algorithm_preferences_,
            std::move(ssl_private_key_remote_));
        break;
      case CertificateResponse::DESTROY_CLIENT_CERT_RESPONDER:
        // Send no response and let the local variable be destroyed.
        break;
    }
    ++on_certificate_requested_counter_;
  }

  void set_certificate_response(CertificateResponse certificate_response) {
    certificate_response_ = certificate_response;
  }

  void set_private_key(scoped_refptr<net::SSLPrivateKey> ssl_private_key) {
    ssl_private_key_ = std::move(ssl_private_key);
    provider_name_ = ssl_private_key_->GetProviderName();
    algorithm_preferences_ = ssl_private_key_->GetAlgorithmPreferences();
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeSSLPrivateKeyImpl>(std::move(ssl_private_key_)),
        ssl_private_key_remote_.InitWithNewPipeAndPassReceiver());
  }

  void set_certificate(scoped_refptr<net::X509Certificate> certificate) {
    certificate_ = std::move(certificate);
  }

  int on_certificate_requested_counter() {
    return on_certificate_requested_counter_;
  }

  void set_url_loader_remote(
      mojo::Remote<mojom::URLLoader>* url_loader_remote) {
    url_loader_remote_ = url_loader_remote;
  }

  void set_credentials_response(CredentialsResponse credentials_response) {
    credentials_response_ = credentials_response;
  }

  int on_auth_required_call_counter() { return on_auth_required_call_counter_; }

  net::HttpResponseHeaders* last_seen_response_headers() {
    return last_seen_response_headers_.get();
  }

 private:
  CredentialsResponse credentials_response_ =
      CredentialsResponse::NO_CREDENTIALS;
  std::optional<net::AuthCredentials> auth_credentials_;
  int on_auth_required_call_counter_ = 0;
  scoped_refptr<net::HttpResponseHeaders> last_seen_response_headers_;
  CertificateResponse certificate_response_ = CertificateResponse::INVALID;
  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
  scoped_refptr<net::X509Certificate> certificate_;
  mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key_remote_;
  std::string provider_name_;
  std::vector<uint16_t> algorithm_preferences_;
  int on_certificate_requested_counter_ = 0;
  raw_ptr<mojo::Remote<mojom::URLLoader>> url_loader_remote_ = nullptr;
};

TEST_F(URLLoaderTest, SetAuth) {
  ClientCertAuthObserver client_auth_observer;
  client_auth_observer.set_credentials_response(
      ClientCertAuthObserver::CredentialsResponse::CORRECT_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_auth_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head()->headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ(1, client_auth_observer.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
  EXPECT_FALSE(client()->response_head()->auth_challenge_info.has_value());
}

TEST_F(URLLoaderTest, CancelAuth) {
  ClientCertAuthObserver client_auth_observer;
  client_auth_observer.set_credentials_response(
      ClientCertAuthObserver::CredentialsResponse::NO_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_auth_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head()->headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(401, headers->response_code());
  EXPECT_EQ(1, client_auth_observer.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
}

TEST_F(URLLoaderTest, TwoChallenges) {
  ClientCertAuthObserver client_auth_observer;
  client_auth_observer.set_credentials_response(
      ClientCertAuthObserver::CredentialsResponse::
          INCORRECT_CREDENTIALS_THEN_CORRECT_ONES);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_auth_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head()->headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  EXPECT_EQ(2, client_auth_observer.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
}

TEST_F(URLLoaderTest, NoAuthRequiredForFavicon) {
  constexpr char kFaviconTestPage[] = "/has_favicon.html";

  ClientCertAuthObserver client_auth_observer;
  client_auth_observer.set_credentials_response(
      ClientCertAuthObserver::CredentialsResponse::CORRECT_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kFaviconTestPage));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_auth_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client()->has_received_response());
  EXPECT_FALSE(client()->has_received_completion());

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  client()->RunUntilComplete();
  EXPECT_TRUE(client()->has_received_completion());
  scoped_refptr<net::HttpResponseHeaders> headers =
      client()->response_head()->headers;
  ASSERT_TRUE(headers);
  EXPECT_EQ(200, headers->response_code());
  // No auth required for favicon.
  EXPECT_EQ(0, client_auth_observer.on_auth_required_call_counter());
  ASSERT_FALSE(url_loader);
}

TEST_F(URLLoaderTest, HttpAuthResponseHeadersAvailable) {
  ClientCertAuthObserver client_auth_observer;
  client_auth_observer.set_credentials_response(
      ClientCertAuthObserver::CredentialsResponse::CORRECT_CREDENTIALS);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL(kTestAuthURL));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_auth_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilResponseBodyArrived();

  // Spin the message loop until the delete callback is invoked, and then delete
  // the URLLoader.
  delete_run_loop.Run();

  EXPECT_EQ(1, client_auth_observer.on_auth_required_call_counter());

  auto* auth_required_headers =
      client_auth_observer.last_seen_response_headers();
  ASSERT_TRUE(auth_required_headers);
  EXPECT_EQ(auth_required_headers->response_code(), 401);
}

// Make sure the client can't call FollowRedirect if there's no pending
// redirect.
TEST_F(URLLoaderTest, FollowRedirectTwice) {
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server()->GetURL("/redirect307-to-echo"));
  request.headers.SetHeader("Header1", "Value1");
  request.headers.SetHeader("Header2", "Value2");

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();

  url_loader->FollowRedirect({}, {}, {}, std::nullopt);
  EXPECT_DCHECK_DEATH(url_loader->FollowRedirect({}, {}, {}, std::nullopt));

  client()->RunUntilComplete();
  delete_run_loop.Run();
}

class TestSSLPrivateKey : public net::SSLPrivateKey {
 public:
  explicit TestSSLPrivateKey(scoped_refptr<net::SSLPrivateKey> key)
      : key_(std::move(key)) {}

  TestSSLPrivateKey(const TestSSLPrivateKey&) = delete;
  TestSSLPrivateKey& operator=(const TestSSLPrivateKey&) = delete;

  void set_fail_signing(bool fail_signing) { fail_signing_ = fail_signing; }
  int sign_count() const { return sign_count_; }

  std::string GetProviderName() override { return key_->GetProviderName(); }
  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return key_->GetAlgorithmPreferences();
  }
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    sign_count_++;
    if (fail_signing_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
};

#if !BUILDFLAG(IS_IOS)
TEST_F(URLLoaderTest, ClientAuthRespondTwice) {
  // This tests that one URLLoader can handle two client cert requests.

  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;

  net::EmbeddedTestServer test_server_1(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server_1.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server_1.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server_1.Start());

  net::EmbeddedTestServer test_server_2(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server_2.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server_2.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server_2.Start());

  std::unique_ptr<net::FakeClientCertIdentity> identity =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::VALID_CERTIFICATE_SIGNATURE);
  client_cert_observer.set_private_key(private_key);
  client_cert_observer.set_certificate(identity->certificate());

  // Create a request to server_1 that will redirect to server_2
  ResourceRequest request = CreateResourceRequest(
      "GET",
      test_server_1.GetURL("/server-redirect-307?" +
                           base::EscapeQueryParamValue(
                               test_server_2.GetURL("/echo").spec(), true)));

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  EXPECT_EQ(0, client_cert_observer.on_certificate_requested_counter());
  EXPECT_EQ(0, private_key->sign_count());

  client()->RunUntilRedirectReceived();
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  // MockNetworkServiceClient gives away the private key when it invokes
  // ContinueWithCertificate, so we have to give it the key again.
  client_cert_observer.set_private_key(private_key);
  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_EQ(net::OK, client()->completion_status().error_code);
  EXPECT_EQ(2, client_cert_observer.on_certificate_requested_counter());
  EXPECT_EQ(2, private_key->sign_count());
}

TEST_F(URLLoaderTest, ClientAuthDestroyResponder) {
  // When URLLoader receives no message from the ClientCertificateResponder and
  // its connection errors out, we expect the request to be canceled rather than
  // just hang.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::
          DESTROY_CLIENT_CERT_RESPONDER);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());
  client_cert_observer.set_url_loader_remote(&loader);

  client()->RunUntilComplete();

  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            client()->completion_status().error_code);
}

TEST_F(URLLoaderTest, ClientAuthCancelConnection) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::
          URL_LOADER_REQUEST_CANCELLED);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());
  client_cert_observer.set_url_loader_remote(&loader);

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

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::
          CANCEL_CERTIFICATE_SELECTION);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();

  EXPECT_EQ(1, client_cert_observer.on_certificate_requested_counter());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            client()->completion_status().error_code);
}

TEST_F(URLLoaderTest, ClientAuthNoCertificate) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;

  // TLS 1.3 client auth errors show up post-handshake, resulting in a read
  // error which on Windows causes the socket to shutdown immediately before the
  // error is read.
  // TODO(crbug.com/41427061): Add support for testing this in TLS 1.3.
  ssl_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;

  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::NULL_CERTIFICATE);

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();

  EXPECT_EQ(1, client_cert_observer.on_certificate_requested_counter());
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

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::VALID_CERTIFICATE_SIGNATURE);
  client_cert_observer.set_private_key(private_key);
  scoped_refptr<net::X509Certificate> certificate =
      test_server.GetCertificate();
  client_cert_observer.set_certificate(std::move(certificate));

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();

  EXPECT_EQ(1, client_cert_observer.on_certificate_requested_counter());
  // The private key should have been used.
  EXPECT_EQ(1, private_key->sign_count());
}

TEST_F(URLLoaderTest, ClientAuthCertificateWithInvalidSignature) {
  std::unique_ptr<net::FakeClientCertIdentity> identity =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());
  private_key->set_fail_signing(true);

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::VALID_CERTIFICATE_SIGNATURE);
  client_cert_observer.set_private_key(private_key);
  scoped_refptr<net::X509Certificate> certificate =
      test_server.GetCertificate();
  client_cert_observer.set_certificate(std::move(certificate));

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/defaultresponse"));
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();

  EXPECT_EQ(1, client_cert_observer.on_certificate_requested_counter());
  // The private key should have been used.
  EXPECT_EQ(1, private_key->sign_count());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
            client()->completion_status().error_code);
}

TEST_F(URLLoaderTest, BlockAllCookies) {
  GURL first_party_url("http://www.example.com.test/");
  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromUrl(first_party_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  ResourceRequest request = CreateResourceRequest("GET", first_party_url);
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.options = mojom::kURLLoadOptionBlockAllCookies;
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  GURL cookie_url = test_server()->GetURL("/");
  auto cc = net::CanonicalCookie::CreateForTesting(
      cookie_url, "a=b", base::Time::Now(), std::nullopt /* server_time */,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com")));

  EXPECT_FALSE(url_loader->AllowCookie(*cc, first_party_url, site_for_cookies));
  EXPECT_FALSE(url_loader->AllowFullCookies(first_party_url, site_for_cookies));
  EXPECT_FALSE(url_loader->AllowFullCookies(third_party_url, site_for_cookies));
}

TEST_F(URLLoaderTest, BlockOnlyThirdPartyCookies) {
  GURL first_party_url("http://www.example.com.test/");
  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromUrl(first_party_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  ResourceRequest request = CreateResourceRequest("GET", first_party_url);
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.options = mojom::kURLLoadOptionBlockThirdPartyCookies;
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  GURL cookie_url = test_server()->GetURL("/");
  auto cc = net::CanonicalCookie::CreateForTesting(
      cookie_url, "a=b", base::Time::Now(), std::nullopt /* server_time */,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com")));

  EXPECT_TRUE(url_loader->AllowCookie(*cc, first_party_url, site_for_cookies));
  EXPECT_TRUE(url_loader->AllowFullCookies(first_party_url, site_for_cookies));
  EXPECT_FALSE(url_loader->AllowFullCookies(third_party_url, site_for_cookies));
}

TEST_F(URLLoaderTest, AllowAllCookies) {
  GURL first_party_url("http://www.example.com.test/");
  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromUrl(first_party_url);
  GURL third_party_url("http://www.some.other.origin.test/");

  ResourceRequest request = CreateResourceRequest("GET", first_party_url);
  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  GURL cookie_url = test_server()->GetURL("/");
  auto cc = net::CanonicalCookie::CreateForTesting(
      cookie_url, "a=b", base::Time::Now(), std::nullopt /* server_time */,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com")));

  EXPECT_TRUE(url_loader->AllowCookie(*cc, first_party_url, site_for_cookies));
  EXPECT_TRUE(url_loader->AllowFullCookies(first_party_url, site_for_cookies));
  EXPECT_TRUE(url_loader->AllowFullCookies(third_party_url, site_for_cookies));
}

class StorageAccessHeaderURLLoaderTest : public URLLoaderTest {
 public:
  StorageAccessHeaderURLLoaderTest() = default;

 protected:
  static constexpr char kStorageAccessRedirectLoadPath[] =
      "/redirect-load-with-storage-access";

 private:
  std::unique_ptr<net::test_server::HttpResponse>
  HandleLoadWithStorageAccessRequest(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.GetURL().path(),
                          kStorageAccessRedirectLoadPath)) {
      return nullptr;
    }
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_content_type("text/plain");
    http_response->AddCustomHeader("Activate-Storage-Access", "load");
    http_response->set_code(net::HTTP_PERMANENT_REDIRECT);
    http_response->AddCustomHeader("Location", "/empty.html");
    return http_response;
  }
};

TEST_F(StorageAccessHeaderURLLoaderTest, StorageAccessHeader_Load_NoStatus) {
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server_.GetURL("/set-header?Activate-Storage-Access: load"));

  test_network_delegate()->set_storage_access_status(std::nullopt);

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // TestNetworkDelegate always returns std::nullopt for the
  // GetStorageAccessStatus call, so the loader's `storage_access_status_` is
  // std::nullopt.
  EXPECT_FALSE(client()->response_head()->load_with_storage_access);
}

TEST_F(StorageAccessHeaderURLLoaderTest, StorageAccessHeader_Load_StatusNone) {
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server_.GetURL("/set-header?Activate-Storage-Access: load"));

  test_network_delegate()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kNone);
  test_network_delegate()->set_is_storage_access_header_enabled(true);
  base::HistogramTester histogram_tester;

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_FALSE(client()->response_head()->load_with_storage_access);
  histogram_tester.ExpectUniqueSample(
      "API.StorageAccessHeader.ActivateStorageAccessLoadOutcome",
      /*sample=*/
      net::cookie_util::ActivateStorageAccessLoadOutcome::kFailureInvalidStatus,
      /*expected_bucket_count=*/1);
}

TEST_F(StorageAccessHeaderURLLoaderTest,
       StorageAccessHeader_Load_StatusInactive) {
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server_.GetURL("/set-header?Activate-Storage-Access: load"));

  test_network_delegate()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kInactive);
  test_network_delegate()->set_is_storage_access_header_enabled(true);

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_TRUE(client()->response_head()->load_with_storage_access);
}

TEST_F(StorageAccessHeaderURLLoaderTest,
       StorageAccessHeader_Load_StatusActive) {
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server_.GetURL("/set-header?Activate-Storage-Access: load"));

  test_network_delegate()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kActive);
  test_network_delegate()->set_is_storage_access_header_enabled(true);
  base::HistogramTester histogram_tester;

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_TRUE(client()->response_head()->load_with_storage_access);
  histogram_tester.ExpectUniqueSample(
      "API.StorageAccessHeader.ActivateStorageAccessLoadOutcome",
      /*sample=*/net::cookie_util::ActivateStorageAccessLoadOutcome::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(StorageAccessHeaderURLLoaderTest, Load_StatusActive_IgnoredParam) {
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest(
      "GET",
      test_server_.GetURL("/set-header?Activate-Storage-Access: load;foo"));

  test_network_delegate()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kActive);
  test_network_delegate()->set_is_storage_access_header_enabled(true);

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_TRUE(client()->response_head()->load_with_storage_access);
}

TEST_F(StorageAccessHeaderURLLoaderTest, Load_StatusActive_IncorrectType) {
  base::RunLoop delete_run_loop;
  // This response will be a comma-separated list, rather than a single item, so
  // it's the wrong type and should be ignored.
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server_.GetURL("/set-header?Activate-Storage-Access: "
                                 "load;bar&Activate-Storage-Access: load;foo"));

  test_network_delegate()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kActive);
  test_network_delegate()->set_is_storage_access_header_enabled(true);

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_FALSE(client()->response_head()->load_with_storage_access);
}

TEST_F(StorageAccessHeaderURLLoaderTest, StorageAccessHeader_RedirectWithLoad) {
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server_.GetURL(kStorageAccessRedirectLoadPath));

  test_network_delegate()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kActive);
  test_network_delegate()->set_is_storage_access_header_enabled(true);

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // The redirect response included the `load` header, but the final response
  // did not, so the URLLoader should not propagate it.
  EXPECT_FALSE(client()->response_head()->load_with_storage_access);
}

TEST_F(StorageAccessHeaderURLLoaderTest,
       StorageAccessHeader_NoLoadWhenHeaderNotenabled) {
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest(
      "GET", test_server_.GetURL("/set-header?Activate-Storage-Access: load"));

  test_network_delegate()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kActive);
  base::HistogramTester histogram_tester;

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  // `TestNetworkDelegate::`OnIsStorageAccessHeaderEnabled` should have returned
  // false when called during the request, so `load_with_storage_access` should
  // still be false.
  EXPECT_FALSE(client()->response_head()->load_with_storage_access);
  histogram_tester.ExpectUniqueSample(
      "API.StorageAccessHeader.ActivateStorageAccessLoadOutcome",
      /*sample=*/
      net::cookie_util::ActivateStorageAccessLoadOutcome::
          kFailureHeaderDisabled,
      /*expected_bucket_count=*/1);
}

class URLLoaderCookieSettingOverridesTest
    : public URLLoaderTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, net::StorageAccessApiStatus, bool>> {
 public:
  ~URLLoaderCookieSettingOverridesTest() override = default;

  void SetUpRequest(ResourceRequest& request) {
    if (IsCors()) {
      request.mode = network::mojom::RequestMode::kCors;
    } else {
      // Request mode is `no-cors` by default.
      EXPECT_EQ(request.mode, network::mojom::RequestMode::kNoCors);
    }
    request.is_outermost_main_frame = IsOuterMostFrame();
    request.storage_access_api_status = StorageAccessApiStatus();
    if (!InitiatorIsOtherOrigin()) {
      request.request_initiator =
          url::Origin::Create(GURL("http://other-origin.test/"));
    }
  }

  net::CookieSettingOverrides ExpectedCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    if (IsCors() && IsOuterMostFrame()) {
      overrides.Put(
          net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
    }
    switch (StorageAccessApiStatus()) {
      case net::StorageAccessApiStatus::kNone:
        break;
      case net::StorageAccessApiStatus::kAccessViaAPI:
        if (InitiatorIsOtherOrigin()) {
          overrides.Put(
              net::CookieSettingOverride::kStorageAccessGrantEligible);
        }
        break;
    }
    return overrides;
  }

  net::CookieSettingOverrides
  ExpectedCookieSettingOverridesForCrossSiteRedirect() const {
    net::CookieSettingOverrides overrides = ExpectedCookieSettingOverrides();
    overrides.Remove(net::CookieSettingOverride::kStorageAccessGrantEligible);
    return overrides;
  }

 private:
  bool IsCors() const { return std::get<0>(GetParam()); }
  bool IsOuterMostFrame() const { return std::get<1>(GetParam()); }
  net::StorageAccessApiStatus StorageAccessApiStatus() const {
    return std::get<2>(GetParam());
  }
  bool InitiatorIsOtherOrigin() const { return std::get<3>(GetParam()); }
};

TEST_P(URLLoaderCookieSettingOverridesTest, CookieSettingOverrides) {
  GURL url("http://www.example.com.test/");
  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest("GET", url);
  SetUpRequest(request);

  mojo::PendingRemote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  const std::vector<net::CookieSettingOverrides> records =
      test_network_delegate()->cookie_setting_overrides_records();
  EXPECT_THAT(records, ElementsAre(ExpectedCookieSettingOverrides(),
                                   ExpectedCookieSettingOverrides()));
}

TEST_P(URLLoaderCookieSettingOverridesTest,
       CookieSettingOverrides_OnSameSiteRedirects) {
  GURL redirecting_url = test_server()->GetURL(
      "/server-redirect?" + test_server()->GetURL("/simple_page.html").spec());

  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest("GET", redirecting_url);
  SetUpRequest(request);

  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client()->RunUntilComplete();
  delete_run_loop.Run();

  const std::vector<net::CookieSettingOverrides> records =
      test_network_delegate()->cookie_setting_overrides_records();

  EXPECT_THAT(records, ElementsAre(ExpectedCookieSettingOverrides(),
                                   ExpectedCookieSettingOverrides(),
                                   ExpectedCookieSettingOverrides(),
                                   ExpectedCookieSettingOverrides()));
}

TEST_P(URLLoaderCookieSettingOverridesTest,
       CookieSettingOverrides_OnCrossSiteRedirects) {
  GURL dest_url("http://www.example.com.test/");
  GURL redirecting_url =
      test_server()->GetURL("/server-redirect?" + dest_url.spec());

  base::RunLoop delete_run_loop;
  ResourceRequest request = CreateResourceRequest("GET", redirecting_url);
  SetUpRequest(request);

  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client()->RunUntilComplete();
  delete_run_loop.Run();

  const std::vector<net::CookieSettingOverrides> records =
      test_network_delegate()->cookie_setting_overrides_records();

  EXPECT_THAT(
      records,
      ElementsAre(ExpectedCookieSettingOverrides(),
                  ExpectedCookieSettingOverrides(),
                  ExpectedCookieSettingOverridesForCrossSiteRedirect(),
                  ExpectedCookieSettingOverridesForCrossSiteRedirect()));
}

TEST_P(URLLoaderCookieSettingOverridesTest,
       CookieSettingOverrides_OnCrossSiteToSameSite) {
  GURL cross_site_to_same_site_redirect_url = test_server()->GetURL(
      kHostnameWithAliases,
      "/server-redirect?" + test_server()->GetURL("/empty.html").spec());

  base::RunLoop delete_run_loop;
  ResourceRequest request =
      CreateResourceRequest("GET", cross_site_to_same_site_redirect_url);
  request.request_initiator = test_server()->GetOrigin();
  SetUpRequest(request);

  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  url_loader = URLLoaderOptions().MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client()->RunUntilComplete();
  delete_run_loop.Run();
  EXPECT_THAT(test_network_delegate()->cookie_setting_overrides_records(),
              ElementsAre(ExpectedCookieSettingOverridesForCrossSiteRedirect(),
                          ExpectedCookieSettingOverridesForCrossSiteRedirect(),
                          ExpectedCookieSettingOverrides(),
                          ExpectedCookieSettingOverrides()));
}
INSTANTIATE_TEST_SUITE_P(
    All,
    URLLoaderCookieSettingOverridesTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Values(net::StorageAccessApiStatus::kNone,
                        net::StorageAccessApiStatus::kAccessViaAPI),
        testing::Bool()));

namespace {

enum class TestMode {
  kCredentialsModeOmit,
  kCredentialsModeOmitWorkaround,
  kCredentialsModeOmitWithFeatureFix,
};

}  // namespace

class URLLoaderParameterTest : public URLLoaderTest,
                               public ::testing::WithParamInterface<TestMode> {
 public:
  ~URLLoaderParameterTest() override = default;

 private:
  void SetUp() override {
    URLLoaderTest::SetUp();
    if (GetParam() == TestMode::kCredentialsModeOmitWithFeatureFix) {
      scoped_feature_list.InitAndEnableFeature(features::kOmitCorsClientCert);
    } else {
      scoped_feature_list.InitAndDisableFeature(features::kOmitCorsClientCert);
    }
  }
  base::test::ScopedFeatureList scoped_feature_list;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    URLLoaderParameterTest,
    testing::Values(TestMode::kCredentialsModeOmit,
                    TestMode::kCredentialsModeOmitWorkaround,
                    TestMode::kCredentialsModeOmitWithFeatureFix));

// Tests that a request with CredentialsMode::kOmit still sends client
// certificates when features::kOmitCorsClientCert is disabled, and when the
// feature is enabled client certificates are not sent. Also test that when
// CredentialsMode::kOmitBug_775438_Workaround is used client certificates are
// not sent as well. This should be removed when crbug.com/775438 is fixed.
TEST_P(URLLoaderParameterTest, CredentialsModeOmitRequireClientCert) {
  // Set up a server that requires certificates.
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  // Make sure the client has valid certificates.
  std::unique_ptr<net::FakeClientCertIdentity> identity =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::VALID_CERTIFICATE_SIGNATURE);
  client_cert_observer.set_private_key(private_key);
  client_cert_observer.set_certificate(identity->certificate());

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/simple_page.html"));
  if (GetParam() == TestMode::kCredentialsModeOmitWorkaround) {
    request.credentials_mode =
        mojom::CredentialsMode::kOmitBug_775438_Workaround;
  } else {
    request.credentials_mode = mojom::CredentialsMode::kOmit;
  }

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();
  if (GetParam() == TestMode::kCredentialsModeOmitWithFeatureFix ||
      GetParam() == TestMode::kCredentialsModeOmitWorkaround) {
    EXPECT_EQ(0, client_cert_observer.on_certificate_requested_counter());
    EXPECT_NE(net::OK, client()->completion_status().error_code);
  } else {
    EXPECT_EQ(1, client_cert_observer.on_certificate_requested_counter());
    EXPECT_EQ(net::OK, client()->completion_status().error_code);
  }
}

// Tests that a request with CredentialsMode::kOmitBug_775438_Workaround, and
// CredentialsMode::kOmit when features::kOmitCorsClientCert is enabled doesn't
// send client certificates with a server that optionally requires certificates.
TEST_P(URLLoaderParameterTest, CredentialsModeOmitOptionalClientCert) {
  // Set up a server that requires certificates.
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::OPTIONAL_CLIENT_CERT;
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(test_server.Start());

  // Make sure the client has valid certificates.
  std::unique_ptr<net::FakeClientCertIdentity> identity =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());

  ClientCertAuthObserver client_cert_observer;
  client_cert_observer.set_certificate_response(
      ClientCertAuthObserver::CertificateResponse::VALID_CERTIFICATE_SIGNATURE);
  client_cert_observer.set_private_key(private_key);
  client_cert_observer.set_certificate(identity->certificate());

  ResourceRequest request =
      CreateResourceRequest("GET", test_server.GetURL("/simple_page.html"));
  if (GetParam() == TestMode::kCredentialsModeOmitWorkaround) {
    request.credentials_mode =
        mojom::CredentialsMode::kOmitBug_775438_Workaround;
  } else {
    request.credentials_mode = mojom::CredentialsMode::kOmit;
  }

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.url_loader_network_observer = client_cert_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request, client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();
  if (GetParam() == TestMode::kCredentialsModeOmitWithFeatureFix ||
      GetParam() == TestMode::kCredentialsModeOmitWorkaround) {
    EXPECT_EQ(0, client_cert_observer.on_certificate_requested_counter());
  } else {
    EXPECT_EQ(1, client_cert_observer.on_certificate_requested_counter());
  }
  EXPECT_EQ(net::OK, client()->completion_status().error_code);
}

#endif  // !BUILDFLAG(IS_IOS)

TEST_F(URLLoaderTest, CookieReporting) {
  {
    TestURLLoaderClient loader_client;
    ResourceRequest request =
        CreateResourceRequest("GET", test_server()->GetURL("/set-cookie?a=b"));

    MockCookieObserver cookie_observer;
    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.cookie_observer = cookie_observer.GetRemote();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    cookie_observer.WaitForCookies(1u);
    EXPECT_THAT(
        cookie_observer.observed_cookies(),
        testing::ElementsAre(MatchesCookieDetails(
            CookieAccessType::kChange,
            CookieOrLine("a=b", mojom::CookieOrLine::Tag::kCookie), true)));
  }

  {
    TestURLLoaderClient loader_client;
    ResourceRequest request =
        CreateResourceRequest("GET", test_server()->GetURL("/nocontent"));

    MockCookieObserver cookie_observer;
    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;

    URLLoaderOptions url_loader_options;
    url_loader_options.cookie_observer = cookie_observer.GetRemote();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    cookie_observer.WaitForCookies(1u);
    EXPECT_THAT(
        cookie_observer.observed_cookies(),
        testing::ElementsAre(MatchesCookieDetails(
            CookieAccessType::kRead,
            CookieOrLine("a=b", mojom::CookieOrLine::Tag::kCookie), true)));
  }
}

TEST_F(URLLoaderTest, CookieReportingRedirect) {
  MockCookieObserver cookie_observer(CookieAccessType::kChange);

  GURL dest_url = test_server()->GetURL("/nocontent");
  GURL redirecting_url =
      test_server()->GetURL("/server-redirect-with-cookie?" + dest_url.spec());

  TestURLLoaderClient loader_client;
  ResourceRequest request = CreateResourceRequest("GET", redirecting_url);

  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  URLLoaderOptions url_loader_options;
  url_loader_options.cookie_observer = cookie_observer.GetRemote();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request,
      loader_client.CreateRemote());

  loader_client.RunUntilRedirectReceived();
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  loader_client.RunUntilComplete();
  delete_run_loop.Run();
  EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

  cookie_observer.WaitForCookies(1u);
  EXPECT_THAT(cookie_observer.observed_cookies(),
              testing::ElementsAre(MatchesCookieDetails(
                  CookieAccessType::kChange,
                  CookieOrLine("server-redirect=true",
                               mojom::CookieOrLine::Tag::kCookie),
                  true)));
  // Make sure that this has the pre-redirect URL, not the post-redirect one.
  EXPECT_EQ(redirecting_url, cookie_observer.observed_cookies()[0].url);
}

TEST_F(URLLoaderTest, CookieReportingAuth) {
  for (auto mode :
       {ClientCertAuthObserver::CredentialsResponse::NO_CREDENTIALS,
        ClientCertAuthObserver::CredentialsResponse::CORRECT_CREDENTIALS}) {
    MockCookieObserver cookie_observer(CookieAccessType::kChange);
    ClientCertAuthObserver client_auth_observer;
    client_auth_observer.set_credentials_response(mode);

    GURL url = test_server()->GetURL(
        "/auth-basic?set-cookie-if-challenged&password=PASS");
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest("GET", url);

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.cookie_observer = cookie_observer.GetRemote();
    url_loader_options.url_loader_network_observer =
        client_auth_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    loader_client.RunUntilComplete();
    delete_run_loop.Run();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    cookie_observer.WaitForCookies(1u);
    EXPECT_THAT(cookie_observer.observed_cookies(),
                testing::ElementsAre(MatchesCookieDetails(
                    CookieAccessType::kChange,
                    CookieOrLine("got_challenged=true",
                                 mojom::CookieOrLine::Tag::kCookie),
                    true)));
  }
}

TEST_F(URLLoaderTest, RawRequestCookies) {
  {
    MockDevToolsObserver devtools_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest(
        "GET", test_server()->GetURL("/echoheader?cookie"));
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    GURL cookie_url = test_server()->GetURL("/");
    auto cookie = net::CanonicalCookie::CreateForTesting(cookie_url, "a=b",
                                                         base::Time::Now());
    url_request_context()->cookie_store()->SetCanonicalCookieAsync(
        std::move(cookie), cookie_url, net::CookieOptions::MakeAllInclusive(),
        base::DoNothing());

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawRequest(1u);
    EXPECT_EQ(200, devtools_observer.raw_response_http_status_code());
    EXPECT_EQ("a", devtools_observer.raw_request_cookies()[0].cookie.Name());
    EXPECT_EQ("b", devtools_observer.raw_request_cookies()[0].cookie.Value());
    EXPECT_TRUE(devtools_observer.raw_request_cookies()[0]
                    .access_result.status.IsInclude());

    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());
  }
}

TEST_F(URLLoaderTest, RawRequestCookiesFlagged) {
  {
    MockDevToolsObserver devtools_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest(
        "GET", test_server()->GetURL("/echoheader?cookie"));
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    // Set the path to an irrelevant url to block the cookie from sending
    GURL cookie_url = test_server()->GetURL("/");
    auto cookie = net::CanonicalCookie::CreateForTesting(
        cookie_url, "a=b;Path=/something-else", base::Time::Now());
    url_request_context()->cookie_store()->SetCanonicalCookieAsync(
        std::move(cookie), cookie_url, net::CookieOptions::MakeAllInclusive(),
        base::DoNothing());

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawRequest(1u);
    EXPECT_EQ(200, devtools_observer.raw_response_http_status_code());
    EXPECT_EQ("a", devtools_observer.raw_request_cookies()[0].cookie.Name());
    EXPECT_EQ("b", devtools_observer.raw_request_cookies()[0].cookie.Value());
    EXPECT_TRUE(devtools_observer.raw_request_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {net::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH}));

    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());
  }
}

TEST_F(URLLoaderTest, RawResponseCookies) {
  {
    MockDevToolsObserver devtools_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request =
        CreateResourceRequest("GET", test_server()->GetURL("/set-cookie?a=b"));
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawResponse(1u);
    EXPECT_EQ(200, devtools_observer.raw_response_http_status_code());
    EXPECT_EQ("a", devtools_observer.raw_response_cookies()[0].cookie->Name());
    EXPECT_EQ("b", devtools_observer.raw_response_cookies()[0].cookie->Value());
    EXPECT_TRUE(devtools_observer.raw_response_cookies()[0]
                    .access_result.status.IsInclude());

    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());

    ASSERT_TRUE(devtools_observer.raw_response_headers());
    EXPECT_NE(devtools_observer.raw_response_headers()->find("Set-Cookie: a=b"),
              std::string::npos);
  }
}

TEST_F(URLLoaderTest, RawResponseCookiesInvalid) {
  {
    MockDevToolsObserver devtools_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest(
        "GET", test_server()->GetURL("/set-invalid-cookie"));
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawResponse(1u);
    EXPECT_EQ(200, devtools_observer.raw_response_http_status_code());
    // On these failures the cookie object is not created
    EXPECT_FALSE(devtools_observer.raw_response_cookies()[0].cookie);
    EXPECT_TRUE(
        devtools_observer.raw_response_cookies()[0]
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER}));

    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());
  }
}

TEST_F(URLLoaderTest, RawResponseCookiesRedirect) {
  // Check a valid cookie
  {
    MockDevToolsObserver devtools_observer;
    GURL dest_url = test_server()->GetURL("a.test", "/nocontent");
    GURL redirecting_url = test_server()->GetURL(
        "a.test", "/server-redirect-with-cookie?" + dest_url.spec());

    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest("GET", redirecting_url);
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    base::RunLoop delete_run_loop;
    mojo::Remote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;

    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.BindNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    loader_client.RunUntilRedirectReceived();

    ASSERT_TRUE(devtools_observer.raw_response_headers());
    EXPECT_NE(devtools_observer.raw_response_headers()->find(
                  "Set-Cookie: server-redirect=true"),
              std::string::npos);

    loader->FollowRedirect({}, {}, {}, std::nullopt);
    loader_client.RunUntilComplete();
    delete_run_loop.Run();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawResponse(1u);
    EXPECT_EQ(204, devtools_observer.raw_response_http_status_code());
    EXPECT_EQ("server-redirect",
              devtools_observer.raw_response_cookies()[0].cookie->Name());
    EXPECT_EQ("true",
              devtools_observer.raw_response_cookies()[0].cookie->Value());
    EXPECT_TRUE(devtools_observer.raw_response_cookies()[0]
                    .access_result.status.IsInclude());

    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());
  }

  // Check a flagged cookie (secure cookie over an insecure connection)
  {
    GURL dest_url = test_server()->GetURL("a.test", "/nocontent");
    GURL redirecting_url = test_server()->GetURL(
        "a.test", "/server-redirect-with-secure-cookie?" + dest_url.spec());

    MockDevToolsObserver devtools_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest("GET", redirecting_url);
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    base::RunLoop delete_run_loop;
    mojo::Remote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.BindNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    loader_client.RunUntilRedirectReceived();
    loader->FollowRedirect({}, {}, {}, std::nullopt);
    loader_client.RunUntilComplete();
    delete_run_loop.Run();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawResponse(1u);
    EXPECT_EQ(204, devtools_observer.raw_response_http_status_code());
    // On these failures the cookie object is created but not included.
    EXPECT_TRUE(
        devtools_observer.raw_response_cookies()[0].cookie->SecureAttribute());
    EXPECT_TRUE(devtools_observer.raw_response_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));
  }
}

TEST_F(URLLoaderTest, RawResponseCookiesAuth) {
  // Check a valid cookie
  {
    MockDevToolsObserver devtools_observer;
    ClientCertAuthObserver client_auth_observer;
    client_auth_observer.set_credentials_response(
        ClientCertAuthObserver::CredentialsResponse::NO_CREDENTIALS);

    GURL url = test_server()->GetURL(
        "a.test", "/auth-basic?set-cookie-if-challenged&password=PASS");
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest("GET", url);
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    loader_client.RunUntilComplete();
    delete_run_loop.Run();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawResponse(1u);
    EXPECT_EQ(401, devtools_observer.raw_response_http_status_code());
    EXPECT_EQ("got_challenged",
              devtools_observer.raw_response_cookies()[0].cookie->Name());
    EXPECT_EQ("true",
              devtools_observer.raw_response_cookies()[0].cookie->Value());
    EXPECT_TRUE(devtools_observer.raw_response_cookies()[0]
                    .access_result.status.IsInclude());

    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());
  }

  // Check a flagged cookie (secure cookie from insecure connection)
  {
    MockDevToolsObserver devtools_observer;
    ClientCertAuthObserver client_auth_observer;
    client_auth_observer.set_credentials_response(
        ClientCertAuthObserver::CredentialsResponse::NO_CREDENTIALS);

    GURL url = test_server()->GetURL(
        "a.test", "/auth-basic?set-secure-cookie-if-challenged&password=PASS");
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest("GET", url);
    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    loader_client.RunUntilComplete();
    delete_run_loop.Run();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);
    EXPECT_EQ(401, devtools_observer.raw_response_http_status_code());
    devtools_observer.WaitUntilRawResponse(1u);
    // On these failures the cookie object is created but not included.
    EXPECT_TRUE(
        devtools_observer.raw_response_cookies()[0].cookie->SecureAttribute());
    EXPECT_TRUE(devtools_observer.raw_response_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());
  }
}

TEST_F(URLLoaderTest, RawResponseQUIC) {
  {
    MockDevToolsObserver devtools_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request =
        CreateResourceRequest("GET", net::QuicSimpleTestServer::GetFileURL(""));

    // Set the devtools id to trigger the RawResponse call
    request.devtools_request_id = "TEST";

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.devtools_observer = devtools_observer.Bind();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    ASSERT_EQ(net::OK, loader_client.completion_status().error_code);

    devtools_observer.WaitUntilRawResponse(0u);
    EXPECT_EQ(404, devtools_observer.raw_response_http_status_code());
    EXPECT_EQ("TEST", devtools_observer.devtools_request_id());

    // QUIC responses don't have raw header text, so there shouldn't be any here
    EXPECT_FALSE(devtools_observer.raw_response_headers());
  }
}

TEST_F(URLLoaderTest, EarlyHints) {
  const std::string kPath = "/hinted";
  const std::string kResponseBody = "content with hints";
  const std::string kPreloadPath = "/hello.txt";

  // Prepare a response with an Early Hints response.
  quiche::HttpHeaderBlock response_headers;
  response_headers[":status"] = "200";
  response_headers[":path"] = kPath;
  std::vector<quiche::HttpHeaderBlock> early_hints;
  quiche::HttpHeaderBlock hints_headers;
  std::string preload_link =
      base::StringPrintf("<%s>; rel=preload", kPreloadPath.c_str());
  hints_headers["link"] = preload_link;

  early_hints.push_back(std::move(hints_headers));
  net::QuicSimpleTestServer::AddResponseWithEarlyHints(
      kPath, response_headers, kResponseBody, early_hints);

  MockDevToolsObserver devtools_observer;
  URLLoaderOptions url_loader_options;
  url_loader_options.devtools_observer = devtools_observer.Bind();

  // Create a loader and a client to request `kPath`. The client should receive
  // an Early Hints which contains a preload link header.
  TestURLLoaderClient loader_client;
  ResourceRequest request = CreateResourceRequest(
      "GET", net::QuicSimpleTestServer::GetFileURL(kPath));
  request.devtools_request_id = "TEST";

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      loader_client.CreateRemote());

  delete_run_loop.Run();
  loader_client.RunUntilComplete();
  ASSERT_EQ(loader_client.completion_status().error_code, net::OK);

  ASSERT_EQ(loader_client.early_hints().size(), 1UL);
  const auto& hints = loader_client.early_hints()[0];
  ASSERT_EQ(hints->headers->link_headers.size(), 1UL);
  const auto& link_header = hints->headers->link_headers[0];
  EXPECT_EQ(link_header->href,
            net::QuicSimpleTestServer::GetFileURL(kPreloadPath));

  // Test the early hint headers sent to the devtools observer.
  devtools_observer.WaitUntilEarlyHints();
  const std::vector<network::mojom::HttpRawHeaderPairPtr>& early_hint_headers =
      devtools_observer.early_hint_headers();
  EXPECT_EQ(early_hint_headers.size(), 1UL);
  network::mojom::HttpRawHeaderPair header_content = *early_hint_headers[0];
  EXPECT_EQ(header_content.key, "link");
  EXPECT_EQ(header_content.value, preload_link);
}

TEST_F(URLLoaderTest, CookieReportingCategories) {
  net::test_server::EmbeddedTestServer https_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("services/test/data")));
  ASSERT_TRUE(https_server.Start());

  // SameSite-by-default deprecation warning.
  {
    MockCookieObserver cookie_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest(
        "GET", https_server.GetURL("a.test", "/set-cookie?a=b;Secure"));
    // Make this a third-party request.
    url::Origin third_party_origin =
        url::Origin::Create(GURL("http://www.example.com"));
    request.site_for_cookies =
        net::SiteForCookies::FromOrigin(third_party_origin);
    request.trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(third_party_origin);

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.cookie_observer = cookie_observer.GetRemote();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    cookie_observer.WaitForCookies(1u);
    EXPECT_THAT(cookie_observer.observed_cookies(),
                testing::ElementsAre(MatchesCookieDetails(
                    CookieAccessType::kChange,
                    CookieOrLine("a=b", mojom::CookieOrLine::Tag::kCookie),
                    false /* is_included */)));
    EXPECT_TRUE(cookie_observer.observed_cookies()[0]
                    .status.HasExactlyExclusionReasonsForTesting(
                        {net::CookieInclusionStatus::
                             EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
    EXPECT_TRUE(cookie_observer.observed_cookies()[0].status.HasWarningReason(
        net::CookieInclusionStatus::WarningReason::
            WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT));
  }

  // Blocked.
  {
    MockCookieObserver cookie_observer(CookieAccessType::kChange);
    TestURLLoaderClient loader_client;
    test_network_delegate()->set_cookie_options(
        net::TestNetworkDelegate::NO_SET_COOKIE);
    ResourceRequest request = CreateResourceRequest(
        "GET", https_server.GetURL("a.test", "/set-cookie?a=b;Secure"));
    // Make this a third-party request.
    url::Origin third_party_origin =
        url::Origin::Create(GURL("http://www.example.com"));
    request.site_for_cookies =
        net::SiteForCookies::FromOrigin(third_party_origin);
    request.trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(third_party_origin);

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.cookie_observer = cookie_observer.GetRemote();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    cookie_observer.WaitForCookies(1u);
    EXPECT_THAT(
        cookie_observer.observed_cookies(),
        testing::ElementsAre(MatchesCookieDetails(
            CookieAccessType::kChange,
            CookieOrLine("a=b", mojom::CookieOrLine::Tag::kCookie), false)));
    EXPECT_TRUE(
        cookie_observer.observed_cookies()[0]
            .status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));

    test_network_delegate()->set_cookie_options(0);
  }

  // Not permitted by cookie rules, but not the sort of thing that's reported
  // to NetworkContextClient. Note: this uses HTTP, not HTTPS, unlike others;
  // and is in 1st-party context.
  {
    MockCookieObserver cookie_observer;
    TestURLLoaderClient loader_client;
    ResourceRequest request = CreateResourceRequest(
        "GET", test_server()->GetURL("a.test", "/set-cookie?a=b;Secure&d=e"));

    base::RunLoop delete_run_loop;
    mojo::PendingRemote<mojom::URLLoader> loader;
    context().mutable_factory_params().process_id = kProcessId;
    context().mutable_factory_params().is_orb_enabled = false;
    URLLoaderOptions url_loader_options;
    url_loader_options.cookie_observer = cookie_observer.GetRemote();
    std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
        context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
        loader.InitWithNewPipeAndPassReceiver(), request,
        loader_client.CreateRemote());

    delete_run_loop.Run();
    loader_client.RunUntilComplete();
    EXPECT_EQ(net::OK, loader_client.completion_status().error_code);

    cookie_observer.WaitForCookies(1u);
    EXPECT_THAT(
        cookie_observer.observed_cookies(),
        testing::ElementsAre(MatchesCookieDetails(
            CookieAccessType::kChange,
            CookieOrLine("d=e", mojom::CookieOrLine::Tag::kCookie), true)));
  }
}

namespace {

enum class SyncOrAsync { kSync, kAsync };

class MockTrustTokenRequestHelper : public TrustTokenRequestHelper {
 public:
  // |operation_synchrony| denotes whether to complete the |Begin|
  // and |Finalize| operations synchronously.
  //
  // |begin_done_flag|, if provided, will be set to true immediately before the
  // |Begin| operation returns.
  MockTrustTokenRequestHelper(
      std::optional<mojom::TrustTokenOperationStatus> on_begin,
      std::optional<mojom::TrustTokenOperationStatus> on_finalize,
      SyncOrAsync operation_synchrony,
      bool* begin_done_flag = nullptr)
      : on_begin_(on_begin),
        on_finalize_(on_finalize),
        operation_synchrony_(operation_synchrony),
        begin_done_flag_(begin_done_flag) {}

  ~MockTrustTokenRequestHelper() override {
    DCHECK(!on_begin_.has_value())
        << "Begin operation was expected but not performed.";
    DCHECK(!on_finalize_.has_value())
        << "Finalize operation was expected but not performed.";
  }

  MockTrustTokenRequestHelper(const MockTrustTokenRequestHelper&) = delete;
  MockTrustTokenRequestHelper& operator=(const MockTrustTokenRequestHelper&) =
      delete;

  // TrustTokenRequestHelper:
  void Begin(const GURL& url,
             base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                                     mojom::TrustTokenOperationStatus)> done)
      override {
    DCHECK(on_begin_.has_value());

    // Clear storage to crash if the method gets called a second time.
    mojom::TrustTokenOperationStatus result = *on_begin_;
    on_begin_.reset();
    std::optional<net::HttpRequestHeaders> headers;
    if (result == mojom::TrustTokenOperationStatus::kOk) {
      headers.emplace();
    }

    switch (operation_synchrony_) {
      case SyncOrAsync::kSync: {
        OnDoneBeginning(
            base::BindOnce(std::move(done), std::move(headers), result));
        return;
      }
      case SyncOrAsync::kAsync: {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &MockTrustTokenRequestHelper::OnDoneBeginning,
                base::Unretained(this),
                base::BindOnce(std::move(done), std::move(headers), result)));
        return;
      }
    }
  }

  void Finalize(net::HttpResponseHeaders& response_headers,
                base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done)
      override {
    DCHECK(on_finalize_.has_value());

    // Clear storage to crash if the method gets called a second time.
    mojom::TrustTokenOperationStatus result = *on_finalize_;
    on_finalize_.reset();

    switch (operation_synchrony_) {
      case SyncOrAsync::kSync: {
        std::move(done).Run(result);
        return;
      }
      case SyncOrAsync::kAsync: {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(done), result));
        return;
      }
    }
  }

  mojom::TrustTokenOperationResultPtr CollectOperationResultWithStatus(
      mojom::TrustTokenOperationStatus status) override {
    mojom::TrustTokenOperationResultPtr operation_result =
        mojom::TrustTokenOperationResult::New();
    operation_result->status = status;
    return operation_result;
  }

 private:
  void OnDoneBeginning(base::OnceClosure done) {
    if (begin_done_flag_) {
      EXPECT_FALSE(*begin_done_flag_);
      *begin_done_flag_ = true;
    }

    std::move(done).Run();
  }

  // Store mocked function results in Optionals to hit a CHECK failure if a mock
  // method is called without having had a return value specified.
  std::optional<mojom::TrustTokenOperationStatus> on_begin_;
  std::optional<mojom::TrustTokenOperationStatus> on_finalize_;

  SyncOrAsync operation_synchrony_;

  raw_ptr<bool> begin_done_flag_;
};

class NoopTrustTokenKeyCommitmentGetter : public TrustTokenKeyCommitmentGetter {
 public:
  NoopTrustTokenKeyCommitmentGetter() = default;
  void Get(const url::Origin& origin,
           base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)>
               on_done) const override {}
};

base::NoDestructor<NoopTrustTokenKeyCommitmentGetter>
    noop_key_commitment_getter{};

mojom::NetworkContextClient* ReturnNullNetworkContextClient() {
  return nullptr;
}

class MockTrustTokenRequestHelperFactory
    : public TrustTokenRequestHelperFactory {
 public:
  MockTrustTokenRequestHelperFactory(
      mojom::TrustTokenOperationStatus creation_failure_error,
      SyncOrAsync sync_or_async)
      : TrustTokenRequestHelperFactory(
            nullptr,
            noop_key_commitment_getter.get(),
            base::BindRepeating(&ReturnNullNetworkContextClient),
            {}),
        sync_or_async_(sync_or_async),
        creation_failure_error_(creation_failure_error) {}

  MockTrustTokenRequestHelperFactory(
      std::optional<mojom::TrustTokenOperationStatus> on_begin,
      std::optional<mojom::TrustTokenOperationStatus> on_finalize,
      SyncOrAsync sync_or_async,
      bool* begin_done_flag)
      : TrustTokenRequestHelperFactory(
            nullptr,
            noop_key_commitment_getter.get(),
            base::BindRepeating(&ReturnNullNetworkContextClient),
            {}),
        sync_or_async_(sync_or_async),
        helper_(
            std::make_unique<MockTrustTokenRequestHelper>(on_begin,
                                                          on_finalize,
                                                          sync_or_async,
                                                          begin_done_flag)) {}

  void CreateTrustTokenHelperForRequest(
      const url::Origin& top_frame_origin,
      const net::HttpRequestHeaders& headers,
      const mojom::TrustTokenParams& params,
      const net::NetLogWithSource& net_log,
      base::OnceCallback<void(TrustTokenStatusOrRequestHelper)> done) override {
    if (creation_failure_error_) {
      switch (sync_or_async_) {
        case SyncOrAsync::kSync: {
          std::move(done).Run(std::move(*creation_failure_error_));
          return;
        }
        case SyncOrAsync::kAsync:
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(done),
                                        std::move(*creation_failure_error_)));
          return;
      }
    }

    switch (sync_or_async_) {
      case SyncOrAsync::kSync: {
        std::move(done).Run(std::move(helper_));
        return;
      }
      case SyncOrAsync::kAsync:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(done), std::move(helper_)));
        return;
    }

    NOTREACHED_IN_MIGRATION();
  }

 private:
  SyncOrAsync sync_or_async_;
  std::optional<mojom::TrustTokenOperationStatus> creation_failure_error_;
  std::unique_ptr<TrustTokenRequestHelper> helper_;
};

class MockTrustTokenDevToolsObserver : public MockDevToolsObserver {
 public:
  MockTrustTokenDevToolsObserver() = default;
  ~MockTrustTokenDevToolsObserver() override = default;

  void OnTrustTokenOperationDone(
      const std::string& devtool_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override {
    // Event should be only triggered once.
    EXPECT_FALSE(trust_token_operation_status_.has_value());
    trust_token_operation_status_ = result->status;
  }

  const std::optional<mojom::TrustTokenOperationStatus>
  trust_token_operation_status() const {
    return trust_token_operation_status_;
  }

 private:
  std::optional<mojom::TrustTokenOperationStatus>
      trust_token_operation_status_ = std::nullopt;
};

class ExpectBypassCacheInterceptor : public net::URLRequestInterceptor {
 public:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_TRUE(request->load_flags() & net::LOAD_BYPASS_CACHE);
    return nullptr;
  }
};

class ExpectCookieSettingOverridesURLRequestInterceptor
    : public net::URLRequestInterceptor {
 public:
  explicit ExpectCookieSettingOverridesURLRequestInterceptor(
      net::CookieSettingOverrides cookie_setting_overrides,
      bool* was_intercepted)
      : cookie_setting_overrides_(cookie_setting_overrides),
        was_intercepted_(was_intercepted) {}

  // net::URLRequestInterceptor:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_FALSE(*was_intercepted_);
    EXPECT_EQ(request->cookie_setting_overrides(), cookie_setting_overrides_);
    *was_intercepted_ = true;
    return nullptr;
  }

 private:
  const net::CookieSettingOverrides cookie_setting_overrides_;
  const raw_ptr<bool> was_intercepted_;
};

}  // namespace

class URLLoaderSyncOrAsyncTrustTokenOperationTest
    : public URLLoaderTest,
      public ::testing::WithParamInterface<SyncOrAsync> {
 public:
  void OnServerReceivedRequest(const net::test_server::HttpRequest&) override {
    EXPECT_TRUE(outbound_trust_token_operation_was_successful_);
  }

 protected:
  ResourceRequest CreateTrustTokenResourceRequest() {
    GURL request_url = test_server()->GetURL("/simple_page.html");
    ResourceRequest request = CreateResourceRequest("GET", request_url);
    request.trust_token_params =
        OptionalTrustTokenParams(mojom::TrustTokenParams::New());
    // Set the devtools id to trigger the OnTrustTokenOperationDone call.
    request.devtools_request_id = "TEST";

    // Any Trust Token URLRequest that makes it to Start() should have the
    // BYPASS_CACHE flag set.
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        request_url, std::make_unique<ExpectBypassCacheInterceptor>());
    return request;
  }

  // Maintain a flag, set by the mock trust token request helper, denoting
  // whether we've successfully executed the outbound Trust Tokens operation.
  // This is used to make URLLoader does not send its main request before it
  // has completed the outbound part of its Trust Tokens operation (this
  // involves checking preconditions and potentially annotating the request with
  // Trust Tokens-related request headers).
  bool outbound_trust_token_operation_was_successful_ = false;
};

INSTANTIATE_TEST_SUITE_P(WithSyncAndAsyncOperations,
                         URLLoaderSyncOrAsyncTrustTokenOperationTest,
                         ::testing::Values(SyncOrAsync::kSync,
                                           SyncOrAsync::kAsync));

// An otherwise-successful request with an associated Trust Tokens operation
// whose Begin and Finalize steps are both successful should succeed overall.
TEST_P(URLLoaderSyncOrAsyncTrustTokenOperationTest,
       HandlesTrustTokenOperationSuccess) {
  base::HistogramTester histogram_tester;
  ResourceRequest request = CreateTrustTokenResourceRequest();

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  MockTrustTokenDevToolsObserver devtools_observer;

  URLLoaderOptions url_loader_options;
  MockTrustTokenObserver trust_token_observer;
  url_loader_options.trust_token_observer = trust_token_observer.GetRemote();
  url_loader_options.trust_token_helper_factory =
      std::make_unique<MockTrustTokenRequestHelperFactory>(
          mojom::TrustTokenOperationStatus::kOk /* on_begin */,
          mojom::TrustTokenOperationStatus::kOk /* on_finalize */, GetParam(),
          &outbound_trust_token_operation_was_successful_);
  url_loader_options.devtools_observer = devtools_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.OperationOutcome.Issuance",
      mojom::TrustTokenOperationStatus::kOk, 1);

  EXPECT_EQ(client()->completion_status().error_code, net::OK);
  EXPECT_EQ(client()->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kOk);
  // Verify the DevTools event was fired and it has the right status.
  EXPECT_EQ(devtools_observer.trust_token_operation_status(),
            mojom::TrustTokenOperationStatus::kOk);
  // The page should still have loaded.
  base::FilePath file = GetTestFilePath("simple_page.html");
  std::string expected;
  if (!base::ReadFileToString(file, &expected)) {
    ADD_FAILURE() << "File not found: " << file.value();
    return;
  }
  EXPECT_EQ(ReadBody(), expected);

  EXPECT_FALSE(client()->response_head()->headers->raw_headers().empty());

  trust_token_observer.WaitForTrustTokens(1u);
  EXPECT_THAT(
      trust_token_observer.observed_tokens(),
      testing::ElementsAre(MatchesTrustTokenDetails(
          test_server()->GetOrigin(), test_server()->GetOrigin(), false)));
}

// A request with an associated Trust Tokens operation whose Begin step returns
// kAlreadyExists should return a success result immediately, without completing
// the load.
//
// (This is the case exactly when the request is for token redemption, and the
// Trust Tokens logic determines that there is already a cached redemption
// record stored locally, obviating the need to execute a redemption
// operation.)
TEST_P(URLLoaderSyncOrAsyncTrustTokenOperationTest,
       HandlesTrustTokenRedemptionRecordCacheHit) {
  ResourceRequest request = CreateTrustTokenResourceRequest();

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  MockTrustTokenDevToolsObserver devtools_observer;

  URLLoaderOptions url_loader_options;
  MockTrustTokenObserver trust_token_observer;
  url_loader_options.trust_token_observer = trust_token_observer.GetRemote();
  url_loader_options.trust_token_helper_factory =
      std::make_unique<MockTrustTokenRequestHelperFactory>(
          mojom::TrustTokenOperationStatus::kAlreadyExists /* on_begin */,
          std::nullopt /* on_finalize */, GetParam(),
          &outbound_trust_token_operation_was_successful_);
  url_loader_options.devtools_observer = devtools_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_EQ(client()->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_SUCCESS_WITHOUT_SENDING_REQUEST);
  EXPECT_EQ(client()->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kAlreadyExists);
  // Verify the DevTools event was fired and it has the right status.
  EXPECT_EQ(devtools_observer.trust_token_operation_status(),
            mojom::TrustTokenOperationStatus::kAlreadyExists);

  EXPECT_FALSE(client()->response_head());
  EXPECT_FALSE(client()->response_body().is_valid());

  trust_token_observer.WaitForTrustTokens(1u);
  EXPECT_THAT(
      trust_token_observer.observed_tokens(),
      testing::ElementsAre(MatchesTrustTokenDetails(
          test_server()->GetOrigin(), test_server()->GetOrigin(), false)));
}

TEST_P(URLLoaderSyncOrAsyncTrustTokenOperationTest,
       HandlesTrustTokenFollowedByAttribution) {
  ResourceRequest request = CreateTrustTokenResourceRequest();

  base::RunLoop delete_run_loop;

  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> url_loader;

  // Request must come from a valid origin for verification operation to run.
  context().mutable_factory_params().isolation_info =
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(
          GURL("https://valid-destination-origin.example")));

  URLLoaderOptions url_loader_options;

  // Hook trust token helper
  url_loader_options.trust_token_helper_factory =
      std::make_unique<MockTrustTokenRequestHelperFactory>(
          /*on_begin=*/mojom::TrustTokenOperationStatus::kOk,
          /*on_finalize=*/mojom::TrustTokenOperationStatus::kOk, GetParam(),
          &outbound_trust_token_operation_was_successful_);

  // Hook attribution helper
  url_loader_options.attribution_request_helper =
      AttributionRequestHelper::CreateForTesting();

  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();
}

// When a request's associated Trust Tokens operation's Begin step fails, the
// request itself should fail immediately.
TEST_P(URLLoaderSyncOrAsyncTrustTokenOperationTest,
       HandlesTrustTokenBeginFailure) {
  base::HistogramTester histogram_tester;
  ResourceRequest request = CreateTrustTokenResourceRequest();

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  MockTrustTokenDevToolsObserver devtools_observer;

  URLLoaderOptions url_loader_options;
  MockTrustTokenObserver trust_token_observer;
  url_loader_options.trust_token_observer = trust_token_observer.GetRemote();
  url_loader_options.trust_token_helper_factory =
      std::make_unique<MockTrustTokenRequestHelperFactory>(
          mojom::TrustTokenOperationStatus::kFailedPrecondition /* on_begin */,
          std::nullopt /* on_finalize */, GetParam(),
          &outbound_trust_token_operation_was_successful_);
  url_loader_options.devtools_observer = devtools_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.OperationOutcome.Issuance",
      mojom::TrustTokenOperationStatus::kFailedPrecondition, 1);

  EXPECT_EQ(client()->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  EXPECT_EQ(client()->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kFailedPrecondition);
  // Verify the DevTools event was fired and it has the right status.
  EXPECT_EQ(devtools_observer.trust_token_operation_status(),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);

  EXPECT_FALSE(client()->response_head());
  EXPECT_FALSE(client()->response_body().is_valid());

  trust_token_observer.WaitForTrustTokens(1u);
  EXPECT_THAT(
      trust_token_observer.observed_tokens(),
      testing::ElementsAre(MatchesTrustTokenDetails(
          test_server()->GetOrigin(), test_server()->GetOrigin(), false)));
}

// When a request's associated Trust Tokens operation's Begin step succeeds but
// its Finalize step fails, the request itself should fail.
TEST_P(URLLoaderSyncOrAsyncTrustTokenOperationTest,
       HandlesTrustTokenFinalizeFailure) {
  ResourceRequest request = CreateTrustTokenResourceRequest();

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  MockTrustTokenDevToolsObserver devtools_observer;

  URLLoaderOptions url_loader_options;
  MockTrustTokenObserver trust_token_observer;
  url_loader_options.trust_token_observer = trust_token_observer.GetRemote();
  url_loader_options.trust_token_helper_factory =
      std::make_unique<MockTrustTokenRequestHelperFactory>(
          mojom::TrustTokenOperationStatus::kOk /* on_begin */,
          mojom::TrustTokenOperationStatus::kBadResponse /* on_finalize */,
          GetParam(), &outbound_trust_token_operation_was_successful_);
  url_loader_options.devtools_observer = devtools_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  EXPECT_EQ(client()->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kBadResponse);
  // Verify the DevTools event was fired and it has the right status.
  EXPECT_EQ(devtools_observer.trust_token_operation_status(),
            mojom::TrustTokenOperationStatus::kBadResponse);

  trust_token_observer.WaitForTrustTokens(1u);
  EXPECT_THAT(
      trust_token_observer.observed_tokens(),
      testing::ElementsAre(MatchesTrustTokenDetails(
          test_server()->GetOrigin(), test_server()->GetOrigin(), false)));
}

// When URLLoader receives a  request parameterized to perform a Trust Tokens
// operation but fails to create a trust token request helper (because a
// universal Trust Tokens precondition is violated, for instance), the request
// should fail entirely.
TEST_P(URLLoaderSyncOrAsyncTrustTokenOperationTest,
       HandlesTrustTokenRequestHelperCreationFailure) {
  ResourceRequest request = CreateTrustTokenResourceRequest();

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  MockTrustTokenDevToolsObserver devtools_observer;

  URLLoaderOptions url_loader_options;
  MockTrustTokenObserver trust_token_observer;
  url_loader_options.trust_token_observer = trust_token_observer.GetRemote();
  url_loader_options.trust_token_helper_factory =
      std::make_unique<MockTrustTokenRequestHelperFactory>(
          mojom::TrustTokenOperationStatus::
              kInternalError /* helper_creation_error */,
          GetParam());
  url_loader_options.devtools_observer = devtools_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_EQ(client()->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  EXPECT_EQ(client()->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kInternalError);
  // Verify the DevTools event was fired and it has the right status.
  EXPECT_EQ(devtools_observer.trust_token_operation_status(),
            mojom::TrustTokenOperationStatus::kInternalError);

  trust_token_observer.WaitForTrustTokens(1u);
  EXPECT_THAT(
      trust_token_observer.observed_tokens(),
      testing::ElementsAre(MatchesTrustTokenDetails(
          test_server()->GetOrigin(), test_server()->GetOrigin(), false)));
}

// When URLLoader receives a request that is blocked by policy, the request
// should fail entirely and report a blocked event to the observer.
TEST_P(URLLoaderSyncOrAsyncTrustTokenOperationTest,
       HandlesTrustTokenRequestHelperCreationBlocked) {
  ResourceRequest request = CreateTrustTokenResourceRequest();

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader_remote;
  std::unique_ptr<URLLoader> url_loader;
  context().mutable_factory_params().process_id = mojom::kBrowserProcessId;
  MockTrustTokenDevToolsObserver devtools_observer;

  URLLoaderOptions url_loader_options;
  MockTrustTokenObserver trust_token_observer;
  url_loader_options.trust_token_observer = trust_token_observer.GetRemote();
  url_loader_options.trust_token_helper_factory =
      std::make_unique<MockTrustTokenRequestHelperFactory>(
          mojom::TrustTokenOperationStatus::
              kUnauthorized /* helper_creation_error */,
          GetParam());
  url_loader_options.devtools_observer = devtools_observer.Bind();
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader_remote.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  delete_run_loop.Run();

  EXPECT_EQ(client()->completion_status().error_code,
            net::ERR_TRUST_TOKEN_OPERATION_FAILED);
  EXPECT_EQ(client()->completion_status().trust_token_operation_status,
            mojom::TrustTokenOperationStatus::kUnauthorized);
  // Verify the DevTools event was fired and it has the right status.
  EXPECT_EQ(devtools_observer.trust_token_operation_status(),
            mojom::TrustTokenOperationStatus::kUnauthorized);

  trust_token_observer.WaitForTrustTokens(1u);
  EXPECT_THAT(
      trust_token_observer.observed_tokens(),
      testing::ElementsAre(MatchesTrustTokenDetails(
          test_server()->GetOrigin(), test_server()->GetOrigin(), true)));
}

TEST_F(URLLoaderTest, OnRawRequestClientSecurityStateFactory) {
  MockDevToolsObserver devtools_observer;
  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/simple_page.html"));
  request.devtools_request_id = "fake-id";

  auto client_security_state = mojom::ClientSecurityState::New();
  client_security_state->is_web_secure_context = false;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  context().mutable_factory_params().client_security_state =
      std::move(client_security_state);
  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  URLLoaderOptions url_loader_options;
  MockTrustTokenObserver trust_token_observer;
  url_loader_options.trust_token_observer = trust_token_observer.GetRemote();
  url_loader_options.devtools_observer = devtools_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  delete_run_loop.Run();
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  devtools_observer.WaitUntilRawRequest(0);
  ASSERT_TRUE(devtools_observer.client_security_state());
  EXPECT_EQ(
      devtools_observer.client_security_state()->private_network_request_policy,
      mojom::PrivateNetworkRequestPolicy::kAllow);
  EXPECT_EQ(devtools_observer.client_security_state()->is_web_secure_context,
            false);
  EXPECT_EQ(devtools_observer.client_security_state()->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

TEST_F(URLLoaderTest, OnRawRequestClientSecurityStateRequest) {
  MockDevToolsObserver devtools_observer;
  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/simple_page.html"));
  request.devtools_request_id = "fake-id";
  auto client_security_state = mojom::ClientSecurityState::New();
  client_security_state->is_web_secure_context = false;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  request.trusted_params->client_security_state =
      std::move(client_security_state);

  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  URLLoaderOptions url_loader_options;
  url_loader_options.devtools_observer = devtools_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());
  delete_run_loop.Run();
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  devtools_observer.WaitUntilRawRequest(0);
  ASSERT_TRUE(devtools_observer.client_security_state());
  EXPECT_EQ(
      devtools_observer.client_security_state()->private_network_request_policy,
      mojom::PrivateNetworkRequestPolicy::kAllow);
  EXPECT_EQ(devtools_observer.client_security_state()->is_web_secure_context,
            false);
  EXPECT_EQ(devtools_observer.client_security_state()->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

TEST_F(URLLoaderTest, OnRawRequestClientSecurityStateNotPresent) {
  MockDevToolsObserver devtools_observer;
  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/simple_page.html"));
  request.devtools_request_id = "fake-id";

  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  URLLoaderOptions url_loader_options;
  url_loader_options.devtools_observer = devtools_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());
  delete_run_loop.Run();
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  devtools_observer.WaitUntilRawRequest(0);
  ASSERT_FALSE(devtools_observer.client_security_state());
}

TEST_F(URLLoaderTest, OnRawResponseIPAddressSpace) {
  MockDevToolsObserver devtools_observer;
  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/simple_page.html"));
  request.devtools_request_id = "fake-id";

  context().mutable_factory_params().process_id = kProcessId;
  context().mutable_factory_params().is_orb_enabled = false;

  base::RunLoop delete_run_loop;
  mojo::PendingRemote<mojom::URLLoader> loader;
  URLLoaderOptions url_loader_options;
  url_loader_options.devtools_observer = devtools_observer.Bind();
  std::unique_ptr<URLLoader> url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());
  delete_run_loop.Run();
  client()->RunUntilComplete();
  EXPECT_EQ(net::OK, client()->completion_status().error_code);

  devtools_observer.WaitUntilRawResponse(0);
  ASSERT_EQ(devtools_observer.resource_address_space(),
            mojom::IPAddressSpace::kLocal);
}

TEST_F(URLLoaderMockSocketTest, OrbDoesNotCloseSocketsWhenResourcesNotBlocked) {
  orb_enabled_ = true;

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, 0,
                     "GET / HTTP/1.1\r\n"
                     "Host: origin.test\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, 1,
                    "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Length: 5\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, 2, "Hello"),
  };

  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  net::SequencedSocketData socket_data_reads_writes(kReads, kWrites);
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_reads_writes);

  GURL url("http://origin.test/");
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kCors;
  request.request_initiator = initiator;
  std::string body;
  EXPECT_EQ(net::OK, LoadRequest(request, &body));
  EXPECT_EQ(body, "Hello");

  // Socket should still be alive, in the socket pool.
  EXPECT_TRUE(socket_data_reads_writes.socket());
}

TEST_F(URLLoaderMockSocketTest, OrbClosesSocketOnReceivingHeaders) {
  orb_enabled_ = true;

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, 0,
                     "GET / HTTP/1.1\r\n"
                     "Host: origin.test\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, 1,
                    "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Type: text/html\r\n"
                    "X-Content-Type-Options: nosniff\r\n"
                    "Content-Length: 23\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, 2, "This should not be read"),
  };

  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  net::SequencedSocketData socket_data_reads_writes(kReads, kWrites);
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_reads_writes);

  GURL url("http://origin.test/");
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  std::string body;
  EXPECT_EQ(net::OK, LoadRequest(request, &body));
  EXPECT_TRUE(body.empty());

  // Socket should have been destroyed, so it will not be reused.
  EXPECT_FALSE(socket_data_reads_writes.socket());
}

TEST_F(URLLoaderMockSocketTest,
       OrbDoesNotCloseSocketsWhenResourcesNotBlockedAfterSniffingMimeType) {
  orb_enabled_ = true;

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, 0,
                     "GET / HTTP/1.1\r\n"
                     "Host: origin.test\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, 1,
                    "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: 17\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, 2, "Not actually JSON"),
  };

  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  net::SequencedSocketData socket_data_reads_writes(kReads, kWrites);
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_reads_writes);

  GURL url("http://origin.test/");
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kCors;
  request.request_initiator = initiator;
  std::string body;
  EXPECT_EQ(net::OK, LoadRequest(request, &body));
  EXPECT_EQ("Not actually JSON", body);

  // Socket should still be alive, in the socket pool.
  EXPECT_TRUE(socket_data_reads_writes.socket());
}

TEST_F(URLLoaderMockSocketTest, OrbClosesSocketOnSniffingMimeType) {
  orb_enabled_ = true;

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, 0,
                     "GET / HTTP/1.1\r\n"
                     "Host: origin.test\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, 1,
                    "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: 9\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, 2, "{\"x\" : 3}"),
  };

  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  net::SequencedSocketData socket_data_reads_writes(kReads, kWrites);
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_reads_writes);

  GURL url("http://origin.test/");
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  std::string body;
  EXPECT_EQ(net::OK, LoadRequest(request, &body));
  EXPECT_TRUE(body.empty());

  // Socket should have been destroyed, so it will not be reused.
  EXPECT_FALSE(socket_data_reads_writes.socket());
}

TEST_F(URLLoaderMockSocketTest, CorpClosesSocket) {
  auto client_security_state = NewSecurityState();
  client_security_state->cross_origin_embedder_policy.value =
      mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, 0,
                     "GET / HTTP/1.1\r\n"
                     "Host: origin.test\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, 1,
                    "HTTP/1.1 200 OK\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Type: test/plain\r\n"
                    "Content-Length: 23\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, 2, "This should not be read"),
  };

  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  net::SequencedSocketData socket_data_reads_writes(kReads, kWrites);
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_reads_writes);

  GURL url("http://origin.test/");
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, LoadRequest(request));

  // Socket should have been destroyed, so it will not be reused.
  EXPECT_FALSE(socket_data_reads_writes.socket());
}

class URLLoaderMockSocketAuctionOnlyTest
    : public URLLoaderMockSocketTest,
      public testing::WithParamInterface<std::string> {};

TEST_P(URLLoaderMockSocketAuctionOnlyTest,
       FetchAuctionOnlySignalsFromRendererClosesSocket) {
  auto client_security_state = NewSecurityState();
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, 0,
                     "GET / HTTP/1.1\r\n"
                     "Host: origin.test\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  const std::string first_read = base::StringPrintf(
      "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "%s"
      "Content-Type: text/plain\r\n"
      "Content-Length: 23\r\n\r\n",
      GetParam().c_str());
  net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, 1, first_read.c_str()),
      net::MockRead(net::SYNCHRONOUS, 2, "This should not be read"),
  };

  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  net::SequencedSocketData socket_data_reads_writes(kReads, kWrites);
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_reads_writes);

  GURL url("http://origin.test/");
  url::Origin initiator = url::Origin::Create(url);

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;

  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, LoadRequest(request, /*body=*/nullptr,
                                                      /*is_trusted=*/false));

  // Socket should have been destroyed, so it will not be reused.
  EXPECT_FALSE(socket_data_reads_writes.socket());
}

TEST_P(URLLoaderMockSocketAuctionOnlyTest,
       FetchAuctionOnlySignalsFromBrowserProcessSucceeds) {
  auto client_security_state = NewSecurityState();
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  set_factory_client_security_state(std::move(client_security_state));

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  const net::MockWrite kWrites[] = {
      net::MockWrite(net::SYNCHRONOUS, 0,
                     "GET / HTTP/1.1\r\n"
                     "Host: origin.test\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  const std::string first_read = base::StringPrintf(
      "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "%s"
      "Content-Type: text/plain\r\n"
      "Content-Length: 23\r\n\r\n",
      GetParam().c_str());
  net::MockRead kReads[] = {
      net::MockRead(net::SYNCHRONOUS, 1, first_read.c_str()),
      net::MockRead(net::SYNCHRONOUS, 2, "This should not be read"),
  };

  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  net::SequencedSocketData socket_data_reads_writes(kReads, kWrites);
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_reads_writes);

  GURL url("http://origin.test/");
  url::Origin initiator = url::Origin::Create(url);

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  EXPECT_EQ(net::OK,
            LoadRequest(request, /*body=*/nullptr, /*is_trusted=*/true));
  EXPECT_TRUE(socket_data_reads_writes.socket());
}

// TODO(crbug.com/40269364): Remove old names once API users have migrated to
// new names.
INSTANTIATE_TEST_SUITE_P(
    All,
    URLLoaderMockSocketAuctionOnlyTest,
    testing::Values(
        "Ad-Auction-Only: true\r\n",
        "X-FLEDGE-Auction-Only: true\r\n",
        "Ad-Auction-Only: true\r\nX-FLEDGE-Auction-Only: true\r\n"));

TEST_F(URLLoaderMockSocketTest, PrivateNetworkRequestPolicyDoesNotCloseSocket) {
  auto client_security_state = NewSecurityState();
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;
  set_factory_client_security_state(std::move(client_security_state));

  net::MockConnect kConnect = net::MockConnect(net::ASYNC, net::OK);
  // No data should be read or written. Trying to do so will assert.
  net::SequencedSocketData socket_data_no_reads_no_writes;
  net::SequencedSocketData socket_data_connect(
      kConnect, base::span<net::MockRead>(), base::span<net::MockWrite>());
  socket_factory_.AddSocketDataProvider(&socket_data_connect);
  socket_factory_.AddSocketDataProvider(&socket_data_no_reads_no_writes);

  GURL url("http://origin.test/");
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request = CreateResourceRequest("GET", url);
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));

  // Socket should not be closed, since it can be reused.
  EXPECT_TRUE(socket_data_no_reads_no_writes.socket());
}

TEST_F(URLLoaderTest, WithDnsAliases) {
  GURL url(test_server_.GetURL(kHostnameWithAliases, "/echo"));

  EXPECT_EQ(net::OK, Load(url));
  ASSERT_TRUE(client_.response_head());
  EXPECT_THAT(client_.response_head()->dns_aliases,
              testing::ElementsAre("alias1", "alias2", "host"));
}

TEST_F(URLLoaderTest, NoAdditionalDnsAliases) {
  GURL url(test_server_.GetURL(kHostnameWithoutAliases, "/echo"));

  EXPECT_EQ(net::OK, Load(url));
  ASSERT_TRUE(client_.response_head());
  EXPECT_THAT(client_.response_head()->dns_aliases,
              testing::ElementsAre(kHostnameWithoutAliases));
}

TEST_F(URLLoaderTest,
       PrivateNetworkRequestPolicyReportsOnPrivateNetworkRequestWarn) {
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/simple_page.html"));
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  request.devtools_request_id = "fake-id";

  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = false;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  set_factory_client_security_state(std::move(client_security_state));

  MockDevToolsObserver devtools_observer;
  set_devtools_observer_for_next_request(&devtools_observer);

  EXPECT_EQ(net::OK, LoadRequest(request));

  devtools_observer.WaitUntilPrivateNetworkRequest();
  ASSERT_TRUE(devtools_observer.private_network_request_params());
  auto& params = *devtools_observer.private_network_request_params();
  ASSERT_TRUE(params.client_security_state);
  auto& state = params.client_security_state;
  EXPECT_EQ(state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kWarn);
  EXPECT_EQ(state->is_web_secure_context, false);
  EXPECT_EQ(state->ip_address_space, mojom::IPAddressSpace::kPublic);
  EXPECT_EQ(params.resource_address_space, mojom::IPAddressSpace::kLocal);
  EXPECT_EQ(params.devtools_request_id, "fake-id");
  EXPECT_TRUE(params.is_warning);
  EXPECT_THAT(params.url.spec(), testing::HasSubstr("simple_page.html"));
}

TEST_F(URLLoaderTest,
       PrivateNetworkRequestPolicyReportsOnPrivateNetworkRequestBlock) {
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/simple_page.html"));
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  request.devtools_request_id = "fake-id";

  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = false;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  set_factory_client_security_state(std::move(client_security_state));

  MockDevToolsObserver devtools_observer;
  set_devtools_observer_for_next_request(&devtools_observer);

  EXPECT_EQ(net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS,
            LoadRequest(request));

  devtools_observer.WaitUntilPrivateNetworkRequest();
  ASSERT_TRUE(devtools_observer.private_network_request_params());
  auto& params = *devtools_observer.private_network_request_params();
  ASSERT_TRUE(params.client_security_state);
  auto& state = params.client_security_state;
  EXPECT_EQ(state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kBlock);
  EXPECT_EQ(state->is_web_secure_context, false);
  EXPECT_EQ(state->ip_address_space, mojom::IPAddressSpace::kPublic);
  EXPECT_EQ(params.resource_address_space, mojom::IPAddressSpace::kLocal);
  EXPECT_EQ(params.devtools_request_id, "fake-id");
  EXPECT_FALSE(params.is_warning);
  EXPECT_THAT(params.url.spec(), testing::HasSubstr("simple_page.html"));
}

TEST_F(URLLoaderTest,
       PrivateNetworkRequestPolicyReportsOnPrivateNetworkRequestAllow) {
  url::Origin initiator =
      url::Origin::Create(GURL("http://other-origin.test/"));

  ResourceRequest request =
      CreateResourceRequest("GET", test_server()->GetURL("/simple_page.html"));
  request.mode = mojom::RequestMode::kNoCors;
  request.request_initiator = initiator;
  request.devtools_request_id = "fake-id";

  auto client_security_state = NewSecurityState();
  client_security_state->is_web_secure_context = false;
  client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;
  client_security_state->ip_address_space = mojom::IPAddressSpace::kPublic;
  set_factory_client_security_state(std::move(client_security_state));

  MockDevToolsObserver devtools_observer;
  set_devtools_observer_for_next_request(&devtools_observer);

  EXPECT_EQ(net::OK, LoadRequest(request));

  // Check that OnPrivateNetworkRequest wasn't triggered.
  devtools_observer.WaitUntilRawResponse(0);
  EXPECT_FALSE(devtools_observer.private_network_request_params());
}

// An empty ACCEPT_CH frame should skip the client call.
TEST_F(URLLoaderFakeTransportInfoTest, AcceptCHFrameEmptyString) {
  net::TransportInfo info = net::DefaultTransportInfo();
  info.accept_ch_frame = "";

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::make_unique<FakeTransportInfoInterceptor>(info));

  MockAcceptCHFrameObserver accept_ch_frame_observer;
  set_accept_ch_frame_observer_for_next_request(&accept_ch_frame_observer);

  // Despite its name, IsError(OK) asserts that the matched value is OK.
  EXPECT_THAT(Load(url), IsError(net::OK));
  EXPECT_FALSE(accept_ch_frame_observer.called());
}

TEST_F(URLLoaderFakeTransportInfoTest, AcceptCHFrameParseString) {
  net::TransportInfo info = net::DefaultTransportInfo();
  info.accept_ch_frame = "Sec-CH-UA-Platform";

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::make_unique<FakeTransportInfoInterceptor>(info));

  MockAcceptCHFrameObserver accept_ch_frame_observer;
  set_accept_ch_frame_observer_for_next_request(&accept_ch_frame_observer);

  // Despite its name, IsError(OK) asserts that the matched value is OK.
  EXPECT_THAT(Load(url), IsError(net::OK));
  EXPECT_THAT(
      accept_ch_frame_observer.accept_ch_frame(),
      testing::ElementsAreArray({mojom::WebClientHintsType::kUAPlatform}));
}

TEST_F(URLLoaderFakeTransportInfoTest, AcceptCHFrameRemoveDuplicates) {
  net::TransportInfo info = net::DefaultTransportInfo();
  info.accept_ch_frame = "Sec-CH-UA-Platform, Sec-CH-UA-Model";

  net::HttpRequestHeaders headers;
  headers.SetHeader("Sec-CH-UA-Model", "foo");
  set_additional_headers(headers);

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::make_unique<FakeTransportInfoInterceptor>(info));

  MockAcceptCHFrameObserver accept_ch_frame_observer;
  set_accept_ch_frame_observer_for_next_request(&accept_ch_frame_observer);

  EXPECT_THAT(Load(url), IsError(net::OK));
  // Headers should be de-dupped.
  EXPECT_THAT(
      accept_ch_frame_observer.accept_ch_frame(),
      testing::ElementsAreArray({mojom::WebClientHintsType::kUAPlatform}));
}

TEST_F(URLLoaderFakeTransportInfoTest, AcceptCHFrameIgnoreMalformed) {
  net::TransportInfo info = net::DefaultTransportInfo();
  // Non-existent hint.
  info.accept_ch_frame = "Foo";

  const GURL url("http://fake-endpoint");

  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::make_unique<FakeTransportInfoInterceptor>(info));

  MockAcceptCHFrameObserver accept_ch_frame_observer;
  set_accept_ch_frame_observer_for_next_request(&accept_ch_frame_observer);

  // Despite its name, IsError(OK) asserts that the matched value is OK.
  EXPECT_THAT(Load(url), IsError(net::OK));
  EXPECT_FALSE(accept_ch_frame_observer.called());
}

TEST_F(URLLoaderTest, CookieSettingOverridesCopiedToURLRequest) {
  GURL url = test_server()->GetURL("/simple_page.html");
  net::CookieSettingOverrides cookie_setting_overrides =
      net::CookieSettingOverrides::All();
  // The overrides are not allowed to start out with the
  // `kStorageAccessGrantEligible` or `kStorageAccessGrantEligibleViaHeader`
  // overrides present.
  cookie_setting_overrides.Remove(
      net::CookieSettingOverride::kStorageAccessGrantEligible);
  cookie_setting_overrides.Remove(
      net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader);

  set_cookie_setting_overrides(cookie_setting_overrides);
  bool was_intercepted = false;
  net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
      url, std::make_unique<ExpectCookieSettingOverridesURLRequestInterceptor>(
               cookie_setting_overrides, &was_intercepted));

  EXPECT_THAT(Load(url), IsOk());
  EXPECT_TRUE(was_intercepted);
}

TEST_F(URLLoaderTest, ReadAndDiscardBody) {
  const std::string file = "simple_page.html";
  const GURL url = test_server()->GetURL("/" + file);
  int64_t actual_size = 0;
  bool got_file_size = base::GetFileSize(GetTestFilePath(file), &actual_size);
  ASSERT_TRUE(got_file_size);

  TestURLLoaderClient loader_client;
  ResourceRequest request = CreateResourceRequest("GET", url);
  SetUpContext(url, /*is_trusted=*/true);
  URLLoaderOptions url_loader_options;
  url_loader_options.options = mojom::kURLLoadOptionReadAndDiscardBody;
  std::unique_ptr<URLLoader> url_loader;
  base::RunLoop delete_run_loop;
  mojo::Remote<mojom::URLLoader> loader;
  url_loader = url_loader_options.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop, &url_loader),
      loader.BindNewPipeAndPassReceiver(), request,
      loader_client.CreateRemote());
  loader_client.RunUntilResponseReceived();

  // Response body should not be set.
  EXPECT_FALSE(loader_client.response_body().is_valid());

  loader_client.RunUntilComplete();
  const auto& completion_status = loader_client.completion_status();
  EXPECT_EQ(completion_status.error_code, net::OK);
  EXPECT_EQ(completion_status.decoded_body_length, actual_size);
  EXPECT_EQ(completion_status.encoded_body_length, actual_size);

  delete_run_loop.Run();
}

class SharedStorageRequestHelperURLLoaderTest : public URLLoaderTest {
 public:
  void RegisterAdditionalHandlers() override {
    SharedStorageRequestCount::Reset();
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleSharedStorageRequestMultiple,
                            GetSharedStorageWriteHeaderValues()));
    net::test_server::RegisterDefaultHandlers(&test_server_);
  }

  std::vector<std::string> GetSharedStorageWriteHeaderValues() const {
    return {"clear, set;value=v;key=k", "append;value=a;key=b, delete;key=k"};
  }

  void SetURLLoaderOptionsForSharedStorageRequest(
      bool shared_storage_writable_eligible) {
    observer_ = std::make_unique<SharedStorageTestURLLoaderNetworkObserver>();
    url_loader_options_.url_loader_network_observer = observer_->Bind();
    url_loader_options_.shared_storage_writable_eligible =
        shared_storage_writable_eligible;
  }

  void WaitForHeadersReceived(size_t expected_total) {
    observer_->FlushReceivers();
    observer_->WaitForHeadersReceived(expected_total);
  }

 protected:
  base::RunLoop delete_run_loop_;
  mojo::PendingRemote<mojom::URLLoader> loader_remote_;
  std::unique_ptr<URLLoader> url_loader_;
  URLLoaderOptions url_loader_options_;
  std::unique_ptr<SharedStorageTestURLLoaderNetworkObserver> observer_;
};

TEST_F(SharedStorageRequestHelperURLLoaderTest, SimpleRequest) {
  const char kHostname[] = "a.test";
  const GURL kRequestUrl =
      test_server_.GetURL(kHostname, MakeSharedStorageTestPath());
  const url::Origin kTestOrigin = url::Origin::Create(kRequestUrl);
  ResourceRequest request = CreateResourceRequest("GET", kRequestUrl);

  SetURLLoaderOptionsForSharedStorageRequest(
      /*shared_storage_writable_eligible=*/true);

  url_loader_ = url_loader_options_.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop_, &url_loader_),
      loader_remote_.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilComplete();
  WaitForHeadersReceived(1);

  EXPECT_EQ(observer_->headers_received().size(), 1u);
  EXPECT_EQ(observer_->headers_received().front().first, kTestOrigin);
  EXPECT_THAT(
      observer_->headers_received().front().second,
      ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/std::nullopt, /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                  /*key=*/"k", /*value=*/"v",
                                  /*ignore_if_present=*/std::nullopt)));

  delete_run_loop_.Run();
}

TEST_F(SharedStorageRequestHelperURLLoaderTest, SimpleRedirect) {
  const char kHostname[] = "a.test";
  const GURL kRequestUrl = test_server_.GetURL(
      kHostname, "/shared_storage/redirect/write.html?destination.html");
  const url::Origin kTestOrigin = url::Origin::Create(kRequestUrl);
  ResourceRequest request = CreateResourceRequest("GET", kRequestUrl);

  SetURLLoaderOptionsForSharedStorageRequest(
      /*shared_storage_writable_eligible=*/true);

  url_loader_ = url_loader_options_.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop_, &url_loader_),
      loader_remote_.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  ASSERT_TRUE(client()->has_received_redirect());
  WaitForHeadersReceived(1);

  EXPECT_EQ(observer_->headers_received().size(), 1u);
  EXPECT_EQ(observer_->headers_received().front().first, kTestOrigin);
  EXPECT_THAT(
      observer_->headers_received().front().second,
      ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/std::nullopt, /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                  /*key=*/"k", /*value=*/"v",
                                  /*ignore_if_present=*/std::nullopt)));

  // Follow redirect is called by the client. Even if the shared storage request
  // helper updates headers, `FollowRedirect()` could still be called by the
  // client without headers changes.
  url_loader_->FollowRedirect(/*removed_headers=*/{}, /*modified_headers=*/{},
                              /*modified_cors_exempt_headers=*/{},
                              std::nullopt);
  client()->RunUntilComplete();

  delete_run_loop_.Run();
}

TEST_F(SharedStorageRequestHelperURLLoaderTest, MultipleRedirects) {
  const char kHostname[] = "a.test";
  const GURL kRequestUrl =
      test_server_.GetURL(kHostname,
                          "/shared_storage/redirect/write.html?redirect/"
                          "no_writing.html%3Fdestination/write.html");
  const url::Origin kTestOrigin = url::Origin::Create(kRequestUrl);
  ResourceRequest request = CreateResourceRequest("GET", kRequestUrl);

  SetURLLoaderOptionsForSharedStorageRequest(
      /*shared_storage_writable_eligible=*/true);

  url_loader_ = url_loader_options_.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop_, &url_loader_),
      loader_remote_.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  ASSERT_TRUE(client()->has_received_redirect());
  WaitForHeadersReceived(1);

  EXPECT_EQ(observer_->headers_received().size(), 1u);
  EXPECT_EQ(observer_->headers_received().front().first, kTestOrigin);
  EXPECT_THAT(
      observer_->headers_received().front().second,
      ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/std::nullopt, /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                  /*key=*/"k", /*value=*/"v",
                                  /*ignore_if_present=*/std::nullopt)));

  client()->ClearHasReceivedRedirect();

  // Follow redirect is called by the client. Even if the shared storage request
  // helper updates headers, `FollowRedirect()` could still be called by the
  // client without headers changes.
  url_loader_->FollowRedirect(/*removed_headers=*/{}, /*modified_headers=*/{},
                              /*modified_cors_exempt_headers=*/{},
                              std::nullopt);
  client()->RunUntilRedirectReceived();
  ASSERT_TRUE(client()->has_received_redirect());

  // No new shared storage headers are observed.
  EXPECT_EQ(observer_->headers_received().size(), 1u);

  // Follow redirect is called by the client. Even if the shared storage request
  // helper updates headers, `FollowRedirect()` could still be called by the
  // client without headers changes.
  url_loader_->FollowRedirect(/*removed_headers=*/{}, /*modified_headers=*/{},
                              /*modified_cors_exempt_headers=*/{},
                              std::nullopt);
  client()->RunUntilComplete();
  WaitForHeadersReceived(2);

  EXPECT_EQ(observer_->headers_received().size(), 2u);
  EXPECT_EQ(observer_->headers_received().back().first, kTestOrigin);
  EXPECT_THAT(
      observer_->headers_received().back().second,
      ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kAppend,
                                  /*key=*/"b", /*value=*/"a",
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kDelete,
                                  /*key=*/"k", /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt)));

  delete_run_loop_.Run();
}

TEST_F(SharedStorageRequestHelperURLLoaderTest, CrossSiteRedirect) {
  const char kHostname[] = "a.test";
  const char kCrossOriginHostname[] = "b.test";
  const GURL kRequestUrl = test_server_.GetURL(
      kHostname,
      base::StrCat({"/cross-site?", std::string(kCrossOriginHostname),
                    "/shared_storage/destination/write.html"}));
  const url::Origin kTestOrigin = url::Origin::Create(kRequestUrl);
  const url::Origin kCrossOrigin =
      url::Origin::Create(test_server_.GetURL(kCrossOriginHostname, "/"));
  ResourceRequest request = CreateResourceRequest("GET", kRequestUrl);

  SetURLLoaderOptionsForSharedStorageRequest(
      /*shared_storage_writable_eligible=*/true);

  url_loader_ = url_loader_options_.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop_, &url_loader_),
      loader_remote_.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  ASSERT_TRUE(client()->has_received_redirect());

  // No shared storage headers are received yet.
  EXPECT_TRUE(observer_->headers_received().empty());

  // Follow redirect is called by the client. Even if the shared storage request
  // helper updates headers, `FollowRedirect()` could still be called by the
  // client without headers changes.
  url_loader_->FollowRedirect(/*removed_headers=*/{}, /*modified_headers=*/{},
                              /*modified_cors_exempt_headers=*/{},
                              std::nullopt);
  client()->RunUntilComplete();
  WaitForHeadersReceived(1);

  EXPECT_EQ(observer_->headers_received().size(), 1u);
  EXPECT_EQ(observer_->headers_received().front().first, kCrossOrigin);
  EXPECT_THAT(
      observer_->headers_received().front().second,
      ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/std::nullopt, /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                  /*key=*/"k", /*value=*/"v",
                                  /*ignore_if_present=*/std::nullopt)));

  delete_run_loop_.Run();
}

TEST_F(SharedStorageRequestHelperURLLoaderTest, RedirectNoLongerEligible) {
  const char kHostname[] = "a.test";
  const GURL kRequestUrl = test_server_.GetURL(
      kHostname, "/shared_storage/redirect/new?shared_storage/write.html");
  const url::Origin kTestOrigin = url::Origin::Create(kRequestUrl);
  ResourceRequest request = CreateResourceRequest("GET", kRequestUrl);

  SetURLLoaderOptionsForSharedStorageRequest(
      /*shared_storage_writable_eligible=*/true);

  url_loader_ = url_loader_options_.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop_, &url_loader_),
      loader_remote_.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  ASSERT_TRUE(client()->has_received_redirect());

  // Simulate having permission revoked by the client, the effect of which is
  // the request header is removed.
  std::vector<std::string> removed_headers(
      {std::string(kSecSharedStorageWritableHeader.data(),
                   kSecSharedStorageWritableHeader.size())});
  url_loader_->FollowRedirect(removed_headers,
                              /*modified_headers=*/{},
                              /*modified_cors_exempt_headers=*/{},
                              std::nullopt);

  // The `SharedStorageRequestHelper` has `shared_storage_writable_eligible_`
  // now set to false because the request header was removed.
  EXPECT_FALSE(url_loader_->shared_storage_request_helper()
                   ->shared_storage_writable_eligible());
  client()->RunUntilComplete();

  // No shared storage headers are received.
  EXPECT_TRUE(observer_->headers_received().empty());

  delete_run_loop_.Run();
}

TEST_F(SharedStorageRequestHelperURLLoaderTest, RedirectBecomesEligible) {
  const char kHostname[] = "a.test";
  const GURL kRequestUrl = test_server_.GetURL(
      kHostname, "/shared_storage/redirect/new?shared_storage/write.html");
  const url::Origin kTestOrigin = url::Origin::Create(kRequestUrl);
  ResourceRequest request = CreateResourceRequest("GET", kRequestUrl);

  SetURLLoaderOptionsForSharedStorageRequest(
      /*shared_storage_writable_eligible=*/false);

  url_loader_ = url_loader_options_.MakeURLLoader(
      context(), DeleteLoaderCallback(&delete_run_loop_, &url_loader_),
      loader_remote_.InitWithNewPipeAndPassReceiver(), request,
      client()->CreateRemote());

  client()->RunUntilRedirectReceived();
  ASSERT_TRUE(client()->has_received_redirect());

  // Simulate having permission restored by the client, the effect of which is
  // the request header is added.
  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(kSecSharedStorageWritableHeader,
                             kSecSharedStorageWritableValue);
  url_loader_->FollowRedirect(/*removed_headers=*/{}, modified_headers,
                              /*modified_cors_exempt_headers=*/{},
                              std::nullopt);

  // The `SharedStorageRequestHelper` has `shared_storage_writable_eligible_`
  // now set to true because the request header was added.
  EXPECT_TRUE(url_loader_->shared_storage_request_helper()
                  ->shared_storage_writable_eligible());
  client()->RunUntilComplete();

  WaitForHeadersReceived(1);

  EXPECT_EQ(observer_->headers_received().size(), 1u);
  EXPECT_EQ(observer_->headers_received().front().first, kTestOrigin);
  EXPECT_THAT(
      observer_->headers_received().front().second,
      ElementsAre(std::make_tuple(mojom::SharedStorageOperationType::kClear,
                                  /*key=*/std::nullopt, /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt),
                  std::make_tuple(mojom::SharedStorageOperationType::kSet,
                                  /*key=*/"k", /*value=*/"v",
                                  /*ignore_if_present=*/std::nullopt)));

  delete_run_loop_.Run();
}

}  // namespace network
