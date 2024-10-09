// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/url_request/url_request.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "crypto/sha2.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/directory_listing.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/transport_info.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/base/url_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/coalescing_cert_verifier.h"
#include "net/cert/crl_set.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_cache.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_source.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/quic_server_info.h"
#include "net/socket/read_buffering_stream_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_server_config.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/storage_access_api/status.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/websocket_handshake_userdata_key.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_WIN)
#include <objbase.h>

#include <windows.h>

#include <shlobj.h>
#include <wrl/client.h>

#include "base/win/scoped_com_initializer.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/network_error_logging/network_error_logging_test_util.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if BUILDFLAG(ENABLE_WEBSOCKETS)
#include "net/websockets/websocket_test_util.h"
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

using net::test::IsError;
using net::test::IsOk;
using net::test_server::RegisterDefaultHandlers;
using testing::_;
using testing::AnyOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Optional;
using testing::UnorderedElementsAre;

using base::ASCIIToUTF16;
using base::Time;
using std::string;

namespace net {

namespace {

namespace test_default {
#include "net/http/transport_security_state_static_unittest_default.h"
}

const std::u16string kSecret(u"secret");
const std::u16string kUser(u"user");

const base::FilePath::CharType kTestFilePath[] =
    FILE_PATH_LITERAL("net/data/url_request_unittest");

// Tests load timing information in the case a fresh connection was used, with
// no proxy.
void TestLoadTimingNotReused(const LoadTimingInfo& load_timing_info,
                             int connect_timing_flags) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  EXPECT_LE(load_timing_info.request_start,
            load_timing_info.connect_timing.connect_start);
  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              connect_timing_flags);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_start);
  EXPECT_LE(load_timing_info.receive_headers_start,
            load_timing_info.receive_headers_end);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
}

// Same as above, but with proxy times.
void TestLoadTimingNotReusedWithProxy(const LoadTimingInfo& load_timing_info,
                                      int connect_timing_flags) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  EXPECT_LE(load_timing_info.request_start,
            load_timing_info.proxy_resolve_start);
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end,
            load_timing_info.connect_timing.connect_start);
  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              connect_timing_flags);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_start);
  EXPECT_LE(load_timing_info.receive_headers_start,
            load_timing_info.receive_headers_end);
}

// Same as above, but with a reused socket and proxy times.
void TestLoadTimingReusedWithProxy(const LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  EXPECT_LE(load_timing_info.request_start,
            load_timing_info.proxy_resolve_start);
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end, load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_start);
  EXPECT_LE(load_timing_info.receive_headers_start,
            load_timing_info.receive_headers_end);
}

CookieList GetAllCookies(URLRequestContext* request_context) {
  CookieList cookie_list;
  base::RunLoop run_loop;
  request_context->cookie_store()->GetAllCookiesAsync(
      base::BindLambdaForTesting([&](const CookieList& cookies) {
        cookie_list = cookies;
        run_loop.Quit();
      }));
  run_loop.Run();
  return cookie_list;
}

void TestLoadTimingCacheHitNoNetwork(const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_EQ(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  EXPECT_LE(load_timing_info.request_start, load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_start);
  EXPECT_LE(load_timing_info.receive_headers_start,
            load_timing_info.receive_headers_end);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
}

// Job that allows monitoring of its priority.
class PriorityMonitoringURLRequestJob : public URLRequestTestJob {
 public:
  // The latest priority of the job is always written to |request_priority_|.
  PriorityMonitoringURLRequestJob(URLRequest* request,
                                  RequestPriority* request_priority)
      : URLRequestTestJob(request), request_priority_(request_priority) {
    *request_priority_ = DEFAULT_PRIORITY;
  }

  void SetPriority(RequestPriority priority) override {
    *request_priority_ = priority;
    URLRequestTestJob::SetPriority(priority);
  }

 private:
  const raw_ptr<RequestPriority> request_priority_;
};

// Do a case-insensitive search through |haystack| for |needle|.
bool ContainsString(const std::string& haystack, const char* needle) {
  std::string::const_iterator it =
      base::ranges::search(haystack, std::string_view(needle),
                           base::CaseInsensitiveCompareASCII<char>());
  return it != haystack.end();
}

std::unique_ptr<UploadDataStream> CreateSimpleUploadData(
    base::span<const uint8_t> data) {
  auto reader = std::make_unique<UploadBytesElementReader>(data);
  return ElementsUploadDataStream::CreateWithReader(std::move(reader));
}

// Verify that the SSLInfo of a successful SSL connection has valid values.
void CheckSSLInfo(const SSLInfo& ssl_info) {
  // The cipher suite TLS_NULL_WITH_NULL_NULL (0) must not be negotiated.
  uint16_t cipher_suite =
      SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
  EXPECT_NE(0U, cipher_suite);
}

// A network delegate that allows the user to choose a subset of request stages
// to block in. When blocking, the delegate can do one of the following:
//  * synchronously return a pre-specified error code, or
//  * asynchronously return that value via an automatically called callback,
//    or
//  * block and wait for the user to do a callback.
// Additionally, the user may also specify a redirect URL -- then each request
// with the current URL different from the redirect target will be redirected
// to that target, in the on-before-URL-request stage, independent of whether
// the delegate blocks in ON_BEFORE_URL_REQUEST or not.
class BlockingNetworkDelegate : public TestNetworkDelegate {
 public:
  // Stages in which the delegate can block.
  enum Stage {
    NOT_BLOCKED = 0,
    ON_BEFORE_URL_REQUEST = 1 << 0,
    ON_BEFORE_SEND_HEADERS = 1 << 1,
    ON_HEADERS_RECEIVED = 1 << 2,
  };

  // Behavior during blocked stages.  During other stages, just
  // returns OK or NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION.
  enum BlockMode {
    SYNCHRONOUS,    // No callback, returns specified return values.
    AUTO_CALLBACK,  // |this| posts a task to run the callback using the
                    // specified return codes.
    USER_CALLBACK,  // User takes care of doing a callback.  |retval_| and
                    // |auth_retval_| are ignored. In every blocking stage the
                    // message loop is quit.
  };

  // Creates a delegate which does not block at all.
  explicit BlockingNetworkDelegate(BlockMode block_mode);

  BlockingNetworkDelegate(const BlockingNetworkDelegate&) = delete;
  BlockingNetworkDelegate& operator=(const BlockingNetworkDelegate&) = delete;

  // Runs the message loop until the delegate blocks.
  void RunUntilBlocked();

  // For users to trigger a callback returning |response|.
  // Side-effects: resets |stage_blocked_for_callback_| and stored callbacks.
  // Only call if |block_mode_| == USER_CALLBACK.
  void DoCallback(int response);

  // Setters.
  void set_retval(int retval) {
    ASSERT_NE(USER_CALLBACK, block_mode_);
    ASSERT_NE(ERR_IO_PENDING, retval);
    ASSERT_NE(OK, retval);
    retval_ = retval;
  }
  void set_redirect_url(const GURL& url) { redirect_url_ = url; }

  void set_block_on(int block_on) { block_on_ = block_on; }

  // Allows the user to check in which state did we block.
  Stage stage_blocked_for_callback() const {
    EXPECT_EQ(USER_CALLBACK, block_mode_);
    return stage_blocked_for_callback_;
  }

 private:
  void OnBlocked();

  void RunCallback(int response, CompletionOnceCallback callback);

  // TestNetworkDelegate implementation.
  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override;

  int OnBeforeStartTransaction(
      URLRequest* request,
      const HttpRequestHeaders& headers,
      OnBeforeStartTransactionCallback callback) override;

  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url) override;

  // Resets the callbacks and |stage_blocked_for_callback_|.
  void Reset();

  // Checks whether we should block in |stage|. If yes, returns an error code
  // and optionally sets up callback based on |block_mode_|. If no, returns OK.
  int MaybeBlockStage(Stage stage, CompletionOnceCallback callback);

  // Configuration parameters, can be adjusted by public methods:
  const BlockMode block_mode_;

  // Values returned on blocking stages when mode is SYNCHRONOUS or
  // AUTO_CALLBACK. For USER_CALLBACK these are set automatically to IO_PENDING.
  int retval_ = OK;

  GURL redirect_url_;  // Used if non-empty during OnBeforeURLRequest.
  int block_on_ = 0;   // Bit mask: in which stages to block.

  // Internal variables, not set by not the user:
  // Last blocked stage waiting for user callback (unused if |block_mode_| !=
  // USER_CALLBACK).
  Stage stage_blocked_for_callback_ = NOT_BLOCKED;

  // Callback objects stored during blocking stages.
  CompletionOnceCallback callback_;

  // Closure to run to exit RunUntilBlocked().
  base::OnceClosure on_blocked_;

  base::WeakPtrFactory<BlockingNetworkDelegate> weak_factory_{this};
};

BlockingNetworkDelegate::BlockingNetworkDelegate(BlockMode block_mode)
    : block_mode_(block_mode) {}

void BlockingNetworkDelegate::RunUntilBlocked() {
  base::RunLoop run_loop;
  on_blocked_ = run_loop.QuitClosure();
  run_loop.Run();
}

void BlockingNetworkDelegate::DoCallback(int response) {
  ASSERT_EQ(USER_CALLBACK, block_mode_);
  ASSERT_NE(NOT_BLOCKED, stage_blocked_for_callback_);
  CompletionOnceCallback callback = std::move(callback_);
  Reset();

  // |callback| may trigger completion of a request, so post it as a task, so
  // it will run under a subsequent TestDelegate::RunUntilComplete() loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::RunCallback,
                                weak_factory_.GetWeakPtr(), response,
                                std::move(callback)));
}

void BlockingNetworkDelegate::OnBlocked() {
  // If this fails due to |on_blocked_| being null then OnBlocked() was run by
  // a RunLoop other than RunUntilBlocked(), indicating a bug in the calling
  // test.
  std::move(on_blocked_).Run();
}

void BlockingNetworkDelegate::RunCallback(int response,
                                          CompletionOnceCallback callback) {
  std::move(callback).Run(response);
}

int BlockingNetworkDelegate::OnBeforeURLRequest(URLRequest* request,
                                                CompletionOnceCallback callback,
                                                GURL* new_url) {
  if (redirect_url_ == request->url())
    return OK;  // We've already seen this request and redirected elsewhere.

  // TestNetworkDelegate always completes synchronously.
  CHECK_NE(ERR_IO_PENDING, TestNetworkDelegate::OnBeforeURLRequest(
                               request, base::NullCallback(), new_url));

  if (!redirect_url_.is_empty())
    *new_url = redirect_url_;

  return MaybeBlockStage(ON_BEFORE_URL_REQUEST, std::move(callback));
}

int BlockingNetworkDelegate::OnBeforeStartTransaction(
    URLRequest* request,
    const HttpRequestHeaders& headers,
    OnBeforeStartTransactionCallback callback) {
  // TestNetworkDelegate always completes synchronously.
  CHECK_NE(ERR_IO_PENDING, TestNetworkDelegate::OnBeforeStartTransaction(
                               request, headers, base::NullCallback()));

  return MaybeBlockStage(
      ON_BEFORE_SEND_HEADERS,
      base::BindOnce(
          [](OnBeforeStartTransactionCallback callback, int result) {
            std::move(callback).Run(result, std::nullopt);
          },
          std::move(callback)));
}

int BlockingNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    const IPEndPoint& endpoint,
    std::optional<GURL>* preserve_fragment_on_redirect_url) {
  // TestNetworkDelegate always completes synchronously.
  CHECK_NE(ERR_IO_PENDING,
           TestNetworkDelegate::OnHeadersReceived(
               request, base::NullCallback(), original_response_headers,
               override_response_headers, endpoint,
               preserve_fragment_on_redirect_url));

  return MaybeBlockStage(ON_HEADERS_RECEIVED, std::move(callback));
}

void BlockingNetworkDelegate::Reset() {
  EXPECT_NE(NOT_BLOCKED, stage_blocked_for_callback_);
  stage_blocked_for_callback_ = NOT_BLOCKED;
  callback_.Reset();
}

int BlockingNetworkDelegate::MaybeBlockStage(
    BlockingNetworkDelegate::Stage stage,
    CompletionOnceCallback callback) {
  // Check that the user has provided callback for the previous blocked stage.
  EXPECT_EQ(NOT_BLOCKED, stage_blocked_for_callback_);

  if ((block_on_ & stage) == 0) {
    return OK;
  }

  switch (block_mode_) {
    case SYNCHRONOUS:
      EXPECT_NE(OK, retval_);
      return retval_;

    case AUTO_CALLBACK:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::RunCallback,
                                    weak_factory_.GetWeakPtr(), retval_,
                                    std::move(callback)));
      return ERR_IO_PENDING;

    case USER_CALLBACK:
      callback_ = std::move(callback);
      stage_blocked_for_callback_ = stage;
      // We may reach here via a callback prior to RunUntilBlocked(), so post
      // a task to fetch and run the |on_blocked_| closure.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::OnBlocked,
                                    weak_factory_.GetWeakPtr()));
      return ERR_IO_PENDING;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

// OCSPErrorTestDelegate caches the SSLInfo passed to OnSSLCertificateError.
// This is needed because after the certificate failure, the URLRequest will
// retry the connection, and return a partial SSLInfo with a cached cert status.
// The partial SSLInfo does not have the OCSP information filled out.
class OCSPErrorTestDelegate : public TestDelegate {
 public:
  void OnSSLCertificateError(URLRequest* request,
                             int net_error,
                             const SSLInfo& ssl_info,
                             bool fatal) override {
    ssl_info_ = ssl_info;
    on_ssl_certificate_error_called_ = true;
    TestDelegate::OnSSLCertificateError(request, net_error, ssl_info, fatal);
  }

  bool on_ssl_certificate_error_called() {
    return on_ssl_certificate_error_called_;
  }

  SSLInfo ssl_info() { return ssl_info_; }

 private:
  bool on_ssl_certificate_error_called_ = false;
  SSLInfo ssl_info_;
};

#if !BUILDFLAG(IS_IOS)
// Compute the root cert's SPKI hash on the fly, to avoid hardcoding it within
// tests.
bool GetTestRootCertSPKIHash(SHA256HashValue* root_hash) {
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  if (!root_cert)
    return false;
  std::string_view root_spki;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer()),
          &root_spki)) {
    return false;
  }
  crypto::SHA256HashString(root_spki, root_hash, sizeof(SHA256HashValue));
  return true;
}
#endif

}  // namespace

// Inherit PlatformTest since we require the autorelease pool on Mac OS X.
class URLRequestTest : public PlatformTest, public WithTaskEnvironment {
 public:
  URLRequestTest() = default;

  ~URLRequestTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();

    SetTransportSecurityStateSourceForTesting(nullptr);
  }

  void SetUp() override {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_net_log(NetLog::Get());
    SetUpContextBuilder(*context_builder);
    // We set the TestNetworkDelegate after calling SetUpContextBuilder as
    // default_network_delegate() relies on this set up and we don't want to
    // allow subclasses to break the assumption.
    context_builder->set_network_delegate(
        std::make_unique<TestNetworkDelegate>());
    default_context_ = context_builder->Build();
    PlatformTest::SetUp();
  }

  void TearDown() override { default_context_.reset(); }

  virtual void SetUpContextBuilder(URLRequestContextBuilder& builder) {}

  TestNetworkDelegate& default_network_delegate() {
    // This cast is safe because we provided a TestNetworkDelegate in SetUp().
    return *static_cast<TestNetworkDelegate*>(
        default_context_->network_delegate());
  }

  URLRequestContext& default_context() const { return *default_context_; }

  // Creates a temp test file and writes |data| to the file. The file will be
  // deleted after the test completes.
  void CreateTestFile(const char* data,
                      size_t data_size,
                      base::FilePath* test_file) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Get an absolute path since |temp_dir| can contain a symbolic link. As of
    // now, Mac and Android bots return a path with a symbolic link.
    base::FilePath absolute_temp_dir =
        base::MakeAbsoluteFilePath(temp_dir_.GetPath());

    ASSERT_TRUE(base::CreateTemporaryFileInDir(absolute_temp_dir, test_file));
    ASSERT_TRUE(base::WriteFile(*test_file, std::string_view(data, data_size)));
  }

  static std::unique_ptr<ConfiguredProxyResolutionService>
  CreateFixedProxyResolutionService(const std::string& proxy) {
    return ConfiguredProxyResolutionService::CreateFixedForTest(
        proxy, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  std::unique_ptr<URLRequest> CreateFirstPartyRequest(
      const URLRequestContext& context,
      const GURL& url,
      URLRequest::Delegate* delegate) {
    auto req = context.CreateRequest(url, DEFAULT_PRIORITY, delegate,
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
    req->set_initiator(url::Origin::Create(url));
    req->set_site_for_cookies(SiteForCookies::FromUrl(url));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, url::Origin::Create(url),
        url::Origin::Create(url), req->site_for_cookies()));
    return req;
  }

 protected:
  RecordingNetLogObserver net_log_observer_;
  std::unique_ptr<URLRequestContext> default_context_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(URLRequestTest, AboutBlankTest) {
  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(
        default_context().CreateRequest(GURL("about:blank"), DEFAULT_PRIORITY,
                                        &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_TRUE(!r->is_pending());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
    EXPECT_TRUE(r->GetResponseRemoteEndpoint().address().empty());
    EXPECT_EQ(0, r->GetResponseRemoteEndpoint().port());
  }
}

TEST_F(URLRequestTest, InvalidUrlTest) {
  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(
        default_context().CreateRequest(GURL("invalid url"), DEFAULT_PRIORITY,
                                        &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();
    EXPECT_TRUE(d.request_failed());
  }
}

// Test that URLRequest rejects WS URLs by default.
TEST_F(URLRequestTest, WsUrlTest) {
  const url::Origin kOrigin = url::Origin::Create(GURL("http://foo.test/"));

  TestDelegate d;
  std::unique_ptr<URLRequest> r(
      default_context().CreateRequest(GURL("ws://foo.test/"), DEFAULT_PRIORITY,
                                      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  // This is not strictly necessary for this test, but used to trigger a DCHECK.
  // See https://crbug.com/1245115.
  r->set_isolation_info(
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                            kOrigin, SiteForCookies::FromOrigin(kOrigin)));

  r->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(d.request_failed());
  EXPECT_THAT(d.request_status(), IsError(ERR_UNKNOWN_URL_SCHEME));
}

// Test that URLRequest rejects WSS URLs by default.
TEST_F(URLRequestTest, WssUrlTest) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.test/"));

  TestDelegate d;
  std::unique_ptr<URLRequest> r(
      default_context().CreateRequest(GURL("wss://foo.test/"), DEFAULT_PRIORITY,
                                      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  // This is not strictly necessary for this test, but used to trigger a DCHECK.
  // See https://crbug.com/1245115.
  r->set_isolation_info(
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                            kOrigin, SiteForCookies::FromOrigin(kOrigin)));

  r->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(d.request_failed());
  EXPECT_THAT(d.request_status(), IsError(ERR_UNKNOWN_URL_SCHEME));
}

TEST_F(URLRequestTest, InvalidReferrerTest) {
  default_network_delegate().set_cancel_request_with_policy_violating_referrer(
      true);
  TestDelegate d;
  std::unique_ptr<URLRequest> req = default_context().CreateRequest(
      GURL("http://localhost/"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  req->SetReferrer("https://somewhere.com/");

  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(d.request_failed());
}

TEST_F(URLRequestTest, RecordsSameOriginReferrerHistogram) {
  default_network_delegate().set_cancel_request_with_policy_violating_referrer(
      false);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://google.com/"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer("http://google.com");
  req->set_referrer_policy(ReferrerPolicy::NEVER_CLEAR);

  base::HistogramTester histograms;

  req->Start();
  d.RunUntilComplete();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerPolicyForRequest.SameOrigin",
      static_cast<int>(ReferrerPolicy::NEVER_CLEAR), 1);
}

TEST_F(URLRequestTest, RecordsCrossOriginReferrerHistogram) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://google.com/"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer("http://origin.com");

  // Set a different policy just to make sure we aren't always logging the same
  // policy.
  req->set_referrer_policy(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE);

  base::HistogramTester histograms;

  req->Start();
  d.RunUntilComplete();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerPolicyForRequest.CrossOrigin",
      static_cast<int>(
          ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      1);
}

TEST_F(URLRequestTest, RecordsReferrerHistogramAgainOnRedirect) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto network_delegate = std::make_unique<BlockingNetworkDelegate>(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate->set_redirect_url(GURL("http://redirect.com/"));
  context_builder->set_network_delegate(std::move(network_delegate));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://google.com/"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer("http://google.com");

  req->set_referrer_policy(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE);

  base::HistogramTester histograms;

  req->Start();
  d.RunUntilRedirect();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerPolicyForRequest.SameOrigin",
      static_cast<int>(
          ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      1);
  req->FollowDeferredRedirect(/*removed_headers=*/std::nullopt,
                              /*modified_headers=*/std::nullopt);
  d.RunUntilComplete();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerPolicyForRequest.CrossOrigin",
      static_cast<int>(
          ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      1);
}

TEST_F(URLRequestTest, RecordsReferrrerWithInformativePath) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto network_delegate = std::make_unique<BlockingNetworkDelegate>(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate->set_cancel_request_with_policy_violating_referrer(true);
  network_delegate->set_redirect_url(GURL("http://redirect.com/"));
  context_builder->set_network_delegate(std::move(network_delegate));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://google.com/"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  // Since this referrer is much more informative than the initiating origin,
  // we should see the histograms' true buckets populated.
  req->SetReferrer("http://google.com/very-informative-path");

  base::HistogramTester histograms;

  req->Start();
  d.RunUntilRedirect();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerHasInformativePath.SameOrigin",
      /* Check the count of the "true" bucket in the boolean histogram. */ true,
      1);
  req->FollowDeferredRedirect(/*removed_headers=*/std::nullopt,
                              /*modified_headers=*/std::nullopt);
  d.RunUntilComplete();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerHasInformativePath.CrossOrigin", true, 1);
}

TEST_F(URLRequestTest, RecordsReferrerWithInformativeQuery) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto network_delegate = std::make_unique<BlockingNetworkDelegate>(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate->set_cancel_request_with_policy_violating_referrer(true);
  network_delegate->set_redirect_url(GURL("http://redirect.com/"));
  context_builder->set_network_delegate(std::move(network_delegate));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://google.com/"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  // Since this referrer is much more informative than the initiating origin,
  // we should see the histograms' true buckets populated.
  req->SetReferrer("http://google.com/?very-informative-query");

  base::HistogramTester histograms;

  req->Start();
  d.RunUntilRedirect();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerHasInformativePath.SameOrigin",
      /* Check the count of the "true" bucket in the boolean histogram. */ true,
      1);
  req->FollowDeferredRedirect(/*removed_headers=*/std::nullopt,
                              /*modified_headers=*/std::nullopt);
  d.RunUntilComplete();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerHasInformativePath.CrossOrigin", true, 1);
}

TEST_F(URLRequestTest, RecordsReferrerWithoutInformativePathOrQuery) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto network_delegate = std::make_unique<BlockingNetworkDelegate>(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate->set_cancel_request_with_policy_violating_referrer(false);
  network_delegate->set_redirect_url(GURL("http://origin.com/"));
  context_builder->set_network_delegate(std::move(network_delegate));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://google.com/"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  // Since this referrer _isn't_ more informative than the initiating origin,
  // we should see the histograms' false buckets populated.
  req->SetReferrer("http://origin.com");

  base::HistogramTester histograms;

  req->Start();
  d.RunUntilRedirect();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerHasInformativePath.CrossOrigin", false, 1);
  req->FollowDeferredRedirect(/*removed_headers=*/std::nullopt,
                              /*modified_headers=*/std::nullopt);
  d.RunUntilComplete();
  histograms.ExpectUniqueSample(
      "Net.URLRequest.ReferrerHasInformativePath.SameOrigin", false, 1);
}

// A URLRequestInterceptor that allows setting the LoadTimingInfo value of the
// URLRequestJobs it creates.
class URLRequestInterceptorWithLoadTimingInfo : public URLRequestInterceptor {
 public:
  // Static getters for canned response header and data strings.
  static std::string ok_data() { return URLRequestTestJob::test_data_1(); }

  static std::string ok_headers() { return URLRequestTestJob::test_headers(); }

  URLRequestInterceptorWithLoadTimingInfo() = default;
  ~URLRequestInterceptorWithLoadTimingInfo() override = default;

  // URLRequestInterceptor implementation:
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    std::unique_ptr<URLRequestTestJob> job =
        std::make_unique<URLRequestTestJob>(request, ok_headers(), ok_data(),
                                            true);
    job->set_load_timing_info(main_request_load_timing_info_);
    return job;
  }

  void set_main_request_load_timing_info(
      const LoadTimingInfo& main_request_load_timing_info) {
    main_request_load_timing_info_ = main_request_load_timing_info;
  }

 private:
  mutable LoadTimingInfo main_request_load_timing_info_;
};

// These tests inject a MockURLRequestInterceptor
class URLRequestLoadTimingTest : public URLRequestTest {
 public:
  URLRequestLoadTimingTest() {
    std::unique_ptr<URLRequestInterceptorWithLoadTimingInfo> interceptor =
        std::make_unique<URLRequestInterceptorWithLoadTimingInfo>();
    interceptor_ = interceptor.get();
    URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", "test_intercept", std::move(interceptor));
  }

  ~URLRequestLoadTimingTest() override {
    URLRequestFilter::GetInstance()->ClearHandlers();
  }

  URLRequestInterceptorWithLoadTimingInfo* interceptor() const {
    return interceptor_;
  }

 private:
  raw_ptr<URLRequestInterceptorWithLoadTimingInfo, DanglingUntriaged>
      interceptor_;
};

// "Normal" LoadTimingInfo as returned by a job.  Everything is in order, not
// reused.  |connect_time_flags| is used to indicate if there should be dns
// or SSL times, and |used_proxy| is used for proxy times.
LoadTimingInfo NormalLoadTimingInfo(base::TimeTicks now,
                                    int connect_time_flags,
                                    bool used_proxy) {
  LoadTimingInfo load_timing;
  load_timing.socket_log_id = 1;

  if (used_proxy) {
    load_timing.proxy_resolve_start = now + base::Days(1);
    load_timing.proxy_resolve_end = now + base::Days(2);
  }

  LoadTimingInfo::ConnectTiming& connect_timing = load_timing.connect_timing;
  if (connect_time_flags & CONNECT_TIMING_HAS_DNS_TIMES) {
    connect_timing.domain_lookup_start = now + base::Days(3);
    connect_timing.domain_lookup_end = now + base::Days(4);
  }
  connect_timing.connect_start = now + base::Days(5);
  if (connect_time_flags & CONNECT_TIMING_HAS_SSL_TIMES) {
    connect_timing.ssl_start = now + base::Days(6);
    connect_timing.ssl_end = now + base::Days(7);
  }
  connect_timing.connect_end = now + base::Days(8);

  load_timing.send_start = now + base::Days(9);
  load_timing.send_end = now + base::Days(10);
  load_timing.receive_headers_start = now + base::Days(11);
  load_timing.receive_headers_end = now + base::Days(12);
  return load_timing;
}

// Same as above, but in the case of a reused socket.
LoadTimingInfo NormalLoadTimingInfoReused(base::TimeTicks now,
                                          bool used_proxy) {
  LoadTimingInfo load_timing;
  load_timing.socket_log_id = 1;
  load_timing.socket_reused = true;

  if (used_proxy) {
    load_timing.proxy_resolve_start = now + base::Days(1);
    load_timing.proxy_resolve_end = now + base::Days(2);
  }

  load_timing.send_start = now + base::Days(9);
  load_timing.send_end = now + base::Days(10);
  load_timing.receive_headers_start = now + base::Days(11);
  load_timing.receive_headers_end = now + base::Days(12);
  return load_timing;
}

LoadTimingInfo RunURLRequestInterceptorLoadTimingTest(
    const LoadTimingInfo& job_load_timing,
    const URLRequestContext& context,
    URLRequestInterceptorWithLoadTimingInfo* interceptor) {
  interceptor->set_main_request_load_timing_info(job_load_timing);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context.CreateRequest(GURL("http://test_intercept/foo"), DEFAULT_PRIORITY,
                            &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  LoadTimingInfo resulting_load_timing;
  req->GetLoadTimingInfo(&resulting_load_timing);

  // None of these should be modified by the URLRequest.
  EXPECT_EQ(job_load_timing.socket_reused, resulting_load_timing.socket_reused);
  EXPECT_EQ(job_load_timing.socket_log_id, resulting_load_timing.socket_log_id);
  EXPECT_EQ(job_load_timing.send_start, resulting_load_timing.send_start);
  EXPECT_EQ(job_load_timing.send_end, resulting_load_timing.send_end);
  EXPECT_EQ(job_load_timing.receive_headers_start,
            resulting_load_timing.receive_headers_start);
  EXPECT_EQ(job_load_timing.receive_headers_end,
            resulting_load_timing.receive_headers_end);
  EXPECT_EQ(job_load_timing.push_start, resulting_load_timing.push_start);
  EXPECT_EQ(job_load_timing.push_end, resulting_load_timing.push_end);

  return resulting_load_timing;
}

// Basic test that the intercept + load timing tests work.
TEST_F(URLRequestLoadTimingTest, InterceptLoadTiming) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_DNS_TIMES, false);

  LoadTimingInfo load_timing_result = RunURLRequestInterceptorLoadTimingTest(
      job_load_timing, default_context(), interceptor());

  // Nothing should have been changed by the URLRequest.
  EXPECT_EQ(job_load_timing.proxy_resolve_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(job_load_timing.proxy_resolve_end,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(job_load_timing.connect_timing.domain_lookup_start,
            load_timing_result.connect_timing.domain_lookup_start);
  EXPECT_EQ(job_load_timing.connect_timing.domain_lookup_end,
            load_timing_result.connect_timing.domain_lookup_end);
  EXPECT_EQ(job_load_timing.connect_timing.connect_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(job_load_timing.connect_timing.connect_end,
            load_timing_result.connect_timing.connect_end);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_start,
            load_timing_result.connect_timing.ssl_start);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_end,
            load_timing_result.connect_timing.ssl_end);

  // Redundant sanity check.
  TestLoadTimingNotReused(load_timing_result, CONNECT_TIMING_HAS_DNS_TIMES);
}

// Another basic test, with proxy and SSL times, but no DNS times.
TEST_F(URLRequestLoadTimingTest, InterceptLoadTimingProxy) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_SSL_TIMES, true);

  LoadTimingInfo load_timing_result = RunURLRequestInterceptorLoadTimingTest(
      job_load_timing, default_context(), interceptor());

  // Nothing should have been changed by the URLRequest.
  EXPECT_EQ(job_load_timing.proxy_resolve_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(job_load_timing.proxy_resolve_end,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(job_load_timing.connect_timing.domain_lookup_start,
            load_timing_result.connect_timing.domain_lookup_start);
  EXPECT_EQ(job_load_timing.connect_timing.domain_lookup_end,
            load_timing_result.connect_timing.domain_lookup_end);
  EXPECT_EQ(job_load_timing.connect_timing.connect_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(job_load_timing.connect_timing.connect_end,
            load_timing_result.connect_timing.connect_end);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_start,
            load_timing_result.connect_timing.ssl_start);
  EXPECT_EQ(job_load_timing.connect_timing.ssl_end,
            load_timing_result.connect_timing.ssl_end);

  // Redundant sanity check.
  TestLoadTimingNotReusedWithProxy(load_timing_result,
                                   CONNECT_TIMING_HAS_SSL_TIMES);
}

// Make sure that URLRequest correctly adjusts proxy times when they're before
// |request_start|, due to already having a connected socket.  This happens in
// the case of reusing a SPDY session.  The connected socket is not considered
// reused in this test (May be a preconnect).
//
// To mix things up from the test above, assumes DNS times but no SSL times.
TEST_F(URLRequestLoadTimingTest, InterceptLoadTimingEarlyProxyResolution) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_DNS_TIMES, true);
  job_load_timing.proxy_resolve_start = now - base::Days(6);
  job_load_timing.proxy_resolve_end = now - base::Days(5);
  job_load_timing.connect_timing.domain_lookup_start = now - base::Days(4);
  job_load_timing.connect_timing.domain_lookup_end = now - base::Days(3);
  job_load_timing.connect_timing.connect_start = now - base::Days(2);
  job_load_timing.connect_timing.connect_end = now - base::Days(1);

  LoadTimingInfo load_timing_result = RunURLRequestInterceptorLoadTimingTest(
      job_load_timing, default_context(), interceptor());

  // Proxy times, connect times, and DNS times should all be replaced with
  // request_start.
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.domain_lookup_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.domain_lookup_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_end);

  // Other times should have been left null.
  TestLoadTimingNotReusedWithProxy(load_timing_result,
                                   CONNECT_TIMING_HAS_DNS_TIMES);
}

// Same as above, but in the reused case.
TEST_F(URLRequestLoadTimingTest,
       InterceptLoadTimingEarlyProxyResolutionReused) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing = NormalLoadTimingInfoReused(now, true);
  job_load_timing.proxy_resolve_start = now - base::Days(4);
  job_load_timing.proxy_resolve_end = now - base::Days(3);

  LoadTimingInfo load_timing_result = RunURLRequestInterceptorLoadTimingTest(
      job_load_timing, default_context(), interceptor());

  // Proxy times and connect times should all be replaced with request_start.
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_end);

  // Other times should have been left null.
  TestLoadTimingReusedWithProxy(load_timing_result);
}

// Make sure that URLRequest correctly adjusts connect times when they're before
// |request_start|, due to reusing a connected socket.  The connected socket is
// not considered reused in this test (May be a preconnect).
//
// To mix things up, the request has SSL times, but no DNS times.
TEST_F(URLRequestLoadTimingTest, InterceptLoadTimingEarlyConnect) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_SSL_TIMES, false);
  job_load_timing.connect_timing.connect_start = now - base::Days(1);
  job_load_timing.connect_timing.ssl_start = now - base::Days(2);
  job_load_timing.connect_timing.ssl_end = now - base::Days(3);
  job_load_timing.connect_timing.connect_end = now - base::Days(4);

  LoadTimingInfo load_timing_result = RunURLRequestInterceptorLoadTimingTest(
      job_load_timing, default_context(), interceptor());

  // Connect times, and SSL times should be replaced with request_start.
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.ssl_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.ssl_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_end);

  // Other times should have been left null.
  TestLoadTimingNotReused(load_timing_result, CONNECT_TIMING_HAS_SSL_TIMES);
}

// Make sure that URLRequest correctly adjusts connect times when they're before
// |request_start|, due to reusing a connected socket in the case that there
// are also proxy times.  The connected socket is not considered reused in this
// test (May be a preconnect).
//
// In this test, there are no SSL or DNS times.
TEST_F(URLRequestLoadTimingTest, InterceptLoadTimingEarlyConnectWithProxy) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY, true);
  job_load_timing.connect_timing.connect_start = now - base::Days(1);
  job_load_timing.connect_timing.connect_end = now - base::Days(2);

  LoadTimingInfo load_timing_result = RunURLRequestInterceptorLoadTimingTest(
      job_load_timing, default_context(), interceptor());

  // Connect times should be replaced with proxy_resolve_end.
  EXPECT_EQ(load_timing_result.proxy_resolve_end,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(load_timing_result.proxy_resolve_end,
            load_timing_result.connect_timing.connect_end);

  // Other times should have been left null.
  TestLoadTimingNotReusedWithProxy(load_timing_result,
                                   CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);
}

TEST_F(URLRequestTest, NetworkDelegateProxyError) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService("myproxy:70"));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddSimulatedTimeoutFailure("*");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");

  req->Start();
  d.RunUntilComplete();

  // Check we see a failed request.
  // The proxy chain should be set before failure.
  EXPECT_EQ(PacResultElementToProxyChain("PROXY myproxy:70"),
            req->proxy_chain());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, d.request_status());
  EXPECT_THAT(req->response_info().resolve_error_info.error,
              IsError(ERR_DNS_TIMED_OUT));

  EXPECT_EQ(1, network_delegate.error_count());
  EXPECT_THAT(network_delegate.last_error(),
              IsError(ERR_PROXY_CONNECTION_FAILED));
  EXPECT_EQ(1, network_delegate.completed_requests());
}

// Test that when host resolution fails with `ERR_DNS_NAME_HTTPS_ONLY` for
// "http://" requests, scheme is upgraded to "https://".
TEST_F(URLRequestTest, DnsNameHttpsOnlyErrorCausesSchemeUpgrade) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Build an http URL that should be auto-upgraded to https.
  const std::string kHost = "foo.a.test";  // Covered by CERT_TEST_NAMES.
  const GURL https_url = https_server.GetURL(kHost, "/defaultresponse");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  const GURL http_url = https_url.ReplaceComponents(replacements);

  // Return `ERR_DNS_NAME_HTTPS_ONLY` for "http://" requests and an address for
  // "https://" requests. This simulates the HostResolver behavior for a domain
  // with an HTTPS DNS record.
  auto host_resolver = std::make_unique<MockHostResolver>();
  MockHostResolverBase::RuleResolver::RuleKey unencrypted_resolve_key;
  unencrypted_resolve_key.scheme = url::kHttpScheme;
  unencrypted_resolve_key.hostname_pattern = kHost;
  host_resolver->rules()->AddRule(std::move(unencrypted_resolve_key),
                                  ERR_DNS_NAME_HTTPS_ONLY);
  MockHostResolverBase::RuleResolver::RuleKey encrypted_resolve_key;
  encrypted_resolve_key.scheme = url::kHttpsScheme;
  encrypted_resolve_key.hostname_pattern = kHost;
  host_resolver->rules()->AddRule(std::move(encrypted_resolve_key),
                                  https_server.GetIPLiteralString());
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::move(host_resolver));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_FALSE(req->url().SchemeIsCryptographic());

  // Note that there is no http server running, so the request should fail or
  // hang if its scheme is not upgraded to https.
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(d.received_redirect_count(), 1);

  EXPECT_EQ(0, network_delegate.error_count());
  EXPECT_EQ(200, req->GetResponseCode());
  ASSERT_TRUE(req->response_headers());
  EXPECT_EQ(200, req->response_headers()->response_code());

  // Observe that the scheme has been upgraded to https.
  EXPECT_TRUE(req->url().SchemeIsCryptographic());
  EXPECT_TRUE(req->url().SchemeIs(url::kHttpsScheme));
}

// Test that DNS-based scheme upgrade supports deferred redirect.
TEST_F(URLRequestTest, DnsNameHttpsOnlyErrorCausesSchemeUpgradeDeferred) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Build an http URL that should be auto-upgraded to https.
  const std::string kHost = "foo.a.test";  // Covered by CERT_TEST_NAMES.
  const GURL https_url = https_server.GetURL(kHost, "/defaultresponse");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  const GURL http_url = https_url.ReplaceComponents(replacements);

  // Return `ERR_DNS_NAME_HTTPS_ONLY` for "http://" requests and an address for
  // "https://" requests. This simulates the HostResolver behavior for a domain
  // with an HTTPS DNS record.
  auto host_resolver = std::make_unique<MockHostResolver>();
  MockHostResolverBase::RuleResolver::RuleKey unencrypted_resolve_key;
  unencrypted_resolve_key.scheme = url::kHttpScheme;
  unencrypted_resolve_key.hostname_pattern = kHost;
  host_resolver->rules()->AddRule(std::move(unencrypted_resolve_key),
                                  ERR_DNS_NAME_HTTPS_ONLY);
  MockHostResolverBase::RuleResolver::RuleKey encrypted_resolve_key;
  encrypted_resolve_key.scheme = url::kHttpsScheme;
  encrypted_resolve_key.hostname_pattern = kHost;
  host_resolver->rules()->AddRule(std::move(encrypted_resolve_key),
                                  https_server.GetIPLiteralString());
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::move(host_resolver));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_FALSE(req->url().SchemeIsCryptographic());

  // Note that there is no http server running, so the request should fail or
  // hang if its scheme is not upgraded to https.
  req->Start();
  d.RunUntilRedirect();

  EXPECT_EQ(d.received_redirect_count(), 1);

  req->FollowDeferredRedirect(/*removed_headers=*/std::nullopt,
                              /*modified_headers=*/std::nullopt);
  d.RunUntilComplete();

  EXPECT_EQ(0, network_delegate.error_count());
  EXPECT_EQ(200, req->GetResponseCode());
  ASSERT_TRUE(req->response_headers());
  EXPECT_EQ(200, req->response_headers()->response_code());

  // Observe that the scheme has been upgraded to https.
  EXPECT_TRUE(req->url().SchemeIsCryptographic());
  EXPECT_TRUE(req->url().SchemeIs(url::kHttpsScheme));
}

#if BUILDFLAG(ENABLE_WEBSOCKETS)
// Test that requests with "ws" scheme are upgraded to "wss" when DNS
// indicates that the name is HTTPS-only.
TEST_F(URLRequestTest, DnsHttpsRecordPresentCausesWsSchemeUpgrade) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Build an http URL that should be auto-upgraded to https.
  const std::string kHost = "foo.a.test";  // Covered by CERT_TEST_NAMES.
  const GURL https_url = https_server.GetURL(kHost, "/defaultresponse");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kWsScheme);
  const GURL ws_url = https_url.ReplaceComponents(replacements);

  auto host_resolver = std::make_unique<MockHostResolver>();
  MockHostResolverBase::RuleResolver::RuleKey unencrypted_resolve_key;
  unencrypted_resolve_key.scheme = url::kHttpScheme;
  unencrypted_resolve_key.hostname_pattern = kHost;
  host_resolver->rules()->AddRule(std::move(unencrypted_resolve_key),
                                  ERR_DNS_NAME_HTTPS_ONLY);
  MockHostResolverBase::RuleResolver::RuleKey encrypted_resolve_key;
  encrypted_resolve_key.scheme = url::kHttpsScheme;
  encrypted_resolve_key.hostname_pattern = kHost;
  host_resolver->rules()->AddRule(std::move(encrypted_resolve_key),
                                  https_server.GetIPLiteralString());
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::move(host_resolver));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      ws_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*is_for_websockets=*/true));
  EXPECT_FALSE(req->url().SchemeIsCryptographic());

  HttpRequestHeaders headers = WebSocketCommonTestHeaders();
  req->SetExtraRequestHeaders(headers);

  auto websocket_stream_create_helper =
      std::make_unique<TestWebSocketHandshakeStreamCreateHelper>();
  req->SetUserData(kWebSocketHandshakeUserDataKey,
                   std::move(websocket_stream_create_helper));

  // Note that there is no ws server running, so the request should fail or hang
  // if its scheme is not upgraded to wss.
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(d.received_redirect_count(), 1);

  // Expect failure because test server is not set up to provide websocket
  // responses.
  EXPECT_EQ(network_delegate.error_count(), 1);
  EXPECT_EQ(network_delegate.last_error(), ERR_INVALID_RESPONSE);

  // Observe that the scheme has been upgraded to wss.
  EXPECT_TRUE(req->url().SchemeIsCryptographic());
  EXPECT_TRUE(req->url().SchemeIs(url::kWssScheme));
}
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

TEST_F(URLRequestTest, DnsHttpsRecordAbsentNoSchemeUpgrade) {
  EmbeddedTestServer http_server(EmbeddedTestServer::TYPE_HTTP);
  RegisterDefaultHandlers(&http_server);
  ASSERT_TRUE(http_server.Start());

  // Build an http URL that should be auto-upgraded to https.
  const std::string kHost = "foo.a.test";  // Covered by CERT_TEST_NAMES.
  const GURL http_url = http_server.GetURL(kHost, "/defaultresponse");

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule(kHost, http_server.GetIPLiteralString());
  context_builder->set_host_resolver(std::move(host_resolver));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_FALSE(req->url().SchemeIsCryptographic());

  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(d.received_redirect_count(), 0);

  EXPECT_EQ(0, network_delegate.error_count());
  EXPECT_EQ(200, req->GetResponseCode());
  ASSERT_TRUE(req->response_headers());
  EXPECT_EQ(200, req->response_headers()->response_code());

  // Observe that the scheme has not been upgraded.
  EXPECT_EQ(http_url, req->url());
  EXPECT_FALSE(req->url().SchemeIsCryptographic());
  EXPECT_TRUE(req->url().SchemeIs(url::kHttpScheme));
}

TEST_F(URLRequestTest, SkipSecureDnsDisabledByDefault) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(SecureDnsPolicy::kAllow,
            static_cast<MockHostResolver*>(context->host_resolver())
                ->last_secure_dns_policy());
}

TEST_F(URLRequestTest, SkipSecureDnsEnabled) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("example.com", "127.0.0.1");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetSecureDnsPolicy(SecureDnsPolicy::kDisable);
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(SecureDnsPolicy::kDisable,
            static_cast<MockHostResolver*>(context->host_resolver())
                ->last_secure_dns_policy());
}

// Make sure that NetworkDelegate::NotifyCompleted is called if
// content is empty.
TEST_F(URLRequestTest, RequestCompletionForEmptyResponse) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      test_server.GetURL("/nocontent"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_THAT(d.request_status(), IsOk());
  EXPECT_EQ(204, req->GetResponseCode());
  EXPECT_EQ("", d.data_received());
  EXPECT_EQ(1, default_network_delegate().completed_requests());
}

// Make sure that SetPriority actually sets the URLRequest's priority
// correctly, both before and after start.
TEST_F(URLRequestTest, SetPriorityBasic) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(DEFAULT_PRIORITY, req->priority());

  req->SetPriority(LOW);
  EXPECT_EQ(LOW, req->priority());

  req->Start();
  EXPECT_EQ(LOW, req->priority());

  req->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, req->priority());
}

// Make sure that URLRequest calls SetPriority on a job before calling
// Start on it.
TEST_F(URLRequestTest, SetJobPriorityBeforeJobStart) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(DEFAULT_PRIORITY, req->priority());

  RequestPriority job_priority;
  std::unique_ptr<URLRequestJob> job =
      std::make_unique<PriorityMonitoringURLRequestJob>(req.get(),
                                                        &job_priority);
  TestScopedURLInterceptor interceptor(req->url(), std::move(job));
  EXPECT_EQ(DEFAULT_PRIORITY, job_priority);

  req->SetPriority(LOW);

  req->Start();
  EXPECT_EQ(LOW, job_priority);
}

// Make sure that URLRequest passes on its priority updates to its
// job.
TEST_F(URLRequestTest, SetJobPriority) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  RequestPriority job_priority;
  std::unique_ptr<URLRequestJob> job =
      std::make_unique<PriorityMonitoringURLRequestJob>(req.get(),
                                                        &job_priority);
  TestScopedURLInterceptor interceptor(req->url(), std::move(job));

  req->SetPriority(LOW);
  req->Start();
  EXPECT_EQ(LOW, job_priority);

  req->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, req->priority());
  EXPECT_EQ(MEDIUM, job_priority);
}

// Setting the IGNORE_LIMITS load flag should be okay if the priority
// is MAXIMUM_PRIORITY.
TEST_F(URLRequestTest, PriorityIgnoreLimits) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), MAXIMUM_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(MAXIMUM_PRIORITY, req->priority());

  RequestPriority job_priority;
  std::unique_ptr<URLRequestJob> job =
      std::make_unique<PriorityMonitoringURLRequestJob>(req.get(),
                                                        &job_priority);
  TestScopedURLInterceptor interceptor(req->url(), std::move(job));

  req->SetLoadFlags(LOAD_IGNORE_LIMITS);
  EXPECT_EQ(MAXIMUM_PRIORITY, req->priority());

  req->SetPriority(MAXIMUM_PRIORITY);
  EXPECT_EQ(MAXIMUM_PRIORITY, req->priority());

  req->Start();
  EXPECT_EQ(MAXIMUM_PRIORITY, req->priority());
  EXPECT_EQ(MAXIMUM_PRIORITY, job_priority);
}

// This test verifies that URLRequest::Delegate's OnConnected() callback is
// never called if the request fails before connecting to a remote endpoint.
TEST_F(URLRequestTest, NotifyDelegateConnectedSkippedOnEarlyFailure) {
  TestDelegate delegate;

  // The request will never connect to anything because the URL is invalid.
  auto request =
      default_context().CreateRequest(GURL("invalid url"), DEFAULT_PRIORITY,
                                      &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.transports(), IsEmpty());
}

// This test verifies that URLRequest::Delegate's OnConnected() method
// is called once for simple redirect-less requests.
TEST_F(URLRequestTest, OnConnected) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestDelegate delegate;

  auto request = default_context().CreateRequest(test_server.GetURL("/echo"),
                                                 DEFAULT_PRIORITY, &delegate,
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  delegate.RunUntilComplete();

  TransportInfo expected_transport;
  expected_transport.endpoint =
      IPEndPoint(IPAddress::IPv4Localhost(), test_server.port());
  expected_transport.negotiated_protocol = kProtoUnknown;
  EXPECT_THAT(delegate.transports(), ElementsAre(expected_transport));

  // Make sure URL_REQUEST_DELEGATE_CONNECTED is logged correctly.
  auto entries = net_log_observer_.GetEntries();
  size_t start_event_index = ExpectLogContainsSomewhere(
      entries, /*min_offset=*/0,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::BEGIN);
  size_t end_event_index = ExpectLogContainsSomewhereAfter(
      entries, /*start_offset=*/start_event_index,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::END);
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, end_event_index + 1,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED));
  ASSERT_LT(end_event_index, entries.size());
  EXPECT_FALSE(GetOptionalNetErrorCodeFromParams(entries[end_event_index]));
}

// This test verifies that URLRequest::Delegate's OnConnected() method is
// called after each redirect.
TEST_F(URLRequestTest, OnConnectedRedirect) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestDelegate delegate;

  // Fetch a page that redirects us once.
  GURL url = test_server.GetURL("/server-redirect?" +
                                test_server.GetURL("/echo").spec());
  auto request = default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  delegate.RunUntilRedirect();

  TransportInfo expected_transport;
  expected_transport.endpoint =
      IPEndPoint(IPAddress::IPv4Localhost(), test_server.port());
  expected_transport.negotiated_protocol = kProtoUnknown;
  EXPECT_THAT(delegate.transports(), ElementsAre(expected_transport));

  request->FollowDeferredRedirect(/*removed_headers=*/{},
                                  /*modified_headers=*/{});
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.transports(),
              ElementsAre(expected_transport, expected_transport));
}

// This test verifies that when the URLRequest Delegate returns an error from
// OnConnected(), the entire request fails with that error.
TEST_F(URLRequestTest, OnConnectedError) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestDelegate delegate;
  delegate.set_on_connected_result(ERR_NOT_IMPLEMENTED);

  auto request = default_context().CreateRequest(test_server.GetURL("/echo"),
                                                 DEFAULT_PRIORITY, &delegate,
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  delegate.RunUntilComplete();

  TransportInfo expected_transport;
  expected_transport.endpoint =
      IPEndPoint(IPAddress::IPv4Localhost(), test_server.port());
  expected_transport.negotiated_protocol = kProtoUnknown;
  EXPECT_THAT(delegate.transports(), ElementsAre(expected_transport));

  EXPECT_TRUE(delegate.request_failed());
  EXPECT_THAT(delegate.request_status(), IsError(ERR_NOT_IMPLEMENTED));

  // Make sure URL_REQUEST_DELEGATE_CONNECTED is logged correctly.
  auto entries = net_log_observer_.GetEntries();
  size_t start_event_index = ExpectLogContainsSomewhere(
      entries, /*min_offset=*/0,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::BEGIN);
  size_t end_event_index = ExpectLogContainsSomewhereAfter(
      entries, /*start_offset=*/start_event_index,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::END);
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, end_event_index + 1,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED));
  ASSERT_LT(end_event_index, entries.size());
  EXPECT_EQ(ERR_NOT_IMPLEMENTED,
            GetOptionalNetErrorCodeFromParams(entries[end_event_index]));
}

TEST_F(URLRequestTest, OnConnectedAsync) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  d.set_on_connected_run_callback(true);
  d.set_on_connected_result(OK);
  std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
      default_context(), test_server.GetURL("/defaultresponse"), &d);
  req->Start();
  d.RunUntilComplete();
  EXPECT_THAT(d.request_status(), IsOk());

  // Make sure URL_REQUEST_DELEGATE_CONNECTED is logged correctly.
  auto entries = net_log_observer_.GetEntries();
  size_t start_event_index = ExpectLogContainsSomewhere(
      entries, /*min_offset=*/0,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::BEGIN);
  size_t end_event_index = ExpectLogContainsSomewhereAfter(
      entries, /*start_offset=*/start_event_index,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::END);
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, end_event_index + 1,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED));
  ASSERT_LT(end_event_index, entries.size());
  EXPECT_FALSE(GetOptionalNetErrorCodeFromParams(entries[end_event_index]));
}

TEST_F(URLRequestTest, OnConnectedAsyncError) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  d.set_on_connected_run_callback(true);
  d.set_on_connected_result(ERR_NOT_IMPLEMENTED);
  std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
      default_context(), test_server.GetURL("/defaultresponse"), &d);
  req->Start();
  d.RunUntilComplete();
  EXPECT_THAT(d.request_status(), IsError(ERR_NOT_IMPLEMENTED));

  // Make sure URL_REQUEST_DELEGATE_CONNECTED is logged correctly.
  auto entries = net_log_observer_.GetEntries();
  size_t start_event_index = ExpectLogContainsSomewhere(
      entries, /*min_offset=*/0,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::BEGIN);
  size_t end_event_index = ExpectLogContainsSomewhereAfter(
      entries, /*start_offset=*/start_event_index,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED, NetLogEventPhase::END);
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, end_event_index + 1,
      NetLogEventType::URL_REQUEST_DELEGATE_CONNECTED));
  ASSERT_LT(end_event_index, entries.size());
  EXPECT_EQ(ERR_NOT_IMPLEMENTED,
            GetOptionalNetErrorCodeFromParams(entries[end_event_index]));
}

TEST_F(URLRequestTest, DelayedCookieCallback) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(std::make_unique<DelayedCookieMonster>());
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();

  // Set up a cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        *context, test_server.GetURL("/set-cookie?CookieToNotSend=1"), &d);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.blocked_annotate_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(1, network_delegate.set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        *context, test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1") !=
                std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_annotate_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DelayedCookieCallbackAsync) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Add a secure cookie so we can try to set an insecure cookie and have
  // SetCanonicalCookie fail.
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("https");
  GURL url = test_server.base_url().ReplaceComponents(replace_scheme);

  auto cookie1 = CanonicalCookie::CreateForTesting(
      url, "AlreadySetCookie=1;Secure", base::Time::Now());
  auto delayed_cm = std::make_unique<DelayedCookieMonster>();
  delayed_cm->SetCanonicalCookieAsync(std::move(cookie1), url,
                                      net::CookieOptions::MakeAllInclusive(),
                                      CookieStore::SetCookiesCallback());

  auto cookie2 = CanonicalCookie::CreateForTesting(
      url, "AlreadySetCookie=1;Secure", base::Time::Now());
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  cm->SetCanonicalCookieAsync(std::move(cookie2), url,
                              net::CookieOptions::MakeAllInclusive(),
                              CookieStore::SetCookiesCallback());

  auto async_context_builder = CreateTestURLRequestContextBuilder();
  async_context_builder->SetCookieStore(std::move(delayed_cm));
  auto& async_filter_network_delegate =
      *async_context_builder->set_network_delegate(
          std::make_unique<FilteringTestNetworkDelegate>());
  auto async_context = async_context_builder->Build();
  async_filter_network_delegate.SetCookieFilter("CookieBlockedOnCanGetCookie");
  TestDelegate async_delegate;

  auto sync_context_builder = CreateTestURLRequestContextBuilder();
  sync_context_builder->SetCookieStore(std::move(cm));
  auto& sync_filter_network_delegate =
      *sync_context_builder->set_network_delegate(
          std::make_unique<FilteringTestNetworkDelegate>());
  auto sync_context = sync_context_builder->Build();
  sync_filter_network_delegate.SetCookieFilter("CookieBlockedOnCanGetCookie");
  TestDelegate sync_delegate;

  std::vector<std::string> cookie_lines(
      {// Fails in SetCanonicalCookie for trying to set a secure cookie
       // on an insecure host.
       "CookieNotSet=1;Secure",
       // Fail in FilteringTestNetworkDelegate::CanGetCookie.
       "CookieBlockedOnCanGetCookie=1",
       // Fails in SetCanonicalCookie for trying to overwrite a secure cookie
       // with an insecure cookie.
       "AlreadySetCookie=1",
       // Succeeds and added cookie to store. Delayed (which makes the callback
       // run asynchronously) in DelayedCookieMonster.
       "CookieSet=1"});

  for (auto first_cookie_line : cookie_lines) {
    for (auto second_cookie_line : cookie_lines) {
      // Run with the delayed cookie monster.
      std::unique_ptr<URLRequest> request = CreateFirstPartyRequest(
          *async_context,
          test_server.GetURL("/set-cookie?" + first_cookie_line + "&" +
                             second_cookie_line),
          &async_delegate);

      request->Start();
      async_delegate.RunUntilComplete();
      EXPECT_THAT(async_delegate.request_status(), IsOk());

      // Run with the regular cookie monster.
      request = CreateFirstPartyRequest(
          *sync_context,
          test_server.GetURL("/set-cookie?" + first_cookie_line + "&" +
                             second_cookie_line),
          &sync_delegate);

      request->Start();
      sync_delegate.RunUntilComplete();
      EXPECT_THAT(sync_delegate.request_status(), IsOk());

      int expected_set_cookie_count = 0;
      int expected_blocked_cookie_count = 0;

      // 2 calls to the delegate's OnCanSetCookie method are expected, even if
      // the cookies don't end up getting set.
      expected_set_cookie_count += 2;

      if (first_cookie_line == "CookieBlockedOnCanGetCookie=1")
        ++expected_blocked_cookie_count;
      if (second_cookie_line == "CookieBlockedOnCanGetCookie=1")
        ++expected_blocked_cookie_count;

      EXPECT_EQ(expected_set_cookie_count,
                async_filter_network_delegate.set_cookie_called_count());
      EXPECT_EQ(expected_blocked_cookie_count,
                async_filter_network_delegate.blocked_set_cookie_count());

      EXPECT_EQ(expected_set_cookie_count,
                sync_filter_network_delegate.set_cookie_called_count());
      EXPECT_EQ(expected_blocked_cookie_count,
                sync_filter_network_delegate.blocked_set_cookie_count());

      async_filter_network_delegate.ResetSetCookieCalledCount();
      async_filter_network_delegate.ResetBlockedSetCookieCount();

      sync_filter_network_delegate.ResetSetCookieCalledCount();
      sync_filter_network_delegate.ResetBlockedSetCookieCount();
    }
  }
}

TEST_F(URLRequestTest, DoNotSendCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/set-cookie?CookieToNotSend=1"),
        &d);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1") !=
                std::string::npos);
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent when credentials are not allowed.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->set_allow_credentials(false);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1") ==
                std::string::npos);

    // When credentials are blocked, OnAnnotateAndMoveUserBlockedCookies() is
    // not invoked.
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        test_server.GetURL("/set-cookie?CookieToNotUpdate=2"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    EXPECT_EQ(1, default_network_delegate().set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        test_server.GetURL("/set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        &d);
    req->SetLoadFlags(LOAD_DO_NOT_SAVE_COOKIES);
    req->Start();

    d.RunUntilComplete();

    // LOAD_DO_NOT_SAVE_COOKIES does not trigger OnSetCookie.
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    EXPECT_EQ(1, default_network_delegate().set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1") ==
                std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2") !=
                std::string::npos);

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    EXPECT_EQ(1, default_network_delegate().set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/set-cookie?CookieToNotSend=1"),
        &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1") !=
                std::string::npos);

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    auto entries = net_log_observer_.GetEntries();
    for (const auto& entry : entries) {
      EXPECT_NE(entry.type,
                NetLogEventType::COOKIE_GET_BLOCKED_BY_NETWORK_DELEGATE);
    }
  }

  // Verify that the cookie isn't sent.
  {
    TestDelegate d;
    default_network_delegate().set_cookie_options(
        TestNetworkDelegate::NO_GET_COOKIES);
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1") ==
                std::string::npos);

    EXPECT_EQ(1, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    auto entries = net_log_observer_.GetEntries();
    ExpectLogContainsSomewhereAfter(
        entries, 0, NetLogEventType::COOKIE_GET_BLOCKED_BY_NETWORK_DELEGATE,
        NetLogEventPhase::NONE);
  }
}

// TODO(crbug.com/41225288) This test is flaky on iOS.
#if BUILDFLAG(IS_IOS)
#define MAYBE_DoNotSaveCookies_ViaPolicy FLAKY_DoNotSaveCookies_ViaPolicy
#else
#define MAYBE_DoNotSaveCookies_ViaPolicy DoNotSaveCookies_ViaPolicy
#endif
TEST_F(URLRequestTest, MAYBE_DoNotSaveCookies_ViaPolicy) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        test_server.GetURL("/set-cookie?CookieToNotUpdate=2"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    auto entries = net_log_observer_.GetEntries();
    for (const auto& entry : entries) {
      EXPECT_NE(entry.type,
                NetLogEventType::COOKIE_SET_BLOCKED_BY_NETWORK_DELEGATE);
    }
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestDelegate d;
    default_network_delegate().set_cookie_options(
        TestNetworkDelegate::NO_SET_COOKIE);
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        test_server.GetURL("/set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        &d);
    req->Start();

    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(2, default_network_delegate().blocked_set_cookie_count());
    auto entries = net_log_observer_.GetEntries();
    ExpectLogContainsSomewhereAfter(
        entries, 0, NetLogEventType::COOKIE_SET_BLOCKED_BY_NETWORK_DELEGATE,
        NetLogEventPhase::NONE);
  }

  // Verify the cookies weren't saved or updated.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1") ==
                std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2") !=
                std::string::npos);

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(2, default_network_delegate().blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveEmptyCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up an empty cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/set-cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    EXPECT_EQ(0, default_network_delegate().set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy_Async) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/set-cookie?CookieToNotSend=1"),
        &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1") !=
                std::string::npos);

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent.
  {
    TestDelegate d;
    default_network_delegate().set_cookie_options(
        TestNetworkDelegate::NO_GET_COOKIES);
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1") ==
                std::string::npos);

    EXPECT_EQ(1, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies_ViaPolicy_Async) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        test_server.GetURL("/set-cookie?CookieToNotUpdate=2"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestDelegate d;
    default_network_delegate().set_cookie_options(
        TestNetworkDelegate::NO_SET_COOKIE);
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        test_server.GetURL("/set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        &d);
    req->Start();

    d.RunUntilComplete();

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(2, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1") ==
                std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2") !=
                std::string::npos);

    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(2, default_network_delegate().blocked_set_cookie_count());
  }
}

// Tests for SameSite cookies. The test param indicates whether the same-site
// calculation considers redirect chains.
class URLRequestSameSiteCookiesTest
    : public URLRequestTest,
      public ::testing::WithParamInterface<bool> {
 public:
  URLRequestSameSiteCookiesTest() {
    if (DoesCookieSameSiteConsiderRedirectChain()) {
      feature_list_.InitAndEnableFeature(
          features::kCookieSameSiteConsidersRedirectChain);
    }
  }

  bool DoesCookieSameSiteConsiderRedirectChain() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(URLRequestSameSiteCookiesTest, SameSiteCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  const std::string kHost = "example.test";
  const std::string kSubHost = "subdomain.example.test";
  const std::string kCrossHost = "cross-origin.test";
  const url::Origin kOrigin =
      url::Origin::Create(test_server.GetURL(kHost, "/"));
  const url::Origin kSubOrigin =
      url::Origin::Create(test_server.GetURL(kSubHost, "/"));
  const url::Origin kCrossOrigin =
      url::Origin::Create(test_server.GetURL(kCrossHost, "/"));
  const SiteForCookies kSiteForCookies = SiteForCookies::FromOrigin(kOrigin);
  const SiteForCookies kCrossSiteForCookies =
      SiteForCookies::FromOrigin(kCrossOrigin);

  // Set up two 'SameSite' cookies on 'example.test'
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        test_server.GetURL(kHost,
                           "/set-cookie?StrictSameSiteCookie=1;SameSite=Strict&"
                           "LaxSameSiteCookie=1;SameSite=Lax"),
        &d);
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
    EXPECT_EQ(2, default_network_delegate().set_cookie_count());
  }

  // Verify that both cookies are sent for same-site requests, whether they are
  // subresource requests, subframe navigations, or main frame navigations.
  for (IsolationInfo::RequestType request_type :
       {IsolationInfo::RequestType::kMainFrame,
        IsolationInfo::RequestType::kSubFrame,
        IsolationInfo::RequestType::kOther}) {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(request_type, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that both cookies are sent when the request has no initiator (can
  // happen for main frame browser-initiated navigations).
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(kSiteForCookies);
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that both cookies are sent for same-registrable-domain requests.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(
        SiteForCookies::FromUrl(test_server.GetURL(kSubHost, "/")));
    req->set_initiator(kSubOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that neither cookie is not sent for cross-site requests.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(kCrossSiteForCookies);
    req->set_initiator(kCrossOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_EQ(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the lax cookie is sent for cross-site initiators when the
  // method is "safe" and the request is a main frame navigation.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kCrossOrigin);
    req->set_method("GET");
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the lax cookie is sent for cross-site initiators when the
  // method is "safe" and the request is being forced to be considered as a
  // main frame navigation.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kCrossOrigin);
    req->set_method("GET");
    req->set_force_main_frame_for_same_site_cookies(true);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that neither cookie is sent for cross-site initiators when the
  // method is unsafe (e.g. POST), even if the request is a main frame
  // navigation.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kCrossOrigin);
    req->set_method("POST");
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_EQ(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that neither cookie is sent for cross-site initiators when the
  // method is safe and the site-for-cookies is same-site, but the request is
  // not a main frame navigation.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kCrossOrigin);
    req->set_method("GET");
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_EQ(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());

    // Check that the appropriate cookie inclusion status is set.
    ASSERT_EQ(2u, req->maybe_sent_cookies().size());
    CookieInclusionStatus expected_strict_status =
        CookieInclusionStatus::MakeFromReasonsForTesting(
            {CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT});
    CookieInclusionStatus expected_lax_status =
        CookieInclusionStatus::MakeFromReasonsForTesting(
            {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX});
    EXPECT_EQ(expected_strict_status,
              req->maybe_sent_cookies()[0].access_result.status);
    EXPECT_EQ(expected_lax_status,
              req->maybe_sent_cookies()[1].access_result.status);
  }
}

TEST_P(URLRequestSameSiteCookiesTest, SameSiteCookies_Redirect) {
  EmbeddedTestServer http_server;
  RegisterDefaultHandlers(&http_server);
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  const std::string kHost = "foo.a.test";
  const std::string kSameSiteHost = "bar.a.test";
  const std::string kCrossSiteHost = "b.test";
  const url::Origin kOrigin =
      url::Origin::Create(https_server.GetURL(kHost, "/"));
  const url::Origin kHttpOrigin =
      url::Origin::Create(http_server.GetURL(kHost, "/"));
  const url::Origin kSameSiteOrigin =
      url::Origin::Create(https_server.GetURL(kSameSiteHost, "/"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(https_server.GetURL(kCrossSiteHost, "/"));
  const SiteForCookies kSiteForCookies = SiteForCookies::FromOrigin(kOrigin);
  const SiteForCookies kHttpSiteForCookies =
      SiteForCookies::FromOrigin(kHttpOrigin);
  const SiteForCookies kCrossSiteForCookies =
      SiteForCookies::FromOrigin(kCrossSiteOrigin);

  // Set up two 'SameSite' cookies on foo.a.test
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        https_server.GetURL(
            kHost,
            "/set-cookie?StrictSameSiteCookie=1;SameSite=Strict&"
            "LaxSameSiteCookie=1;SameSite=Lax"),
        &d);
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(2u, GetAllCookies(&default_context()).size());
  }

  // Verify that both cookies are sent for same-site, unredirected requests.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        https_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1u, req->url_chain().size());
    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }

  // Verify that both cookies are sent for a same-origin redirected top level
  // navigation.
  {
    TestDelegate d;
    GURL url = https_server.GetURL(
        kHost, "/server-redirect?" +
                   https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2u, req->url_chain().size());
    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }

  // Verify that both cookies are sent for a same-site redirected top level
  // navigation.
  {
    TestDelegate d;
    GURL url = https_server.GetURL(
        kSameSiteHost,
        "/server-redirect?" +
            https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kMainFrame, kSameSiteOrigin,
        kSameSiteOrigin, kSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2u, req->url_chain().size());
    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }

  // If redirect chains are considered:
  // Verify that the Strict cookie may or may not be sent for a cross-scheme
  // (same-registrable-domain) redirected top level navigation, depending on the
  // status of Schemeful Same-Site. The Lax cookie is sent regardless, because
  // this is a top-level navigation.
  //
  // If redirect chains are not considered:
  // Verify that both cookies are sent, because this is a top-level navigation.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSchemefulSameSite);
    TestDelegate d;
    GURL url = http_server.GetURL(
        kHost, "/server-redirect?" +
                   https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame,
                              kHttpOrigin, kHttpOrigin, kHttpSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kHttpSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2u, req->url_chain().size());
    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSchemefulSameSite);
    TestDelegate d;
    GURL url = http_server.GetURL(
        kHost, "/server-redirect?" +
                   https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame,
                              kHttpOrigin, kHttpOrigin, kHttpSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kHttpSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2u, req->url_chain().size());
    EXPECT_EQ(
        DoesCookieSameSiteConsiderRedirectChain(),
        std::string::npos == d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }

  // Verify that (depending on whether redirect chains are considered), the
  // Strict cookie is (not) sent for a cross-site redirected top level
  // navigation...
  {
    TestDelegate d;
    GURL url = https_server.GetURL(
        kCrossSiteHost,
        "/server-redirect?" +
            https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kMainFrame, kCrossSiteOrigin,
        kCrossSiteOrigin, kCrossSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kCrossSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2u, req->url_chain().size());
    EXPECT_EQ(
        DoesCookieSameSiteConsiderRedirectChain(),
        std::string::npos == d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }
  // ... even if the initial URL is same-site.
  {
    TestDelegate d;
    GURL middle_url = https_server.GetURL(
        kCrossSiteHost,
        "/server-redirect?" +
            https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    GURL url =
        https_server.GetURL(kHost, "/server-redirect?" + middle_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(3u, req->url_chain().size());
    EXPECT_EQ(
        DoesCookieSameSiteConsiderRedirectChain(),
        std::string::npos == d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }

  // Verify that (depending on whether redirect chains are considered), neither
  // (or both) SameSite cookie is sent for a cross-site redirected subresource
  // request...
  {
    TestDelegate d;
    GURL url = https_server.GetURL(
        kCrossSiteHost,
        "/server-redirect?" +
            https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2u, req->url_chain().size());
    EXPECT_EQ(
        DoesCookieSameSiteConsiderRedirectChain(),
        std::string::npos == d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_EQ(
        DoesCookieSameSiteConsiderRedirectChain(),
        std::string::npos == d.data_received().find("LaxSameSiteCookie=1"));
  }
  // ... even if the initial URL is same-site.
  {
    TestDelegate d;
    GURL middle_url = https_server.GetURL(
        kCrossSiteHost,
        "/server-redirect?" +
            https_server.GetURL(kHost, "/echoheader?Cookie").spec());
    GURL url =
        https_server.GetURL(kHost, "/server-redirect?" + middle_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(3u, req->url_chain().size());
    EXPECT_EQ(
        DoesCookieSameSiteConsiderRedirectChain(),
        std::string::npos == d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_EQ(
        DoesCookieSameSiteConsiderRedirectChain(),
        std::string::npos == d.data_received().find("LaxSameSiteCookie=1"));
  }
}

TEST_P(URLRequestSameSiteCookiesTest, SettingSameSiteCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  const std::string kHost = "example.test";
  const std::string kSubHost = "subdomain.example.test";
  const std::string kCrossHost = "cross-origin.test";
  const url::Origin kOrigin =
      url::Origin::Create(test_server.GetURL(kHost, "/"));
  const url::Origin kSubOrigin =
      url::Origin::Create(test_server.GetURL(kSubHost, "/"));
  const url::Origin kCrossOrigin =
      url::Origin::Create(test_server.GetURL(kCrossHost, "/"));
  const SiteForCookies kSiteForCookies = SiteForCookies::FromOrigin(kOrigin);
  const SiteForCookies kCrossSiteForCookies =
      SiteForCookies::FromOrigin(kCrossOrigin);

  int expected_cookies = 0;

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?Strict1=1;SameSite=Strict&"
                           "Lax1=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);

    // 'SameSite' cookies are settable from strict same-site contexts
    // (same-origin site_for_cookies, same-origin initiator), so this request
    // should result in two cookies being set.
    expected_cookies += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_cookies, default_network_delegate().set_cookie_count());
  }

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?Strict2=1;SameSite=Strict&"
                           "Lax2=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kCrossOrigin);

    // 'SameSite' cookies are settable from lax same-site contexts (same-origin
    // site_for_cookies, cross-site initiator, main frame navigation), so this
    // request should result in two cookies being set.
    expected_cookies += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_cookies, default_network_delegate().set_cookie_count());
  }

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?Strict3=1;SameSite=Strict&"
                           "Lax3=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame,
                              kSubOrigin, kSubOrigin, kSiteForCookies));
    req->set_site_for_cookies(
        SiteForCookies::FromUrl(test_server.GetURL(kSubHost, "/")));
    req->set_initiator(kCrossOrigin);

    // 'SameSite' cookies are settable from lax same-site contexts (same-site
    // site_for_cookies, cross-site initiator, main frame navigation), so this
    // request should result in two cookies being set.
    expected_cookies += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_cookies, default_network_delegate().set_cookie_count());
  }

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?Strict4=1;SameSite=Strict&"
                           "Lax4=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(
        SiteForCookies::FromUrl(test_server.GetURL(kSubHost, "/")));

    // 'SameSite' cookies are settable from strict same-site contexts (same-site
    // site_for_cookies, no initiator), so this request should result in two
    // cookies being set.
    expected_cookies += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_cookies, default_network_delegate().set_cookie_count());
  }

  int expected_network_delegate_set_cookie_count;
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?Strict5=1;SameSite=Strict&"
                           "Lax5=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(kCrossSiteForCookies);
    req->set_initiator(kCrossOrigin);

    // 'SameSite' cookies are not settable from cross-site contexts, so this
    // should not result in any new cookies being set.
    expected_cookies += 0;
    // This counts the number of successful calls to CanSetCookie() when
    // attempting to set a cookie. The two cookies above were created and
    // attempted to be set, and were not rejected by the NetworkDelegate, so the
    // count here is 2 more than the number of cookies actually set.
    expected_network_delegate_set_cookie_count = expected_cookies + 2;

    req->Start();
    d.RunUntilComplete();
    // This counts the number of cookies actually set.
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_network_delegate_set_cookie_count,
              default_network_delegate().set_cookie_count());
  }

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?Strict6=1;SameSite=Strict&"
                           "Lax6=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kCrossOrigin);

    // Same-site site-for-cookies, cross-site initiator, non main frame
    // navigation -> context is considered cross-site so no SameSite cookies are
    // set.
    expected_cookies += 0;
    // This counts the number of successful calls to CanSetCookie() when
    // attempting to set a cookie. The two cookies above were created and
    // attempted to be set, and were not rejected by the NetworkDelegate, so the
    // count here is 2 more than the number of cookies actually set.
    expected_network_delegate_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_network_delegate_set_cookie_count,
              default_network_delegate().set_cookie_count());

    // Check that the appropriate cookie inclusion status is set.
    ASSERT_EQ(2u, req->maybe_stored_cookies().size());
    CookieInclusionStatus expected_strict_status =
        CookieInclusionStatus::MakeFromReasonsForTesting(
            {CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT},
            {} /* warning_reasons */);
    CookieInclusionStatus expected_lax_status =
        CookieInclusionStatus::MakeFromReasonsForTesting(
            {CookieInclusionStatus::EXCLUDE_SAMESITE_LAX},
            {} /* warning_reasons */);
    EXPECT_EQ(expected_strict_status,
              req->maybe_stored_cookies()[0].access_result.status);
    EXPECT_EQ(expected_lax_status,
              req->maybe_stored_cookies()[1].access_result.status);
  }

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?Strict7=1;SameSite=Strict&"
                           "Lax7=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kCrossOrigin);
    req->set_force_main_frame_for_same_site_cookies(true);

    // 'SameSite' cookies are settable from lax same-site contexts (same-origin
    // site_for_cookies, cross-site initiator, main frame navigation), so this
    // request should result in two cookies being set.
    expected_cookies += 2;
    expected_network_delegate_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_network_delegate_set_cookie_count,
              default_network_delegate().set_cookie_count());
  }
}

// Tests special chrome:// scheme that is supposed to always attach SameSite
// cookies if the requested site is secure.
TEST_P(URLRequestSameSiteCookiesTest, SameSiteCookiesSpecialScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SchemeType::SCHEME_WITH_HOST);

  EmbeddedTestServer https_test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_test_server);
  ASSERT_TRUE(https_test_server.Start());
  EmbeddedTestServer http_test_server(EmbeddedTestServer::TYPE_HTTP);
  RegisterDefaultHandlers(&http_test_server);
  ASSERT_TRUE(http_test_server.Start());
  ASSERT_NE(https_test_server.port(), http_test_server.port());
  // Both hostnames should be 127.0.0.1 (so that we can use the same set of
  // cookies on both, for convenience).
  ASSERT_EQ(https_test_server.host_port_pair().host(),
            http_test_server.host_port_pair().host());

  // Set up special schemes
  auto cad = std::make_unique<TestCookieAccessDelegate>();
  cad->SetIgnoreSameSiteRestrictionsScheme("chrome", true);
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  cm->SetCookieAccessDelegate(std::move(cad));

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(std::move(cm));
  auto context = context_builder->Build();

  // SameSite cookies are not set for 'chrome' scheme if requested origin is not
  // secure.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        http_test_server.GetURL(
            "/set-cookie?StrictSameSiteCookie=1;SameSite=Strict&"
            "LaxSameSiteCookie=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(
        SiteForCookies::FromUrl(GURL("chrome://whatever/")));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0u, GetAllCookies(context.get()).size());
  }

  // But they are set for 'chrome' scheme if the requested origin is secure.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test_server.GetURL(
            "/set-cookie?StrictSameSiteCookie=1;SameSite=Strict&"
            "LaxSameSiteCookie=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(
        SiteForCookies::FromUrl(GURL("chrome://whatever/")));
    req->Start();
    d.RunUntilComplete();
    CookieList cookies = GetAllCookies(context.get());
    EXPECT_EQ(2u, cookies.size());
  }

  // Verify that they are both sent when the site_for_cookies scheme is
  // 'chrome' and the requested origin is secure.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(
        SiteForCookies::FromUrl(GURL("chrome://whatever/")));
    req->Start();
    d.RunUntilComplete();
    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
  }

  // Verify that they are not sent when the site_for_cookies scheme is
  // 'chrome' and the requested origin is not secure.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        http_test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(
        SiteForCookies::FromUrl(GURL("chrome://whatever/")));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie"));
    EXPECT_EQ(std::string::npos, d.data_received().find("LaxSameSiteCookie"));
  }
}

TEST_P(URLRequestSameSiteCookiesTest, SettingSameSiteCookies_Redirect) {
  EmbeddedTestServer http_server;
  RegisterDefaultHandlers(&http_server);
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  auto& network_delegate = default_network_delegate();

  const std::string kHost = "foo.a.test";
  const std::string kSameSiteHost = "bar.a.test";
  const std::string kCrossSiteHost = "b.test";
  const url::Origin kOrigin =
      url::Origin::Create(https_server.GetURL(kHost, "/"));
  const url::Origin kHttpOrigin =
      url::Origin::Create(http_server.GetURL(kHost, "/"));
  const url::Origin kSameSiteOrigin =
      url::Origin::Create(https_server.GetURL(kSameSiteHost, "/"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(https_server.GetURL(kCrossSiteHost, "/"));
  const SiteForCookies kSiteForCookies = SiteForCookies::FromOrigin(kOrigin);
  const SiteForCookies kHttpSiteForCookies =
      SiteForCookies::FromOrigin(kHttpOrigin);
  const SiteForCookies kCrossSiteForCookies =
      SiteForCookies::FromOrigin(kCrossSiteOrigin);

  int expected_cookies = 0;
  int expected_set_cookie_count = 0;

  // Verify that SameSite cookies can be set for a same-origin redirected
  // top-level navigation request.
  {
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict1=1;SameSite=Strict&Lax1=1;SameSite=Lax");
    GURL url =
        https_server.GetURL(kHost, "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                              kOrigin, kSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);

    expected_cookies += 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }

  // Verify that SameSite cookies can be set for a same-site redirected
  // top-level navigation request.
  {
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict2=1;SameSite=Strict&Lax2=1;SameSite=Lax");
    GURL url = https_server.GetURL(kSameSiteHost,
                                   "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kMainFrame, kSameSiteOrigin,
        kSameSiteOrigin, kSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kSameSiteOrigin);

    expected_cookies += 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }

  // Verify that SameSite cookies can be set for a cross-site redirected
  // top-level navigation request.
  {
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict3=1;SameSite=Strict&Lax3=1;SameSite=Lax");
    GURL url = https_server.GetURL(kCrossSiteHost,
                                   "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kMainFrame, kCrossSiteOrigin,
        kCrossSiteOrigin, kCrossSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kCrossSiteForCookies);
    req->set_initiator(kCrossSiteOrigin);

    expected_cookies += 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }

  // Verify that SameSite cookies can be set for a same-origin redirected
  // subresource request.
  {
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict4=1;SameSite=Strict&Lax4=1;SameSite=Lax");
    GURL url =
        https_server.GetURL(kHost, "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);

    expected_cookies += 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }

  // Verify that SameSite cookies can be set for a same-site redirected
  // subresource request.
  {
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict5=1;SameSite=Strict&Lax5=1;SameSite=Lax");
    GURL url = https_server.GetURL(kSameSiteHost,
                                   "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kSameSiteOrigin, kSameSiteOrigin,
        kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kSameSiteOrigin);

    expected_cookies += 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }

  // Verify that (depending on whether redirect chains are considered) SameSite
  // cookies can/cannot be set for a cross-site redirected subresource request,
  // even if the site-for-cookies and initiator are same-site, ...
  {
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict6=1;SameSite=Strict&Lax6=1;SameSite=Lax");
    GURL url = https_server.GetURL(kCrossSiteHost,
                                   "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);

    expected_cookies += DoesCookieSameSiteConsiderRedirectChain() ? 0 : 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }
  // ... even if the initial URL is same-site.
  {
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict7=1;SameSite=Strict&Lax7=1;SameSite=Lax");
    GURL middle_url = https_server.GetURL(
        kCrossSiteHost, "/server-redirect?" + set_cookie_url.spec());
    GURL url =
        https_server.GetURL(kHost, "/server-redirect?" + middle_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(IsolationInfo::Create(
        IsolationInfo::RequestType::kOther, kOrigin, kOrigin, kSiteForCookies));
    req->set_site_for_cookies(kSiteForCookies);
    req->set_initiator(kOrigin);

    expected_cookies += DoesCookieSameSiteConsiderRedirectChain() ? 0 : 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }

  // Verify that SameSite cookies may or may not be set for a cross-scheme
  // (same-registrable-domain) redirected subresource request, depending on the
  // status of Schemeful Same-Site and whether redirect chains are considered.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kSchemefulSameSite);
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict8=1;SameSite=Strict&Lax8=1;SameSite=Lax");
    GURL url =
        http_server.GetURL(kHost, "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kOther, kHttpOrigin,
                              kHttpOrigin, kHttpSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kHttpSiteForCookies);
    req->set_initiator(kOrigin);

    expected_cookies += 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kSchemefulSameSite);
    TestDelegate d;
    GURL set_cookie_url = https_server.GetURL(
        kHost, "/set-cookie?Strict9=1;SameSite=Strict&Lax9=1;SameSite=Lax");
    GURL url =
        http_server.GetURL(kHost, "/server-redirect?" + set_cookie_url.spec());
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(
        IsolationInfo::Create(IsolationInfo::RequestType::kOther, kHttpOrigin,
                              kHttpOrigin, kHttpSiteForCookies));
    req->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
    req->set_site_for_cookies(kHttpSiteForCookies);
    req->set_initiator(kOrigin);

    expected_cookies += DoesCookieSameSiteConsiderRedirectChain() ? 0 : 2;
    expected_set_cookie_count += 2;

    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(expected_cookies,
              static_cast<int>(GetAllCookies(&default_context()).size()));
    EXPECT_EQ(expected_set_cookie_count, network_delegate.set_cookie_count());
  }
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         URLRequestSameSiteCookiesTest,
                         ::testing::Bool());

TEST_F(URLRequestTest, PartitionedCookiesRedirect) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());

  const std::string kHost = "a.test";
  const std::string kCrossSiteHost = "b.test";

  const GURL create_cookie_url = https_server.GetURL(kHost, "/");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(
      std::make_unique<CookieMonster>(nullptr, nullptr));
  auto context = context_builder->Build();
  auto& cm = *static_cast<CookieMonster*>(context->cookie_store());

  // Set partitioned cookie with same-site partitionkey.
  {
    auto same_site_partitioned_cookie = CanonicalCookie::CreateForTesting(
        create_cookie_url, "samesite_partitioned=1;Secure;Partitioned",
        base::Time::Now(), std::nullopt,
        CookiePartitionKey::FromURLForTesting(
            create_cookie_url,
            CookiePartitionKey::AncestorChainBit::kSameSite));
    ASSERT_TRUE(same_site_partitioned_cookie);
    ASSERT_TRUE(same_site_partitioned_cookie->IsPartitioned());
    base::test::TestFuture<CookieAccessResult> future;
    cm.SetCanonicalCookieAsync(
        std::move(same_site_partitioned_cookie), create_cookie_url,
        CookieOptions::MakeAllInclusive(), future.GetCallback());
    ASSERT_TRUE(future.Get().status.IsInclude());
  }

  // Set a partitioned cookie with a cross-site partition key.
  // In the redirect below from site B to A, this cookie's partition key is site
  // B it should not be sent in the redirected request.
  {
    auto cross_site_partitioned_cookie = CanonicalCookie::CreateForTesting(
        create_cookie_url, "xsite_partitioned=1;Secure;Partitioned",
        base::Time::Now(), std::nullopt,
        CookiePartitionKey::FromURLForTesting(
            https_server.GetURL(kCrossSiteHost, "/")));
    ASSERT_TRUE(cross_site_partitioned_cookie);
    ASSERT_TRUE(cross_site_partitioned_cookie->IsPartitioned());
    base::test::TestFuture<CookieAccessResult> future;
    cm.SetCanonicalCookieAsync(
        std::move(cross_site_partitioned_cookie), create_cookie_url,
        CookieOptions::MakeAllInclusive(), future.GetCallback());
    ASSERT_TRUE(future.Get().status.IsInclude());
  }

  const auto kCrossSiteOrigin =
      url::Origin::Create(https_server.GetURL(kCrossSiteHost, "/"));
  const auto kCrossSiteSiteForCookies =
      SiteForCookies::FromOrigin(kCrossSiteOrigin);

  // Test that when a request is redirected that the partitioned cookies
  // attached to the redirected request match the partition key of the new
  // request.
  TestDelegate d;
  GURL url = https_server.GetURL(
      kCrossSiteHost,
      "/server-redirect?" +
          https_server.GetURL(kHost, "/echoheader?Cookie").spec());
  std::unique_ptr<URLRequest> req = context->CreateRequest(
      url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS);
  req->set_isolation_info(IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kCrossSiteOrigin,
      kCrossSiteOrigin, kCrossSiteSiteForCookies));
  req->set_first_party_url_policy(
      RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
  req->set_site_for_cookies(kCrossSiteSiteForCookies);
  req->set_initiator(kCrossSiteOrigin);
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(2u, req->url_chain().size());
  EXPECT_NE(std::string::npos,
            d.data_received().find("samesite_partitioned=1"));
  EXPECT_EQ(std::string::npos, d.data_received().find("xsite_partitioned=1"));
}

// Tests that __Secure- cookies can't be set on non-secure origins.
TEST_F(URLRequestTest, SecureCookiePrefixOnNonsecureOrigin) {
  EmbeddedTestServer http_server;
  RegisterDefaultHandlers(&http_server);
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  // Try to set a Secure __Secure- cookie on http://a.test (non-secure origin).
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        http_server.GetURL("a.test",
                           "/set-cookie?__Secure-nonsecure-origin=1;Secure&"
                           "cookienotsecure=1"),
        &d);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the __Secure- cookie was not set by checking cookies for
  // https://a.test (secure origin).
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), https_server.GetURL("a.test", "/echoheader?Cookie"),
        &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(d.data_received().find("__Secure-nonsecure-origin=1"),
              std::string::npos);
    EXPECT_NE(d.data_received().find("cookienotsecure=1"), std::string::npos);
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, SecureCookiePrefixNonsecure) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Try to set a non-Secure __Secure- cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), https_server.GetURL("/set-cookie?__Secure-foo=1"),
        &d);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, default_network_delegate().set_cookie_count());
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the cookie is not set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), https_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(d.data_received().find("__Secure-foo=1"), std::string::npos);
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, SecureCookiePrefixSecure) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Try to set a Secure __Secure- cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        https_server.GetURL("/set-cookie?__Secure-bar=1;Secure"), &d);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), https_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(d.data_received().find("__Secure-bar=1"), std::string::npos);
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }
}

// Tests that secure cookies can't be set on non-secure origins if strict secure
// cookies are enabled.
TEST_F(URLRequestTest, StrictSecureCookiesOnNonsecureOrigin) {
  EmbeddedTestServer http_server;
  RegisterDefaultHandlers(&http_server);
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  // Try to set a Secure cookie and a non-Secure cookie from a nonsecure origin.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(),
        http_server.GetURL("a.test",
                           "/set-cookie?nonsecure-origin=1;Secure&"
                           "cookienotsecure=1"),
        &d);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }

  // Verify that the Secure cookie was not set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        default_context(), https_server.GetURL("a.test", "/echoheader?Cookie"),
        &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(d.data_received().find("nonsecure-origin=1"), std::string::npos);
    EXPECT_NE(d.data_received().find("cookienotsecure=1"), std::string::npos);
    EXPECT_EQ(0, default_network_delegate().blocked_annotate_cookies_count());
    EXPECT_EQ(0, default_network_delegate().blocked_set_cookie_count());
  }
}

// FixedDateNetworkDelegate swaps out the server's HTTP Date response header
// value for the `fixed_date_` member.
class FixedDateNetworkDelegate : public TestNetworkDelegate {
 public:
  explicit FixedDateNetworkDelegate(std::string_view fixed_date)
      : fixed_date_(fixed_date) {}

  FixedDateNetworkDelegate(const FixedDateNetworkDelegate&) = delete;
  FixedDateNetworkDelegate& operator=(const FixedDateNetworkDelegate&) = delete;

  ~FixedDateNetworkDelegate() override = default;

  void set_fixed_date(std::string_view fixed_date) {
    fixed_date_ = static_cast<std::string>(fixed_date);
  }

  // NetworkDelegate implementation
  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url) override;

 private:
  std::string fixed_date_;
};

int FixedDateNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    const IPEndPoint& endpoint,
    std::optional<GURL>* preserve_fragment_on_redirect_url) {
  *override_response_headers = base::MakeRefCounted<HttpResponseHeaders>(
      original_response_headers->raw_headers());

  (*override_response_headers)->SetHeader("Date", fixed_date_);

  return TestNetworkDelegate::OnHeadersReceived(
      request, std::move(callback), original_response_headers,
      override_response_headers, endpoint, preserve_fragment_on_redirect_url);
}

// Test that cookie expiration times are adjusted for server/client clock
// skew and that we handle incorrect timezone specifier "UTC" in HTTP Date
// headers by defaulting to GMT. (crbug.com/135131)
TEST_F(URLRequestTest, AcceptClockSkewCookieWithWrongDateTimezone) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<FixedDateNetworkDelegate>("04-Jan-2004 04:09:25 UTC"));
  auto context = context_builder->Build();

  // Set up an expired cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        *context,
        test_server.GetURL(
            "/set-cookie?StillGood=1;expires=Mon,18-Apr-1977,22:50:13,GMT"),
        &d);
    req->Start();
    d.RunUntilComplete();
  }
  // Verify that the cookie is not set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        *context, test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("StillGood=1") == std::string::npos);
  }
  // Set up a cookie with clock skew and "UTC" HTTP Date timezone specifier.
  {
    TestDelegate d;
    network_delegate.set_fixed_date("18-Apr-1977 22:49:13 UTC");
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        *context,
        test_server.GetURL(
            "/set-cookie?StillGood=1;expires=Mon,18-Apr-1977,22:50:13,GMT"),
        &d);
    req->Start();
    d.RunUntilComplete();
  }
  // Verify that the cookie is set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
        *context, test_server.GetURL("/echoheader?Cookie"), &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("StillGood=1") != std::string::npos);
  }
}

// Check that it is impossible to change the referrer in the extra headers of
// an URLRequest.
TEST_F(URLRequestTest, DoNotOverrideReferrer) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // If extra headers contain referer and the request contains a referer,
  // only the latter shall be respected.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetReferrer("http://foo.com/");

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kReferer, "http://bar.com/");
    req->SetExtraRequestHeaders(headers);

    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ("http://foo.com/", d.data_received());
  }

  // If extra headers contain a referer but the request does not, no referer
  // shall be sent in the header.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kReferer, "http://bar.com/");
    req->SetExtraRequestHeaders(headers);
    req->SetLoadFlags(LOAD_VALIDATE_CACHE);

    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ("None", d.data_received());
  }
}

class URLRequestTestHTTP : public URLRequestTest {
 public:
  const url::Origin origin1_;
  const url::Origin origin2_;
  const IsolationInfo isolation_info1_;
  const IsolationInfo isolation_info2_;

  URLRequestTestHTTP()
      : origin1_(url::Origin::Create(GURL("https://foo.test/"))),
        origin2_(url::Origin::Create(GURL("https://bar.test/"))),
        isolation_info1_(IsolationInfo::CreateForInternalRequest(origin1_)),
        isolation_info2_(IsolationInfo::CreateForInternalRequest(origin2_)),
        test_server_(base::FilePath(kTestFilePath)) {
  }

 protected:
  // ProtocolHandler for the scheme that's unsafe to redirect to.
  class NET_EXPORT UnsafeRedirectProtocolHandler
      : public URLRequestJobFactory::ProtocolHandler {
   public:
    UnsafeRedirectProtocolHandler() = default;

    UnsafeRedirectProtocolHandler(const UnsafeRedirectProtocolHandler&) =
        delete;
    UnsafeRedirectProtocolHandler& operator=(
        const UnsafeRedirectProtocolHandler&) = delete;

    ~UnsafeRedirectProtocolHandler() override = default;

    // URLRequestJobFactory::ProtocolHandler implementation:

    std::unique_ptr<URLRequestJob> CreateJob(
        URLRequest* request) const override {
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }

    bool IsSafeRedirectTarget(const GURL& location) const override {
      return false;
    }
  };

  // URLRequestTest interface:
  void SetUpContextBuilder(URLRequestContextBuilder& builder) override {
    // Add support for an unsafe scheme to the default URLRequestContext.
    builder.SetProtocolHandler(
        "unsafe", std::make_unique<UnsafeRedirectProtocolHandler>());
  }

  // Requests |redirect_url|, which must return a HTTP 3xx redirect.
  // |request_method| is the method to use for the initial request.
  // |redirect_method| is the method that is expected to be used for the second
  // request, after redirection.
  // If |include_data| is true, data is uploaded with the request.  The
  // response body is expected to match it exactly, if and only if
  // |request_method| == |redirect_method|.
  void HTTPRedirectMethodTest(const GURL& redirect_url,
                              const std::string& request_method,
                              const std::string& redirect_method,
                              bool include_data) {
    static const char kData[] = "hello world";
    TestDelegate d;
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(default_context(), redirect_url, &d);
    req->set_method(request_method);
    if (include_data) {
      req->set_upload(
          CreateSimpleUploadData(base::byte_span_from_cstring(kData)));
      HttpRequestHeaders headers;
      headers.SetHeader(HttpRequestHeaders::kContentLength,
                        base::NumberToString(std::size(kData) - 1));
      headers.SetHeader(HttpRequestHeaders::kContentType, "text/plain");
      req->SetExtraRequestHeaders(headers);
    }
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(redirect_method, req->method());
    EXPECT_EQ(OK, d.request_status());
    if (include_data) {
      if (request_method == redirect_method) {
        EXPECT_TRUE(req->extra_request_headers().HasHeader(
            HttpRequestHeaders::kContentLength));
        EXPECT_TRUE(req->extra_request_headers().HasHeader(
            HttpRequestHeaders::kContentType));
        EXPECT_EQ(kData, d.data_received());
      } else {
        EXPECT_FALSE(req->extra_request_headers().HasHeader(
            HttpRequestHeaders::kContentLength));
        EXPECT_FALSE(req->extra_request_headers().HasHeader(
            HttpRequestHeaders::kContentType));
        EXPECT_NE(kData, d.data_received());
      }
    }
    if (HasFailure())
      LOG(WARNING) << "Request method was: " << request_method;
  }

  // Requests |redirect_url|, which must return a HTTP 3xx redirect. It's also
  // used as the initial origin.
  // |request_method| is the method to use for the initial request.
  // |redirect_method| is the method that is expected to be used for the second
  // request, after redirection.
  // |expected_origin_value| is the expected value for the Origin header after
  // redirection. If empty, expects that there will be no Origin header.
  void HTTPRedirectOriginHeaderTest(const GURL& redirect_url,
                                    const std::string& request_method,
                                    const std::string& redirect_method,
                                    const std::string& expected_origin_value) {
    TestDelegate d;
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(default_context(), redirect_url, &d);
    req->set_method(request_method);
    req->SetExtraRequestHeaderByName(
        HttpRequestHeaders::kOrigin,
        redirect_url.DeprecatedGetOriginAsURL().spec(), false);
    req->Start();

    d.RunUntilComplete();

    EXPECT_EQ(redirect_method, req->method());
    // Note that there is no check for request success here because, for
    // purposes of testing, the request very well may fail. For example, if the
    // test redirects to an HTTPS server from an HTTP origin, thus it is cross
    // origin, there is not an HTTPS server in this unit test framework, so the
    // request would fail. However, that's fine, as long as the request headers
    // are in order and pass the checks below.
    if (expected_origin_value.empty()) {
      EXPECT_FALSE(
          req->extra_request_headers().HasHeader(HttpRequestHeaders::kOrigin));
    } else {
      EXPECT_EQ(expected_origin_value, req->extra_request_headers().GetHeader(
                                           HttpRequestHeaders::kOrigin));
    }
  }

  void HTTPUploadDataOperationTest(const std::string& method) {
    const int kMsgSize = 20000;  // multiple of 10
    const int kIterations = 50;
    auto uploadBytes = base::HeapArray<char>::Uninit(kMsgSize);
    char* ptr = uploadBytes.data();
    char marker = 'a';
    for (int idx = 0; idx < kMsgSize / 10; idx++) {
      memcpy(ptr, "----------", 10);
      ptr += 10;
      if (idx % 100 == 0) {
        ptr--;
        *ptr++ = marker;
        if (++marker > 'z')
          marker = 'a';
      }
    }

    for (int i = 0; i < kIterations; ++i) {
      TestDelegate d;
      std::unique_ptr<URLRequest> r(default_context().CreateRequest(
          test_server_.GetURL("/echo"), DEFAULT_PRIORITY, &d,
          TRAFFIC_ANNOTATION_FOR_TESTS));
      r->set_method(method);

      r->set_upload(
          CreateSimpleUploadData(base::as_bytes(uploadBytes.as_span())));

      r->Start();
      EXPECT_TRUE(r->is_pending());

      d.RunUntilComplete();

      ASSERT_EQ(1, d.response_started_count())
          << "request failed. Error: " << d.request_status();

      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_EQ(base::as_string_view(uploadBytes.as_span()), d.data_received());
    }
  }

  HttpTestServer* http_test_server() { return &test_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;

  HttpTestServer test_server_;
};

namespace {

std::unique_ptr<test_server::HttpResponse> HandleRedirectConnect(
    const test_server::HttpRequest& request) {
  if (request.headers.find("Host") == request.headers.end() ||
      request.headers.at("Host") != "www.redirect.com" ||
      request.method != test_server::METHOD_CONNECT) {
    return nullptr;
  }

  auto http_response = std::make_unique<test_server::BasicHttpResponse>();
  http_response->set_code(HTTP_FOUND);
  http_response->AddCustomHeader("Location",
                                 "http://www.destination.com/foo.js");
  return std::move(http_response);
}

}  // namespace

// In this unit test, we're using the HTTPTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.redirect.com".
// The EmbeddedTestServer will return a 302 response, which we should not
// follow.
TEST_F(URLRequestTestHTTP, ProxyTunnelRedirectTest) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRedirectConnect));
  ASSERT_TRUE(http_test_server()->Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService(
          http_test_server()->host_port_pair().ToString()));
  auto context = context_builder->Build();

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        GURL("https://www.redirect.com/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The proxy chain should be set before failure.
    EXPECT_EQ(ProxyChain(ProxyServer::SCHEME_HTTP,
                         http_test_server()->host_port_pair()),
              r->proxy_chain());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, d.request_status());
    EXPECT_EQ(1, d.response_started_count());
    // We should not have followed the redirect.
    EXPECT_EQ(0, d.received_redirect_count());
  }
}

// This is the same as the previous test, but checks that the network delegate
// registers the error.
TEST_F(URLRequestTestHTTP, NetworkDelegateTunnelConnectionFailed) {
  ASSERT_TRUE(http_test_server()->Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService(
          http_test_server()->host_port_pair().ToString()));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        GURL("https://www.redirect.com/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The proxy chain should be set before failure.
    EXPECT_EQ(ProxyChain(ProxyServer::SCHEME_HTTP,
                         http_test_server()->host_port_pair()),
              r->proxy_chain());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, d.request_status());
    // We should not have followed the redirect.
    EXPECT_EQ(0, d.received_redirect_count());

    EXPECT_EQ(1, network_delegate.error_count());
    EXPECT_THAT(network_delegate.last_error(),
                IsError(ERR_TUNNEL_CONNECTION_FAILED));
  }
}

// Tests that we can block and asynchronously return OK in various stages.
TEST_F(URLRequestTestHTTP, NetworkDelegateBlockAsynchronously) {
  static const BlockingNetworkDelegate::Stage blocking_stages[] = {
      BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
      BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
      BlockingNetworkDelegate::ON_HEADERS_RECEIVED};

  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::USER_CALLBACK));
  network_delegate.set_block_on(
      BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST |
      BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS |
      BlockingNetworkDelegate::ON_HEADERS_RECEIVED);
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    for (auto stage : blocking_stages) {
      network_delegate.RunUntilBlocked();
      EXPECT_EQ(stage, network_delegate.stage_blocked_for_callback());
      network_delegate.DoCallback(OK);
    }
    d.RunUntilComplete();
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can block and cancel a request.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService(
          http_test_server()->host_port_pair().ToString()));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::AUTO_CALLBACK));
  auto context = context_builder->Build();

  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  network_delegate.set_retval(ERR_EMPTY_RESPONSE);
  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    // The proxy chain is not set before cancellation.
    EXPECT_FALSE(r->proxy_chain().IsValid());
    EXPECT_EQ(ERR_EMPTY_RESPONSE, d.request_status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Helper function for NetworkDelegateCancelRequestAsynchronously and
// NetworkDelegateCancelRequestSynchronously. Sets up a blocking network
// delegate operating in |block_mode| and a request for |url|. It blocks the
// request in |stage| and cancels it with ERR_BLOCKED_BY_CLIENT.
void NetworkDelegateCancelRequest(BlockingNetworkDelegate::BlockMode block_mode,
                                  BlockingNetworkDelegate::Stage stage,
                                  const GURL& url) {
  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(block_mode));
  network_delegate.set_retval(ERR_BLOCKED_BY_CLIENT);
  network_delegate.set_block_on(stage);

  auto context = context_builder->Build();
  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    // The proxy chain is not set before cancellation.
    if (stage == BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST ||
        stage == BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS) {
      EXPECT_FALSE(r->proxy_chain().IsValid());
    } else if (stage == BlockingNetworkDelegate::ON_HEADERS_RECEIVED) {
      EXPECT_TRUE(r->proxy_chain().is_direct());
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    EXPECT_EQ(ERR_BLOCKED_BY_CLIENT, d.request_status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// The following 3 tests check that the network delegate can cancel a request
// synchronously in various stages of the request.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestSynchronously1) {
  ASSERT_TRUE(http_test_server()->Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::SYNCHRONOUS,
                               BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
                               http_test_server()->GetURL("/"));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestSynchronously2) {
  ASSERT_TRUE(http_test_server()->Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::SYNCHRONOUS,
                               BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
                               http_test_server()->GetURL("/"));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestSynchronously3) {
  ASSERT_TRUE(http_test_server()->Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::SYNCHRONOUS,
                               BlockingNetworkDelegate::ON_HEADERS_RECEIVED,
                               http_test_server()->GetURL("/"));
}

// The following 3 tests check that the network delegate can cancel a request
// asynchronously in various stages of the request.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestAsynchronously1) {
  ASSERT_TRUE(http_test_server()->Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::AUTO_CALLBACK,
                               BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
                               http_test_server()->GetURL("/"));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestAsynchronously2) {
  ASSERT_TRUE(http_test_server()->Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::AUTO_CALLBACK,
                               BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
                               http_test_server()->GetURL("/"));
}

TEST_F(URLRequestTestHTTP, NetworkDelegateCancelRequestAsynchronously3) {
  ASSERT_TRUE(http_test_server()->Start());
  NetworkDelegateCancelRequest(BlockingNetworkDelegate::AUTO_CALLBACK,
                               BlockingNetworkDelegate::ON_HEADERS_RECEIVED,
                               http_test_server()->GetURL("/"));
}

// Tests that the network delegate can block and redirect a request to a new
// URL.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService(
          http_test_server()->host_port_pair().ToString()));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::AUTO_CALLBACK));
  auto context = context_builder->Build();

  GURL redirect_url("http://does.not.resolve.test/simple.html");
  network_delegate.set_redirect_url(redirect_url);
  {
    GURL original_url("http://does.not.resolve.test/defaultresponse");
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    // Quit after hitting the redirect, so can check the headers.
    r->Start();
    d.RunUntilRedirect();

    // Check headers from URLRequestJob.
    EXPECT_EQ(307, r->GetResponseCode());
    EXPECT_EQ(307, r->response_headers()->response_code());
    std::string location;
    ASSERT_TRUE(
        r->response_headers()->EnumerateHeader(nullptr, "Location", &location));
    EXPECT_EQ(redirect_url, GURL(location));

    // Let the request finish.
    r->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                              std::nullopt /* modified_headers */);
    d.RunUntilComplete();
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(ProxyChain(ProxyServer::SCHEME_HTTP,
                         http_test_server()->host_port_pair()),
              r->proxy_chain());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(redirect_url, r->url());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can block and redirect a request to a new
// URL by setting a redirect_url and returning in OnBeforeURLRequest directly.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequestSynchronously) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService(
          http_test_server()->host_port_pair().ToString()));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::SYNCHRONOUS));
  auto context = context_builder->Build();

  GURL redirect_url("http://does.not.resolve.test/simple.html");
  network_delegate.set_redirect_url(redirect_url);
  {
    GURL original_url("http://does.not.resolve.test/defaultresponse");
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    // Quit after hitting the redirect, so can check the headers.
    r->Start();
    d.RunUntilRedirect();

    // Check headers from URLRequestJob.
    EXPECT_EQ(307, r->GetResponseCode());
    EXPECT_EQ(307, r->response_headers()->response_code());
    std::string location;
    ASSERT_TRUE(
        r->response_headers()->EnumerateHeader(nullptr, "Location", &location));
    EXPECT_EQ(redirect_url, GURL(location));

    // Let the request finish.
    r->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                              std::nullopt /* modified_headers */);
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(ProxyChain(ProxyServer::SCHEME_HTTP,
                         http_test_server()->host_port_pair()),
              r->proxy_chain());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(redirect_url, r->url());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that redirects caused by the network delegate preserve POST data.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequestPost) {
  ASSERT_TRUE(http_test_server()->Start());

  const char kData[] = "hello world";

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::AUTO_CALLBACK));
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  GURL redirect_url(http_test_server()->GetURL("/echo"));
  network_delegate.set_redirect_url(redirect_url);

  auto context = context_builder->Build();

  {
    GURL original_url(http_test_server()->GetURL("/defaultresponse"));
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method("POST");
    r->set_upload(CreateSimpleUploadData(base::byte_span_from_cstring(kData)));
    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kContentLength,
                      base::NumberToString(std::size(kData) - 1));
    r->SetExtraRequestHeaders(headers);

    // Quit after hitting the redirect, so can check the headers.
    r->Start();
    d.RunUntilRedirect();

    // Check headers from URLRequestJob.
    EXPECT_EQ(307, r->GetResponseCode());
    EXPECT_EQ(307, r->response_headers()->response_code());
    std::string location;
    ASSERT_TRUE(
        r->response_headers()->EnumerateHeader(nullptr, "Location", &location));
    EXPECT_EQ(redirect_url, GURL(location));

    // Let the request finish.
    r->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                              std::nullopt /* modified_headers */);
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(redirect_url, r->url());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
    EXPECT_EQ("POST", r->method());
    EXPECT_EQ(kData, d.data_received());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can block and redirect a request to a new
// URL during OnHeadersReceived.
TEST_F(URLRequestTestHTTP, NetworkDelegateRedirectRequestOnHeadersReceived) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService(
          http_test_server()->host_port_pair().ToString()));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::AUTO_CALLBACK));
  auto context = context_builder->Build();

  network_delegate.set_block_on(BlockingNetworkDelegate::ON_HEADERS_RECEIVED);
  GURL redirect_url("http://does.not.resolve.test/simple.html");
  network_delegate.set_redirect_on_headers_received_url(redirect_url);
  {
    GURL original_url("http://does.not.resolve.test/defaultresponse");
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(ProxyChain(ProxyServer::SCHEME_HTTP,
                         http_test_server()->host_port_pair()),
              r->proxy_chain());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(redirect_url, r->url());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(2, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by taking no action. This indicates that the NetworkDelegate does not want to
// handle the challenge, and is passing the buck along to the
// URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncNoAction) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::SYNCHRONOUS));
  auto context = context_builder->Build();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that NetworkDelegate header overrides from the 401 response do not
// affect the 200 response. This is a regression test for
// https://crbug.com/801237.
TEST_F(URLRequestTestHTTP, NetworkDelegateOverrideHeadersWithAuth) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  d.set_credentials(AuthCredentials(kUser, kSecret));
  default_network_delegate().set_add_header_to_first_response(true);

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_FALSE(r->response_headers()->HasHeader("X-Network-Delegate"));
  }

  {
    GURL url(http_test_server()->GetURL("/defaultresponse"));
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    d.RunUntilComplete();

    // Check that set_add_header_to_first_response normally adds a header.
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_TRUE(r->response_headers()->HasHeader("X-Network-Delegate"));
  }
}

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 1: Request is cancelled while waiting for OnBeforeURLRequest callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting1) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::USER_CALLBACK));
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    network_delegate.RunUntilBlocked();
    EXPECT_EQ(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST,
              network_delegate.stage_blocked_for_callback());
    EXPECT_EQ(0, network_delegate.completed_requests());
    // Cancel before callback.
    r->Cancel();
    // Ensure that network delegate is notified.
    EXPECT_EQ(1, network_delegate.completed_requests());
    EXPECT_EQ(1, network_delegate.canceled_requests());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 2: Request is cancelled while waiting for OnBeforeStartTransaction
// callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting2) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::USER_CALLBACK));
  network_delegate.set_block_on(
      BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS);
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    network_delegate.RunUntilBlocked();
    EXPECT_EQ(BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS,
              network_delegate.stage_blocked_for_callback());
    EXPECT_EQ(0, network_delegate.completed_requests());
    // Cancel before callback.
    r->Cancel();
    // Ensure that network delegate is notified.
    EXPECT_EQ(1, network_delegate.completed_requests());
    EXPECT_EQ(1, network_delegate.canceled_requests());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 3: Request is cancelled while waiting for OnHeadersReceived callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting3) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::USER_CALLBACK));
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_HEADERS_RECEIVED);
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    network_delegate.RunUntilBlocked();
    EXPECT_EQ(BlockingNetworkDelegate::ON_HEADERS_RECEIVED,
              network_delegate.stage_blocked_for_callback());
    EXPECT_EQ(0, network_delegate.completed_requests());
    // Cancel before callback.
    r->Cancel();
    // Ensure that network delegate is notified.
    EXPECT_EQ(1, network_delegate.completed_requests());
    EXPECT_EQ(1, network_delegate.canceled_requests());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

namespace {

std::unique_ptr<test_server::HttpResponse> HandleServerAuthConnect(
    const test_server::HttpRequest& request) {
  if (request.headers.find("Host") == request.headers.end() ||
      request.headers.at("Host") != "www.server-auth.com" ||
      request.method != test_server::METHOD_CONNECT) {
    return nullptr;
  }

  auto http_response = std::make_unique<test_server::BasicHttpResponse>();
  http_response->set_code(HTTP_UNAUTHORIZED);
  http_response->AddCustomHeader("WWW-Authenticate",
                                 "Basic realm=\"WallyWorld\"");
  return http_response;
}

}  // namespace

// In this unit test, we're using the EmbeddedTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.server-auth.com".
// The EmbeddedTestServer will return a 401 response, which we should balk at.
TEST_F(URLRequestTestHTTP, UnexpectedServerAuthTest) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleServerAuthConnect));
  ASSERT_TRUE(http_test_server()->Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(
      CreateFixedProxyResolutionService(
          http_test_server()->host_port_pair().ToString()));
  auto context = context_builder->Build();

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        GURL("https://www.server-auth.com/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The proxy chain should be set before failure.
    EXPECT_EQ(ProxyChain(ProxyServer::SCHEME_HTTP,
                         http_test_server()->host_port_pair()),
              r->proxy_chain());
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, d.request_status());
  }
}

TEST_F(URLRequestTestHTTP, GetTest_NoCache) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(http_test_server()->host_port_pair().host(),
              r->GetResponseRemoteEndpoint().ToStringWithoutPort());
    EXPECT_EQ(http_test_server()->host_port_pair().port(),
              r->GetResponseRemoteEndpoint().port());

    // TODO(eroman): Add back the NetLog tests...
  }
}

TEST_F(URLRequestTestHTTP, GetTest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(http_test_server()->host_port_pair().host(),
              r->GetResponseRemoteEndpoint().ToStringWithoutPort());
    EXPECT_EQ(http_test_server()->host_port_pair().port(),
              r->GetResponseRemoteEndpoint().port());
  }
}

TEST_F(URLRequestTestHTTP, GetTestLoadTiming) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    LoadTimingInfo load_timing_info;
    r->GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(http_test_server()->host_port_pair().host(),
              r->GetResponseRemoteEndpoint().ToStringWithoutPort());
    EXPECT_EQ(http_test_server()->host_port_pair().port(),
              r->GetResponseRemoteEndpoint().port());
  }
}

namespace {

// Sends the correct Content-Length matching the compressed length.
const char kZippedContentLengthCompressed[] = "C";
// Sends an incorrect Content-Length matching the uncompressed length.
const char kZippedContentLengthUncompressed[] = "U";
// Sends an incorrect Content-Length shorter than the compressed length.
const char kZippedContentLengthShort[] = "S";
// Sends an incorrect Content-Length between the compressed and uncompressed
// lengths.
const char kZippedContentLengthMedium[] = "M";
// Sends an incorrect Content-Length larger than both compressed and
// uncompressed lengths.
const char kZippedContentLengthLong[] = "L";

// Sends |compressed_content| which, when decoded with deflate, should have
// length |uncompressed_length|. The Content-Length header will be sent based on
// which of the constants above is sent in the query string.
std::unique_ptr<test_server::HttpResponse> HandleZippedRequest(
    const std::string& compressed_content,
    size_t uncompressed_length,
    const test_server::HttpRequest& request) {
  GURL url = request.GetURL();
  if (url.path_piece() != "/compressedfiles/BullRunSpeech.txt")
    return nullptr;

  size_t length;
  if (url.query_piece() == kZippedContentLengthCompressed) {
    length = compressed_content.size();
  } else if (url.query_piece() == kZippedContentLengthUncompressed) {
    length = uncompressed_length;
  } else if (url.query_piece() == kZippedContentLengthShort) {
    length = compressed_content.size() / 2;
  } else if (url.query_piece() == kZippedContentLengthMedium) {
    length = (compressed_content.size() + uncompressed_length) / 2;
  } else if (url.query_piece() == kZippedContentLengthLong) {
    length = compressed_content.size() + uncompressed_length;
  } else {
    return nullptr;
  }

  std::string headers = "HTTP/1.1 200 OK\r\n";
  headers += "Content-Encoding: deflate\r\n";
  base::StringAppendF(&headers, "Content-Length: %zu\r\n", length);
  return std::make_unique<test_server::RawHttpResponse>(headers,
                                                        compressed_content);
}

}  // namespace

TEST_F(URLRequestTestHTTP, GetZippedTest) {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path = file_path.Append(kTestFilePath);
  std::string expected_content, compressed_content;
  ASSERT_TRUE(base::ReadFileToString(
      file_path.Append(FILE_PATH_LITERAL("BullRunSpeech.txt")),
      &expected_content));
  // This file is the output of the Python zlib.compress function on
  // |expected_content|.
  ASSERT_TRUE(base::ReadFileToString(
      file_path.Append(FILE_PATH_LITERAL("BullRunSpeech.txt.deflate")),
      &compressed_content));

  http_test_server()->RegisterRequestHandler(base::BindRepeating(
      &HandleZippedRequest, compressed_content, expected_content.size()));
  ASSERT_TRUE(http_test_server()->Start());

  static const struct {
    const char* parameter;
    bool expect_success;
  } kTests[] = {
      // Sending the compressed Content-Length is correct.
      {kZippedContentLengthCompressed, true},
      // Sending the uncompressed Content-Length is incorrect, but we accept it
      // to workaround some broken servers.
      {kZippedContentLengthUncompressed, true},
      // Sending too long of Content-Length is rejected.
      {kZippedContentLengthLong, false},
      {kZippedContentLengthMedium, false},
      // Sending too short of Content-Length successfully fetches a response
      // body, but it will be truncated.
      {kZippedContentLengthShort, true},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.parameter);
    TestDelegate d;
    std::string test_file = base::StringPrintf(
        "/compressedfiles/BullRunSpeech.txt?%s", test.parameter);

    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_network_delegate(
        std::make_unique<TestNetworkDelegate>());
    auto context = context_builder->Build();

    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL(test_file), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    VLOG(1) << " Received " << d.bytes_received() << " bytes"
            << " error = " << d.request_status();
    if (test.expect_success) {
      EXPECT_EQ(OK, d.request_status())
          << " Parameter = \"" << test_file << "\"";
      if (strcmp(test.parameter, kZippedContentLengthShort) == 0) {
        // When content length is smaller than both compressed length and
        // uncompressed length, HttpStreamParser might not read the full
        // response body.
        EXPECT_EQ(expected_content.substr(0, d.data_received().size()),
                  d.data_received());
      } else {
        EXPECT_EQ(expected_content, d.data_received());
      }
    } else {
      EXPECT_EQ(ERR_CONTENT_LENGTH_MISMATCH, d.request_status())
          << " Parameter = \"" << test_file << "\"";
    }
  }
}

TEST_F(URLRequestTestHTTP, RedirectLoadTiming) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL destination_url = http_test_server()->GetURL("/");
  GURL original_url =
      http_test_server()->GetURL("/server-redirect?" + destination_url.spec());
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(1, d.received_redirect_count());
  EXPECT_EQ(destination_url, req->url());
  EXPECT_EQ(original_url, req->original_url());
  ASSERT_EQ(2U, req->url_chain().size());
  EXPECT_EQ(original_url, req->url_chain()[0]);
  EXPECT_EQ(destination_url, req->url_chain()[1]);

  LoadTimingInfo load_timing_info_before_redirect;
  EXPECT_TRUE(default_network_delegate().GetLoadTimingInfoBeforeRedirect(
      &load_timing_info_before_redirect));
  TestLoadTimingNotReused(load_timing_info_before_redirect,
                          CONNECT_TIMING_HAS_DNS_TIMES);

  LoadTimingInfo load_timing_info;
  req->GetLoadTimingInfo(&load_timing_info);
  TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);

  // Check that a new socket was used on redirect, since the server does not
  // supposed keep-alive sockets, and that the times before the redirect are
  // before the ones recorded for the second request.
  EXPECT_NE(load_timing_info_before_redirect.socket_log_id,
            load_timing_info.socket_log_id);
  EXPECT_LE(load_timing_info_before_redirect.receive_headers_end,
            load_timing_info.connect_timing.connect_start);
}

TEST_F(URLRequestTestHTTP, MultipleRedirectTest) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL destination_url = http_test_server()->GetURL("/");
  GURL middle_redirect_url =
      http_test_server()->GetURL("/server-redirect?" + destination_url.spec());
  GURL original_url = http_test_server()->GetURL("/server-redirect?" +
                                                 middle_redirect_url.spec());
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(2, d.received_redirect_count());
  EXPECT_EQ(destination_url, req->url());
  EXPECT_EQ(original_url, req->original_url());
  ASSERT_EQ(3U, req->url_chain().size());
  EXPECT_EQ(original_url, req->url_chain()[0]);
  EXPECT_EQ(middle_redirect_url, req->url_chain()[1]);
  EXPECT_EQ(destination_url, req->url_chain()[2]);
}

// This is a regression test for https://crbug.com/942073.
TEST_F(URLRequestTestHTTP, RedirectEscaping) {
  ASSERT_TRUE(http_test_server()->Start());

  // Assemble the destination URL as a string so it is not escaped by GURL.
  GURL destination_base = http_test_server()->GetURL("/defaultresponse");
  // Add a URL fragment of U+2603 unescaped, U+2603 escaped, and then a UTF-8
  // encoding error.
  std::string destination_url =
      destination_base.spec() + "#\xE2\x98\x83_%E2%98%83_\xE0\xE0";
  // Redirect resolution should percent-escape bytes and preserve the UTF-8
  // error at the end.
  std::string destination_escaped =
      destination_base.spec() + "#%E2%98%83_%E2%98%83_%E0%E0";
  GURL original_url = http_test_server()->GetURL(
      "/server-redirect?" +
      base::EscapeQueryParamValue(destination_url, false));
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(1, d.received_redirect_count());
  EXPECT_EQ(destination_escaped, req->url().spec());
  EXPECT_EQ(original_url, req->original_url());
  ASSERT_EQ(2U, req->url_chain().size());
  EXPECT_EQ(original_url, req->url_chain()[0]);
  EXPECT_EQ(destination_escaped, req->url_chain()[1].spec());
}

// First and second pieces of information logged by delegates to URLRequests.
const char kFirstDelegateInfo[] = "Wonderful delegate";
const char16_t kFirstDelegateInfo16[] = u"Wonderful delegate";
const char kSecondDelegateInfo[] = "Exciting delegate";
const char16_t kSecondDelegateInfo16[] = u"Exciting delegate";

// Logs delegate information to a URLRequest.  The first string is logged
// synchronously on Start(), using DELEGATE_INFO_DEBUG_ONLY.  The second is
// logged asynchronously, using DELEGATE_INFO_DISPLAY_TO_USER.  Then
// another asynchronous call is used to clear the delegate information
// before calling a callback.  The object then deletes itself.
class AsyncDelegateLogger : public base::RefCounted<AsyncDelegateLogger> {
 public:
  using Callback = base::OnceCallback<void()>;

  AsyncDelegateLogger(const AsyncDelegateLogger&) = delete;
  AsyncDelegateLogger& operator=(const AsyncDelegateLogger&) = delete;

  // Each time delegate information is added to the URLRequest, the resulting
  // load state is checked.  The expected load state after each request is
  // passed in as an argument.
  static void Run(URLRequest* url_request,
                  LoadState expected_first_load_state,
                  LoadState expected_second_load_state,
                  LoadState expected_third_load_state,
                  Callback callback) {
    // base::MakeRefCounted<AsyncDelegateLogger> is unavailable here, since the
    // constructor of AsyncDelegateLogger is private.
    auto logger = base::WrapRefCounted(new AsyncDelegateLogger(
        url_request, expected_first_load_state, expected_second_load_state,
        expected_third_load_state, std::move(callback)));
    logger->Start();
  }

  // Checks that the log entries, starting with log_position, contain the
  // DELEGATE_INFO NetLog events that an AsyncDelegateLogger should have
  // recorded.  Returns the index of entry after the expected number of
  // events this logged, or entries.size() if there aren't enough entries.
  static size_t CheckDelegateInfo(const std::vector<NetLogEntry>& entries,
                                  size_t log_position) {
    // There should be 4 DELEGATE_INFO events: Two begins and two ends.
    if (log_position + 3 >= entries.size()) {
      ADD_FAILURE() << "Not enough log entries";
      return entries.size();
    }
    std::string delegate_info;
    EXPECT_EQ(NetLogEventType::DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::BEGIN, entries[log_position].phase);
    EXPECT_EQ(
        kFirstDelegateInfo,
        GetStringValueFromParams(entries[log_position], "delegate_blocked_by"));

    ++log_position;
    EXPECT_EQ(NetLogEventType::DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);

    ++log_position;
    EXPECT_EQ(NetLogEventType::DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::BEGIN, entries[log_position].phase);
    EXPECT_EQ(
        kSecondDelegateInfo,
        GetStringValueFromParams(entries[log_position], "delegate_blocked_by"));

    ++log_position;
    EXPECT_EQ(NetLogEventType::DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);

    return log_position + 1;
  }

 private:
  friend class base::RefCounted<AsyncDelegateLogger>;

  AsyncDelegateLogger(URLRequest* url_request,
                      LoadState expected_first_load_state,
                      LoadState expected_second_load_state,
                      LoadState expected_third_load_state,
                      Callback callback)
      : url_request_(url_request),
        expected_first_load_state_(expected_first_load_state),
        expected_second_load_state_(expected_second_load_state),
        expected_third_load_state_(expected_third_load_state),
        callback_(std::move(callback)) {}

  ~AsyncDelegateLogger() = default;

  void Start() {
    url_request_->LogBlockedBy(kFirstDelegateInfo);
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_first_load_state_, load_state.state);
    EXPECT_NE(kFirstDelegateInfo16, load_state.param);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AsyncDelegateLogger::LogSecondDelegate, this));
  }

  void LogSecondDelegate() {
    url_request_->LogAndReportBlockedBy(kSecondDelegateInfo);
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_second_load_state_, load_state.state);
    if (expected_second_load_state_ == LOAD_STATE_WAITING_FOR_DELEGATE) {
      EXPECT_EQ(kSecondDelegateInfo16, load_state.param);
    } else {
      EXPECT_NE(kSecondDelegateInfo16, load_state.param);
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AsyncDelegateLogger::LogComplete, this));
  }

  void LogComplete() {
    url_request_->LogUnblocked();
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_third_load_state_, load_state.state);
    if (expected_second_load_state_ == LOAD_STATE_WAITING_FOR_DELEGATE)
      EXPECT_EQ(std::u16string(), load_state.param);
    std::move(callback_).Run();
  }

  raw_ptr<URLRequest> url_request_;
  const int expected_first_load_state_;
  const int expected_second_load_state_;
  const int expected_third_load_state_;
  Callback callback_;
};

// NetworkDelegate that logs delegate information before a request is started,
// before headers are sent, when headers are read, and when auth information
// is requested.  Uses AsyncDelegateLogger.
class AsyncLoggingNetworkDelegate : public TestNetworkDelegate {
 public:
  AsyncLoggingNetworkDelegate() = default;

  AsyncLoggingNetworkDelegate(const AsyncLoggingNetworkDelegate&) = delete;
  AsyncLoggingNetworkDelegate& operator=(const AsyncLoggingNetworkDelegate&) =
      delete;

  ~AsyncLoggingNetworkDelegate() override = default;

  // NetworkDelegate implementation.
  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override {
    // TestNetworkDelegate always completes synchronously.
    CHECK_NE(ERR_IO_PENDING, TestNetworkDelegate::OnBeforeURLRequest(
                                 request, base::NullCallback(), new_url));
    return RunCallbackAsynchronously(request, std::move(callback));
  }

  int OnBeforeStartTransaction(
      URLRequest* request,
      const HttpRequestHeaders& headers,
      OnBeforeStartTransactionCallback callback) override {
    // TestNetworkDelegate always completes synchronously.
    CHECK_NE(ERR_IO_PENDING, TestNetworkDelegate::OnBeforeStartTransaction(
                                 request, headers, base::NullCallback()));
    return RunCallbackAsynchronously(
        request, base::BindOnce(
                     [](OnBeforeStartTransactionCallback callback, int result) {
                       std::move(callback).Run(result, std::nullopt);
                     },
                     std::move(callback)));
  }

  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url) override {
    // TestNetworkDelegate always completes synchronously.
    CHECK_NE(ERR_IO_PENDING,
             TestNetworkDelegate::OnHeadersReceived(
                 request, base::NullCallback(), original_response_headers,
                 override_response_headers, endpoint,
                 preserve_fragment_on_redirect_url));
    return RunCallbackAsynchronously(request, std::move(callback));
  }

 private:
  static int RunCallbackAsynchronously(URLRequest* request,
                                       CompletionOnceCallback callback) {
    AsyncDelegateLogger::Run(request, LOAD_STATE_WAITING_FOR_DELEGATE,
                             LOAD_STATE_WAITING_FOR_DELEGATE,
                             LOAD_STATE_WAITING_FOR_DELEGATE,
                             base::BindOnce(std::move(callback), OK));
    return ERR_IO_PENDING;
  }
};

// URLRequest::Delegate that logs delegate information when the headers
// are received, when each read completes, and during redirects.  Uses
// AsyncDelegateLogger.  Can optionally cancel a request in any phase.
//
// Inherits from TestDelegate to reuse the TestDelegate code to handle
// advancing to the next step in most cases, as well as cancellation.
class AsyncLoggingUrlRequestDelegate : public TestDelegate {
 public:
  enum CancelStage {
    NO_CANCEL = 0,
    CANCEL_ON_RECEIVED_REDIRECT,
    CANCEL_ON_RESPONSE_STARTED,
    CANCEL_ON_READ_COMPLETED
  };

  explicit AsyncLoggingUrlRequestDelegate(CancelStage cancel_stage)
      : cancel_stage_(cancel_stage) {
    if (cancel_stage == CANCEL_ON_RECEIVED_REDIRECT)
      set_cancel_in_received_redirect(true);
    else if (cancel_stage == CANCEL_ON_RESPONSE_STARTED)
      set_cancel_in_response_started(true);
    else if (cancel_stage == CANCEL_ON_READ_COMPLETED)
      set_cancel_in_received_data(true);
  }

  AsyncLoggingUrlRequestDelegate(const AsyncLoggingUrlRequestDelegate&) =
      delete;
  AsyncLoggingUrlRequestDelegate& operator=(
      const AsyncLoggingUrlRequestDelegate&) = delete;

  ~AsyncLoggingUrlRequestDelegate() override = default;

  // URLRequest::Delegate implementation:
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    *defer_redirect = true;
    AsyncDelegateLogger::Run(
        request, LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE, LOAD_STATE_WAITING_FOR_DELEGATE,
        base::BindOnce(
            &AsyncLoggingUrlRequestDelegate::OnReceivedRedirectLoggingComplete,
            base::Unretained(this), request, redirect_info));
  }

  void OnResponseStarted(URLRequest* request, int net_error) override {
    AsyncDelegateLogger::Run(
        request, LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE, LOAD_STATE_WAITING_FOR_DELEGATE,
        base::BindOnce(
            &AsyncLoggingUrlRequestDelegate::OnResponseStartedLoggingComplete,
            base::Unretained(this), request, net_error));
  }

  void OnReadCompleted(URLRequest* request, int bytes_read) override {
    AsyncDelegateLogger::Run(
        request, LOAD_STATE_IDLE, LOAD_STATE_IDLE, LOAD_STATE_IDLE,
        base::BindOnce(
            &AsyncLoggingUrlRequestDelegate::AfterReadCompletedLoggingComplete,
            base::Unretained(this), request, bytes_read));
  }

 private:
  void OnReceivedRedirectLoggingComplete(URLRequest* request,
                                         const RedirectInfo& redirect_info) {
    bool defer_redirect = false;
    TestDelegate::OnReceivedRedirect(request, redirect_info, &defer_redirect);
    // FollowDeferredRedirect should not be called after cancellation.
    if (cancel_stage_ == CANCEL_ON_RECEIVED_REDIRECT)
      return;
    if (!defer_redirect) {
      request->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                                      std::nullopt /* modified_headers */);
    }
  }

  void OnResponseStartedLoggingComplete(URLRequest* request, int net_error) {
    // The parent class continues the request.
    TestDelegate::OnResponseStarted(request, net_error);
  }

  void AfterReadCompletedLoggingComplete(URLRequest* request, int bytes_read) {
    // The parent class continues the request.
    TestDelegate::OnReadCompleted(request, bytes_read);
  }

  const CancelStage cancel_stage_;
};

// Tests handling of delegate info before a request starts.
TEST_F(URLRequestTestHTTP, DelegateInfoBeforeStart) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate request_delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_net_log(NetLog::Get());
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY,
        &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(std::u16string(), load_state.param);

    AsyncDelegateLogger::Run(
        r.get(), LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE, LOAD_STATE_IDLE,
        base::BindOnce(&URLRequest::Start, base::Unretained(r.get())));

    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
  }

  auto entries = net_log_observer_.GetEntries();
  size_t log_position = ExpectLogContainsSomewhereAfter(
      entries, 0, NetLogEventType::DELEGATE_INFO, NetLogEventPhase::BEGIN);

  log_position = AsyncDelegateLogger::CheckDelegateInfo(entries, log_position);

  // Nothing else should add any delegate info to the request.
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(entries, log_position + 1,
                                             NetLogEventType::DELEGATE_INFO));
}

// Tests handling of delegate info from a network delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateInfo) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate request_delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<AsyncLoggingNetworkDelegate>());
  context_builder->set_net_log(NetLog::Get());
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/simple.html"), DEFAULT_PRIORITY,
        &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(std::u16string(), load_state.param);

    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  auto entries = net_log_observer_.GetEntries();
  static const NetLogEventType kExpectedEvents[] = {
      NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST,
      NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION,
      NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED,
  };
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLogEventTypeToString(event));
    log_position = ExpectLogContainsSomewhereAfter(
        entries, log_position + 1, event, NetLogEventPhase::BEGIN);

    log_position =
        AsyncDelegateLogger::CheckDelegateInfo(entries, log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(event, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(entries, log_position + 1,
                                             NetLogEventType::DELEGATE_INFO));
}

// Tests handling of delegate info from a network delegate in the case of an
// HTTP redirect.
TEST_F(URLRequestTestHTTP, NetworkDelegateInfoRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate request_delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<AsyncLoggingNetworkDelegate>());
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/server-redirect?simple.html"),
        DEFAULT_PRIORITY, &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(std::u16string(), load_state.param);

    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
    EXPECT_EQ(2, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  auto entries = net_log_observer_.GetEntries();
  static const NetLogEventType kExpectedEvents[] = {
      NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST,
      NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION,
      NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED,
  };
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLogEventTypeToString(event));
    log_position = ExpectLogContainsSomewhereAfter(
        entries, log_position + 1, event, NetLogEventPhase::BEGIN);

    log_position =
        AsyncDelegateLogger::CheckDelegateInfo(entries, log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(event, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);
  }

  // The URLRequest::Delegate then gets informed about the redirect.
  log_position = ExpectLogContainsSomewhereAfter(
      entries, log_position + 1,
      NetLogEventType::URL_REQUEST_DELEGATE_RECEIVED_REDIRECT,
      NetLogEventPhase::BEGIN);

  // The NetworkDelegate logged information in the same three events as before.
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLogEventTypeToString(event));
    log_position = ExpectLogContainsSomewhereAfter(
        entries, log_position + 1, event, NetLogEventPhase::BEGIN);

    log_position =
        AsyncDelegateLogger::CheckDelegateInfo(entries, log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(event, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(entries, log_position + 1,
                                             NetLogEventType::DELEGATE_INFO));
}

// Tests handling of delegate info from a URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, URLRequestDelegateInfo) {
  ASSERT_TRUE(http_test_server()->Start());

  AsyncLoggingUrlRequestDelegate request_delegate(
      AsyncLoggingUrlRequestDelegate::NO_CANCEL);
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_net_log(NetLog::Get());
  auto context = context_builder->Build();

  {
    // A chunked response with delays between chunks is used to make sure that
    // attempts by the URLRequest delegate to log information while reading the
    // body are ignored.  Since they are ignored, this test is robust against
    // the possibility of multiple reads being combined in the unlikely event
    // that it occurs.
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/chunked?waitBetweenChunks=20"),
        DEFAULT_PRIORITY, &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
  }

  auto entries = net_log_observer_.GetEntries();

  size_t log_position = 0;

  // The delegate info should only have been logged on header complete.  Other
  // times it should silently be ignored.
  log_position = ExpectLogContainsSomewhereAfter(
      entries, log_position + 1,
      NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED,
      NetLogEventPhase::BEGIN);

  log_position =
      AsyncDelegateLogger::CheckDelegateInfo(entries, log_position + 1);

  ASSERT_LT(log_position, entries.size());
  EXPECT_EQ(NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED,
            entries[log_position].type);
  EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(entries, log_position + 1,
                                             NetLogEventType::DELEGATE_INFO));
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, log_position + 1,
      NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED));
}

// Tests handling of delegate info from a URLRequest::Delegate in the case of
// an HTTP redirect.
TEST_F(URLRequestTestHTTP, URLRequestDelegateInfoOnRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  AsyncLoggingUrlRequestDelegate request_delegate(
      AsyncLoggingUrlRequestDelegate::NO_CANCEL);
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_net_log(NetLog::Get());
  auto context = context_builder->Build();

  {
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        http_test_server()->GetURL("/server-redirect?simple.html"),
        DEFAULT_PRIORITY, &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
  }

  auto entries = net_log_observer_.GetEntries();

  // Delegate info should only have been logged in OnReceivedRedirect and
  // OnResponseStarted.
  size_t log_position = 0;
  static const NetLogEventType kExpectedEvents[] = {
      NetLogEventType::URL_REQUEST_DELEGATE_RECEIVED_REDIRECT,
      NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED,
  };
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLogEventTypeToString(event));
    log_position = ExpectLogContainsSomewhereAfter(entries, log_position, event,
                                                   NetLogEventPhase::BEGIN);

    log_position =
        AsyncDelegateLogger::CheckDelegateInfo(entries, log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(event, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(entries, log_position + 1,
                                             NetLogEventType::DELEGATE_INFO));
}

// Tests handling of delegate info from a URLRequest::Delegate in the case of
// an HTTP redirect, with cancellation at various points.
TEST_F(URLRequestTestHTTP, URLRequestDelegateOnRedirectCancelled) {
  ASSERT_TRUE(http_test_server()->Start());

  const AsyncLoggingUrlRequestDelegate::CancelStage kCancelStages[] = {
      AsyncLoggingUrlRequestDelegate::CANCEL_ON_RECEIVED_REDIRECT,
      AsyncLoggingUrlRequestDelegate::CANCEL_ON_RESPONSE_STARTED,
      AsyncLoggingUrlRequestDelegate::CANCEL_ON_READ_COMPLETED,
  };

  for (auto cancel_stage : kCancelStages) {
    AsyncLoggingUrlRequestDelegate request_delegate(cancel_stage);
    RecordingNetLogObserver net_log_observer;
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_net_log(NetLog::Get());
    auto context = context_builder->Build();

    {
      std::unique_ptr<URLRequest> r(context->CreateRequest(
          http_test_server()->GetURL("/server-redirect?simple.html"),
          DEFAULT_PRIORITY, &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
      LoadStateWithParam load_state = r->GetLoadState();
      r->Start();
      request_delegate.RunUntilComplete();
      EXPECT_EQ(ERR_ABORTED, request_delegate.request_status());

      // Spin the message loop to run AsyncDelegateLogger task(s) posted after
      // the |request_delegate| completion task.
      base::RunLoop().RunUntilIdle();
    }

    auto entries = net_log_observer.GetEntries();

    // Delegate info is always logged in both OnReceivedRedirect and
    // OnResponseStarted.  In the CANCEL_ON_RECEIVED_REDIRECT, the
    // OnResponseStarted delegate call is after cancellation, but logging is
    // still currently supported in that call.
    size_t log_position = 0;
    static const NetLogEventType kExpectedEvents[] = {
        NetLogEventType::URL_REQUEST_DELEGATE_RECEIVED_REDIRECT,
        NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED,
    };
    for (NetLogEventType event : kExpectedEvents) {
      SCOPED_TRACE(NetLogEventTypeToString(event));
      log_position = ExpectLogContainsSomewhereAfter(
          entries, log_position, event, NetLogEventPhase::BEGIN);

      log_position =
          AsyncDelegateLogger::CheckDelegateInfo(entries, log_position + 1);

      ASSERT_LT(log_position, entries.size());
      EXPECT_EQ(event, entries[log_position].type);
      EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);
    }

    EXPECT_FALSE(LogContainsEntryWithTypeAfter(entries, log_position + 1,
                                               NetLogEventType::DELEGATE_INFO));
  }
}

namespace {

const char kExtraHeader[] = "Allow-Snafu";
const char kExtraValue[] = "fubar";

class RedirectWithAdditionalHeadersDelegate : public TestDelegate {
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    TestDelegate::OnReceivedRedirect(request, redirect_info, defer_redirect);
    request->SetExtraRequestHeaderByName(kExtraHeader, kExtraValue, false);
  }
};

}  // namespace

TEST_F(URLRequestTestHTTP, RedirectWithAdditionalHeadersTest) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL destination_url =
      http_test_server()->GetURL("/echoheader?" + std::string(kExtraHeader));
  GURL original_url =
      http_test_server()->GetURL("/server-redirect?" + destination_url.spec());
  RedirectWithAdditionalHeadersDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  const HttpRequestHeaders& headers = req->extra_request_headers();
  EXPECT_EQ(kExtraValue, headers.GetHeader(kExtraHeader));
  EXPECT_FALSE(req->is_pending());
  EXPECT_FALSE(req->is_redirecting());
  EXPECT_EQ(kExtraValue, d.data_received());
}

namespace {

const char kExtraHeaderToRemove[] = "To-Be-Removed";

class RedirectWithHeaderRemovalDelegate : public TestDelegate {
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    TestDelegate::OnReceivedRedirect(request, redirect_info, defer_redirect);
    request->RemoveRequestHeaderByName(kExtraHeaderToRemove);
  }
};

}  // namespace

TEST_F(URLRequestTestHTTP, RedirectWithHeaderRemovalTest) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL destination_url = http_test_server()->GetURL(
      "/echoheader?" + std::string(kExtraHeaderToRemove));
  GURL original_url =
      http_test_server()->GetURL("/server-redirect?" + destination_url.spec());
  RedirectWithHeaderRemovalDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetExtraRequestHeaderByName(kExtraHeaderToRemove, "dummy", false);
  req->Start();
  d.RunUntilComplete();

  const HttpRequestHeaders& headers = req->extra_request_headers();
  EXPECT_FALSE(headers.GetHeader(kExtraHeaderToRemove));
  EXPECT_FALSE(req->is_pending());
  EXPECT_FALSE(req->is_redirecting());
  EXPECT_EQ("None", d.data_received());
}

TEST_F(URLRequestTestHTTP, CancelAfterStart) {
  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        GURL("http://www.google.com/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    r->Cancel();

    d.RunUntilComplete();

    // We expect to receive OnResponseStarted even though the request has been
    // cancelled.
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }
}

TEST_F(URLRequestTestHTTP, CancelInResponseStarted) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    d.set_cancel_in_response_started(true);

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(ERR_ABORTED, d.request_status());
  }
}

TEST_F(URLRequestTestHTTP, CancelOnDataReceived) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    d.set_cancel_in_received_data(true);

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_NE(0, d.received_bytes_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(ERR_ABORTED, d.request_status());
  }
}

TEST_F(URLRequestTestHTTP, CancelDuringEofRead) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    // This returns an empty response (With headers).
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    d.set_cancel_in_received_data(true);

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.received_bytes_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(ERR_ABORTED, d.request_status());
  }
}

TEST_F(URLRequestTestHTTP, CancelByDestroyingAfterStart) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    // The request will be implicitly canceled when it is destroyed. The
    // test delegate must not post a quit message when this happens because
    // this test doesn't actually have a message loop. The quit message would
    // get put on this thread's message queue and the next test would exit
    // early, causing problems.
    d.set_on_complete(base::DoNothing());
  }
  // expect things to just cleanup properly.

  // we won't actually get a received response here because we've never run the
  // message loop
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_EQ(0, d.bytes_received());
}

TEST_F(URLRequestTestHTTP, CancelWhileReadingFromCache) {
  ASSERT_TRUE(http_test_server()->Start());

  // populate cache
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/cachetime"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();
    EXPECT_EQ(OK, d.request_status());
  }

  // cancel read from cache (see bug 990242)
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/cachetime"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    r->Cancel();
    d.RunUntilComplete();

    EXPECT_EQ(ERR_ABORTED, d.request_status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }
}

TEST_F(URLRequestTestHTTP, PostTest) {
  ASSERT_TRUE(http_test_server()->Start());
  HTTPUploadDataOperationTest("POST");
}

TEST_F(URLRequestTestHTTP, PutTest) {
  ASSERT_TRUE(http_test_server()->Start());
  HTTPUploadDataOperationTest("PUT");
}

TEST_F(URLRequestTestHTTP, PostEmptyTest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/echo"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method("POST");

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    ASSERT_EQ(1, d.response_started_count())
        << "request failed. Error: " << d.request_status();

    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_TRUE(d.data_received().empty());
  }
}

TEST_F(URLRequestTestHTTP, PostFileTest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/echo"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method("POST");

    base::FilePath dir;
    base::PathService::Get(base::DIR_EXE, &dir);
    base::SetCurrentDirectory(dir);

    std::vector<std::unique_ptr<UploadElementReader>> element_readers;

    base::FilePath path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
    path = path.Append(kTestFilePath);
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));
    element_readers.push_back(std::make_unique<UploadFileElementReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), path, 0,
        std::numeric_limits<uint64_t>::max(), base::Time()));
    r->set_upload(std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    std::optional<int64_t> size64 = base::GetFileSize(path);
    ASSERT_TRUE(size64.has_value());
    ASSERT_LE(size64.value(), std::numeric_limits<int>::max());
    int size = static_cast<int>(size64.value());
    auto buf = std::make_unique<char[]>(size);

    ASSERT_EQ(size, base::ReadFile(path, buf.get(), size));

    ASSERT_EQ(1, d.response_started_count())
        << "request failed. Error: " << d.request_status();

    EXPECT_FALSE(d.received_data_before_response());

    EXPECT_EQ(size, d.bytes_received());
    EXPECT_EQ(std::string(&buf[0], size), d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, PostUnreadableFileTest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/echo"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method("POST");

    std::vector<std::unique_ptr<UploadElementReader>> element_readers;

    element_readers.push_back(std::make_unique<UploadFileElementReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        base::FilePath(FILE_PATH_LITERAL(
            "c:\\path\\to\\non\\existant\\file.randomness.12345")),
        0, std::numeric_limits<uint64_t>::max(), base::Time()));
    r->set_upload(std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_TRUE(d.request_failed());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_EQ(ERR_FILE_NOT_FOUND, d.request_status());
  }
}

namespace {

// Adds a standard set of data to an upload for chunked upload integration
// tests.
void AddDataToUpload(ChunkedUploadDataStream::Writer* writer) {
  const auto append = [writer](std::string_view str, bool is_done) {
    writer->AppendData(base::as_byte_span(str), is_done);
  };
  append("a", false);
  append("bcd", false);
  append("this is a longer chunk than before.", false);
  append("\r\n\r\n", false);
  append("0", false);
  append("2323", true);
}

// Checks that the upload data added in AddChunksToUpload() was echoed back from
// the server.
void VerifyReceivedDataMatchesChunks(URLRequest* r, TestDelegate* d) {
  // This should match the chunks sent by AddChunksToUpload().
  const std::string expected_data =
      "abcdthis is a longer chunk than before.\r\n\r\n02323";

  ASSERT_EQ(1, d->response_started_count())
      << "request failed. Error: " << d->request_status();

  EXPECT_FALSE(d->received_data_before_response());

  EXPECT_EQ(expected_data.size(), static_cast<size_t>(d->bytes_received()));
  EXPECT_EQ(expected_data, d->data_received());
}

}  // namespace

TEST_F(URLRequestTestHTTP, TestPostChunkedDataBeforeStart) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/echo"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    auto upload_data_stream = std::make_unique<ChunkedUploadDataStream>(0);
    std::unique_ptr<ChunkedUploadDataStream::Writer> writer =
        upload_data_stream->CreateWriter();
    r->set_upload(std::move(upload_data_stream));
    r->set_method("POST");
    AddDataToUpload(writer.get());
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    VerifyReceivedDataMatchesChunks(r.get(), &d);
  }
}

TEST_F(URLRequestTestHTTP, TestPostChunkedDataJustAfterStart) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/echo"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    auto upload_data_stream = std::make_unique<ChunkedUploadDataStream>(0);
    std::unique_ptr<ChunkedUploadDataStream::Writer> writer =
        upload_data_stream->CreateWriter();
    r->set_upload(std::move(upload_data_stream));
    r->set_method("POST");
    r->Start();
    EXPECT_TRUE(r->is_pending());
    AddDataToUpload(writer.get());
    d.RunUntilComplete();

    VerifyReceivedDataMatchesChunks(r.get(), &d);
  }
}

TEST_F(URLRequestTestHTTP, TestPostChunkedDataAfterStart) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/echo"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    auto upload_data_stream = std::make_unique<ChunkedUploadDataStream>(0);
    std::unique_ptr<ChunkedUploadDataStream::Writer> writer =
        upload_data_stream->CreateWriter();
    r->set_upload(std::move(upload_data_stream));
    r->set_method("POST");
    r->Start();
    EXPECT_TRUE(r->is_pending());

    // Pump messages until we start sending headers..
    base::RunLoop().RunUntilIdle();

    // And now wait for completion.
    base::RunLoop run_loop;
    d.set_on_complete(run_loop.QuitClosure());
    AddDataToUpload(writer.get());
    run_loop.Run();

    VerifyReceivedDataMatchesChunks(r.get(), &d);
  }
}

TEST_F(URLRequestTestHTTP, ResponseHeadersTest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/with-headers.html"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  const HttpResponseHeaders* headers = req->response_headers();

  // Simple sanity check that response_info() accesses the same data.
  EXPECT_EQ(headers, req->response_info().headers.get());

  std::string header;
  EXPECT_TRUE(headers->GetNormalizedHeader("cache-control", &header));
  EXPECT_EQ("private", header);

  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("content-type", &header));
  EXPECT_EQ("text/html; charset=ISO-8859-1", header);

  // The response has two "X-Multiple-Entries" headers.
  // This verfies our output has them concatenated together.
  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("x-multiple-entries", &header));
  EXPECT_EQ("a, b", header);
}

// TODO(svaldez): iOS tests are flaky with EmbeddedTestServer and transport
// security state. (see http://crbug.com/550977).
#if !BUILDFLAG(IS_IOS)
TEST_F(URLRequestTestHTTP, ProcessSTS) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  std::string test_server_hostname = https_test_server.GetURL("/").host();
  TestDelegate d;
  std::unique_ptr<URLRequest> request(default_context().CreateRequest(
      https_test_server.GetURL("/hsts-headers.html"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  TransportSecurityState* security_state =
      default_context().transport_security_state();
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(
      security_state->GetDynamicSTSState(test_server_hostname, &sts_state));
  EXPECT_FALSE(
      security_state->GetDynamicPKPState(test_server_hostname, &pkp_state));
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_FALSE(pkp_state.include_subdomains);
#if BUILDFLAG(IS_ANDROID)
  // Android's CertVerifyProc does not (yet) handle pins.
#else
  EXPECT_FALSE(pkp_state.HasPublicKeyPins());
#endif
}

TEST_F(URLRequestTestHTTP, STSNotProcessedOnIP) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());
  // Make sure this test fails if the test server is changed to not
  // listen on an IP by default.
  ASSERT_TRUE(https_test_server.GetURL("/").HostIsIPAddress());
  std::string test_server_hostname = https_test_server.GetURL("/").host();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(default_context().CreateRequest(
      https_test_server.GetURL("/hsts-headers.html"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();
  TransportSecurityState* security_state =
      default_context().transport_security_state();
  TransportSecurityState::STSState sts_state;
  EXPECT_FALSE(
      security_state->GetDynamicSTSState(test_server_hostname, &sts_state));
}

TEST_F(URLRequestTestHTTP, PKPBypassRecorded) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  // Set up a MockCertVerifier to be a local root that violates the pin
  scoped_refptr<X509Certificate> cert = https_test_server.GetCertificate();
  ASSERT_TRUE(cert);

  CertVerifyResult verify_result;
  verify_result.verified_cert = cert;
  verify_result.is_issued_by_known_root = false;
  HashValue hash;
  ASSERT_TRUE(
      hash.FromString("sha256/1111111111111111111111111111111111111111111="));
  verify_result.public_key_hashes.push_back(hash);
  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->AddResultForCert(cert.get(), verify_result, OK);

  std::string test_server_hostname = "www.example.org";

  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCertVerifier(std::move(cert_verifier));
  auto context = context_builder->Build();
  context->transport_security_state()->EnableStaticPinsForTesting();
  context->transport_security_state()->SetPinningListAlwaysTimelyForTesting(
      true);

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      https_test_server.GetURL(test_server_hostname, "/simple.html"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->set_isolation_info(IsolationInfo::CreateTransient());
  request->Start();
  d.RunUntilComplete();

  // Check that the request succeeded and that PKP was bypassed.
  EXPECT_EQ(OK, d.request_status());
  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(context->transport_security_state()->GetStaticPKPState(
      test_server_hostname, &pkp_state));
  EXPECT_TRUE(pkp_state.HasPublicKeyPins());
  EXPECT_TRUE(request->ssl_info().pkp_bypassed);
}

TEST_F(URLRequestTestHTTP, ProcessSTSOnce) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  std::string test_server_hostname = https_test_server.GetURL("/").host();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(default_context().CreateRequest(
      https_test_server.GetURL("/hsts-multiple-headers.html"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  // We should have set parameters from the first header, not the second.
  TransportSecurityState* security_state =
      default_context().transport_security_state();
  TransportSecurityState::STSState sts_state;
  EXPECT_TRUE(
      security_state->GetDynamicSTSState(test_server_hostname, &sts_state));
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_FALSE(sts_state.include_subdomains);
}

#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(ENABLE_REPORTING)

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_DontReportIfNetworkNotAccessed) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_test_server);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/cachetime");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  // Populate the cache.
  TestDelegate d;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->set_isolation_info(isolation_info1_);
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error.uri);
  EXPECT_EQ(200, error.status_code);
  EXPECT_EQ(OK, error.type);

  request = context->CreateRequest(request_url, DEFAULT_PRIORITY, &d,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  request->set_isolation_info(isolation_info1_);
  request->Start();
  d.RunUntilComplete();

  EXPECT_FALSE(request->response_info().network_accessed);
  EXPECT_TRUE(request->response_info().was_cached);
  // No additional NEL report was generated.
  EXPECT_EQ(1u, nel_service.errors().size());
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_BasicSuccess) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/simple.html");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error.uri);
  EXPECT_EQ(200, error.status_code);
  EXPECT_EQ(OK, error.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_BasicError) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_test_server);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/close-socket");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error.uri);
  EXPECT_EQ(0, error.status_code);
  EXPECT_EQ(ERR_EMPTY_RESPONSE, error.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_Redirect) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/redirect-test.html");
  GURL redirect_url = https_test_server.GetURL("/with-headers.html");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(2u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error1 =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error1.uri);
  EXPECT_EQ(302, error1.status_code);
  EXPECT_EQ(OK, error1.type);
  const TestNetworkErrorLoggingService::RequestDetails& error2 =
      nel_service.errors()[1];
  EXPECT_EQ(redirect_url, error2.uri);
  EXPECT_EQ(200, error2.status_code);
  EXPECT_EQ(OK, error2.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_RedirectWithoutLocationHeader) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/308-without-location-header");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error.uri);
  EXPECT_EQ(308, error.status_code);
  // The body of the response was successfully read.
  EXPECT_EQ(OK, error.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_Auth) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_test_server);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/auth-basic");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  d.set_credentials(AuthCredentials(kUser, kSecret));
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(2u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error1 =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error1.uri);
  EXPECT_EQ(401, error1.status_code);
  EXPECT_EQ(OK, error1.type);
  const TestNetworkErrorLoggingService::RequestDetails& error2 =
      nel_service.errors()[1];
  EXPECT_EQ(request_url, error2.uri);
  EXPECT_EQ(200, error2.status_code);
  EXPECT_EQ(OK, error2.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_304Response) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_test_server);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/auth-basic");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  // populate the cache
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_isolation_info(isolation_info1_);
    r->Start();
    d.RunUntilComplete();
  }
  ASSERT_EQ(2u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error1 =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error1.uri);
  EXPECT_EQ(401, error1.status_code);
  EXPECT_EQ(OK, error1.type);
  const TestNetworkErrorLoggingService::RequestDetails& error2 =
      nel_service.errors()[1];
  EXPECT_EQ(request_url, error2.uri);
  EXPECT_EQ(200, error2.status_code);
  EXPECT_EQ(OK, error2.type);

  // repeat request with end-to-end validation.  since auth-basic results in a
  // cachable page, we expect this test to result in a 304.  in which case, the
  // response should be fetched from the cache.
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));
    std::unique_ptr<URLRequest> r(context->CreateRequest(
        request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetLoadFlags(LOAD_VALIDATE_CACHE);
    r->set_isolation_info(isolation_info1_);
    r->Start();
    d.RunUntilComplete();

    // Should be the same cached document.
    EXPECT_TRUE(r->was_cached());
  }
  ASSERT_EQ(3u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error3 =
      nel_service.errors()[2];
  EXPECT_EQ(request_url, error3.uri);
  EXPECT_EQ(304, error3.status_code);
  EXPECT_EQ(OK, error3.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_CancelInResponseStarted) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/simple.html");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  d.set_cancel_in_response_started(true);
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error.uri);
  EXPECT_EQ(200, error.status_code);
  // Headers were received and the body should have been read but was not.
  EXPECT_EQ(ERR_ABORTED, error.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_CancelOnDataReceived) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/simple.html");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  d.set_cancel_in_received_data(true);
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error.uri);
  EXPECT_EQ(200, error.status_code);
  // Data was received but the body was not completely read.
  EXPECT_EQ(ERR_ABORTED, error.type);
}

TEST_F(URLRequestTestHTTP, NetworkErrorLogging_CancelRedirect) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/redirect-test.html");

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_network_error_logging_enabled(true);
  auto& nel_service = *context_builder->SetNetworkErrorLoggingServiceForTesting(
      std::make_unique<TestNetworkErrorLoggingService>());
  auto context = context_builder->Build();

  TestDelegate d;
  d.set_cancel_in_received_redirect(true);
  std::unique_ptr<URLRequest> request(context->CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  const TestNetworkErrorLoggingService::RequestDetails& error =
      nel_service.errors()[0];
  EXPECT_EQ(request_url, error.uri);
  EXPECT_EQ(302, error.status_code);
  // A valid HTTP response was received, even though the request was cancelled.
  EXPECT_EQ(OK, error.type);
}

#endif  // BUILDFLAG(ENABLE_REPORTING)

TEST_F(URLRequestTestHTTP, ContentTypeNormalizationTest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/content-type-normalization.html"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  std::string mime_type;
  req->GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  std::string charset;
  req->GetCharset(&charset);
  EXPECT_EQ("utf-8", charset);
  req->Cancel();
}

TEST_F(URLRequestTestHTTP, FileRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/redirect-to-file.html"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, d.request_status());
  EXPECT_EQ(1, d.received_redirect_count());
}

TEST_F(URLRequestTestHTTP, DataRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/redirect-to-data.html"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, d.request_status());
  EXPECT_EQ(1, d.received_redirect_count());
}

TEST_F(URLRequestTestHTTP, RestrictUnsafeRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL(
          "/server-redirect?unsafe://here-there-be-dragons"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(ERR_UNSAFE_REDIRECT, d.request_status());

  // The redirect should have been rejected before reporting it to the
  // caller. See https://crbug.com/723796
  EXPECT_EQ(0, d.received_redirect_count());
}

// Test that redirects to invalid URLs are rejected. See
// https://crbug.com/462272.
TEST_F(URLRequestTestHTTP, RedirectToInvalidURL) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/redirect-to-invalid-url.html"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(ERR_INVALID_REDIRECT, d.request_status());

  // The redirect should have been rejected before reporting it to the caller.
  EXPECT_EQ(0, d.received_redirect_count());
}

// Make sure redirects are cached, despite not reading their bodies.
TEST_F(URLRequestTestHTTP, CacheRedirect) {
  ASSERT_TRUE(http_test_server()->Start());
  GURL redirect_url =
      http_test_server()->GetURL("/redirect302-to-echo-cacheable");

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        redirect_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(isolation_info1_);
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(http_test_server()->GetURL("/echo"), req->url());
  }

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        redirect_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(isolation_info1_);
    req->Start();
    d.RunUntilRedirect();

    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(0, d.response_started_count());
    EXPECT_TRUE(req->was_cached());

    req->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                                std::nullopt /* modified_headers */);
    d.RunUntilComplete();
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(http_test_server()->GetURL("/echo"), req->url());
  }
}

// Make sure a request isn't cached when a NetworkDelegate forces a redirect
// when the headers are read, since the body won't have been read.
TEST_F(URLRequestTestHTTP, NoCacheOnNetworkDelegateRedirect) {
  ASSERT_TRUE(http_test_server()->Start());
  // URL that is normally cached.
  GURL initial_url = http_test_server()->GetURL("/cachetime");

  {
    // Set up the TestNetworkDelegate tp force a redirect.
    GURL redirect_to_url = http_test_server()->GetURL("/echo");
    default_network_delegate().set_redirect_on_headers_received_url(
        redirect_to_url);

    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        initial_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(redirect_to_url, req->url());
  }

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        initial_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_FALSE(req->was_cached());
    EXPECT_EQ(0, d.received_redirect_count());
    EXPECT_EQ(initial_url, req->url());
  }
}

// Check that |preserve_fragment_on_redirect_url| is respected.
TEST_F(URLRequestTestHTTP, PreserveFragmentOnRedirectUrl) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL original_url(http_test_server()->GetURL("/original#fragment1"));
  GURL preserve_fragement_url(http_test_server()->GetURL("/echo"));

  default_network_delegate().set_redirect_on_headers_received_url(
      preserve_fragement_url);
  default_network_delegate().set_preserve_fragment_on_redirect_url(
      preserve_fragement_url);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(preserve_fragement_url, r->url());
  }
}

// Check that |preserve_fragment_on_redirect_url| has no effect when it doesn't
// match the URL being redirected to.
TEST_F(URLRequestTestHTTP, PreserveFragmentOnRedirectUrlMismatch) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL original_url(http_test_server()->GetURL("/original#fragment1"));
  GURL preserve_fragement_url(http_test_server()->GetURL("/echo#fragment2"));
  GURL redirect_url(http_test_server()->GetURL("/echo"));
  GURL expected_url(http_test_server()->GetURL("/echo#fragment1"));

  default_network_delegate().set_redirect_on_headers_received_url(redirect_url);
  default_network_delegate().set_preserve_fragment_on_redirect_url(
      preserve_fragement_url);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(expected_url, r->url());
  }
}

// When a URLRequestRedirectJob is created, the redirection must be followed and
// the reference fragment of the target URL must not be modified.
TEST_F(URLRequestTestHTTP, RedirectJobWithReferenceFragment) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL original_url(
      http_test_server()->GetURL("/original#should-not-be-appended"));
  GURL redirect_url(http_test_server()->GetURL("/echo"));

  TestDelegate d;
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  std::unique_ptr<URLRequestRedirectJob> job =
      std::make_unique<URLRequestRedirectJob>(
          r.get(), redirect_url, RedirectUtil::ResponseCode::REDIRECT_302_FOUND,
          "Very Good Reason");
  TestScopedURLInterceptor interceptor(r->url(), std::move(job));

  r->Start();
  d.RunUntilComplete();

  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(original_url, r->original_url());
  EXPECT_EQ(redirect_url, r->url());
}

TEST_F(URLRequestTestHTTP, UnsupportedReferrerScheme) {
  ASSERT_TRUE(http_test_server()->Start());

  const std::string referrer("foobar://totally.legit.referrer");
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer(referrer);
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(std::string("None"), d.data_received());
}

TEST_F(URLRequestTestHTTP, NoUserPassInReferrer) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer("http://user:pass@foo.com/");
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(std::string("http://foo.com/"), d.data_received());
}

TEST_F(URLRequestTestHTTP, NoFragmentInReferrer) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer("http://foo.com/test#fragment");
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(std::string("http://foo.com/test"), d.data_received());
}

TEST_F(URLRequestTestHTTP, EmptyReferrerAfterValidReferrer) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer("http://foo.com/test#fragment");
  req->SetReferrer("");
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(std::string("None"), d.data_received());
}

TEST_F(URLRequestTestHTTP, CapRefererHeaderLength) {
  ASSERT_TRUE(http_test_server()->Start());

  // Verify that referrers over 4k are stripped to an origin, and referrers at
  // or under 4k are unmodified.
  {
    std::string original_header = "http://example.com/";
    original_header.resize(4097, 'a');

    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetReferrer(original_header);
    req->Start();
    d.RunUntilComplete();

    // The request's referrer will be stripped since (1) there will be a
    // mismatch between the request's referrer and the output of
    // URLRequestJob::ComputeReferrerForPolicy and (2) the delegate, when
    // offered the opportunity to cancel the request for this reason, will
    // decline.
    EXPECT_EQ("None", d.data_received());
  }
  {
    std::string original_header = "http://example.com/";
    original_header.resize(4096, 'a');

    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetReferrer(original_header);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(original_header, d.data_received());
  }
  {
    std::string original_header = "http://example.com/";
    original_header.resize(4095, 'a');

    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/echoheader?Referer"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetReferrer(original_header);
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(original_header, d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, CancelRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    d.set_cancel_in_received_redirect(true);
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/redirect-test.html"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(ERR_ABORTED, d.request_status());
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    GURL test_url(http_test_server()->GetURL("/redirect-test.html"));
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    req->Start();
    d.RunUntilRedirect();

    EXPECT_EQ(1, d.received_redirect_count());

    req->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                                std::nullopt /* modified_headers */);
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(OK, d.request_status());

    base::FilePath path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
    path = path.Append(kTestFilePath);
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));

    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect_ModifiedHeaders) {
  test_server::HttpRequest http_request;
  int num_observed_requests = 0;
  http_test_server()->RegisterRequestMonitor(
      base::BindLambdaForTesting([&](const test_server::HttpRequest& request) {
        http_request = request;
        ++num_observed_requests;
      }));
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    GURL test_url(http_test_server()->GetURL("/redirect-test.html"));
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    // Set initial headers for the request.
    req->SetExtraRequestHeaderByName("Header1", "Value1", true /* overwrite */);
    req->SetExtraRequestHeaderByName("Header2", "Value2", true /* overwrite */);

    req->Start();
    d.RunUntilRedirect();

    // Initial request should only have initial headers.
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(1, num_observed_requests);
    EXPECT_EQ("Value1", http_request.headers["Header1"]);
    EXPECT_EQ("Value2", http_request.headers["Header2"]);
    EXPECT_EQ(0u, http_request.headers.count("Header3"));

    // Overwrite Header2 and add Header3.
    net::HttpRequestHeaders modified_headers;
    modified_headers.SetHeader("Header2", "");
    modified_headers.SetHeader("Header3", "Value3");

    req->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                                modified_headers);
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(OK, d.request_status());

    // Redirected request should also have modified headers.
    EXPECT_EQ(2, num_observed_requests);
    EXPECT_EQ("Value1", http_request.headers["Header1"]);
    EXPECT_EQ(1u, http_request.headers.count("Header2"));
    EXPECT_EQ("", http_request.headers["Header2"]);
    EXPECT_EQ("Value3", http_request.headers["Header3"]);
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect_RemovedHeaders) {
  test_server::HttpRequest http_request;
  int num_observed_requests = 0;
  http_test_server()->RegisterRequestMonitor(
      base::BindLambdaForTesting([&](const test_server::HttpRequest& request) {
        http_request = request;
        ++num_observed_requests;
      }));
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    GURL test_url(http_test_server()->GetURL("/redirect-test.html"));
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    // Set initial headers for the request.
    req->SetExtraRequestHeaderByName("Header1", "Value1", true /* overwrite */);
    req->SetExtraRequestHeaderByName("Header2", "Value2", true /* overwrite */);

    req->Start();
    d.RunUntilRedirect();

    // Initial request should have initial headers.
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(1, num_observed_requests);
    EXPECT_EQ("Value1", http_request.headers["Header1"]);
    EXPECT_EQ("Value2", http_request.headers["Header2"]);

    // Keep Header1 and remove Header2.
    std::vector<std::string> removed_headers({"Header2"});
    req->FollowDeferredRedirect(removed_headers,
                                std::nullopt /* modified_headers */);
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(OK, d.request_status());

    // Redirected request should have modified headers.
    EXPECT_EQ(2, num_observed_requests);
    EXPECT_EQ("Value1", http_request.headers["Header1"]);
    EXPECT_EQ(0u, http_request.headers.count("Header2"));
  }
}

TEST_F(URLRequestTestHTTP, CancelDeferredRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/redirect-test.html"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilRedirect();

    EXPECT_EQ(1, d.received_redirect_count());

    req->Cancel();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(ERR_ABORTED, d.request_status());
  }
}

TEST_F(URLRequestTestHTTP, VaryHeader) {
  ASSERT_TRUE(http_test_server()->Start());

  // Populate the cache.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/echoheadercache?foo"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "1");
    req->SetExtraRequestHeaders(headers);
    req->set_isolation_info(isolation_info1_);
    req->Start();
    d.RunUntilComplete();

    LoadTimingInfo load_timing_info;
    req->GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
  }

  // Expect a cache hit.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/echoheadercache?foo"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "1");
    req->SetExtraRequestHeaders(headers);
    req->set_isolation_info(isolation_info1_);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(req->was_cached());

    LoadTimingInfo load_timing_info;
    req->GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingCacheHitNoNetwork(load_timing_info);
  }

  // Expect a cache miss.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/echoheadercache?foo"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    HttpRequestHeaders headers;
    headers.SetHeader("foo", "2");
    req->SetExtraRequestHeaders(headers);
    req->set_isolation_info(isolation_info1_);
    req->Start();
    d.RunUntilComplete();

    EXPECT_FALSE(req->was_cached());

    LoadTimingInfo load_timing_info;
    req->GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
  }
}

TEST_F(URLRequestTestHTTP, BasicAuth) {
  ASSERT_TRUE(http_test_server()->Start());

  // populate the cache
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_isolation_info(isolation_info1_);
    r->Start();

    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);
  }

  // repeat request with end-to-end validation.  since auth-basic results in a
  // cachable page, we expect this test to result in a 304.  in which case, the
  // response should be fetched from the cache.
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetLoadFlags(LOAD_VALIDATE_CACHE);
    r->set_isolation_info(isolation_info1_);
    r->Start();

    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Should be the same cached document.
    EXPECT_TRUE(r->was_cached());
  }
}

// Check that Set-Cookie headers in 401 responses are respected.
// http://crbug.com/6450
TEST_F(URLRequestTestHTTP, BasicAuthWithCookies) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL url_requiring_auth =
      http_test_server()->GetURL("/auth-basic?set-cookie-if-challenged");

  // Request a page that will give a 401 containing a Set-Cookie header.
  // Verify that when the transaction is restarted, it includes the new cookie.
  TestDelegate d;
  d.set_credentials(AuthCredentials(kUser, kSecret));

  std::unique_ptr<URLRequest> r =
      CreateFirstPartyRequest(default_context(), url_requiring_auth, &d);
  r->Start();

  d.RunUntilComplete();

  EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

  // Make sure we sent the cookie in the restarted transaction.
  EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true") !=
              std::string::npos);
}

// Same test as above, except this time the restart is initiated earlier
// (without user intervention since identity is embedded in the URL).
TEST_F(URLRequestTestHTTP, BasicAuthWithCredentialsWithCookies) {
  ASSERT_TRUE(http_test_server()->Start());
  GURL url_requiring_auth =
      http_test_server()->GetURL("/auth-basic?set-cookie-if-challenged");
  GURL::Replacements replacements;
  replacements.SetUsernameStr("user2");
  replacements.SetPasswordStr("secret");
  GURL url_with_identity = url_requiring_auth.ReplaceComponents(replacements);

  TestDelegate d;

  std::unique_ptr<URLRequest> r =
      CreateFirstPartyRequest(default_context(), url_with_identity, &d);
  r->Start();

  d.RunUntilComplete();

  EXPECT_TRUE(d.data_received().find("user2/secret") != std::string::npos);

  // Make sure we sent the cookie in the restarted transaction.
  EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true") !=
              std::string::npos);
}

TEST_F(URLRequestTestHTTP, BasicAuthWithCookiesCancelAuth) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL url_requiring_auth =
      http_test_server()->GetURL("/auth-basic?set-cookie-if-challenged");

  // Request a page that will give a 401 containing a Set-Cookie header.
  // Verify that cookies are set before credentials are provided, and then
  // cancelling auth does not result in setting the cookies again.
  TestDelegate d;

  EXPECT_TRUE(GetAllCookies(&default_context()).empty());

  std::unique_ptr<URLRequest> r =
      CreateFirstPartyRequest(default_context(), url_requiring_auth, &d);
  r->Start();
  d.RunUntilAuthRequired();

  // Cookie should have been set.
  EXPECT_EQ(1, default_network_delegate().set_cookie_count());
  CookieList cookies = GetAllCookies(&default_context());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("got_challenged", cookies[0].Name());
  EXPECT_EQ("true", cookies[0].Value());

  // Delete cookie.
  default_context().cookie_store()->DeleteAllAsync(
      CookieStore::DeleteCallback());

  // Cancel auth and continue the request.
  r->CancelAuth();
  d.RunUntilComplete();
  ASSERT_TRUE(r->response_headers());
  EXPECT_EQ(401, r->response_headers()->response_code());

  // Cookie should not have been set again.
  EXPECT_TRUE(GetAllCookies(&default_context()).empty());
  EXPECT_EQ(1, default_network_delegate().set_cookie_count());
}

// Tests the IsolationInfo is updated approiately on redirect.
TEST_F(URLRequestTestHTTP, IsolationInfoUpdatedOnRedirect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kSplitCacheByNetworkIsolationKey);

  ASSERT_TRUE(http_test_server()->Start());

  GURL redirect_url =
      http_test_server()->GetURL("redirected.test", "/cachetime");
  GURL original_url = http_test_server()->GetURL(
      "original.test", "/server-redirect?" + redirect_url.spec());

  url::Origin original_origin = url::Origin::Create(original_url);
  url::Origin redirect_origin = url::Origin::Create(redirect_url);

  // Since transient IsolationInfos use opaque origins, need to create a single
  // consistent transient origin one for be used as the original and updated
  // info in the same test case.
  IsolationInfo transient_isolation_info = IsolationInfo::CreateTransient();

  const struct {
    IsolationInfo info_before_redirect;
    IsolationInfo expected_info_after_redirect;
  } kTestCases[] = {
      {IsolationInfo(), IsolationInfo()},
      {IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame,
                             original_origin, original_origin,
                             SiteForCookies()),
       IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame,
                             redirect_origin, redirect_origin,
                             SiteForCookies::FromOrigin(redirect_origin))},
      {IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame,
                             original_origin, original_origin,
                             SiteForCookies::FromOrigin(original_origin)),
       IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame,
                             original_origin, redirect_origin,
                             SiteForCookies::FromOrigin(original_origin))},
      {IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                             original_origin, original_origin,
                             SiteForCookies()),
       IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                             original_origin, original_origin,
                             SiteForCookies())},
      {transient_isolation_info, transient_isolation_info},
  };

  for (const auto& test_case : kTestCases) {
    // Populate the cache, using the expected final IsolationInfo.
    {
      TestDelegate d;

      std::unique_ptr<URLRequest> r(default_context().CreateRequest(
          redirect_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
      r->set_isolation_info(test_case.expected_info_after_redirect);
      r->Start();
      d.RunUntilComplete();
      EXPECT_THAT(d.request_status(), IsOk());
    }

    // Send a request using the initial IsolationInfo that should be redirected
    // to the cached url, and should use the cached entry if the NIK was
    // updated, except in the case the IsolationInfo's NIK was empty.
    {
      TestDelegate d;

      std::unique_ptr<URLRequest> r(default_context().CreateRequest(
          original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
      r->set_isolation_info(test_case.info_before_redirect);
      r->Start();
      d.RunUntilComplete();
      EXPECT_THAT(d.request_status(), IsOk());
      EXPECT_EQ(redirect_url, r->url());

      EXPECT_EQ(!test_case.expected_info_after_redirect.network_isolation_key()
                     .IsTransient(),
                r->was_cached());
      EXPECT_EQ(test_case.expected_info_after_redirect.request_type(),
                r->isolation_info().request_type());
      EXPECT_EQ(test_case.expected_info_after_redirect.top_frame_origin(),
                r->isolation_info().top_frame_origin());
      EXPECT_EQ(test_case.expected_info_after_redirect.frame_origin(),
                r->isolation_info().frame_origin());
      EXPECT_EQ(test_case.expected_info_after_redirect.network_isolation_key(),
                r->isolation_info().network_isolation_key());
      EXPECT_TRUE(test_case.expected_info_after_redirect.site_for_cookies()
                      .IsEquivalent(r->isolation_info().site_for_cookies()));
    }
  }
}

// Tests that |key_auth_cache_by_network_anonymization_key| is respected.
TEST_F(URLRequestTestHTTP, AuthWithNetworkAnonymizationKey) {
  ASSERT_TRUE(http_test_server()->Start());

  for (bool key_auth_cache_by_network_anonymization_key : {false, true}) {
    auto context_builder = CreateTestURLRequestContextBuilder();
    HttpNetworkSessionParams network_session_params;
    network_session_params
        .key_auth_cache_server_entries_by_network_anonymization_key =
        key_auth_cache_by_network_anonymization_key;
    context_builder->set_http_network_session_params(network_session_params);
    auto context = context_builder->Build();

    // Populate the auth cache using one NetworkAnonymizationKey.
    {
      TestDelegate d;
      GURL url(base::StringPrintf(
          "http://%s:%s@%s/auth-basic", base::UTF16ToASCII(kUser).c_str(),
          base::UTF16ToASCII(kSecret).c_str(),
          http_test_server()->host_port_pair().ToString().c_str()));

      std::unique_ptr<URLRequest> r(context->CreateRequest(
          url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
      r->SetLoadFlags(LOAD_BYPASS_CACHE);
      r->set_isolation_info(isolation_info1_);
      r->Start();

      d.RunUntilComplete();
      EXPECT_THAT(d.request_status(), IsOk());
      ASSERT_TRUE(r->response_headers());
      EXPECT_EQ(200, r->response_headers()->response_code());
      EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);
    }

    // Make a request with another NetworkAnonymizationKey. This may or may not
    // use the cached auth credentials, depending on whether or not the
    // HttpAuthCache is configured to respect the NetworkAnonymizationKey.
    {
      TestDelegate d;

      std::unique_ptr<URLRequest> r(context->CreateRequest(
          http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY, &d,
          TRAFFIC_ANNOTATION_FOR_TESTS));
      r->SetLoadFlags(LOAD_BYPASS_CACHE);
      r->set_isolation_info(isolation_info2_);
      r->Start();

      d.RunUntilComplete();

      EXPECT_THAT(d.request_status(), IsOk());
      ASSERT_TRUE(r->response_headers());
      if (key_auth_cache_by_network_anonymization_key) {
        EXPECT_EQ(401, r->response_headers()->response_code());
      } else {
        EXPECT_EQ(200, r->response_headers()->response_code());
      }

      EXPECT_EQ(!key_auth_cache_by_network_anonymization_key,
                d.data_received().find("user/secret") != std::string::npos);
    }
  }
}

TEST_F(URLRequestTest, ReportCookieActivity) {
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  RecordingNetLogObserver net_log_observer;
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<FilteringTestNetworkDelegate>());
  network_delegate.SetCookieFilter("not_stored_cookie");
  network_delegate.set_block_annotate_cookies();
  context_builder->set_net_log(net::NetLog::Get());
  auto context = context_builder->Build();
  // Make sure cookies blocked from being stored are caught, and those that are
  // accepted are reported as well.
  GURL set_cookie_test_url = test_server.GetURL(
      "/set-cookie?not_stored_cookie=true&"
      "stored_cookie=tasty"
      "&path_cookie=narrow;path=/"
      "set-cookie&partitioned_cookie=partitioned;path=/;partitioned;secure");
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(*context, set_cookie_test_url, &d);
    req->Start();
    d.RunUntilComplete();

    ASSERT_EQ(4u, req->maybe_stored_cookies().size());
    EXPECT_EQ("not_stored_cookie",
              req->maybe_stored_cookies()[0].cookie->Name());
    EXPECT_TRUE(req->maybe_stored_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
    EXPECT_EQ("stored_cookie", req->maybe_stored_cookies()[1].cookie->Name());
    EXPECT_TRUE(
        req->maybe_stored_cookies()[1].access_result.status.IsInclude());
    EXPECT_EQ("stored_cookie", req->maybe_stored_cookies()[1].cookie->Name());
    EXPECT_TRUE(
        req->maybe_stored_cookies()[2].access_result.status.IsInclude());
    EXPECT_EQ("path_cookie", req->maybe_stored_cookies()[2].cookie->Name());
    EXPECT_TRUE(
        req->maybe_stored_cookies()[3].access_result.status.IsInclude());
    EXPECT_EQ("partitioned_cookie",
              req->maybe_stored_cookies()[3].cookie->Name());
    auto entries = net_log_observer.GetEntriesWithType(
        NetLogEventType::COOKIE_INCLUSION_STATUS);
    EXPECT_EQ(4u, entries.size());
    EXPECT_EQ("{\"domain\":\"" + set_cookie_test_url.host() +
                  R"x(","name":"not_stored_cookie","operation":"store",)x"
                  R"x("partition_key":"(none)","path":"/",)x"
                  R"x("status":"EXCLUDE_USER_PREFERENCES, )x"
                  R"x(DO_NOT_WARN, NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[0].params));
    EXPECT_EQ("{\"domain\":\"" + set_cookie_test_url.host() +
                  R"x(","name":"stored_cookie","operation":"store",)x"
                  R"x("partition_key":"(none)","path":"/",)x"
                  R"x("status":"INCLUDE, DO_NOT_WARN, NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[1].params));
    EXPECT_EQ("{\"domain\":\"" + set_cookie_test_url.host() +
                  R"x(","name":"path_cookie","operation":"store",)x"
                  R"x("partition_key":"(none)",)x"
                  R"x("path":"/set-cookie","status":"INCLUDE, DO_NOT_WARN, )x"
                  R"x(NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[2].params));
    EXPECT_EQ("{\"domain\":\"" + set_cookie_test_url.host() +
                  R"x(","name":"partitioned_cookie","operation":"store",)x"
                  R"x("partition_key":")x" +
                  set_cookie_test_url.scheme() + "://" +
                  set_cookie_test_url.host() +
                  ", same-site"
                  R"x(","path":"/","status":"INCLUDE, DO_NOT_WARN, )x"
                  R"x(NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[3].params));
    net_log_observer.Clear();
  }
  {
    TestDelegate d;
    // Make sure cookies blocked from being sent are caught.
    GURL test_url = test_server.GetURL("/echoheader?Cookie");
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(*context, test_url, &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("stored_cookie=tasty") ==
                std::string::npos);

    ASSERT_EQ(3u, req->maybe_sent_cookies().size());
    EXPECT_EQ("path_cookie", req->maybe_sent_cookies()[0].cookie.Name());
    EXPECT_TRUE(
        req->maybe_sent_cookies()[0]
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH,
                 net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
    EXPECT_EQ("stored_cookie", req->maybe_sent_cookies()[1].cookie.Name());
    EXPECT_TRUE(
        req->maybe_sent_cookies()[1]
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
    EXPECT_TRUE(
        req->maybe_sent_cookies()[2]
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
    auto entries = net_log_observer.GetEntriesWithType(
        NetLogEventType::COOKIE_INCLUSION_STATUS);
    EXPECT_EQ(3u, entries.size());
    EXPECT_EQ("{\"domain\":\"" + set_cookie_test_url.host() +
                  R"x(","name":"path_cookie","operation":"send",)x"
                  R"x("partition_key":"(none)","path":)x"
                  R"x("/set-cookie","status":"EXCLUDE_NOT_ON_PATH, )x"
                  R"x(EXCLUDE_USER_PREFERENCES, DO_NOT_WARN, NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[0].params));
    EXPECT_EQ("{\"domain\":\"" + set_cookie_test_url.host() +
                  R"x(","name":"stored_cookie","operation":"send",)x"
                  R"x("partition_key":"(none)","path":"/)x"
                  R"x(","status":"EXCLUDE_USER_PREFERENCES, DO_NOT_WARN, )x"
                  R"x(NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[1].params));
    EXPECT_EQ("{\"domain\":\"" + set_cookie_test_url.host() +
                  R"x(","name":"partitioned_cookie","operation":"send",)x"
                  R"x("partition_key":")x" +
                  set_cookie_test_url.scheme() + "://" +
                  set_cookie_test_url.host() +
                  ", same-site"
                  R"x(","path":"/)x"
                  R"x(","status":"EXCLUDE_USER_PREFERENCES, DO_NOT_WARN, )x"
                  R"x(NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[2].params));
    net_log_observer.Clear();
  }
  {
    TestDelegate d;
    // Ensure that the log does not contain cookie names when not set to collect
    // sensitive data.
    net_log_observer.SetObserverCaptureMode(NetLogCaptureMode::kDefault);

    GURL test_url = test_server.GetURL("/echoheader?Cookie");
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(*context, test_url, &d);
    req->Start();
    d.RunUntilComplete();

    auto entries = net_log_observer.GetEntriesWithType(
        NetLogEventType::COOKIE_INCLUSION_STATUS);
    EXPECT_EQ(3u, entries.size());

    // Ensure that the potentially-sensitive |name|, |domain|, and |path| fields
    // are omitted, but other fields are logged as expected.
    EXPECT_EQ(R"x({"operation":"send","partition_key":"(none)",)x"
              R"x("status":"EXCLUDE_NOT_ON_PATH, )x"
              R"x(EXCLUDE_USER_PREFERENCES, DO_NOT_WARN, NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[0].params));
    EXPECT_EQ(R"x({"operation":"send","partition_key":"(none)",)x"
              R"x("status":"EXCLUDE_USER_PREFERENCES, )x"
              R"x(DO_NOT_WARN, NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[1].params));
    EXPECT_EQ(R"x({"operation":"send","partition_key":")x" +
                  set_cookie_test_url.scheme() + "://" +
                  set_cookie_test_url.host() +
                  ", same-site"
                  R"x(","status":"EXCLUDE_USER_PREFERENCES, )x"
                  R"x(DO_NOT_WARN, NO_EXEMPTION"})x",
              SerializeNetLogValueToJson(entries[2].params));

    net_log_observer.Clear();
    net_log_observer.SetObserverCaptureMode(
        NetLogCaptureMode::kIncludeSensitive);
  }

  network_delegate.unset_block_annotate_cookies();
  {
    // Now with sending cookies re-enabled, it should actually be sent.
    TestDelegate d;
    GURL test_url = test_server.GetURL("/echoheader?Cookie");
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(*context, test_url, &d);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("stored_cookie=tasty") !=
                std::string::npos);

    ASSERT_EQ(3u, req->maybe_sent_cookies().size());
    EXPECT_EQ("path_cookie", req->maybe_sent_cookies()[0].cookie.Name());
    EXPECT_TRUE(req->maybe_sent_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {net::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH}));
    EXPECT_EQ("stored_cookie", req->maybe_sent_cookies()[1].cookie.Name());
    EXPECT_TRUE(req->maybe_sent_cookies()[1].access_result.status.IsInclude());
    auto entries = net_log_observer.GetEntriesWithType(
        NetLogEventType::COOKIE_INCLUSION_STATUS);
    EXPECT_EQ(3u, entries.size());
    EXPECT_EQ(
        "{\"domain\":\"" + set_cookie_test_url.host() +
            R"x(","name":"path_cookie","operation":"send",)x"
            R"x("partition_key":"(none)","path":"/)x"
            R"x(set-cookie","status":"EXCLUDE_NOT_ON_PATH, DO_NOT_WARN, )x"
            R"x(NO_EXEMPTION"})x",
        SerializeNetLogValueToJson(entries[0].params));
    EXPECT_EQ(
        "{\"domain\":\"" + set_cookie_test_url.host() +
            R"x(","name":"stored_cookie","operation":"send",)x"
            R"x("partition_key":"(none)",)x"
            R"x("path":"/","status":"INCLUDE, DO_NOT_WARN, NO_EXEMPTION"})x",
        SerializeNetLogValueToJson(entries[1].params));
    EXPECT_EQ(
        "{\"domain\":\"" + set_cookie_test_url.host() +
            R"x(","name":"partitioned_cookie","operation":"send",)x"
            R"x("partition_key":")x" +
            set_cookie_test_url.scheme() + "://" + set_cookie_test_url.host() +
            ", same-site"
            R"x(","path":"/","status":"INCLUDE, DO_NOT_WARN, NO_EXEMPTION"})x",
        SerializeNetLogValueToJson(entries[2].params));
    net_log_observer.Clear();
  }
}

// Test that the SameSite-by-default CookieInclusionStatus warnings do not get
// set if the cookie would have been rejected for other reasons.
// Regression test for https://crbug.com/1027318.
TEST_F(URLRequestTest, NoCookieInclusionStatusWarningIfWouldBeExcludedAnyway) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<FilteringTestNetworkDelegate>());
  network_delegate.SetCookieFilter("blockeduserpreference");
  context_builder->SetCookieStore(
      std::make_unique<CookieMonster>(nullptr, nullptr));
  auto context = context_builder->Build();
  auto& cm = *static_cast<CookieMonster*>(context->cookie_store());

  // Set cookies
  {
    // Attempt to set some cookies in a cross-site context without a SameSite
    // attribute. They should all be blocked. Only the one that would have been
    // included had it not been for the new SameSite features should have a
    // warning attached.
    TestDelegate d;
    GURL test_url = test_server.GetURL("this.example",
                                       "/set-cookie?blockeduserpreference=true&"
                                       "unspecifiedsamesite=1&"
                                       "invalidsecure=1;Secure");
    GURL cross_site_url = test_server.GetURL("other.example", "/");
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(
        net::SiteForCookies::FromUrl(cross_site_url));  // cross-site context
    req->Start();
    d.RunUntilComplete();

    ASSERT_EQ(3u, req->maybe_stored_cookies().size());

    // Cookie blocked by user preferences is not warned about.
    EXPECT_EQ("blockeduserpreference",
              req->maybe_stored_cookies()[0].cookie->Name());
    // It doesn't pick up the EXCLUDE_UNSPECIFIED_TREATED_AS_LAX because it
    // doesn't even make it to the cookie store (it is filtered out beforehand).
    EXPECT_TRUE(req->maybe_stored_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
    EXPECT_FALSE(
        req->maybe_stored_cookies()[0].access_result.status.ShouldWarn());

    // Cookie that would be included had it not been for the new SameSite rules
    // is warned about.
    EXPECT_EQ("unspecifiedsamesite",
              req->maybe_stored_cookies()[1].cookie->Name());
    EXPECT_TRUE(req->maybe_stored_cookies()[1]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {CookieInclusionStatus::
                             EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
    EXPECT_TRUE(req->maybe_stored_cookies()[1]
                    .access_result.status.HasExactlyWarningReasonsForTesting(
                        {CookieInclusionStatus::
                             WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT}));

    // Cookie that is blocked because of invalid Secure attribute is not warned
    // about.
    EXPECT_EQ("invalidsecure", req->maybe_stored_cookies()[2].cookie->Name());
    EXPECT_TRUE(req->maybe_stored_cookies()[2]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {CookieInclusionStatus::EXCLUDE_SECURE_ONLY,
                         CookieInclusionStatus::
                             EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
    EXPECT_TRUE(req->maybe_stored_cookies()[2]
                    .access_result.status.HasExactlyWarningReasonsForTesting(
                        {CookieInclusionStatus::
                             WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME}));
  }

  // Get cookies (blocked by user preference)
  network_delegate.set_block_annotate_cookies();
  {
    GURL url = test_server.GetURL("/");
    auto cookie1 = CanonicalCookie::CreateForTesting(url, "cookienosamesite=1",
                                                     base::Time::Now());
    base::RunLoop run_loop;
    CookieAccessResult access_result;
    cm.SetCanonicalCookieAsync(
        std::move(cookie1), url, CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting([&](CookieAccessResult result) {
          access_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_TRUE(access_result.status.IsInclude());

    TestDelegate d;
    GURL test_url = test_server.GetURL("/echoheader?Cookie");
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    GURL cross_site_url = test_server.GetURL("other.example", "/");
    req->set_site_for_cookies(
        net::SiteForCookies::FromUrl(cross_site_url));  // cross-site context
    req->Start();
    d.RunUntilComplete();

    // No cookies were sent with the request because getting cookies is blocked.
    EXPECT_EQ("None", d.data_received());
    ASSERT_EQ(1u, req->maybe_sent_cookies().size());
    EXPECT_EQ("cookienosamesite", req->maybe_sent_cookies()[0].cookie.Name());
    EXPECT_TRUE(req->maybe_sent_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
                         CookieInclusionStatus::
                             EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
    // Cookie should not be warned about because it was blocked because of user
    // preferences.
    EXPECT_FALSE(
        req->maybe_sent_cookies()[0].access_result.status.ShouldWarn());
  }
  network_delegate.unset_block_annotate_cookies();

  // Get cookies
  {
    GURL url = test_server.GetURL("/");
    auto cookie2 = CanonicalCookie::CreateForTesting(
        url, "cookiewithpath=1;path=/foo", base::Time::Now());
    base::RunLoop run_loop;
    // Note: cookie1 from the previous testcase is still in the cookie store.
    CookieAccessResult access_result;
    cm.SetCanonicalCookieAsync(
        std::move(cookie2), url, CookieOptions::MakeAllInclusive(),
        base::BindLambdaForTesting([&](CookieAccessResult result) {
          access_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_TRUE(access_result.status.IsInclude());

    TestDelegate d;
    GURL test_url = test_server.GetURL("/echoheader?Cookie");
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    GURL cross_site_url = test_server.GetURL("other.example", "/");
    req->set_site_for_cookies(
        net::SiteForCookies::FromUrl(cross_site_url));  // cross-site context
    req->Start();
    d.RunUntilComplete();

    // No cookies were sent with the request because they don't specify SameSite
    // and the request is cross-site.
    EXPECT_EQ("None", d.data_received());
    ASSERT_EQ(2u, req->maybe_sent_cookies().size());
    // Cookie excluded for other reasons is not warned about.
    // Note: this cookie is first because the cookies are sorted by path length
    // with longest first. See CookieSorter() in cookie_monster.cc.
    EXPECT_EQ("cookiewithpath", req->maybe_sent_cookies()[0].cookie.Name());
    EXPECT_TRUE(req->maybe_sent_cookies()[0]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {CookieInclusionStatus::EXCLUDE_NOT_ON_PATH,
                         CookieInclusionStatus::
                             EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
    EXPECT_FALSE(
        req->maybe_sent_cookies()[0].access_result.status.ShouldWarn());
    // Cookie that was only blocked because of unspecified SameSite should be
    // warned about.
    EXPECT_EQ("cookienosamesite", req->maybe_sent_cookies()[1].cookie.Name());
    EXPECT_TRUE(req->maybe_sent_cookies()[1]
                    .access_result.status.HasExactlyExclusionReasonsForTesting(
                        {CookieInclusionStatus::
                             EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
    EXPECT_TRUE(req->maybe_sent_cookies()[1]
                    .access_result.status.HasExactlyWarningReasonsForTesting(
                        {CookieInclusionStatus::
                             WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT}));
  }
}

TEST_F(URLRequestTestHTTP, AuthChallengeCancelCookieCollect) {
  ASSERT_TRUE(http_test_server()->Start());
  GURL url_requiring_auth =
      http_test_server()->GetURL("/auth-basic?set-cookie-if-challenged");

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto filtering_network_delegate =
      std::make_unique<FilteringTestNetworkDelegate>();
  filtering_network_delegate->SetCookieFilter("got_challenged");
  context_builder->set_network_delegate(std::move(filtering_network_delegate));
  auto context = context_builder->Build();

  TestDelegate delegate;

  std::unique_ptr<URLRequest> request =
      CreateFirstPartyRequest(*context, url_requiring_auth, &delegate);
  request->Start();

  delegate.RunUntilAuthRequired();
  ASSERT_EQ(1u, request->maybe_stored_cookies().size());
  EXPECT_TRUE(request->maybe_stored_cookies()[0]
                  .access_result.status.HasExactlyExclusionReasonsForTesting(
                      {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
  EXPECT_EQ("got_challenged=true",
            request->maybe_stored_cookies()[0].cookie_string);

  // This shouldn't DCHECK-fail.
  request->CancelAuth();
  delegate.RunUntilComplete();
}

TEST_F(URLRequestTestHTTP, AuthChallengeWithFilteredCookies) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL url_requiring_auth =
      http_test_server()->GetURL("/auth-basic?set-cookie-if-challenged");
  GURL url_requiring_auth_wo_cookies =
      http_test_server()->GetURL("/auth-basic");
  // Check maybe_stored_cookies is populated first round trip, and cleared on
  // the second.
  {
    auto context_builder = CreateTestURLRequestContextBuilder();
    auto& filtering_network_delegate = *context_builder->set_network_delegate(
        std::make_unique<FilteringTestNetworkDelegate>());
    filtering_network_delegate.SetCookieFilter("got_challenged");
    auto context = context_builder->Build();

    TestDelegate delegate;

    std::unique_ptr<URLRequest> request =
        CreateFirstPartyRequest(*context, url_requiring_auth, &delegate);
    request->Start();

    delegate.RunUntilAuthRequired();
    // Make sure it was blocked once.
    EXPECT_EQ(1, filtering_network_delegate.blocked_set_cookie_count());

    // The number of cookies blocked from the most recent round trip.
    ASSERT_EQ(1u, request->maybe_stored_cookies().size());
    EXPECT_TRUE(
        request->maybe_stored_cookies()
            .front()
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));

    // Now check the second round trip
    request->SetAuth(AuthCredentials(kUser, kSecret));
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());

    // There are DCHECKs in URLRequestHttpJob that would fail if
    // maybe_sent_cookies and maybe_stored_cookies were not cleared properly.

    // Make sure the cookie was actually filtered and not sent.
    EXPECT_EQ(std::string::npos,
              delegate.data_received().find("Cookie: got_challenged=true"));

    // The number of cookies that most recent round trip tried to set.
    ASSERT_EQ(0u, request->maybe_stored_cookies().size());
  }

  // Check maybe_sent_cookies on first round trip (and cleared for the second).
  {
    auto context_builder = CreateTestURLRequestContextBuilder();
    auto& filtering_network_delegate = *context_builder->set_network_delegate(
        std::make_unique<FilteringTestNetworkDelegate>());
    filtering_network_delegate.set_block_annotate_cookies();
    context_builder->SetCookieStore(
        std::make_unique<CookieMonster>(nullptr, nullptr));
    auto context = context_builder->Build();

    auto* cm = static_cast<CookieMonster*>(context->cookie_store());
    auto another_cookie = CanonicalCookie::CreateForTesting(
        url_requiring_auth_wo_cookies, "another_cookie=true",
        base::Time::Now());
    cm->SetCanonicalCookieAsync(std::move(another_cookie),
                                url_requiring_auth_wo_cookies,
                                net::CookieOptions::MakeAllInclusive(),
                                CookieStore::SetCookiesCallback());

    TestDelegate delegate;

    std::unique_ptr<URLRequest> request = CreateFirstPartyRequest(
        *context, url_requiring_auth_wo_cookies, &delegate);
    request->Start();

    delegate.RunUntilAuthRequired();

    ASSERT_EQ(1u, request->maybe_sent_cookies().size());
    EXPECT_EQ("another_cookie",
              request->maybe_sent_cookies().front().cookie.Name());
    EXPECT_EQ("true", request->maybe_sent_cookies().front().cookie.Value());
    EXPECT_TRUE(
        request->maybe_sent_cookies()
            .front()
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));

    // Check maybe_sent_cookies on second roundtrip.
    request->set_maybe_sent_cookies({});
    cm->DeleteAllAsync(CookieStore::DeleteCallback());
    auto one_more_cookie = CanonicalCookie::CreateForTesting(
        url_requiring_auth_wo_cookies, "one_more_cookie=true",
        base::Time::Now());
    cm->SetCanonicalCookieAsync(std::move(one_more_cookie),
                                url_requiring_auth_wo_cookies,
                                net::CookieOptions::MakeAllInclusive(),
                                CookieStore::SetCookiesCallback());

    request->SetAuth(AuthCredentials(kUser, kSecret));
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());

    // There are DCHECKs in URLRequestHttpJob that would fail if
    // maybe_sent_cookies and maybe_stored_cookies were not cleared properly.

    // Make sure the cookie was actually filtered.
    EXPECT_EQ(std::string::npos,
              delegate.data_received().find("Cookie: one_more_cookie=true"));
    // got_challenged was set after the first request and blocked on the second,
    // so it should only have been blocked this time
    EXPECT_EQ(2, filtering_network_delegate.blocked_annotate_cookies_count());

    // // The number of cookies blocked from the most recent round trip.
    ASSERT_EQ(1u, request->maybe_sent_cookies().size());
    EXPECT_EQ("one_more_cookie",
              request->maybe_sent_cookies().front().cookie.Name());
    EXPECT_TRUE(
        request->maybe_sent_cookies()
            .front()
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
  }
}

// Tests that load timing works as expected with auth and the cache.
TEST_F(URLRequestTestHTTP, BasicAuthLoadTiming) {
  ASSERT_TRUE(http_test_server()->Start());

  // populate the cache
  {
    TestDelegate d;

    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_isolation_info(isolation_info1_);
    r->Start();
    d.RunUntilAuthRequired();

    LoadTimingInfo load_timing_info_before_auth;
    r->GetLoadTimingInfo(&load_timing_info_before_auth);
    TestLoadTimingNotReused(load_timing_info_before_auth,
                            CONNECT_TIMING_HAS_DNS_TIMES);

    r->SetAuth(AuthCredentials(kUser, kSecret));
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);
    LoadTimingInfo load_timing_info;
    r->GetLoadTimingInfo(&load_timing_info);
    // The test server does not support keep alive sockets, so the second
    // request with auth should use a new socket.
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
    EXPECT_NE(load_timing_info_before_auth.socket_log_id,
              load_timing_info.socket_log_id);
    EXPECT_LE(load_timing_info_before_auth.receive_headers_end,
              load_timing_info.connect_timing.connect_start);
  }

  // Repeat request with end-to-end validation.  Since auth-basic results in a
  // cachable page, we expect this test to result in a 304.  In which case, the
  // response should be fetched from the cache.
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetLoadFlags(LOAD_VALIDATE_CACHE);
    r->set_isolation_info(isolation_info1_);
    r->Start();

    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Should be the same cached document.
    EXPECT_TRUE(r->was_cached());

    // Since there was a request that went over the wire, the load timing
    // information should include connection times.
    LoadTimingInfo load_timing_info;
    r->GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNotReused(load_timing_info, CONNECT_TIMING_HAS_DNS_TIMES);
  }
}

// In this test, we do a POST which the server will 302 redirect.
// The subsequent transaction should use GET, and should not send the
// Content-Type header.
// http://code.google.com/p/chromium/issues/detail?id=843
TEST_F(URLRequestTestHTTP, Post302RedirectGet) {
  ASSERT_TRUE(http_test_server()->Start());

  const char kData[] = "hello world";

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/redirect-to-echoall"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("POST");
  req->set_upload(CreateSimpleUploadData(base::byte_span_from_cstring(kData)));

  // Set headers (some of which are specific to the POST).
  HttpRequestHeaders headers;
  headers.SetHeader("Content-Type",
                    "multipart/form-data;"
                    "boundary=----WebKitFormBoundaryAADeAA+NAAWMAAwZ");
  headers.SetHeader("Accept",
                    "text/xml,application/xml,application/xhtml+xml,"
                    "text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5");
  headers.SetHeader("Accept-Language", "en-US,en");
  headers.SetHeader("Accept-Charset", "ISO-8859-1,*,utf-8");
  headers.SetHeader("Content-Length", "11");
  headers.SetHeader("Origin", "http://localhost:1337/");
  req->SetExtraRequestHeaders(headers);
  req->Start();
  d.RunUntilComplete();

  std::string mime_type;
  req->GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  const std::string& data = d.data_received();

  // Check that the post-specific headers were stripped:
  EXPECT_FALSE(ContainsString(data, "Content-Length:"));
  EXPECT_FALSE(ContainsString(data, "Content-Type:"));
  EXPECT_FALSE(ContainsString(data, "Origin:"));

  // These extra request headers should not have been stripped.
  EXPECT_TRUE(ContainsString(data, "Accept:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Language:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Charset:"));
}

// The following tests check that we handle mutating the request for HTTP
// redirects as expected.
// See https://crbug.com/56373, https://crbug.com/102130, and
// https://crbug.com/465517.

TEST_F(URLRequestTestHTTP, Redirect301Tests) {
  ASSERT_TRUE(http_test_server()->Start());

  const GURL url = http_test_server()->GetURL("/redirect301-to-echo");
  const GURL https_redirect_url =
      http_test_server()->GetURL("/redirect301-to-https");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "PUT", "PUT", "null");
}

TEST_F(URLRequestTestHTTP, Redirect302Tests) {
  ASSERT_TRUE(http_test_server()->Start());

  const GURL url = http_test_server()->GetURL("/redirect302-to-echo");
  const GURL https_redirect_url =
      http_test_server()->GetURL("/redirect302-to-https");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "PUT", "PUT", "null");
}

TEST_F(URLRequestTestHTTP, Redirect303Tests) {
  ASSERT_TRUE(http_test_server()->Start());

  const GURL url = http_test_server()->GetURL("/redirect303-to-echo");
  const GURL https_redirect_url =
      http_test_server()->GetURL("/redirect303-to-https");

  HTTPRedirectMethodTest(url, "POST", "GET", true);
  HTTPRedirectMethodTest(url, "PUT", "GET", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);

  HTTPRedirectOriginHeaderTest(url, "DELETE", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "DELETE", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "HEAD", "HEAD",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "HEAD", "HEAD", "null");
  HTTPRedirectOriginHeaderTest(url, "OPTIONS", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "OPTIONS", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "PUT", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "PUT", "GET", std::string());
}

TEST_F(URLRequestTestHTTP, Redirect307Tests) {
  ASSERT_TRUE(http_test_server()->Start());

  const GURL url = http_test_server()->GetURL("/redirect307-to-echo");
  const GURL https_redirect_url =
      http_test_server()->GetURL("/redirect307-to-https");

  HTTPRedirectMethodTest(url, "POST", "POST", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "POST",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null");
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "PUT", "PUT", "null");
}

TEST_F(URLRequestTestHTTP, Redirect308Tests) {
  ASSERT_TRUE(http_test_server()->Start());

  const GURL url = http_test_server()->GetURL("/redirect308-to-echo");
  const GURL https_redirect_url =
      http_test_server()->GetURL("/redirect308-to-https");

  HTTPRedirectMethodTest(url, "POST", "POST", true);
  HTTPRedirectMethodTest(url, "PUT", "PUT", true);
  HTTPRedirectMethodTest(url, "HEAD", "HEAD", false);

  HTTPRedirectOriginHeaderTest(url, "GET", "GET",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "POST",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null");
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT",
                               url.DeprecatedGetOriginAsURL().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "PUT", "PUT", "null");
}

// Make sure that 308 responses without bodies are not treated as redirects.
// Certain legacy apis that pre-date the response code expect this behavior
// (Like Google Drive).
TEST_F(URLRequestTestHTTP, NoRedirectOn308WithoutLocationHeader) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  const GURL url = http_test_server()->GetURL("/308-without-location-header");

  std::unique_ptr<URLRequest> request(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();
  d.RunUntilComplete();
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(0, d.received_redirect_count());
  EXPECT_EQ(308, request->response_headers()->response_code());
  EXPECT_EQ("This is not a redirect.", d.data_received());
}

TEST_F(URLRequestTestHTTP, Redirect302PreserveReferenceFragment) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL original_url(
      http_test_server()->GetURL("/redirect302-to-echo#fragment"));
  GURL expected_url(http_test_server()->GetURL("/echo#fragment"));

  TestDelegate d;
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  d.RunUntilComplete();

  EXPECT_EQ(2U, r->url_chain().size());
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(original_url, r->original_url());
  EXPECT_EQ(expected_url, r->url());
}

TEST_F(URLRequestTestHTTP, RedirectWithFilteredCookies) {
  ASSERT_TRUE(http_test_server()->Start());

  // FilteringTestNetworkDelegate filters by name, so the names of the two
  // cookies have to be the same. The values have been set to different strings
  // (the value of the server-redirect cookies is "true" and set-cookie is
  // "other") to differentiate between the two round trips.
  GURL redirect_to(
      http_test_server()->GetURL("/set-cookie?server-redirect=other"));

  GURL original_url(http_test_server()->GetURL("/server-redirect-with-cookie?" +
                                               redirect_to.spec()));

  GURL original_url_wo_cookie(
      http_test_server()->GetURL("/server-redirect?" + redirect_to.spec()));
  // Check maybe_stored_cookies on first round trip.
  {
    auto context_builder = CreateTestURLRequestContextBuilder();
    auto& filtering_network_delegate = *context_builder->set_network_delegate(
        std::make_unique<FilteringTestNetworkDelegate>());
    filtering_network_delegate.SetCookieFilter(
        "server-redirect");  // Filter the cookie server-redirect sets.
    auto context = context_builder->Build();

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request =
        CreateFirstPartyRequest(*context, original_url, &delegate);

    request->Start();
    delegate.RunUntilRedirect();

    // Make sure it was blocked once.
    EXPECT_EQ(1, filtering_network_delegate.blocked_set_cookie_count());

    // The number of cookies blocked from the most recent round trip.
    ASSERT_EQ(1u, request->maybe_stored_cookies().size());
    EXPECT_EQ("server-redirect",
              request->maybe_stored_cookies().front().cookie->Name());
    EXPECT_EQ("true", request->maybe_stored_cookies().front().cookie->Value());
    EXPECT_TRUE(
        request->maybe_stored_cookies()
            .front()
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));

    // Check maybe_stored_cookies on second round trip (and clearing from the
    // first).
    request->FollowDeferredRedirect(std::nullopt, std::nullopt);
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());

    // There are DCHECKs in URLRequestHttpJob that would fail if
    // maybe_sent_cookies and maybe_stored_cookies we not cleared properly.

    // Make sure it was blocked twice.
    EXPECT_EQ(2, filtering_network_delegate.blocked_set_cookie_count());

    // The number of cookies blocked from the most recent round trip.
    ASSERT_EQ(1u, request->maybe_stored_cookies().size());
    EXPECT_EQ("server-redirect",
              request->maybe_stored_cookies().front().cookie->Name());
    EXPECT_EQ("other", request->maybe_stored_cookies().front().cookie->Value());
    EXPECT_TRUE(
        request->maybe_stored_cookies()
            .front()
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
  }

  // Check maybe_sent_cookies on first round trip.
  {
    auto context_builder = CreateTestURLRequestContextBuilder();
    auto& filtering_network_delegate = *context_builder->set_network_delegate(
        std::make_unique<FilteringTestNetworkDelegate>());
    filtering_network_delegate.set_block_annotate_cookies();
    context_builder->SetCookieStore(
        std::make_unique<CookieMonster>(nullptr, nullptr));
    auto context = context_builder->Build();

    auto* cm = static_cast<CookieMonster*>(context->cookie_store());
    auto another_cookie = CanonicalCookie::CreateForTesting(
        original_url, "another_cookie=true", base::Time::Now());
    cm->SetCanonicalCookieAsync(std::move(another_cookie), original_url,
                                net::CookieOptions::MakeAllInclusive(),
                                CookieStore::SetCookiesCallback());

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request =
        CreateFirstPartyRequest(*context, original_url_wo_cookie, &delegate);
    request->Start();

    delegate.RunUntilRedirect();

    ASSERT_EQ(1u, request->maybe_sent_cookies().size());
    EXPECT_EQ("another_cookie",
              request->maybe_sent_cookies().front().cookie.Name());
    EXPECT_TRUE(
        request->maybe_sent_cookies()
            .front()
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));

    // Check maybe_sent_cookies on second round trip
    request->set_maybe_sent_cookies({});
    cm->DeleteAllAsync(CookieStore::DeleteCallback());
    auto one_more_cookie = CanonicalCookie::CreateForTesting(
        original_url_wo_cookie, "one_more_cookie=true", base::Time::Now());
    cm->SetCanonicalCookieAsync(std::move(one_more_cookie),
                                original_url_wo_cookie,
                                net::CookieOptions::MakeAllInclusive(),
                                CookieStore::SetCookiesCallback());

    request->FollowDeferredRedirect(std::nullopt, std::nullopt);
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());

    // There are DCHECKs in URLRequestHttpJob that would fail if
    // maybe_sent_cookies and maybe_stored_cookies we not cleared properly.

    EXPECT_EQ(2, filtering_network_delegate.blocked_annotate_cookies_count());

    // The number of cookies blocked from the most recent round trip.
    ASSERT_EQ(1u, request->maybe_sent_cookies().size());
    EXPECT_EQ("one_more_cookie",
              request->maybe_sent_cookies().front().cookie.Name());
    EXPECT_EQ("true", request->maybe_sent_cookies().front().cookie.Value());
    EXPECT_TRUE(
        request->maybe_sent_cookies()
            .front()
            .access_result.status.HasExactlyExclusionReasonsForTesting(
                {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
  }
}

TEST_F(URLRequestTestHTTP, RedirectPreserveFirstPartyURL) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL url(http_test_server()->GetURL("/redirect302-to-echo"));
  GURL first_party_url("http://example.com");

  TestDelegate d;
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->set_site_for_cookies(SiteForCookies::FromUrl(first_party_url));

  r->Start();
  d.RunUntilComplete();

  EXPECT_EQ(2U, r->url_chain().size());
  EXPECT_EQ(OK, d.request_status());
  EXPECT_TRUE(SiteForCookies::FromUrl(first_party_url)
                  .IsEquivalent(r->site_for_cookies()));
}

TEST_F(URLRequestTestHTTP, RedirectUpdateFirstPartyURL) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL url(http_test_server()->GetURL("/redirect302-to-echo"));
  GURL original_first_party_url("http://example.com");
  GURL expected_first_party_url(http_test_server()->GetURL("/echo"));

  TestDelegate d;

    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_site_for_cookies(SiteForCookies::FromUrl(original_first_party_url));
    r->set_first_party_url_policy(
        RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_TRUE(SiteForCookies::FromUrl(expected_first_party_url)
                    .IsEquivalent(r->site_for_cookies()));
}

TEST_F(URLRequestTestHTTP, InterceptPost302RedirectGet) {
  ASSERT_TRUE(http_test_server()->Start());

  const char kData[] = "hello world";

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("POST");
  req->set_upload(CreateSimpleUploadData(base::byte_span_from_cstring(kData)));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::NumberToString(std::size(kData) - 1));
  req->SetExtraRequestHeaders(headers);

  std::unique_ptr<URLRequestRedirectJob> job =
      std::make_unique<URLRequestRedirectJob>(
          req.get(), http_test_server()->GetURL("/echo"),
          RedirectUtil::ResponseCode::REDIRECT_302_FOUND, "Very Good Reason");
  TestScopedURLInterceptor interceptor(req->url(), std::move(job));

  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ("GET", req->method());
}

TEST_F(URLRequestTestHTTP, InterceptPost307RedirectPost) {
  ASSERT_TRUE(http_test_server()->Start());

  const char kData[] = "hello world";

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("POST");
  req->set_upload(CreateSimpleUploadData(base::byte_span_from_cstring(kData)));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::NumberToString(std::size(kData) - 1));
  req->SetExtraRequestHeaders(headers);

  std::unique_ptr<URLRequestRedirectJob> job =
      std::make_unique<URLRequestRedirectJob>(
          req.get(), http_test_server()->GetURL("/echo"),
          RedirectUtil::ResponseCode::REDIRECT_307_TEMPORARY_REDIRECT,
          "Very Good Reason");
  TestScopedURLInterceptor interceptor(req->url(), std::move(job));

  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ("POST", req->method());
  EXPECT_EQ(kData, d.data_received());
}

// Check that default A-L header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptLanguage) {
  ASSERT_TRUE(http_test_server()->Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_http_user_agent_settings(
      std::make_unique<StaticHttpUserAgentSettings>("en", "test-ua"));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL("/echoheader?Accept-Language"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ("en", d.data_received());
}

// Check that an empty A-L header is not sent. http://crbug.com/77365.
TEST_F(URLRequestTestHTTP, EmptyAcceptLanguage) {
  ASSERT_TRUE(http_test_server()->Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_http_user_agent_settings(
      std::make_unique<StaticHttpUserAgentSettings>(std::string(), "test-ua"));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL("/echoheader?Accept-Language"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ("None", d.data_received());
}

// Check that if request overrides the A-L header, the default is not appended.
// See http://crbug.com/20894
TEST_F(URLRequestTestHTTP, OverrideAcceptLanguage) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Accept-Language"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptLanguage, "ru");
  req->SetExtraRequestHeaders(headers);
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ(std::string("ru"), d.data_received());
}

// Check that default A-E header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptEncoding) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Accept-Encoding"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  HttpRequestHeaders headers;
  req->SetExtraRequestHeaders(headers);
  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(ContainsString(d.data_received(), "gzip"));
}

// Check that it's possible to override the default A-E header.
TEST_F(URLRequestTestHTTP, DefaultAcceptEncodingOverriden) {
  ASSERT_TRUE(http_test_server()->Start());

  struct {
    base::flat_set<net::SourceStream::SourceType> accepted_types;
    const char* expected_accept_encoding;
  } tests[] = {{{net::SourceStream::SourceType::TYPE_DEFLATE}, "deflate"},
               {{}, "None"},
               {{net::SourceStream::SourceType::TYPE_GZIP}, "gzip"},
               {{net::SourceStream::SourceType::TYPE_GZIP,
                 net::SourceStream::SourceType::TYPE_DEFLATE},
                "gzip, deflate"}};
  for (auto test : tests) {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        http_test_server()->GetURL("/echoheader?Accept-Encoding"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_accepted_stream_types(test.accepted_types);
    req->Start();
    d.RunUntilComplete();
    EXPECT_STRCASEEQ(d.data_received().c_str(), test.expected_accept_encoding);
  }
}

// Check that if request overrides the A-E header, the default is not appended.
// See http://crbug.com/47381
TEST_F(URLRequestTestHTTP, OverrideAcceptEncoding) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Accept-Encoding"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptEncoding, "identity");
  req->SetExtraRequestHeaders(headers);
  req->Start();
  d.RunUntilComplete();
  EXPECT_FALSE(ContainsString(d.data_received(), "gzip"));
  EXPECT_TRUE(ContainsString(d.data_received(), "identity"));
}

// Check that setting the A-C header sends the proper header.
TEST_F(URLRequestTestHTTP, SetAcceptCharset) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?Accept-Charset"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kAcceptCharset, "koi-8r");
  req->SetExtraRequestHeaders(headers);
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ(std::string("koi-8r"), d.data_received());
}

// Check that default User-Agent header is sent.
TEST_F(URLRequestTestHTTP, DefaultUserAgent) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?User-Agent"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ(default_context().http_user_agent_settings()->GetUserAgent(),
            d.data_received());
}

// Check that if request overrides the User-Agent header,
// the default is not appended.
// TODO(crbug.com/41225288) This test is flaky on iOS.
#if BUILDFLAG(IS_IOS)
#define MAYBE_OverrideUserAgent FLAKY_OverrideUserAgent
#else
#define MAYBE_OverrideUserAgent OverrideUserAgent
#endif
TEST_F(URLRequestTestHTTP, MAYBE_OverrideUserAgent) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/echoheader?User-Agent"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kUserAgent, "Lynx (textmode)");
  req->SetExtraRequestHeaders(headers);
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ(std::string("Lynx (textmode)"), d.data_received());
}

// Check that a NULL HttpUserAgentSettings causes the corresponding empty
// User-Agent header to be sent but does not send the Accept-Language and
// Accept-Charset headers.
TEST_F(URLRequestTestHTTP, EmptyHttpUserAgentSettings) {
  ASSERT_TRUE(http_test_server()->Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_http_user_agent_settings(nullptr);
  auto context = context_builder->Build();

  struct {
    const char* request;
    const char* expected_response;
  } tests[] = {{"/echoheader?Accept-Language", "None"},
               {"/echoheader?Accept-Charset", "None"},
               {"/echoheader?User-Agent", ""}};

  for (const auto& test : tests) {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        http_test_server()->GetURL(test.request), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(test.expected_response, d.data_received())
        << " Request = \"" << test.request << "\"";
  }
}

// Make sure that URLRequest passes on its priority updates to
// newly-created jobs after the first one.
TEST_F(URLRequestTestHTTP, SetSubsequentJobPriority) {
  GURL initial_url("http://foo.test/");
  GURL redirect_url("http://bar.test/");

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      initial_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(DEFAULT_PRIORITY, req->priority());

  std::unique_ptr<URLRequestRedirectJob> redirect_job =
      std::make_unique<URLRequestRedirectJob>(
          req.get(), redirect_url,
          RedirectUtil::ResponseCode::REDIRECT_302_FOUND, "Very Good Reason");
  auto interceptor = std::make_unique<TestScopedURLInterceptor>(
      initial_url, std::move(redirect_job));

  req->SetPriority(LOW);
  req->Start();
  EXPECT_TRUE(req->is_pending());
  d.RunUntilRedirect();
  interceptor.reset();

  RequestPriority job_priority;
  std::unique_ptr<URLRequestJob> job =
      std::make_unique<PriorityMonitoringURLRequestJob>(req.get(),
                                                        &job_priority);
  interceptor =
      std::make_unique<TestScopedURLInterceptor>(redirect_url, std::move(job));

  // Should trigger |job| to be started.
  req->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                              std::nullopt /* modified_headers */);
  d.RunUntilComplete();
  EXPECT_EQ(LOW, job_priority);
}

// Check that creating a network request while entering/exiting suspend mode
// fails as it should.  This is the only case where an HttpTransactionFactory
// does not return an HttpTransaction.
TEST_F(URLRequestTestHTTP, NetworkSuspendTest) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCreateHttpTransactionFactoryCallback(
      base::BindOnce([](HttpNetworkSession* session) {
        // Create a new HttpNetworkLayer that thinks it's suspended.
        auto network_layer = std::make_unique<HttpNetworkLayer>(session);
        network_layer->OnSuspend();
        std::unique_ptr<HttpTransactionFactory> factory =
            std::make_unique<HttpCache>(std::move(network_layer),
                                        HttpCache::DefaultBackend::InMemory(0));
        return factory;
      }));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://127.0.0.1/"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(d.request_failed());
  EXPECT_EQ(ERR_NETWORK_IO_SUSPENDED, d.request_status());
}

namespace {

// HttpTransactionFactory that synchronously fails to create transactions.
class FailingHttpTransactionFactory : public HttpTransactionFactory {
 public:
  explicit FailingHttpTransactionFactory(HttpNetworkSession* network_session)
      : network_session_(network_session) {}

  FailingHttpTransactionFactory(const FailingHttpTransactionFactory&) = delete;
  FailingHttpTransactionFactory& operator=(
      const FailingHttpTransactionFactory&) = delete;

  ~FailingHttpTransactionFactory() override = default;

  // HttpTransactionFactory methods:
  int CreateTransaction(RequestPriority priority,
                        std::unique_ptr<HttpTransaction>* trans) override {
    return ERR_FAILED;
  }

  HttpCache* GetCache() override { return nullptr; }

  HttpNetworkSession* GetSession() override { return network_session_; }

 private:
  raw_ptr<HttpNetworkSession> network_session_;
};

}  // namespace

// Check that when a request that fails to create an HttpTransaction can be
// cancelled while the failure notification is pending, and doesn't send two
// failure notifications.
//
// This currently only happens when in suspend mode and there's no cache, but
// just use a special HttpTransactionFactory, to avoid depending on those
// behaviors.
TEST_F(URLRequestTestHTTP, NetworkCancelAfterCreateTransactionFailsTest) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCreateHttpTransactionFactoryCallback(
      base::BindOnce([](HttpNetworkSession* session) {
        std::unique_ptr<HttpTransactionFactory> factory =
            std::make_unique<FailingHttpTransactionFactory>(session);
        return factory;
      }));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL("http://127.0.0.1/"), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  // Don't send cookies (Collecting cookies is asynchronous, and need request to
  // try to create an HttpNetworkTransaction synchronously on start).
  req->set_allow_credentials(false);
  req->Start();
  req->Cancel();
  d.RunUntilComplete();
  // Run pending error task, if there is one.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(d.request_failed());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(ERR_ABORTED, d.request_status());

  // NetworkDelegate should see the cancellation, but not the error.
  EXPECT_EQ(1, network_delegate.canceled_requests());
  EXPECT_EQ(0, network_delegate.error_count());
}

TEST_F(URLRequestTestHTTP, NetworkAccessedSetOnNetworkRequest) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  GURL test_url(http_test_server()->GetURL("/"));
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  req->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(req->response_info().network_accessed);
}

TEST_F(URLRequestTestHTTP, NetworkAccessedClearOnCachedResponse) {
  ASSERT_TRUE(http_test_server()->Start());

  // Populate the cache.
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/cachetime"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_isolation_info(isolation_info1_);
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(OK, d.request_status());
  EXPECT_TRUE(req->response_info().network_accessed);
  EXPECT_FALSE(req->response_info().was_cached);

  req = default_context().CreateRequest(
      http_test_server()->GetURL("/cachetime"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  req->set_isolation_info(isolation_info1_);
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(OK, d.request_status());
  EXPECT_FALSE(req->response_info().network_accessed);
  EXPECT_TRUE(req->response_info().was_cached);
}

TEST_F(URLRequestTestHTTP, NetworkAccessedClearOnLoadOnlyFromCache) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  GURL test_url(http_test_server()->GetURL("/"));
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetLoadFlags(LOAD_ONLY_FROM_CACHE | LOAD_SKIP_CACHE_VALIDATION);

  req->Start();
  d.RunUntilComplete();

  EXPECT_FALSE(req->response_info().network_accessed);
}

// Test that a single job with a THROTTLED priority completes
// correctly in the absence of contention.
TEST_F(URLRequestTestHTTP, ThrottledPriority) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  GURL test_url(http_test_server()->GetURL("/"));
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      test_url, THROTTLED, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(OK, d.request_status());
}

TEST_F(URLRequestTestHTTP, RawBodyBytesNoContentEncoding) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/simple.html"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(5, req->GetRawBodyBytes());
}

TEST_F(URLRequestTestHTTP, RawBodyBytesGzipEncoding) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/gzip-encoded"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(30, req->GetRawBodyBytes());
}

// Check that if NetworkDelegate::OnBeforeStartTransaction returns an error,
// the delegate isn't called back synchronously.
TEST_F(URLRequestTestHTTP, TesBeforeStartTransactionFails) {
  ASSERT_TRUE(http_test_server()->Start());
  default_network_delegate().set_before_start_transaction_fails();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  DCHECK(!d.response_completed());
  d.RunUntilComplete();
  DCHECK(d.response_completed());
  EXPECT_EQ(ERR_FAILED, d.request_status());
}

class URLRequestTestReferrerPolicy : public URLRequestTest {
 public:
  URLRequestTestReferrerPolicy() = default;

  void InstantiateSameOriginServers(net::EmbeddedTestServer::Type type) {
    origin_server_ = std::make_unique<EmbeddedTestServer>(type);
    RegisterDefaultHandlers(origin_server_.get());
    ASSERT_TRUE(origin_server_->Start());
  }

  void InstantiateCrossOriginServers(net::EmbeddedTestServer::Type origin_type,
                                     net::EmbeddedTestServer::Type dest_type) {
    origin_server_ = std::make_unique<EmbeddedTestServer>(origin_type);
    RegisterDefaultHandlers(origin_server_.get());
    ASSERT_TRUE(origin_server_->Start());

    destination_server_ = std::make_unique<EmbeddedTestServer>(dest_type);
    RegisterDefaultHandlers(destination_server_.get());
    ASSERT_TRUE(destination_server_->Start());
  }

  void VerifyReferrerAfterRedirect(ReferrerPolicy policy,
                                   const GURL& referrer,
                                   const GURL& expected) {
    // Create and execute the request: we'll only have a |destination_server_|
    // if the origins are meant to be distinct. Otherwise, we'll use the
    // |origin_server_| for both endpoints.
    GURL destination_url =
        destination_server_ ? destination_server_->GetURL("/echoheader?Referer")
                            : origin_server_->GetURL("/echoheader?Referer");
    GURL origin_url =
        origin_server_->GetURL("/server-redirect?" + destination_url.spec());

    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        origin_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_referrer_policy(policy);
    req->SetReferrer(referrer.spec());
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(destination_url, req->url());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, req->response_headers()->response_code());

    EXPECT_EQ(expected.spec(), req->referrer());
    if (expected.is_empty())
      EXPECT_EQ("None", d.data_received());
    else
      EXPECT_EQ(expected.spec(), d.data_received());
  }

  EmbeddedTestServer* origin_server() const { return origin_server_.get(); }

 private:
  std::unique_ptr<EmbeddedTestServer> origin_server_;
  std::unique_ptr<EmbeddedTestServer> destination_server_;
};

TEST_F(URLRequestTestReferrerPolicy, HTTPToSameOriginHTTP) {
  InstantiateSameOriginServers(net::EmbeddedTestServer::TYPE_HTTP);

  GURL referrer = origin_server()->GetURL("/path/to/file.html");
  VerifyReferrerAfterRedirect(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE, referrer,
      referrer);

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer);

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer);

  VerifyReferrerAfterRedirect(ReferrerPolicy::NEVER_CLEAR, referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(ReferrerPolicy::ORIGIN,
                              referrer.DeprecatedGetOriginAsURL(),
                              referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN,
                              referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.DeprecatedGetOriginAsURL(), referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPToCrossOriginHTTP) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTP,
                                net::EmbeddedTestServer::TYPE_HTTP);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE, referrer,
      referrer);

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::NEVER_CLEAR, referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(ReferrerPolicy::ORIGIN,
                              referrer.DeprecatedGetOriginAsURL(),
                              referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN,
                              referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.DeprecatedGetOriginAsURL(), referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPSToSameOriginHTTPS) {
  InstantiateSameOriginServers(net::EmbeddedTestServer::TYPE_HTTPS);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE, referrer,
      referrer);

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer);

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer);

  VerifyReferrerAfterRedirect(ReferrerPolicy::NEVER_CLEAR, referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(ReferrerPolicy::ORIGIN,
                              referrer.DeprecatedGetOriginAsURL(),
                              referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN,
                              referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.DeprecatedGetOriginAsURL(), referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPSToCrossOriginHTTPS) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTPS,
                                net::EmbeddedTestServer::TYPE_HTTPS);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE, referrer,
      referrer);

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(ReferrerPolicy::NEVER_CLEAR, referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(ReferrerPolicy::ORIGIN,
                              referrer.DeprecatedGetOriginAsURL(),
                              referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN,
                              referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.DeprecatedGetOriginAsURL(), referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPToHTTPS) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTP,
                                net::EmbeddedTestServer::TYPE_HTTPS);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE, referrer,
      referrer);

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(ReferrerPolicy::NEVER_CLEAR, referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(ReferrerPolicy::ORIGIN,
                              referrer.DeprecatedGetOriginAsURL(),
                              referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN,
                              referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.DeprecatedGetOriginAsURL(), referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPSToHTTP) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTPS,
                                net::EmbeddedTestServer::TYPE_HTTP);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE, referrer,
      GURL());

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      GURL());

  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(ReferrerPolicy::NEVER_CLEAR, referrer, referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(ReferrerPolicy::ORIGIN,
                              referrer.DeprecatedGetOriginAsURL(),
                              referrer.DeprecatedGetOriginAsURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN,
                              referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin, though it should be
  // subsequently cleared during the downgrading redirect.
  VerifyReferrerAfterRedirect(
      ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.DeprecatedGetOriginAsURL(), GURL());

  VerifyReferrerAfterRedirect(ReferrerPolicy::NO_REFERRER, GURL(), GURL());
}

class HTTPSRequestTest : public TestWithTaskEnvironment {
 public:
  HTTPSRequestTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    default_context_ = context_builder->Build();
  }
  ~HTTPSRequestTest() override {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }

  URLRequestContext& default_context() { return *default_context_; }

 private:
  std::unique_ptr<URLRequestContext> default_context_;
};

TEST_F(HTTPSRequestTest, HTTPSGetTest) {
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    CheckSSLInfo(r->ssl_info());
    EXPECT_EQ(test_server.host_port_pair().host(),
              r->GetResponseRemoteEndpoint().ToStringWithoutPort());
    EXPECT_EQ(test_server.host_port_pair().port(),
              r->GetResponseRemoteEndpoint().port());
  }
}

TEST_F(HTTPSRequestTest, HTTPSMismatchedTest) {
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  bool err_allowed = true;
  for (int i = 0; i < 2; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      std::unique_ptr<URLRequest> r(default_context().CreateRequest(
          test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
          TRAFFIC_ANNOTATION_FOR_TESTS));

      r->Start();
      EXPECT_TRUE(r->is_pending());

      d.RunUntilComplete();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_TRUE(d.have_certificate_errors());
      if (err_allowed) {
        EXPECT_NE(0, d.bytes_received());
        CheckSSLInfo(r->ssl_info());
      } else {
        EXPECT_EQ(0, d.bytes_received());
      }
    }
  }
}

TEST_F(HTTPSRequestTest, HTTPSExpiredTest) {
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  // Iterate from false to true, just so that we do the opposite of the
  // previous test in order to increase test coverage.
  bool err_allowed = false;
  for (int i = 0; i < 2; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      std::unique_ptr<URLRequest> r(default_context().CreateRequest(
          test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
          TRAFFIC_ANNOTATION_FOR_TESTS));

      r->Start();
      EXPECT_TRUE(r->is_pending());

      d.RunUntilComplete();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_TRUE(d.have_certificate_errors());
      if (err_allowed) {
        EXPECT_NE(0, d.bytes_received());
        CheckSSLInfo(r->ssl_info());
      } else {
        EXPECT_EQ(0, d.bytes_received());
      }
    }
  }
}

// A TestDelegate used to test that an appropriate net error code is provided
// when an SSL certificate error occurs.
class SSLNetErrorTestDelegate : public TestDelegate {
 public:
  void OnSSLCertificateError(URLRequest* request,
                             int net_error,
                             const SSLInfo& ssl_info,
                             bool fatal) override {
    net_error_ = net_error;
    on_ssl_certificate_error_called_ = true;
    TestDelegate::OnSSLCertificateError(request, net_error, ssl_info, fatal);
  }

  bool on_ssl_certificate_error_called() {
    return on_ssl_certificate_error_called_;
  }

  int net_error() { return net_error_; }

 private:
  bool on_ssl_certificate_error_called_ = false;
  int net_error_ = net::OK;
};

// Tests that the URLRequest::Delegate receives an appropriate net error code
// when an SSL certificate error occurs.
TEST_F(HTTPSRequestTest, SSLNetErrorReportedToDelegate) {
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  SSLNetErrorTestDelegate d;
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  r->Start();
  EXPECT_TRUE(r->is_pending());
  d.RunUntilComplete();

  EXPECT_TRUE(d.on_ssl_certificate_error_called());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, d.net_error());
}

// TODO(svaldez): iOS tests are flaky with EmbeddedTestServer and transport
// security state. (see http://crbug.com/550977).
#if !BUILDFLAG(IS_IOS)
// This tests that a load of a domain with preloaded HSTS and HPKP with a
// certificate error sets the |certificate_errors_are_fatal| flag correctly.
// This flag will cause the interstitial to be fatal.
TEST_F(HTTPSRequestTest, HTTPSPreloadedHSTSTest) {
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);

  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  test_server.ServeFilesFromSourceDirectory("net/data/ssl");
  ASSERT_TRUE(test_server.Start());

  // We require that the URL be hsts-hpkp-preloaded.test. This is a test domain
  // that has a preloaded HSTS+HPKP entry in the TransportSecurityState. This
  // means that we have to use a MockHostResolver in order to direct
  // hsts-hpkp-preloaded.test to the testserver.

  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("hsts-hpkp-preloaded.test",
                                  test_server.GetIPLiteralString());
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::move(host_resolver));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> r(context->CreateRequest(
      GURL(base::StringPrintf("https://hsts-hpkp-preloaded.test:%d",
                              test_server.host_port_pair().port())),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  EXPECT_TRUE(r->is_pending());

  d.RunUntilComplete();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_TRUE(d.have_certificate_errors());
  EXPECT_TRUE(d.certificate_errors_are_fatal());
}

// This tests that cached HTTPS page loads do not cause any updates to the
// TransportSecurityState.
TEST_F(HTTPSRequestTest, HTTPSErrorsNoClobberTSSTest) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);

  // The actual problem -- CERT_MISMATCHED_NAME in this case -- doesn't
  // matter. It just has to be any error.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  test_server.ServeFilesFromSourceDirectory("net/data/ssl");
  ASSERT_TRUE(test_server.Start());

  // We require that the URL be hsts-hpkp-preloaded.test. This is a test domain
  // that has a preloaded HSTS+HPKP entry in the TransportSecurityState. This
  // means that we have to use a MockHostResolver in order to direct
  // hsts-hpkp-preloaded.test to the testserver.

  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("hsts-hpkp-preloaded.test",
                                  test_server.GetIPLiteralString());
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::move(host_resolver));
  auto context = context_builder->Build();
  ASSERT_TRUE(context->transport_security_state());
  TransportSecurityState& transport_security_state =
      *context->transport_security_state();

  transport_security_state.EnableStaticPinsForTesting();
  transport_security_state.SetPinningListAlwaysTimelyForTesting(true);

  TransportSecurityState::STSState static_sts_state;
  TransportSecurityState::PKPState static_pkp_state;
  EXPECT_TRUE(transport_security_state.GetStaticSTSState(
      "hsts-hpkp-preloaded.test", &static_sts_state));
  EXPECT_TRUE(transport_security_state.GetStaticPKPState(
      "hsts-hpkp-preloaded.test", &static_pkp_state));

  TransportSecurityState::STSState dynamic_sts_state;
  TransportSecurityState::PKPState dynamic_pkp_state;
  EXPECT_FALSE(transport_security_state.GetDynamicSTSState(
      "hsts-hpkp-preloaded.test", &dynamic_sts_state));
  EXPECT_FALSE(transport_security_state.GetDynamicPKPState(
      "hsts-hpkp-preloaded.test", &dynamic_pkp_state));

  TestDelegate d;
  std::unique_ptr<URLRequest> r(context->CreateRequest(
      GURL(base::StringPrintf("https://hsts-hpkp-preloaded.test:%d",
                              test_server.host_port_pair().port())),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  EXPECT_TRUE(r->is_pending());

  d.RunUntilComplete();

  EXPECT_EQ(1, d.response_started_count());
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_TRUE(d.have_certificate_errors());
  EXPECT_TRUE(d.certificate_errors_are_fatal());

  // Get a fresh copy of the states, and check that they haven't changed.
  TransportSecurityState::STSState new_static_sts_state;
  TransportSecurityState::PKPState new_static_pkp_state;
  EXPECT_TRUE(transport_security_state.GetStaticSTSState(
      "hsts-hpkp-preloaded.test", &new_static_sts_state));
  EXPECT_TRUE(transport_security_state.GetStaticPKPState(
      "hsts-hpkp-preloaded.test", &new_static_pkp_state));
  TransportSecurityState::STSState new_dynamic_sts_state;
  TransportSecurityState::PKPState new_dynamic_pkp_state;
  EXPECT_FALSE(transport_security_state.GetDynamicSTSState(
      "hsts-hpkp-preloaded.test", &new_dynamic_sts_state));
  EXPECT_FALSE(transport_security_state.GetDynamicPKPState(
      "hsts-hpkp-preloaded.test", &new_dynamic_pkp_state));

  EXPECT_EQ(new_static_sts_state.upgrade_mode, static_sts_state.upgrade_mode);
  EXPECT_EQ(new_static_sts_state.include_subdomains,
            static_sts_state.include_subdomains);
  EXPECT_EQ(new_static_pkp_state.include_subdomains,
            static_pkp_state.include_subdomains);
  EXPECT_EQ(new_static_pkp_state.spki_hashes, static_pkp_state.spki_hashes);
  EXPECT_EQ(new_static_pkp_state.bad_spki_hashes,
            static_pkp_state.bad_spki_hashes);
}

// Make sure HSTS preserves a POST request's method and body.
TEST_F(HTTPSRequestTest, HSTSPreservesPosts) {
  static const char kData[] = "hello world";

  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  // Per spec, TransportSecurityState expects a domain name, rather than an IP
  // address, so a MockHostResolver is needed to redirect www.somewhere.com to
  // the EmbeddedTestServer.
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddRule("www.somewhere.com",
                                  test_server.GetIPLiteralString());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(std::move(host_resolver));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestNetworkDelegate>());
  auto context = context_builder->Build();
  ASSERT_TRUE(context->transport_security_state());
  TransportSecurityState& transport_security_state =
      *context->transport_security_state();
  // Force https for www.somewhere.com.
  base::Time expiry = base::Time::Now() + base::Days(1000);
  bool include_subdomains = false;
  transport_security_state.AddHSTS("www.somewhere.com", expiry,
                                   include_subdomains);

  TestDelegate d;
  // Navigating to https://www.somewhere.com instead of https://127.0.0.1 will
  // cause a certificate error.  Ignore the error.
  d.set_allow_certificate_errors(true);

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      GURL(base::StringPrintf("http://www.somewhere.com:%d/echo",
                              test_server.host_port_pair().port())),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("POST");
  req->set_upload(CreateSimpleUploadData(base::byte_span_from_cstring(kData)));

  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ("https", req->url().scheme());
  EXPECT_EQ("POST", req->method());
  EXPECT_EQ(kData, d.data_received());

  LoadTimingInfo load_timing_info;
  network_delegate.GetLoadTimingInfoBeforeRedirect(&load_timing_info);
  // LoadTimingInfo of HSTS redirects is similar to that of network cache hits
  TestLoadTimingCacheHitNoNetwork(load_timing_info);
}

// Make sure that the CORS headers are added to cross-origin HSTS redirects.
TEST_F(HTTPSRequestTest, HSTSCrossOriginAddHeaders) {
  static const char kOriginHeaderValue[] = "http://www.example.com";

  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory("net/data/ssl");
  ASSERT_TRUE(test_server.Start());

  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->set_default_result(OK);

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCertVerifier(std::move(cert_verifier));
  auto context = context_builder->Build();
  ASSERT_TRUE(context->transport_security_state());
  TransportSecurityState& transport_security_state =
      *context->transport_security_state();
  base::Time expiry = base::Time::Now() + base::Days(1);
  bool include_subdomains = false;
  transport_security_state.AddHSTS("example.net", expiry, include_subdomains);

  GURL hsts_http_url(base::StringPrintf("http://example.net:%d/somehstssite",
                                        test_server.host_port_pair().port()));
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  GURL hsts_https_url = hsts_http_url.ReplaceComponents(replacements);

  TestDelegate d;

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      hsts_http_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  // Set Origin header to simulate a cross-origin request.
  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Origin", kOriginHeaderValue);
  req->SetExtraRequestHeaders(request_headers);

  req->Start();
  d.RunUntilRedirect();

  EXPECT_EQ(1, d.received_redirect_count());

  const HttpResponseHeaders* headers = req->response_headers();
  std::string redirect_location;
  EXPECT_TRUE(
      headers->EnumerateHeader(nullptr, "Location", &redirect_location));
  EXPECT_EQ(hsts_https_url.spec(), redirect_location);

  std::string received_cors_header;
  EXPECT_TRUE(headers->EnumerateHeader(nullptr, "Access-Control-Allow-Origin",
                                       &received_cors_header));
  EXPECT_EQ(kOriginHeaderValue, received_cors_header);

  std::string received_corp_header;
  EXPECT_TRUE(headers->EnumerateHeader(nullptr, "Cross-Origin-Resource-Policy",
                                       &received_corp_header));
  EXPECT_EQ("Cross-Origin", received_corp_header);
}

namespace {

class SSLClientAuthTestDelegate : public TestDelegate {
 public:
  SSLClientAuthTestDelegate() { set_on_complete(base::DoNothing()); }
  void OnCertificateRequested(URLRequest* request,
                              SSLCertRequestInfo* cert_request_info) override {
    on_certificate_requested_count_++;
    std::move(on_certificate_requested_).Run();
  }
  void RunUntilCertificateRequested() {
    base::RunLoop run_loop;
    on_certificate_requested_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  int on_certificate_requested_count() {
    return on_certificate_requested_count_;
  }

 private:
  int on_certificate_requested_count_ = 0;
  base::OnceClosure on_certificate_requested_;
};

class TestSSLPrivateKey : public SSLPrivateKey {
 public:
  explicit TestSSLPrivateKey(scoped_refptr<SSLPrivateKey> key)
      : key_(std::move(key)) {}

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
                                    ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
                                    std::vector<uint8_t>()));
    } else {
      key_->Sign(algorithm, input, std::move(callback));
    }
  }

 private:
  ~TestSSLPrivateKey() override = default;

  scoped_refptr<SSLPrivateKey> key_;
  bool fail_signing_ = false;
  int sign_count_ = 0;
};

}  // namespace

// TODO(davidben): Test the rest of the code. Specifically,
// - Filtering which certificates to select.
// - Getting a certificate request in an SSL renegotiation sending the
//   HTTP request.
TEST_F(HTTPSRequestTest, ClientAuthNoCertificate) {
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      SSLServerConfig::ClientCertType::OPTIONAL_CLIENT_CERT;
  test_server.SetSSLConfig(EmbeddedTestServer::CERT_OK, ssl_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  SSLClientAuthTestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilCertificateRequested();
    EXPECT_TRUE(r->is_pending());

    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    // Send no certificate.
    // TODO(davidben): Get temporary client cert import (with keys) working on
    // all platforms so we can test sending a cert as well.
    r->ContinueWithCertificate(nullptr, nullptr);

    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
  }
}

TEST_F(HTTPSRequestTest, ClientAuth) {
  std::unique_ptr<FakeClientCertIdentity> identity =
      FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());

  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(EmbeddedTestServer::CERT_OK, ssl_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilCertificateRequested();
    EXPECT_TRUE(r->is_pending());

    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    // Send a certificate.
    r->ContinueWithCertificate(identity->certificate(), private_key);

    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());

    // The private key should have been used.
    EXPECT_EQ(1, private_key->sign_count());
  }

  // Close all connections and clear the session cache to force a new handshake.
  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->CloseAllConnections(ERR_FAILED, "Very good reason");
  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->ClearSSLSessionCache();

  // Connecting again should not call OnCertificateRequested. The identity is
  // taken from the client auth cache.
  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(0, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());

    // The private key should have been used.
    EXPECT_EQ(2, private_key->sign_count());
  }
}

// Test that private keys that fail to sign anything get evicted from the cache.
TEST_F(HTTPSRequestTest, ClientAuthFailSigning) {
  std::unique_ptr<FakeClientCertIdentity> identity =
      FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());
  private_key->set_fail_signing(true);

  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(EmbeddedTestServer::CERT_OK, ssl_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilCertificateRequested();
    EXPECT_TRUE(r->is_pending());

    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    // Send a certificate.
    r->ContinueWithCertificate(identity->certificate(), private_key);
    d.RunUntilComplete();

    // The private key cannot sign anything, so we report an error.
    EXPECT_EQ(ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED, d.request_status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    // The private key should have been used.
    EXPECT_EQ(1, private_key->sign_count());
  }

  // Close all connections and clear the session cache to force a new handshake.
  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->CloseAllConnections(ERR_FAILED, "Very good reason");
  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->ClearSSLSessionCache();

  // The bad identity should have been evicted from the cache, so connecting
  // again should call OnCertificateRequested again.
  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilCertificateRequested();
    EXPECT_TRUE(r->is_pending());

    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    // There should have been no additional uses of the private key.
    EXPECT_EQ(1, private_key->sign_count());
  }
}

// Test that cached private keys that fail to sign anything trigger a
// retry. This is so we handle unplugged smartcards
// gracefully. https://crbug.com/813022.
TEST_F(HTTPSRequestTest, ClientAuthFailSigningRetry) {
  std::unique_ptr<FakeClientCertIdentity> identity =
      FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);
  scoped_refptr<TestSSLPrivateKey> private_key =
      base::MakeRefCounted<TestSSLPrivateKey>(identity->ssl_private_key());

  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  test_server.SetSSLConfig(EmbeddedTestServer::CERT_OK, ssl_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  // Connect with a client certificate to put it in the client auth cache.
  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilCertificateRequested();
    EXPECT_TRUE(r->is_pending());

    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());

    r->ContinueWithCertificate(identity->certificate(), private_key);
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());

    // The private key should have been used.
    EXPECT_EQ(1, private_key->sign_count());
  }

  // Close all connections and clear the session cache to force a new handshake.
  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->CloseAllConnections(ERR_FAILED, "Very good reason");
  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->ClearSSLSessionCache();

  // Cause the private key to fail. Connecting again should attempt to use it,
  // notice the failure, and then request a new identity via
  // OnCertificateRequested.
  private_key->set_fail_signing(true);

  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilCertificateRequested();
    EXPECT_TRUE(r->is_pending());

    // There was an additional signing call on the private key (the one which
    // failed).
    EXPECT_EQ(2, private_key->sign_count());

    // That caused another OnCertificateRequested call.
    EXPECT_EQ(1, d.on_certificate_requested_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());
  }
}

TEST_F(HTTPSRequestTest, ResumeTest) {
  // Test that we attempt resume sessions when making two connections to the
  // same host.
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto url = test_server.GetURL("/");

  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->ClearSSLSessionCache();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r->ssl_info().handshake_type);
  }

  reinterpret_cast<HttpCache*>(default_context().http_transaction_factory())
      ->CloseAllConnections(ERR_FAILED, "Very good reason");

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, r->ssl_info().handshake_type);
  }
}

// Test that sessions aren't resumed across URLRequestContexts.
TEST_F(HTTPSRequestTest, SSLSessionCacheShardTest) {
  // Start a server.
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto url = test_server.GetURL("/");

  // Connect to the server once. This will add an entry to the session cache.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r->ssl_info().handshake_type);
  }

  // Clear the socket pools and connect again. This should resume the previous
  // session.
  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->CloseAllConnections(ERR_FAILED, /*net_log_reason_utf8=*/"");

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, r->ssl_info().handshake_type);
  }

  // Now fetch on a new URLRequestContext. This should not resume the session.
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto other_context = context_builder->Build();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(other_context->CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r->ssl_info().handshake_type);
  }
}

// Test that sessions started with privacy mode enabled cannot be resumed when
// it is disabled, and vice versa.
TEST_F(HTTPSRequestTest, NoSessionResumptionBetweenPrivacyModes) {
  // Start a server.
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto url = test_server.GetURL("/");

  auto ConnectAndCheckHandshake = [this, url](bool allow_credentials,
                                              auto expected_handshake) {
    // Construct request and indirectly set the privacy mode.
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_allow_credentials(allow_credentials);

    // Start the request and check the SSL handshake type.
    r->Start();
    EXPECT_TRUE(r->is_pending());
    d.RunUntilComplete();
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(expected_handshake, r->ssl_info().handshake_type);
  };

  // Exhaustively check all pairs of privacy mode values. Note that we are using
  // allow_credentials to indirectly enable/disable privacy mode.
  const bool kAllowCredentialsValues[] = {false, true};
  for (const auto allow_creds_1 : kAllowCredentialsValues) {
    for (const auto allow_creds_2 : kAllowCredentialsValues) {
      SCOPED_TRACE(base::StringPrintf("allow_creds_1=%d, allow_creds_2=%d",
                                      allow_creds_1, allow_creds_2));

      // The session cache starts off empty, so we expect a full handshake.
      ConnectAndCheckHandshake(allow_creds_1, SSLInfo::HANDSHAKE_FULL);

      // The second handshake depends on whether we are using the same session
      // cache as the first request.
      ConnectAndCheckHandshake(allow_creds_2, allow_creds_1 == allow_creds_2
                                                  ? SSLInfo::HANDSHAKE_RESUME
                                                  : SSLInfo::HANDSHAKE_FULL);
      // Flush both session caches.
      auto* network_session =
          default_context().http_transaction_factory()->GetSession();
      network_session->ClearSSLSessionCache();
    }
  }
}

class HTTPSFallbackTest : public TestWithTaskEnvironment {
 public:
  HTTPSFallbackTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_ssl_config_service(
        std::make_unique<TestSSLConfigService>(SSLContextConfig()));
    context_ = context_builder->Build();
  }
  ~HTTPSFallbackTest() override = default;

 protected:
  void DoFallbackTest(const SSLServerConfig& ssl_config) {
    DCHECK(!request_);
    delegate_.set_allow_certificate_errors(true);

    EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
    test_server.SetSSLConfig(EmbeddedTestServer::CERT_OK, ssl_config);
    ASSERT_TRUE(test_server.Start());

    request_ =
        context_->CreateRequest(test_server.GetURL("/"), DEFAULT_PRIORITY,
                                &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
    request_->Start();

    delegate_.RunUntilComplete();
  }

  void ExpectConnection(int version) {
    EXPECT_EQ(1, delegate_.response_started_count());
    EXPECT_NE(0, delegate_.bytes_received());
    EXPECT_EQ(version, SSLConnectionStatusToVersion(
                           request_->ssl_info().connection_status));
  }

  void ExpectFailure(int error) {
    EXPECT_EQ(1, delegate_.response_started_count());
    EXPECT_EQ(error, delegate_.request_status());
  }

 private:
  TestDelegate delegate_;
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<URLRequest> request_;
};

// Tests the TLS 1.0 fallback doesn't happen.
TEST_F(HTTPSFallbackTest, TLSv1NoFallback) {
  net::SSLServerConfig ssl_config;
  ssl_config.client_hello_callback_for_testing =
      base::BindRepeating([](const SSL_CLIENT_HELLO* client_hello) {
        // Reject ClientHellos with version >= TLS 1.1.
        return client_hello->version <= TLS1_VERSION;
      });

  ASSERT_NO_FATAL_FAILURE(DoFallbackTest(ssl_config));
  ExpectFailure(ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
}

// Tests the TLS 1.1 fallback doesn't happen.
TEST_F(HTTPSFallbackTest, TLSv1_1NoFallback) {
  net::SSLServerConfig ssl_config;
  ssl_config.client_hello_callback_for_testing =
      base::BindRepeating([](const SSL_CLIENT_HELLO* client_hello) {
        // Reject ClientHellos with version >= TLS 1.2.
        return client_hello->version <= TLS1_1_VERSION;
      });

  ASSERT_NO_FATAL_FAILURE(DoFallbackTest(ssl_config));
  ExpectFailure(ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
}

// Tests the TLS 1.2 fallback doesn't happen.
TEST_F(HTTPSFallbackTest, TLSv1_2NoFallback) {
  net::SSLServerConfig ssl_config;
  ssl_config.client_hello_callback_for_testing =
      base::BindRepeating([](const SSL_CLIENT_HELLO* client_hello) {
        // Reject ClientHellos with a supported_versions extension. TLS 1.3 is
        // signaled via an extension rather than the legacy version field.
        const uint8_t* data;
        size_t len;
        return !SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_supported_versions, &data, &len);
      });

  ASSERT_NO_FATAL_FAILURE(DoFallbackTest(ssl_config));
  ExpectFailure(ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
}

class HTTPSSessionTest : public TestWithTaskEnvironment {
 public:
  HTTPSSessionTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->SetCertVerifier(std::make_unique<MockCertVerifier>());
    default_context_ = context_builder->Build();

    default_cert_verifier().set_default_result(OK);
  }
  ~HTTPSSessionTest() override = default;

  URLRequestContext& default_context() { return *default_context_; }
  MockCertVerifier& default_cert_verifier() {
    // This cast is safe because we provided a MockCertVerifier in the ctor.
    return *static_cast<MockCertVerifier*>(default_context_->cert_verifier());
  }

 private:
  std::unique_ptr<URLRequestContext> default_context_;
};

// Tests that session resumption is not attempted if an invalid certificate
// is presented.
TEST_F(HTTPSSessionTest, DontResumeSessionsForInvalidCertificates) {
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto url = test_server.GetURL("/");

  default_context()
      .http_transaction_factory()
      ->GetSession()
      ->ClearSSLSessionCache();

  // Simulate the certificate being expired and attempt a connection.
  default_cert_verifier().set_default_result(ERR_CERT_DATE_INVALID);
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
  }

  reinterpret_cast<HttpCache*>(default_context().http_transaction_factory())
      ->CloseAllConnections(ERR_FAILED, "Very good reason");

  // Now change the certificate to be acceptable (so that the response is
  // loaded), and ensure that no session id is presented to the peer.
  default_cert_verifier().set_default_result(OK);
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r->ssl_info().handshake_type);
  }
}

// Interceptor to check that secure DNS has been disabled. Secure DNS should be
// disabled for any network fetch triggered during certificate verification as
// it could cause a deadlock.
class SecureDnsInterceptor : public net::URLRequestInterceptor {
 public:
  SecureDnsInterceptor() = default;
  ~SecureDnsInterceptor() override = default;

 private:
  // URLRequestInterceptor implementation:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_EQ(SecureDnsPolicy::kDisable, request->secure_dns_policy());
    return nullptr;
  }
};

class HTTPSCertNetFetchingTest : public HTTPSRequestTest {
 public:
  HTTPSCertNetFetchingTest() = default;

  void SetUp() override {
    cert_net_fetcher_ = base::MakeRefCounted<CertNetFetcherURLRequest>();
    auto cert_verifier =
        CertVerifier::CreateDefaultWithoutCaching(cert_net_fetcher_);
    updatable_cert_verifier_ = cert_verifier.get();

    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->SetCertVerifier(std::make_unique<CachingCertVerifier>(
        std::make_unique<CoalescingCertVerifier>(std::move(cert_verifier))));
    context_ = context_builder->Build();

    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", "127.0.0.1", std::make_unique<SecureDnsInterceptor>());

    cert_net_fetcher_->SetURLRequestContext(context_.get());
    context_->cert_verifier()->SetConfig(GetCertVerifierConfig());
  }

  void TearDown() override {
    cert_net_fetcher_->Shutdown();
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void DoConnectionWithDelegate(
      std::string_view hostname,
      const EmbeddedTestServer::ServerCertificateConfig& cert_config,
      TestDelegate* delegate,
      SSLInfo* out_ssl_info) {
    // Always overwrite |out_ssl_info|.
    out_ssl_info->Reset();

    EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
    test_server.SetSSLConfig(cert_config);
    RegisterDefaultHandlers(&test_server);
    ASSERT_TRUE(test_server.Start());

    delegate->set_allow_certificate_errors(true);
    std::unique_ptr<URLRequest> r(context_->CreateRequest(
        test_server.GetURL(hostname, "/"), DEFAULT_PRIORITY, delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    delegate->RunUntilComplete();
    EXPECT_EQ(1, delegate->response_started_count());

    *out_ssl_info = r->ssl_info();
  }

  void DoConnectionWithDelegate(
      const EmbeddedTestServer::ServerCertificateConfig& cert_config,
      TestDelegate* delegate,
      SSLInfo* out_ssl_info) {
    DoConnectionWithDelegate("127.0.0.1", cert_config, delegate, out_ssl_info);
  }

  void DoConnection(
      std::string_view hostname,
      const EmbeddedTestServer::ServerCertificateConfig& cert_config,
      CertStatus* out_cert_status) {
    // Always overwrite |out_cert_status|.
    *out_cert_status = 0;

    TestDelegate d;
    SSLInfo ssl_info;
    ASSERT_NO_FATAL_FAILURE(
        DoConnectionWithDelegate(hostname, cert_config, &d, &ssl_info));

    *out_cert_status = ssl_info.cert_status;
  }

  void DoConnection(
      const EmbeddedTestServer::ServerCertificateConfig& cert_config,
      CertStatus* out_cert_status) {
    DoConnection("127.0.0.1", cert_config, out_cert_status);
  }

 protected:
  // GetCertVerifierConfig() configures the URLRequestContext that will be used
  // for making connections to the testserver. This can be overridden in test
  // subclasses for different behaviour.
  virtual CertVerifier::Config GetCertVerifierConfig() {
    CertVerifier::Config config;
    return config;
  }

  void UpdateCertVerifier(scoped_refptr<CRLSet> crl_set) {
    net::CertVerifyProc::ImplParams params;
    params.crl_set = std::move(crl_set);
    updatable_cert_verifier_->UpdateVerifyProcData(cert_net_fetcher_, params,
                                                   {});
  }

  scoped_refptr<CertNetFetcherURLRequest> cert_net_fetcher_;
  std::unique_ptr<URLRequestContext> context_;
  raw_ptr<CertVerifierWithUpdatableProc> updatable_cert_verifier_;
};

// The test EV policy OID used for generated certs.
static const char kEVTestCertPolicy[] = "1.3.6.1.4.1.11129.2.4.1";

class HTTPSEVTest : public HTTPSCertNetFetchingTest {
 public:
  void SetUp() override {
    HTTPSCertNetFetchingTest::SetUp();

    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
    ASSERT_TRUE(root_cert);

    ev_test_policy_ = std::make_unique<ScopedTestEVPolicy>(
        EVRootCAMetadata::GetInstance(),
        X509Certificate::CalculateFingerprint256(root_cert->cert_buffer()),
        kEVTestCertPolicy);
  }

  void TearDown() override { HTTPSCertNetFetchingTest::TearDown(); }

 private:
  std::unique_ptr<ScopedTestEVPolicy> ev_test_policy_;
};

class HTTPSOCSPTest : public HTTPSCertNetFetchingTest {
 public:
  CertVerifier::Config GetCertVerifierConfig() override {
    CertVerifier::Config config;
    config.enable_rev_checking = true;
    return config;
  }
};

static bool UsingBuiltinCertVerifier() {
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  return true;
#else
  return false;
#endif
}

// SystemSupportsHardFailRevocationChecking returns true iff the current
// operating system supports revocation checking and can distinguish between
// situations where a given certificate lacks any revocation information (eg:
// no CRLDistributionPoints and no OCSP Responder AuthorityInfoAccess) and when
// revocation information cannot be obtained (eg: the CRL was unreachable).
// If it does not, then tests which rely on 'hard fail' behaviour should be
// skipped.
static bool SystemSupportsHardFailRevocationChecking() {
  return UsingBuiltinCertVerifier();
}

// SystemUsesChromiumEVMetadata returns true iff the current operating system
// uses Chromium's EV metadata (i.e. EVRootCAMetadata). If it does not, then
// several tests are effected because our testing EV certificate won't be
// recognised as EV.
static bool SystemUsesChromiumEVMetadata() {
#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
  return true;
#else
  return false;
#endif
}

static bool SystemSupportsOCSP() {
#if BUILDFLAG(IS_ANDROID)
  // Unsupported, see http://crbug.com/117478.
  return false;
#else
  return true;
#endif
}

static bool SystemSupportsOCSPStapling() {
  if (UsingBuiltinCertVerifier())
    return true;
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

static bool SystemSupportsCRLSets() {
  if (UsingBuiltinCertVerifier())
    return true;
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

TEST_F(HTTPSEVTest, EVCheckNoOCSP) {
  if (!SystemUsesChromiumEVMetadata()) {
    LOG(WARNING) << "Skipping test because system doesn't support EV";
    return;
  }
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.policy_oids = {kEVTestCertPolicy};

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));

  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, Valid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, Revoked) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, Invalid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kInvalidResponse);

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  // Without a positive OCSP response, we shouldn't show the EV status, but also
  // should not show any revocation checking errors.
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateValid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  cert_config.intermediate_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateResponseOldButStillValid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  // Use an OCSP response for the intermediate that would be too old for a leaf
  // cert, but is still valid for an intermediate.
  cert_config.intermediate_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateResponseTooOldKnownRoot) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root_cert);
  ScopedTestKnownRoot scoped_known_root(root_cert.get());

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  cert_config.intermediate_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLonger}});
  cert_config.dns_names = {"example.com"};

  CertStatus cert_status;
  DoConnection("example.com", cert_config, &cert_status);

  if (UsingBuiltinCertVerifier()) {
    // The intermediate certificate is marked as a known root and has an OCSP
    // response indicating REVOKED status, but the response is too old
    // according to the Baseline Requirements, thus the response should be
    // ignored and the verification succeeds.
    EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  } else {
    EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  }
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateResponseTooOld) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  cert_config.intermediate_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLonger}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  // The test root is not a known root, therefore the Baseline Requirements
  // limits on maximum age of a response do not apply and the intermediate OCSP
  // response indicating revoked is honored.
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateRevoked) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  cert_config.intermediate_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, ValidStapled) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, RevokedStapled) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, OldStapledAndInvalidAIA) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // Stapled response indicates good, but is too old.
  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, OldStapledButValidAIA) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // Stapled response indicates good, but response is too old.
  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, and returns a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

static const struct OCSPVerifyTestData {
  EmbeddedTestServer::OCSPConfig ocsp_config;
  bssl::OCSPVerifyResult::ResponseStatus expected_response_status;
  // |expected_cert_status| is only used if |expected_response_status| is
  // PROVIDED.
  bssl::OCSPRevocationStatus expected_cert_status;
} kOCSPVerifyData[] = {
    // 0
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::GOOD},

    // 1
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 2
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kEarly}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 3
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 4
    {EmbeddedTestServer::OCSPConfig(
         EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater),
     bssl::OCSPVerifyResult::ERROR_RESPONSE,
     bssl::OCSPRevocationStatus::UNKNOWN},

    // 5
    {EmbeddedTestServer::OCSPConfig(
         EmbeddedTestServer::OCSPConfig::ResponseType::kInvalidResponse),
     bssl::OCSPVerifyResult::PARSE_RESPONSE_ERROR,
     bssl::OCSPRevocationStatus::UNKNOWN},

    // 6
    {EmbeddedTestServer::OCSPConfig(
         EmbeddedTestServer::OCSPConfig::ResponseType::kInvalidResponseData),
     bssl::OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR,
     bssl::OCSPRevocationStatus::UNKNOWN},

    // 7
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::REVOKED,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kEarly}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 8
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::UNKNOWN,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::UNKNOWN},

    // 9
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::UNKNOWN,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 10
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::UNKNOWN,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kEarly}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 11
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kBeforeCert),
     bssl::OCSPVerifyResult::BAD_PRODUCED_AT,
     bssl::OCSPRevocationStatus::UNKNOWN},

    // 12
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kAfterCert),
     bssl::OCSPVerifyResult::BAD_PRODUCED_AT,
     bssl::OCSPRevocationStatus::UNKNOWN},

    // 13
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::GOOD},

    // 14
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kEarly},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::GOOD},

    // 15
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::GOOD},

    // 16
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kEarly},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 17
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::UNKNOWN,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid},
          {bssl::OCSPRevocationStatus::REVOKED,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::REVOKED},

    // 18
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::UNKNOWN,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::UNKNOWN},

    // 19
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::UNKNOWN,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid},
          {bssl::OCSPRevocationStatus::REVOKED,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong},
          {bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::UNKNOWN},

    // 20
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Serial::kMismatch}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::NO_MATCHING_RESPONSE,
     bssl::OCSPRevocationStatus::UNKNOWN},

    // 21
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::GOOD,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kEarly,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Serial::kMismatch}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::NO_MATCHING_RESPONSE,
     bssl::OCSPRevocationStatus::UNKNOWN},

    // 22
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::REVOKED,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::REVOKED},

    // 23
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::REVOKED,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},

    // 24
    {EmbeddedTestServer::OCSPConfig(
         {{bssl::OCSPRevocationStatus::REVOKED,
           EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong}},
         EmbeddedTestServer::OCSPConfig::Produced::kValid),
     bssl::OCSPVerifyResult::INVALID_DATE, bssl::OCSPRevocationStatus::UNKNOWN},
};

class HTTPSOCSPVerifyTest
    : public HTTPSOCSPTest,
      public testing::WithParamInterface<OCSPVerifyTestData> {};

TEST_P(HTTPSOCSPVerifyTest, VerifyResult) {
  OCSPVerifyTestData test = GetParam();

  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root_cert);
  ScopedTestKnownRoot scoped_known_root(root_cert.get());

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.stapled_ocsp_config = test.ocsp_config;
  cert_config.dns_names = {"example.com"};

  SSLInfo ssl_info;
  OCSPErrorTestDelegate delegate;
  ASSERT_NO_FATAL_FAILURE(DoConnectionWithDelegate("example.com", cert_config,
                                                   &delegate, &ssl_info));

  // The SSLInfo must be extracted from |delegate| on error, due to how
  // URLRequest caches certificate errors.
  if (delegate.have_certificate_errors()) {
    ASSERT_TRUE(delegate.on_ssl_certificate_error_called());
    ssl_info = delegate.ssl_info();
  }

  EXPECT_EQ(test.expected_response_status,
            ssl_info.ocsp_result.response_status);

  if (test.expected_response_status == bssl::OCSPVerifyResult::PROVIDED) {
    EXPECT_EQ(test.expected_cert_status,
              ssl_info.ocsp_result.revocation_status);
  }
}

INSTANTIATE_TEST_SUITE_P(OCSPVerify,
                         HTTPSOCSPVerifyTest,
                         testing::ValuesIn(kOCSPVerifyData));

class HTTPSAIATest : public HTTPSCertNetFetchingTest {};

TEST_F(HTTPSAIATest, AIAFetching) {
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kByAIA;
  test_server.SetSSLConfig(cert_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  d.set_allow_certificate_errors(true);
  std::unique_ptr<URLRequest> r(context_->CreateRequest(
      test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  EXPECT_TRUE(r->is_pending());

  d.RunUntilComplete();

  EXPECT_EQ(1, d.response_started_count());

  CertStatus cert_status = r->ssl_info().cert_status;
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  ASSERT_TRUE(r->ssl_info().cert);
  EXPECT_EQ(2u, r->ssl_info().cert->intermediate_buffers().size());
  ASSERT_TRUE(r->ssl_info().unverified_cert);
  EXPECT_EQ(0u, r->ssl_info().unverified_cert->intermediate_buffers().size());
}

class HTTPSHardFailTest : public HTTPSOCSPTest {
 protected:
  CertVerifier::Config GetCertVerifierConfig() override {
    CertVerifier::Config config;
    config.require_rev_checking_local_anchors = true;
    return config;
  }
};

TEST_F(HTTPSHardFailTest, Valid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, Revoked) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, FailsOnOCSPInvalid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kInvalidResponse);

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
            cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, IntermediateResponseOldButStillValid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  // Use an OCSP response for the intermediate that would be too old for a leaf
  // cert, but is still valid for an intermediate.
  cert_config.intermediate_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLong}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, IntermediateResponseTooOld) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = EmbeddedTestServer::IntermediateType::kInHandshake;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  // Use an OCSP response for the intermediate that is too old according to
  // BRs, but is fine for a locally trusted root.
  cert_config.intermediate_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kLonger}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, ValidStapled) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, RevokedStapled) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, OldStapledAndInvalidAIA) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // Stapled response indicates good, but is too old.
  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, but does not return a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kTryLater);

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
            cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSHardFailTest, OldStapledButValidAIA) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  if (!SystemSupportsHardFailRevocationChecking()) {
    LOG(WARNING) << "Skipping test because system doesn't support hard fail "
                 << "revocation checking";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;

  // Stapled response indicates good, but response is too old.
  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kOld}});

  // AIA OCSP url is included, and returns a successful ocsp response.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

class HTTPSCRLSetTest : public HTTPSCertNetFetchingTest {};

TEST_F(HTTPSCRLSetTest, ExpiredCRLSet) {
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kInvalidResponse);

  UpdateCertVerifier(CRLSet::ExpiredCRLSetForTesting());

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  // If we're not trying EV verification then, even if the CRLSet has expired,
  // we don't fall back to online revocation checks.
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSCRLSetTest, ExpiredCRLSetAndRevoked) {
  // Test that when online revocation checking is disabled, and the leaf
  // certificate is not EV, that no revocation checking actually happens.
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  UpdateCertVerifier(CRLSet::ExpiredCRLSetForTesting());

  CertStatus cert_status;
  DoConnection(cert_config, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSCRLSetTest, CRLSetRevoked) {
  if (!SystemSupportsCRLSets()) {
    LOG(WARNING) << "Skipping test because system doesn't support CRLSets";
    return;
  }

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  test_server.SetSSLConfig(cert_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
  SHA256HashValue root_cert_spki_hash;
  ASSERT_TRUE(GetTestRootCertSPKIHash(&root_cert_spki_hash));
  auto crl_set =
      CRLSet::ForTesting(false, &root_cert_spki_hash,
                         test_server.GetCertificate()->serial_number(), "", {});
  ASSERT_TRUE(crl_set);
  UpdateCertVerifier(crl_set);

  TestDelegate d;
  d.set_allow_certificate_errors(true);
  std::unique_ptr<URLRequest> r(context_->CreateRequest(
      test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  r->Start();
  EXPECT_TRUE(r->is_pending());
  d.RunUntilComplete();
  EXPECT_EQ(1, d.response_started_count());
  CertStatus cert_status = r->ssl_info().cert_status;

  // If the certificate is recorded as revoked in the CRLSet, that should be
  // reflected without online revocation checking.
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSCRLSetTest, CRLSetRevokedBySubject) {
  if (!SystemSupportsCRLSets()) {
    LOG(WARNING) << "Skipping test because system doesn't support CRLSets";
    return;
  }

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});
  test_server.SetSSLConfig(cert_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  std::string common_name = test_server.GetCertificate()->subject().common_name;

  {
    auto crl_set = CRLSet::ForTesting(false, nullptr, "", common_name, {});
    ASSERT_TRUE(crl_set);
    UpdateCertVerifier(crl_set);

    TestDelegate d;
    d.set_allow_certificate_errors(true);
    std::unique_ptr<URLRequest> r(context_->CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());
    d.RunUntilComplete();
    EXPECT_EQ(1, d.response_started_count());
    CertStatus cert_status = r->ssl_info().cert_status;

    // If the certificate is recorded as revoked in the CRLSet, that should be
    // reflected without online revocation checking.
    EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
    EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
  }

  HashValue spki_hash_value;
  ASSERT_TRUE(x509_util::CalculateSha256SpkiHash(
      test_server.GetCertificate()->cert_buffer(), &spki_hash_value));
  std::string spki_hash(spki_hash_value.data(),
                        spki_hash_value.data() + spki_hash_value.size());
  {
    auto crl_set =
        CRLSet::ForTesting(false, nullptr, "", common_name, {spki_hash});
    ASSERT_TRUE(crl_set);
    UpdateCertVerifier(crl_set);

    TestDelegate d;
    d.set_allow_certificate_errors(true);
    std::unique_ptr<URLRequest> r(context_->CreateRequest(
        test_server.GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());
    d.RunUntilComplete();
    EXPECT_EQ(1, d.response_started_count());
    CertStatus cert_status = r->ssl_info().cert_status;

    // When the correct SPKI hash is specified in
    // |acceptable_spki_hashes_for_cn|, the connection should succeed even
    // though the subject is listed in the CRLSet.
    EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  }
}

using HTTPSLocalCRLSetTest = TestWithTaskEnvironment;

// Use a real CertVerifier to attempt to connect to the TestServer, and ensure
// that when a CRLSet is provided that marks a given SPKI (the TestServer's
// root SPKI) as known for interception, that it's adequately flagged.
TEST_F(HTTPSLocalCRLSetTest, KnownInterceptionBlocked) {
  auto cert_verifier = CertVerifier::CreateDefaultWithoutCaching(
      /*cert_net_fetcher=*/nullptr);
  CertVerifierWithUpdatableProc* updatable_cert_verifier_ = cert_verifier.get();

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCertVerifier(std::make_unique<CachingCertVerifier>(
      std::make_unique<CoalescingCertVerifier>(std::move(cert_verifier))));
  auto context = context_builder->Build();

  // Verify the connection succeeds without being flagged.
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&https_server);
  https_server.SetSSLConfig(EmbeddedTestServer::CERT_OK_BY_INTERMEDIATE);
  ASSERT_TRUE(https_server.Start());

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(
        context->CreateRequest(https_server.GetURL("/"), DEFAULT_PRIORITY, &d,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.request_failed());
    EXPECT_FALSE(d.have_certificate_errors());
    EXPECT_FALSE(req->ssl_info().cert_status &
                 CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  }

  // Configure a CRL that will mark |root_ca_cert| as a blocked interception
  // root.
  std::string crl_set_bytes;
  net::CertVerifyProc::ImplParams params;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestCertsDirectory().AppendASCII(
                                 "crlset_blocked_interception_by_root.raw"),
                             &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &params.crl_set));

  updatable_cert_verifier_->UpdateVerifyProcData(
      /*cert_net_fetcher=*/nullptr, params, {});

  // Verify the connection fails as being a known interception root.
  {
    TestDelegate d;
    d.set_allow_certificate_errors(true);
    std::unique_ptr<URLRequest> req(
        context->CreateRequest(https_server.GetURL("/"), DEFAULT_PRIORITY, &d,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.request_failed());
    if (SystemSupportsCRLSets()) {
      EXPECT_TRUE(d.have_certificate_errors());
      EXPECT_FALSE(d.certificate_errors_are_fatal());
      EXPECT_EQ(ERR_CERT_KNOWN_INTERCEPTION_BLOCKED, d.certificate_net_error());
      EXPECT_TRUE(req->ssl_info().cert_status &
                  CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
    } else {
      EXPECT_FALSE(d.have_certificate_errors());
      EXPECT_TRUE(req->ssl_info().cert_status &
                  CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
    }
  }
}

TEST_F(HTTPSLocalCRLSetTest, InterceptionBlockedAllowOverrideOnHSTS) {
  constexpr char kHSTSHost[] = "include-subdomains-hsts-preloaded.test";
  constexpr char kHSTSSubdomainWithKnownInterception[] =
      "www.include-subdomains-hsts-preloaded.test";

  EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK_BY_INTERMEDIATE);
  https_server.ServeFilesFromSourceDirectory(base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_server.Start());

  // Configure the CertVerifier to simulate:
  //   - For the test server host, that the certificate is issued by an
  //     unknown authority; this SHOULD NOT be a fatal error when signaled
  //     to the delegate.
  //   - For |kHSTSHost|, that the certificate is issued by an unknown
  //     authority; this SHOULD be a fatal error.
  // Combined, these two states represent the baseline: non-fatal for non-HSTS
  // hosts, fatal for HSTS host.
  //   - For |kHSTSSubdomainWithKnownInterception|, that the certificate is
  //     issued by a known interception cert. This SHOULD be an error, but
  //     SHOULD NOT be a fatal error
  auto cert_verifier = std::make_unique<MockCertVerifier>();

  scoped_refptr<X509Certificate> cert = https_server.GetCertificate();
  ASSERT_TRUE(cert);

  HashValue filler_hash;
  ASSERT_TRUE(filler_hash.FromString(
      "sha256/3333333333333333333333333333333333333333333="));

  CertVerifyResult fake_result;
  fake_result.verified_cert = cert;
  fake_result.is_issued_by_known_root = false;

  // Configure for the test server's default host.
  CertVerifyResult test_result = fake_result;
  test_result.public_key_hashes.push_back(filler_hash);
  test_result.cert_status |= CERT_STATUS_AUTHORITY_INVALID;
  cert_verifier->AddResultForCertAndHost(
      cert.get(), https_server.host_port_pair().host(), test_result,
      ERR_CERT_AUTHORITY_INVALID);

  // Configure for kHSTSHost.
  CertVerifyResult sts_base_result = fake_result;
  sts_base_result.public_key_hashes.push_back(filler_hash);
  sts_base_result.cert_status |= CERT_STATUS_AUTHORITY_INVALID;
  cert_verifier->AddResultForCertAndHost(cert.get(), kHSTSHost, sts_base_result,
                                         ERR_CERT_AUTHORITY_INVALID);

  // Configure for kHSTSSubdomainWithKnownInterception
  CertVerifyResult sts_sub_result = fake_result;
  SHA256HashValue root_hash;
  ASSERT_TRUE(GetTestRootCertSPKIHash(&root_hash));
  sts_sub_result.public_key_hashes.push_back(HashValue(root_hash));
  sts_sub_result.cert_status |=
      CERT_STATUS_REVOKED | CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED;
  cert_verifier->AddResultForCertAndHost(
      cert.get(), kHSTSSubdomainWithKnownInterception, sts_sub_result,
      ERR_CERT_KNOWN_INTERCEPTION_BLOCKED);

  // Configure the initial context.
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCertVerifier(std::move(cert_verifier));
  auto context = context_builder->Build();

  // Enable preloaded HSTS for |kHSTSHost|.
  ASSERT_TRUE(context->transport_security_state());
  TransportSecurityState& security_state = *context->transport_security_state();
  security_state.EnableStaticPinsForTesting();
  security_state.SetPinningListAlwaysTimelyForTesting(true);
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);

  // Connect to the test server and see the certificate error flagged, but
  // not fatal.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(
        context->CreateRequest(https_server.GetURL("/"), DEFAULT_PRIORITY, &d,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_TRUE(d.request_failed());
    EXPECT_TRUE(d.have_certificate_errors());
    EXPECT_FALSE(d.certificate_errors_are_fatal());
    EXPECT_FALSE(req->ssl_info().cert_status &
                 CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  }

  // Connect to kHSTSHost and see the certificate errors are flagged, and are
  // fatal.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_server.GetURL(kHSTSHost, "/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_TRUE(d.request_failed());
    EXPECT_TRUE(d.have_certificate_errors());
    EXPECT_TRUE(d.certificate_errors_are_fatal());
    EXPECT_FALSE(req->ssl_info().cert_status &
                 CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  }

  // Verify the connection fails as being a known interception root.
  {
    TestDelegate d;
    d.set_allow_certificate_errors(true);
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_server.GetURL(kHSTSSubdomainWithKnownInterception, "/"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.request_failed());
    EXPECT_TRUE(d.have_certificate_errors());
    EXPECT_FALSE(d.certificate_errors_are_fatal());
    EXPECT_EQ(ERR_CERT_KNOWN_INTERCEPTION_BLOCKED, d.certificate_net_error());
    EXPECT_TRUE(req->ssl_info().cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  }
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(URLRequestTest, NetworkAccessedSetOnHostResolutionFailure) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto host_resolver = std::make_unique<MockHostResolver>();
  host_resolver->rules()->AddSimulatedTimeoutFailure("*");
  context_builder->set_host_resolver(std::move(host_resolver));
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_FALSE(req->response_info().network_accessed);

  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(req->response_info().network_accessed);
  EXPECT_THAT(req->response_info().resolve_error_info.error,
              IsError(ERR_DNS_TIMED_OUT));
}

// Test that URLRequest is canceled correctly.
// See http://crbug.com/508900
TEST_F(URLRequestTest, URLRequestRedirectJobCancelRequest) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://not-a-real-domain/"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  std::unique_ptr<URLRequestRedirectJob> job =
      std::make_unique<URLRequestRedirectJob>(
          req.get(), GURL("http://this-should-never-be-navigated-to/"),
          RedirectUtil::ResponseCode::REDIRECT_307_TEMPORARY_REDIRECT,
          "Jumbo shrimp");
  TestScopedURLInterceptor interceptor(req->url(), std::move(job));

  req->Start();
  req->Cancel();
  d.RunUntilComplete();
  EXPECT_EQ(ERR_ABORTED, d.request_status());
  EXPECT_EQ(0, d.received_redirect_count());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_HeadersCallbacks DISABLED_HeadersCallbacks
#else
#define MAYBE_HeadersCallbacks HeadersCallbacks
#endif
TEST_F(URLRequestTestHTTP, MAYBE_HeadersCallbacks) {
  ASSERT_TRUE(http_test_server()->Start());
  GURL url(http_test_server()->GetURL("/cachetime"));
  TestDelegate delegate;
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");

  {
    HttpRawRequestHeaders raw_req_headers;
    scoped_refptr<const HttpResponseHeaders> raw_resp_headers;

    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetExtraRequestHeaders(extra_headers);
    r->SetRequestHeadersCallback(base::BindRepeating(
        &HttpRawRequestHeaders::Assign, base::Unretained(&raw_req_headers)));
    r->SetResponseHeadersCallback(base::BindRepeating(
        [](scoped_refptr<const HttpResponseHeaders>* left,
           scoped_refptr<const HttpResponseHeaders> right) { *left = right; },
        base::Unretained(&raw_resp_headers)));
    r->set_isolation_info(isolation_info1_);
    r->Start();
    delegate.RunUntilComplete();
    EXPECT_FALSE(raw_req_headers.headers().empty());
    std::string value;
    EXPECT_TRUE(raw_req_headers.FindHeaderForTest("X-Foo", &value));
    EXPECT_EQ("bar", value);
    EXPECT_TRUE(raw_req_headers.FindHeaderForTest("Accept-Encoding", &value));
    EXPECT_EQ("gzip, deflate", value);
    EXPECT_TRUE(raw_req_headers.FindHeaderForTest("Connection", &value));
    EXPECT_TRUE(raw_req_headers.FindHeaderForTest("Host", &value));
    EXPECT_EQ("GET /cachetime HTTP/1.1\r\n", raw_req_headers.request_line());
    EXPECT_EQ(raw_resp_headers.get(), r->response_headers());
  }
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetExtraRequestHeaders(extra_headers);
    r->SetRequestHeadersCallback(base::BindRepeating([](HttpRawRequestHeaders) {
      FAIL() << "Callback should not be called unless request is sent";
    }));
    r->SetResponseHeadersCallback(
        base::BindRepeating([](scoped_refptr<const HttpResponseHeaders>) {
          FAIL() << "Callback should not be called unless request is sent";
        }));
    r->set_isolation_info(isolation_info1_);
    r->Start();
    delegate.RunUntilComplete();
    EXPECT_TRUE(r->was_cached());
  }
}

TEST_F(URLRequestTestHTTP, HeadersCallbacksWithRedirect) {
  ASSERT_TRUE(http_test_server()->Start());
  HttpRawRequestHeaders raw_req_headers;
  scoped_refptr<const HttpResponseHeaders> raw_resp_headers;

  TestDelegate delegate;
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");
  GURL url(http_test_server()->GetURL("/redirect-test.html"));
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->SetExtraRequestHeaders(extra_headers);
  r->SetRequestHeadersCallback(base::BindRepeating(
      &HttpRawRequestHeaders::Assign, base::Unretained(&raw_req_headers)));
  r->SetResponseHeadersCallback(base::BindRepeating(
      [](scoped_refptr<const HttpResponseHeaders>* left,
         scoped_refptr<const HttpResponseHeaders> right) { *left = right; },
      base::Unretained(&raw_resp_headers)));
  r->Start();
  delegate.RunUntilRedirect();

  ASSERT_EQ(1, delegate.received_redirect_count());
  std::string value;
  EXPECT_TRUE(raw_req_headers.FindHeaderForTest("X-Foo", &value));
  EXPECT_EQ("bar", value);
  EXPECT_TRUE(raw_req_headers.FindHeaderForTest("Accept-Encoding", &value));
  EXPECT_EQ("gzip, deflate", value);
  EXPECT_EQ(1, delegate.received_redirect_count());
  EXPECT_EQ("GET /redirect-test.html HTTP/1.1\r\n",
            raw_req_headers.request_line());
  EXPECT_TRUE(raw_resp_headers->HasHeader("Location"));
  EXPECT_EQ(302, raw_resp_headers->response_code());
  EXPECT_EQ("Redirect", raw_resp_headers->GetStatusText());

  raw_req_headers = HttpRawRequestHeaders();
  raw_resp_headers = nullptr;
  r->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                            std::nullopt /* modified_headers */);
  delegate.RunUntilComplete();
  EXPECT_TRUE(raw_req_headers.FindHeaderForTest("X-Foo", &value));
  EXPECT_EQ("bar", value);
  EXPECT_TRUE(raw_req_headers.FindHeaderForTest("Accept-Encoding", &value));
  EXPECT_EQ("gzip, deflate", value);
  EXPECT_EQ("GET /with-headers.html HTTP/1.1\r\n",
            raw_req_headers.request_line());
  EXPECT_EQ(r->response_headers(), raw_resp_headers.get());
}

TEST_F(URLRequestTest, HeadersCallbacksConnectFailed) {
  TestDelegate request_delegate;

  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      GURL("http://127.0.0.1:9/"), DEFAULT_PRIORITY, &request_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  r->SetRequestHeadersCallback(
      base::BindRepeating([](net::HttpRawRequestHeaders) {
        FAIL() << "Callback should not be called unless request is sent";
      }));
  r->SetResponseHeadersCallback(
      base::BindRepeating([](scoped_refptr<const net::HttpResponseHeaders>) {
        FAIL() << "Callback should not be called unless request is sent";
      }));
  r->Start();
  request_delegate.RunUntilComplete();
  EXPECT_FALSE(r->is_pending());
}

TEST_F(URLRequestTestHTTP, HeadersCallbacksAuthRetry) {
  ASSERT_TRUE(http_test_server()->Start());
  GURL url(http_test_server()->GetURL("/auth-basic"));

  TestDelegate delegate;

  delegate.set_credentials(AuthCredentials(kUser, kSecret));
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");

  using ReqHeadersVector = std::vector<std::unique_ptr<HttpRawRequestHeaders>>;
  ReqHeadersVector raw_req_headers;

  using RespHeadersVector =
      std::vector<scoped_refptr<const HttpResponseHeaders>>;
  RespHeadersVector raw_resp_headers;

  auto req_headers_callback = base::BindRepeating(
      [](ReqHeadersVector* vec, HttpRawRequestHeaders headers) {
        vec->emplace_back(
            std::make_unique<HttpRawRequestHeaders>(std::move(headers)));
      },
      &raw_req_headers);
  auto resp_headers_callback = base::BindRepeating(
      [](RespHeadersVector* vec,
         scoped_refptr<const HttpResponseHeaders> headers) {
        vec->push_back(headers);
      },
      &raw_resp_headers);
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->SetExtraRequestHeaders(extra_headers);
  r->SetRequestHeadersCallback(req_headers_callback);
  r->SetResponseHeadersCallback(resp_headers_callback);
  r->set_isolation_info(isolation_info1_);
  r->Start();
  delegate.RunUntilComplete();
  EXPECT_FALSE(r->is_pending());
  ASSERT_EQ(raw_req_headers.size(), 2u);
  ASSERT_EQ(raw_resp_headers.size(), 2u);
  std::string value;
  EXPECT_FALSE(raw_req_headers[0]->FindHeaderForTest("Authorization", &value));
  EXPECT_TRUE(raw_req_headers[0]->FindHeaderForTest("X-Foo", &value));
  EXPECT_EQ("bar", value);
  EXPECT_TRUE(raw_req_headers[1]->FindHeaderForTest("Authorization", &value));
  EXPECT_TRUE(raw_req_headers[1]->FindHeaderForTest("X-Foo", &value));
  EXPECT_EQ("bar", value);
  EXPECT_EQ(raw_resp_headers[1], r->response_headers());
  EXPECT_NE(raw_resp_headers[0], raw_resp_headers[1]);
  EXPECT_EQ(401, raw_resp_headers[0]->response_code());
  EXPECT_EQ("Unauthorized", raw_resp_headers[0]->GetStatusText());

  std::unique_ptr<URLRequest> r2(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  r2->SetExtraRequestHeaders(extra_headers);
  r2->SetRequestHeadersCallback(req_headers_callback);
  r2->SetResponseHeadersCallback(resp_headers_callback);
  r2->SetLoadFlags(LOAD_VALIDATE_CACHE);
  r2->set_isolation_info(isolation_info1_);
  r2->Start();
  delegate.RunUntilComplete();
  EXPECT_FALSE(r2->is_pending());
  ASSERT_EQ(raw_req_headers.size(), 3u);
  ASSERT_EQ(raw_resp_headers.size(), 3u);
  EXPECT_TRUE(raw_req_headers[2]->FindHeaderForTest("If-None-Match", &value));
  EXPECT_NE(raw_resp_headers[2].get(), r2->response_headers());
  EXPECT_EQ(304, raw_resp_headers[2]->response_code());
  EXPECT_EQ("Not Modified", raw_resp_headers[2]->GetStatusText());
}

TEST_F(URLRequestTest, UpgradeIfInsecureFlagSet) {
  TestDelegate d;
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test");
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::SYNCHRONOUS));
  network_delegate.set_redirect_url(kRedirectUrl);
  auto context = context_builder->Build();

  std::unique_ptr<URLRequest> r(context->CreateRequest(
      kOriginalUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->set_upgrade_if_insecure(true);
  r->Start();
  d.RunUntilRedirect();
  GURL::Replacements replacements;
  // Check that the redirect URL was upgraded to HTTPS since upgrade_if_insecure
  // was set.
  replacements.SetSchemeStr("https");
  EXPECT_EQ(kRedirectUrl.ReplaceComponents(replacements),
            d.redirect_info().new_url);
  EXPECT_TRUE(d.redirect_info().insecure_scheme_was_upgraded);
}

TEST_F(URLRequestTest, UpgradeIfInsecureFlagSetExplicitPort80) {
  TestDelegate d;
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test:80");
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::SYNCHRONOUS));
  network_delegate.set_redirect_url(kRedirectUrl);
  auto context = context_builder->Build();

  std::unique_ptr<URLRequest> r(context->CreateRequest(
      kOriginalUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->set_upgrade_if_insecure(true);
  r->Start();
  d.RunUntilRedirect();
  GURL::Replacements replacements;
  // The URL host should have not been changed.
  EXPECT_EQ(d.redirect_info().new_url.host(), kRedirectUrl.host());
  // The scheme should now be https, and the effective port should now be 443.
  EXPECT_TRUE(d.redirect_info().new_url.SchemeIs("https"));
  EXPECT_EQ(d.redirect_info().new_url.EffectiveIntPort(), 443);
  EXPECT_TRUE(d.redirect_info().insecure_scheme_was_upgraded);
}

TEST_F(URLRequestTest, UpgradeIfInsecureFlagSetNonStandardPort) {
  TestDelegate d;
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test:1234");
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::SYNCHRONOUS));
  network_delegate.set_redirect_url(kRedirectUrl);
  auto context = context_builder->Build();

  std::unique_ptr<URLRequest> r(context->CreateRequest(
      kOriginalUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->set_upgrade_if_insecure(true);
  r->Start();
  d.RunUntilRedirect();
  GURL::Replacements replacements;
  // Check that the redirect URL was upgraded to HTTPS since upgrade_if_insecure
  // was set, nonstandard port should not have been modified.
  replacements.SetSchemeStr("https");
  EXPECT_EQ(kRedirectUrl.ReplaceComponents(replacements),
            d.redirect_info().new_url);
  EXPECT_TRUE(d.redirect_info().insecure_scheme_was_upgraded);
}

TEST_F(URLRequestTest, UpgradeIfInsecureFlagNotSet) {
  TestDelegate d;
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test");
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<BlockingNetworkDelegate>(
          BlockingNetworkDelegate::SYNCHRONOUS));
  network_delegate.set_redirect_url(kRedirectUrl);
  auto context = context_builder->Build();

  std::unique_ptr<URLRequest> r(context->CreateRequest(
      kOriginalUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->set_upgrade_if_insecure(false);
  r->Start();
  d.RunUntilRedirect();
  // The redirect URL should not be changed if the upgrade_if_insecure flag is
  // not set.
  EXPECT_EQ(kRedirectUrl, d.redirect_info().new_url);
  EXPECT_FALSE(d.redirect_info().insecure_scheme_was_upgraded);
}

// Test that URLRequests get properly tagged.
#if BUILDFLAG(IS_ANDROID)
TEST_F(URLRequestTestHTTP, TestTagging) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  ASSERT_TRUE(http_test_server()->Start());

  // The tag under which the system reports untagged traffic.
  static const int32_t UNTAGGED_TAG = 0;

  uint64_t old_traffic = GetTaggedBytes(UNTAGGED_TAG);

  // Untagged traffic should be tagged with tag UNTAGGED_TAG.
  TestDelegate delegate;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(SocketTag(), req->socket_tag());
  req->Start();
  delegate.RunUntilComplete();

  EXPECT_GT(GetTaggedBytes(UNTAGGED_TAG), old_traffic);

  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  old_traffic = GetTaggedBytes(tag_val1);

  // Test specific tag value.
  req = default_context().CreateRequest(http_test_server()->GetURL("/"),
                                        DEFAULT_PRIORITY, &delegate,
                                        TRAFFIC_ANNOTATION_FOR_TESTS);
  req->set_socket_tag(tag1);
  EXPECT_EQ(tag1, req->socket_tag());
  req->Start();
  delegate.RunUntilComplete();

  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);
}
#endif

namespace {

class ReadBufferingListener
    : public test_server::EmbeddedTestServerConnectionListener {
 public:
  ReadBufferingListener() = default;
  ~ReadBufferingListener() override = default;

  void BufferNextConnection(int buffer_size) { buffer_size_ = buffer_size; }

  std::unique_ptr<StreamSocket> AcceptedSocket(
      std::unique_ptr<StreamSocket> socket) override {
    if (!buffer_size_) {
      return socket;
    }
    auto wrapped =
        std::make_unique<ReadBufferingStreamSocket>(std::move(socket));
    wrapped->BufferNextRead(buffer_size_);
    // Do not buffer subsequent connections, which may be a 0-RTT retry.
    buffer_size_ = 0;
    return wrapped;
  }

  void ReadFromSocket(const StreamSocket& socket, int rv) override {}

 private:
  int buffer_size_ = 0;
};

// Provides a response to the 0RTT request indicating whether it was received
// as early data, sending HTTP_TOO_EARLY if enabled.
class ZeroRTTResponse : public test_server::BasicHttpResponse {
 public:
  ZeroRTTResponse(bool zero_rtt, bool send_too_early)
      : zero_rtt_(zero_rtt), send_too_early_(send_too_early) {}

  ZeroRTTResponse(const ZeroRTTResponse&) = delete;
  ZeroRTTResponse& operator=(const ZeroRTTResponse&) = delete;

  ~ZeroRTTResponse() override = default;

  void SendResponse(
      base::WeakPtr<test_server::HttpResponseDelegate> delegate) override {
    AddCustomHeader("Vary", "Early-Data");
    set_content_type("text/plain");
    AddCustomHeader("Cache-Control", "no-cache");
    if (zero_rtt_) {
      if (send_too_early_)
        set_code(HTTP_TOO_EARLY);
      set_content("1");
    } else {
      set_content("0");
    }

    // Since the EmbeddedTestServer doesn't keep the socket open by default,
    // it is explicitly kept alive to allow the remaining leg of the 0RTT
    // handshake to be received after the early data.
    delegate->SendResponseHeaders(code(), GetHttpReasonPhrase(code()),
                                  BuildHeaders());
    delegate->SendContents(content(), base::DoNothing());
  }

 private:
  bool zero_rtt_;
  bool send_too_early_;
};

std::unique_ptr<test_server::HttpResponse> HandleZeroRTTRequest(
    const test_server::HttpRequest& request) {
  DCHECK(request.ssl_info);

  if (request.GetURL().path() != "/zerortt")
    return nullptr;
  return std::make_unique<ZeroRTTResponse>(
      request.ssl_info->early_data_received, false);
}

}  // namespace

class HTTPSEarlyDataTest : public TestWithTaskEnvironment {
 public:
  HTTPSEarlyDataTest() : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    HttpNetworkSessionParams params;
    params.enable_early_data = true;

    auto cert_verifier = std::make_unique<MockCertVerifier>();
    cert_verifier->set_default_result(OK);

    SSLContextConfig config;
    config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;

    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_http_network_session_params(params);
    context_builder->SetCertVerifier(std::move(cert_verifier));
    context_builder->set_ssl_config_service(
        std::make_unique<TestSSLConfigService>(config));
    context_ = context_builder->Build();

    test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_OK,
        CreateSSLServerConfig(SSL_PROTOCOL_VERSION_TLS1_3));
    RegisterDefaultHandlers(&test_server_);
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleZeroRTTRequest));
    test_server_.SetConnectionListener(&listener_);
  }

  ~HTTPSEarlyDataTest() override = default;

  URLRequestContext& context() { return *context_; }

  static SSLServerConfig CreateSSLServerConfig(uint16_t version) {
    SSLServerConfig ssl_config;
    ssl_config.version_max = version;
    ssl_config.early_data_enabled = true;
    return ssl_config;
  }

  void ResetSSLConfig(net::EmbeddedTestServer::ServerCertificate cert,
                      uint16_t version) {
    SSLServerConfig ssl_config = CreateSSLServerConfig(version);
    test_server_.ResetSSLConfig(cert, ssl_config);
  }

 protected:
  std::unique_ptr<URLRequestContext> context_;

  ReadBufferingListener listener_;
  EmbeddedTestServer test_server_;
};

// TLSEarlyDataTest tests that we handle early data correctly.
TEST_F(HTTPSEarlyDataTest, TLSEarlyDataTest) {
  ASSERT_TRUE(test_server_.Start());
  context().http_transaction_factory()->GetSession()->ClearSSLSessionCache();

  // kParamSize must be larger than any ClientHello sent by the client, but
  // smaller than the maximum amount of early data allowed by the server.
  const int kParamSize = 4 * 1024;
  const GURL kUrl =
      test_server_.GetURL("/zerortt?" + std::string(kParamSize, 'a'));

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        kUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }

  context().http_transaction_factory()->GetSession()->CloseAllConnections(
      ERR_FAILED, "Very good reason");

  // 0-RTT inherently involves a race condition: if the server responds with the
  // ServerHello before the client sends the HTTP request (the client may be
  // busy verifying a certificate), the client will send data over 1-RTT keys
  // rather than 0-RTT.
  //
  // This test ensures 0-RTT is sent if relevant by making the test server wait
  // for both the ClientHello and 0-RTT HTTP request before responding. We use
  // a ReadBufferingStreamSocket and enable buffering for the 0-RTT request. The
  // buffer size must be larger than the ClientHello but smaller than the
  // ClientHello combined with the HTTP request.
  listener_.BufferNextConnection(kParamSize);

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        kUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be a single '1' in the resumed request, and
    // the handler should return "1".
    EXPECT_EQ("1", d.data_received());
  }
}

// TLSEarlyDataTest tests that we handle early data correctly for POST.
TEST_F(HTTPSEarlyDataTest, TLSEarlyDataPOSTTest) {
  ASSERT_TRUE(test_server_.Start());
  context().http_transaction_factory()->GetSession()->ClearSSLSessionCache();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }

  context().http_transaction_factory()->GetSession()->CloseAllConnections(
      ERR_FAILED, "Very good reason");

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method("POST");
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the request, since we don't
    // send POSTs over early data, and the handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }
}

// TLSEarlyDataTest tests that the 0-RTT is enabled for idempotent POST request.
TEST_F(HTTPSEarlyDataTest, TLSEarlyDataIdempotentPOSTTest) {
  ASSERT_TRUE(test_server_.Start());
  context().http_transaction_factory()->GetSession()->ClearSSLSessionCache();
  const int kParamSize = 4 * 1024;
  const GURL kUrl =
      test_server_.GetURL("/zerortt?" + std::string(kParamSize, 'a'));

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        kUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }

  context().http_transaction_factory()->GetSession()->CloseAllConnections(
      ERR_FAILED, "Very good reason");
  listener_.BufferNextConnection(kParamSize);

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        kUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method("POST");
    r->SetIdempotency(net::IDEMPOTENT);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be set since the request is set as an
    // idempotent POST request.
    EXPECT_EQ("1", d.data_received());
  }
}

// TLSEarlyDataTest tests that the 0-RTT is disabled for non-idempotent request.
TEST_F(HTTPSEarlyDataTest, TLSEarlyDataNonIdempotentRequestTest) {
  ASSERT_TRUE(test_server_.Start());
  context().http_transaction_factory()->GetSession()->ClearSSLSessionCache();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }

  context().http_transaction_factory()->GetSession()->CloseAllConnections(
      ERR_FAILED, "Very good reason");

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    // Sets the GET request as not idempotent.
    r->SetIdempotency(net::NOT_IDEMPOTENT);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request even
    // though it is a GET request, since the request is set as not idempotent.
    EXPECT_EQ("0", d.data_received());
  }
}

std::unique_ptr<test_server::HttpResponse> HandleTooEarly(
    bool* sent_425,
    const test_server::HttpRequest& request) {
  DCHECK(request.ssl_info);

  if (request.GetURL().path() != "/tooearly")
    return nullptr;
  if (request.ssl_info->early_data_received)
    *sent_425 = true;
  return std::make_unique<ZeroRTTResponse>(
      request.ssl_info->early_data_received, true);
}

// Test that we handle 425 (Too Early) correctly.
TEST_F(HTTPSEarlyDataTest, TLSEarlyDataTooEarlyTest) {
  bool sent_425 = false;
  test_server_.RegisterRequestHandler(
      base::BindRepeating(&HandleTooEarly, base::Unretained(&sent_425)));
  ASSERT_TRUE(test_server_.Start());
  context().http_transaction_factory()->GetSession()->ClearSSLSessionCache();

  // kParamSize must be larger than any ClientHello sent by the client, but
  // smaller than the maximum amount of early data allowed by the server.
  const int kParamSize = 4 * 1024;
  const GURL kUrl =
      test_server_.GetURL("/tooearly?" + std::string(kParamSize, 'a'));

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        kUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
    EXPECT_FALSE(sent_425);
  }

  context().http_transaction_factory()->GetSession()->CloseAllConnections(
      ERR_FAILED, "Very good reason");

  // 0-RTT inherently involves a race condition: if the server responds with the
  // ServerHello before the client sends the HTTP request (the client may be
  // busy verifying a certificate), the client will send data over 1-RTT keys
  // rather than 0-RTT.
  //
  // This test ensures 0-RTT is sent if relevant by making the test server wait
  // for both the ClientHello and 0-RTT HTTP request before responding. We use
  // a ReadBufferingStreamSocket and enable buffering for the 0-RTT request. The
  // buffer size must be larger than the ClientHello but smaller than the
  // ClientHello combined with the HTTP request.
  //
  // We must buffer exactly one connection because the HTTP 425 response will
  // trigger a retry, potentially on a new connection.
  listener_.BufferNextConnection(kParamSize);

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        kUrl, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The resumption request will encounter a 425 error and retry without early
    // data, and the handler should return "0".
    EXPECT_EQ("0", d.data_received());
    EXPECT_TRUE(sent_425);
  }
}

// TLSEarlyDataRejectTest tests that we gracefully handle an early data reject
// and retry without early data.
TEST_F(HTTPSEarlyDataTest, TLSEarlyDataRejectTest) {
  ASSERT_TRUE(test_server_.Start());
  context().http_transaction_factory()->GetSession()->ClearSSLSessionCache();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }

  context().http_transaction_factory()->GetSession()->CloseAllConnections(
      ERR_FAILED, "Very good reason");

  // The certificate in the resumption is changed to confirm that the
  // certificate change is observed.
  scoped_refptr<X509Certificate> old_cert = test_server_.GetCertificate();
  ResetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED,
                 SSL_PROTOCOL_VERSION_TLS1_3);

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));
    EXPECT_FALSE(old_cert->EqualsIncludingChain(r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the rejected request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }
}

// TLSEarlyDataTLS12RejectTest tests that we gracefully handle an early data
// reject from a TLS 1.2 server and retry without early data.
TEST_F(HTTPSEarlyDataTest, TLSEarlyDataTLS12RejectTest) {
  ASSERT_TRUE(test_server_.Start());
  context().http_transaction_factory()->GetSession()->ClearSSLSessionCache();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the initial request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }

  context().http_transaction_factory()->GetSession()->CloseAllConnections(
      ERR_FAILED, "Very good reason");

  // The certificate in the resumption is changed to confirm that the
  // certificate change is observed.
  scoped_refptr<X509Certificate> old_cert = test_server_.GetCertificate();
  ResetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED,
                 SSL_PROTOCOL_VERSION_TLS1_2);

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context().CreateRequest(
        test_server_.GetURL("/zerortt"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());

    EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_2,
              SSLConnectionStatusToVersion(r->ssl_info().connection_status));
    EXPECT_TRUE(r->ssl_info().unverified_cert.get());
    EXPECT_TRUE(test_server_.GetCertificate()->EqualsIncludingChain(
        r->ssl_info().cert.get()));
    EXPECT_FALSE(old_cert->EqualsIncludingChain(r->ssl_info().cert.get()));

    // The Early-Data header should be omitted in the rejected request, and the
    // handler should return "0".
    EXPECT_EQ("0", d.data_received());
  }
}

// Tests that AuthChallengeInfo is available on the request.
TEST_F(URLRequestTestHTTP, AuthChallengeInfo) {
  ASSERT_TRUE(http_test_server()->Start());
  GURL url(http_test_server()->GetURL("/auth-basic"));

  TestDelegate delegate;

  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->Start();
  delegate.RunUntilComplete();
  ASSERT_TRUE(r->auth_challenge_info().has_value());
  EXPECT_FALSE(r->auth_challenge_info()->is_proxy);
  EXPECT_EQ(url::SchemeHostPort(url), r->auth_challenge_info()->challenger);
  EXPECT_EQ("basic", r->auth_challenge_info()->scheme);
  EXPECT_EQ("testrealm", r->auth_challenge_info()->realm);
  EXPECT_EQ("Basic realm=\"testrealm\"", r->auth_challenge_info()->challenge);
  EXPECT_EQ("/auth-basic", r->auth_challenge_info()->path);
}

TEST_F(URLRequestTestHTTP, ConnectNoSupported) {
  ASSERT_TRUE(http_test_server()->Start());
  TestDelegate delegate;
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      http_test_server()->GetURL("/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  r->set_method("CONNECT");
  r->Start();
  delegate.RunUntilComplete();
  EXPECT_EQ(ERR_METHOD_NOT_SUPPORTED, delegate.request_status());
}

class URLRequestDnsAliasTest : public TestWithTaskEnvironment {
 protected:
  URLRequestDnsAliasTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_host_resolver(std::make_unique<MockHostResolver>());
    context_ = context_builder->Build();
  }

  URLRequestContext& context() { return *context_; }
  MockHostResolver& host_resolver() {
    // This cast is safe because we provided a MockHostResolver in the ctor.
    return *static_cast<MockHostResolver*>(context_->host_resolver());
  }

  void SetUp() override { ASSERT_TRUE(test_server_.Start()); }

  std::unique_ptr<URLRequestContext> context_;
  TestDelegate test_delegate_;
  EmbeddedTestServer test_server_;
};

TEST_F(URLRequestDnsAliasTest, WithDnsAliases) {
  GURL url(test_server_.GetURL("www.example.test", "/echo"));
  std::vector<std::string> aliases({"alias1", "alias2", "host"});
  host_resolver().rules()->AddIPLiteralRuleWithDnsAliases(
      "www.example.test", "127.0.0.1", std::move(aliases));

  std::unique_ptr<URLRequest> request(context().CreateRequest(
      url, DEFAULT_PRIORITY, &test_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();

  test_delegate_.RunUntilComplete();
  EXPECT_THAT(test_delegate_.request_status(), IsOk());
  EXPECT_THAT(request->response_info().dns_aliases,
              testing::ElementsAre("alias1", "alias2", "host"));
}

TEST_F(URLRequestDnsAliasTest, NoAdditionalDnsAliases) {
  GURL url(test_server_.GetURL("www.example.test", "/echo"));
  host_resolver().rules()->AddIPLiteralRuleWithDnsAliases(
      "www.example.test", "127.0.0.1", /*dns_aliases=*/std::set<std::string>());

  std::unique_ptr<URLRequest> request(context().CreateRequest(
      url, DEFAULT_PRIORITY, &test_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS));

  request->Start();

  test_delegate_.RunUntilComplete();
  EXPECT_THAT(test_delegate_.request_status(), IsOk());
  EXPECT_THAT(request->response_info().dns_aliases,
              testing::ElementsAre("www.example.test"));
}

TEST_F(URLRequestTest, SetURLChain) {
  TestDelegate d;
  {
    GURL original_url("http://localhost");
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    EXPECT_EQ(r->url_chain().size(), 1u);
    EXPECT_EQ(r->url_chain()[0], original_url);

    const std::vector<GURL> url_chain = {
        GURL("http://foo.test"),
        GURL("http://bar.test"),
        GURL("http://baz.test"),
    };

    r->SetURLChain(url_chain);

    EXPECT_EQ(r->url_chain().size(), 3u);
    EXPECT_EQ(r->url_chain()[0], url_chain[0]);
    EXPECT_EQ(r->url_chain()[1], url_chain[1]);
    EXPECT_EQ(r->url_chain()[2], original_url);
  }
}

TEST_F(URLRequestTest, SetIsolationInfoFromNak) {
  TestDelegate d;
  SchemefulSite site_a = SchemefulSite(GURL("https://a.com/"));
  SchemefulSite site_b = SchemefulSite(GURL("https://b.com/"));
  base::UnguessableToken nak_nonce = base::UnguessableToken::Create();
  auto populated_cross_site_nak = NetworkAnonymizationKey::CreateFromParts(
      site_a, /*is_cross_site=*/true, nak_nonce);
  IsolationInfo expected_isolation_info_populated_cross_site_nak =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            url::Origin::Create(GURL("https://a.com/")),
                            url::Origin(), SiteForCookies(), nak_nonce);

  auto populated_same_site_nak = NetworkAnonymizationKey::CreateFromParts(
      site_a, /*is_cross_site=*/false, nak_nonce);
  IsolationInfo expected_isolation_info_populated_same_site_nak =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            url::Origin::Create(GURL("https://a.com/")),
                            url::Origin::Create(GURL("https://a.com/")),
                            SiteForCookies(), nak_nonce);

  NetworkAnonymizationKey empty_nak;
  GURL original_url("http://localhost");
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->set_isolation_info_from_network_anonymization_key(
      populated_cross_site_nak);
  r->SetLoadFlags(LOAD_DISABLE_CACHE);
  r->set_allow_credentials(false);
  EXPECT_TRUE(r->is_created_from_network_anonymization_key());
  EXPECT_EQ(r->isolation_info().network_anonymization_key(),
            populated_cross_site_nak);
  EXPECT_EQ(
      r->isolation_info().top_frame_origin(),
      expected_isolation_info_populated_cross_site_nak.top_frame_origin());
  // The cross-site bit in the NAK causes the IsolationInfo's NIK to have, an
  // anonymous origin, but the bit is not enough to reconstruct a different
  // frame_site.
  EXPECT_FALSE(r->isolation_info().IsEqualForTesting(
      expected_isolation_info_populated_cross_site_nak));

  r->set_isolation_info_from_network_anonymization_key(populated_same_site_nak);
  EXPECT_TRUE(r->is_created_from_network_anonymization_key());
  EXPECT_EQ(r->isolation_info().network_anonymization_key(),
            populated_same_site_nak);
  EXPECT_TRUE(r->isolation_info().IsEqualForTesting(
      expected_isolation_info_populated_same_site_nak));

  r->set_isolation_info_from_network_anonymization_key(empty_nak);
  EXPECT_TRUE(r->is_created_from_network_anonymization_key());
  EXPECT_EQ(r->isolation_info().network_anonymization_key(), empty_nak);
  EXPECT_TRUE(r->isolation_info().IsEqualForTesting(net::IsolationInfo()));
  r->Start();
  d.RunUntilComplete();
}

TEST_F(URLRequestTest, CookiePartitionKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAncestorChainBitEnabledInPartitionedCookies);

  const url::Origin kOrigin = url::Origin::Create(GURL("http://foo.test/"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(GURL("http://b.test/"));

  struct {
    const GURL request_url;
    IsolationInfo::RequestType request_type;
    const url::Origin frame_origin;
    const SiteForCookies site_for_cookies;
    const url::Origin initiator;
    bool expected_third_party;
    // If present, change the initiator
    std::optional<GURL> change_initator = std::nullopt;

  } cases[]{
      // Request from the main frame: first party partitioned
      {GURL("ws://foo.test/"), IsolationInfo::RequestType::kMainFrame, kOrigin,
       SiteForCookies::FromOrigin(kOrigin), kOrigin, false},
      // Request from the main frame with 3rd party initiator: first party
      // partitioned
      {GURL("ws://foo.test/"), IsolationInfo::RequestType::kMainFrame, kOrigin,
       SiteForCookies::FromOrigin(kOrigin), kOrigin, false,
       GURL("ws://b.test/")},
      // Request from first party subframe to cross-site subframe: third party
      // partitioned
      {GURL("ws://foo.test/"), IsolationInfo::RequestType::kSubFrame, kOrigin,
       SiteForCookies(), kCrossSiteOrigin, true},
      // Request from cross-site subframe: third party partitioned
      {GURL("ws://b.test/"), IsolationInfo::RequestType::kSubFrame,
       kCrossSiteOrigin, SiteForCookies(), kCrossSiteOrigin, true},
      // Request from cross-site subframe with 1st party initiator: third party
      // partitioned
      {GURL("ws://b.test/"), IsolationInfo::RequestType::kSubFrame,
       kCrossSiteOrigin, SiteForCookies(), kCrossSiteOrigin, true,
       GURL("ws://foo.test/")},
      // Check that mismatch between request initiator and SiteForCookies: third
      // party partitioned
      {GURL("ws://b.test/"), IsolationInfo::RequestType::kSubFrame,
       kCrossSiteOrigin, SiteForCookies::FromOrigin(kOrigin), kCrossSiteOrigin,
       true},
      // Request from first party subframe with null SiteForCookies indicating
      // A1->B->A2 embed: third party partitioned.
      {GURL("ws://foo.test/"), IsolationInfo::RequestType::kSubFrame, kOrigin,
       SiteForCookies(), kOrigin, true},
  };

  for (const auto& tc : cases) {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        tc.request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->set_isolation_info(IsolationInfo::Create(
        tc.request_type, kOrigin, tc.frame_origin, tc.site_for_cookies));

    if (tc.change_initator.has_value()) {
      r->set_initiator(url::Origin::Create(tc.change_initator.value()));
    }
    EXPECT_TRUE(r->cookie_partition_key().has_value());
    EXPECT_EQ(r->cookie_partition_key()->site(), SchemefulSite(kOrigin));
    EXPECT_EQ(r->cookie_partition_key()->IsThirdParty(),
              tc.expected_third_party);
  }
}

class URLRequestMaybeAsyncFirstPartySetsTest
    : public URLRequestTest,
      public testing::WithParamInterface<bool> {
 public:
  URLRequestMaybeAsyncFirstPartySetsTest() { CHECK(test_server_.Start()); }

  std::unique_ptr<CookieStore> CreateCookieStore() {
    auto cookie_monster = std::make_unique<CookieMonster>(/*store=*/nullptr,
                                                          /*net_log=*/nullptr);
    auto cookie_access_delegate = std::make_unique<TestCookieAccessDelegate>();
    cookie_access_delegate->set_invoke_callbacks_asynchronously(
        invoke_callbacks_asynchronously());
    cookie_monster->SetCookieAccessDelegate(std::move(cookie_access_delegate));
    return cookie_monster;
  }

  bool invoke_callbacks_asynchronously() { return GetParam(); }

  HttpTestServer& test_server() { return test_server_; }

 private:
  HttpTestServer test_server_;
};

TEST_P(URLRequestMaybeAsyncFirstPartySetsTest, SimpleRequest) {
  const std::string kHost = "example.test";
  const url::Origin kOrigin =
      url::Origin::Create(test_server().GetURL(kHost, "/"));
  const SiteForCookies kSiteForCookies = SiteForCookies::FromOrigin(kOrigin);

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(CreateCookieStore());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      test_server().GetURL(kHost, "/echo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_isolation_info(
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                            kOrigin, kSiteForCookies));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(d.data_received(), "Echo");
  EXPECT_THAT(d.request_status(), IsOk());
  EXPECT_EQ(req->GetResponseCode(), 200);
}

TEST_P(URLRequestMaybeAsyncFirstPartySetsTest, SingleRedirect) {
  const std::string kHost = "example.test";
  const url::Origin kOrigin =
      url::Origin::Create(test_server().GetURL(kHost, "/"));
  const SiteForCookies kSiteForCookies = SiteForCookies::FromOrigin(kOrigin);

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(CreateCookieStore());
  auto context = context_builder->Build();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context->CreateRequest(
      test_server().GetURL(kHost,
                           base::StrCat({
                               "/server-redirect?",
                               test_server().GetURL(kHost, "/echo").spec(),
                           })),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_isolation_info(
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin,
                            kOrigin, kSiteForCookies));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(d.data_received(), "Echo");
  EXPECT_THAT(d.request_status(), IsOk());
  EXPECT_EQ(req->GetResponseCode(), 200);
}

INSTANTIATE_TEST_SUITE_P(,
                         URLRequestMaybeAsyncFirstPartySetsTest,
                         testing::Bool());

class PartitionConnectionsByNetworkAnonymizationKey : public URLRequestTest {
 public:
  PartitionConnectionsByNetworkAnonymizationKey() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
  const SchemefulSite kTestSiteC = SchemefulSite(GURL("http://c.test/"));
  const base::UnguessableToken kNonceA = base::UnguessableToken::Create();
  const base::UnguessableToken kNonceB = base::UnguessableToken::Create();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PartitionConnectionsByNetworkAnonymizationKey,
       DifferentTopFrameSitesNeverShareConnections) {
  // Start server
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto original_url = test_server.GetURL("/echo");
  const auto network_anonymization_key1 =
      NetworkAnonymizationKey::CreateSameSite(kTestSiteA);
  const auto network_anonymization_key2 =
      NetworkAnonymizationKey::CreateSameSite(kTestSiteB);

  // Create a request from first party `kTestSiteA`.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r1(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r1->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key1);
    r1->SetLoadFlags(LOAD_DISABLE_CACHE);
    r1->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly
    EXPECT_TRUE(r1->is_created_from_network_anonymization_key());
    EXPECT_EQ(r1->isolation_info().network_anonymization_key(),
              network_anonymization_key1);
    // Run request
    r1->Start();
    d.RunUntilComplete();

    // Verify request started with a full handshake
    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r1->ssl_info().handshake_type);
  }

  // Create a request from first party `kTestSiteB`. This request should never
  // share a key with r1 regardless of the NIK/NAK key schemes.
  {
    TestDelegate d;
    // Create request and create IsolationInfo from
    // `network_anonymization_key2`
    std::unique_ptr<URLRequest> r2(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r2->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key2);
    r2->SetLoadFlags(LOAD_DISABLE_CACHE);
    r2->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly.
    EXPECT_TRUE(r2->is_created_from_network_anonymization_key());
    EXPECT_EQ(r2->isolation_info().network_anonymization_key(),
              network_anonymization_key2);
    // Run request
    r2->Start();
    d.RunUntilComplete();

    // Verify request started with a full handshake
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r2->ssl_info().handshake_type);
  }
}

TEST_F(PartitionConnectionsByNetworkAnonymizationKey,
       FirstPartyIsSeparatedFromCrossSiteFrames) {
  // Start server
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto original_url = test_server.GetURL("/echo");
  const auto network_anonymization_key1 =
      NetworkAnonymizationKey::CreateSameSite(kTestSiteA);
  const auto network_anonymization_key2 =
      NetworkAnonymizationKey::CreateCrossSite(kTestSiteA);

  // Create a request from first party `kTestSiteA`.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r1(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r1->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key1);
    r1->SetLoadFlags(LOAD_DISABLE_CACHE);
    r1->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly
    EXPECT_TRUE(r1->is_created_from_network_anonymization_key());
    EXPECT_EQ(r1->isolation_info().network_anonymization_key(),
              network_anonymization_key1);
    // Run request
    r1->Start();
    d.RunUntilComplete();
    // Verify request started with a full handshake
    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r1->ssl_info().handshake_type);
  }

  // Create a request from third party `kTestSiteB` embedded in `kTestSiteA`.
  // This request should share a key with r1 when NetworkAnonymizationKey is in
  // double keyed scheme and should not share a key with r1 when
  // NetworkAnonymizationKey is triple keyed or in cross site flag scheme.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r2(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r2->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key2);
    r2->SetLoadFlags(LOAD_DISABLE_CACHE);
    r2->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly.
    EXPECT_TRUE(r2->is_created_from_network_anonymization_key());
    EXPECT_EQ(r2->isolation_info().network_anonymization_key(),
              network_anonymization_key2);
    // Run request
    r2->Start();
    d.RunUntilComplete();

    EXPECT_THAT(d.request_status(), IsOk());
    // We should not share a connection with r1.
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r2->ssl_info().handshake_type);
  }
}

TEST_F(
    PartitionConnectionsByNetworkAnonymizationKey,
    DifferentCrossSiteFramesAreSeparatedOnlyWhenNetworkAnonymizationKeyIsTripleKeyed) {
  // Start server
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto original_url = test_server.GetURL("/echo");
  const auto network_anonymization_key1 =
      NetworkAnonymizationKey::CreateCrossSite(kTestSiteA);
  const auto network_anonymization_key2 =
      NetworkAnonymizationKey::CreateCrossSite(kTestSiteA);

  // Create a request from first party `kTestSiteA`.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r1(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r1->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key1);
    r1->SetLoadFlags(LOAD_DISABLE_CACHE);
    r1->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly
    EXPECT_TRUE(r1->is_created_from_network_anonymization_key());
    EXPECT_EQ(r1->isolation_info().network_anonymization_key(),
              network_anonymization_key1);
    // Run request
    r1->Start();
    d.RunUntilComplete();
    // Verify request started with a full handshake
    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r1->ssl_info().handshake_type);
  }

  // Create a request from third party `kTestSiteB` embedded in `kTestSiteA`.
  // This request should share a key with r1 when NetworkAnonymizationKey is in
  // double keyed scheme and should not share a key with r1 when
  // NetworkAnonymizationKey is triple keyed or in cross site flag scheme.
  {
    TestDelegate d;
    // Create request and create IsolationInfo from
    // `network_anonymization_key2`
    std::unique_ptr<URLRequest> r2(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r2->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key2);
    r2->SetLoadFlags(LOAD_DISABLE_CACHE);
    r2->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly.
    EXPECT_TRUE(r2->is_created_from_network_anonymization_key());
    EXPECT_EQ(r2->isolation_info().network_anonymization_key(),
              network_anonymization_key2);
    // Run request
    r2->Start();
    d.RunUntilComplete();

    EXPECT_THAT(d.request_status(), IsOk());
    // We should share a connection with r1
    EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, r2->ssl_info().handshake_type);
  }
}

TEST_F(PartitionConnectionsByNetworkAnonymizationKey,
       DifferentNoncesAreAlwaysSeparated) {
  // Start server
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());
  const auto original_url = test_server.GetURL("/echo");
  const auto network_anonymization_key1 =
      NetworkAnonymizationKey::CreateFromParts(
          kTestSiteA, /*is_cross_site=*/false, kNonceA);
  const auto network_anonymization_key2 =
      NetworkAnonymizationKey::CreateFromParts(
          kTestSiteA, /*is_cross_site=*/false, kNonceB);

  // Create a request from first party `kTestSiteA`.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r1(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r1->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key1);
    r1->SetLoadFlags(LOAD_DISABLE_CACHE);
    r1->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly
    EXPECT_TRUE(r1->is_created_from_network_anonymization_key());
    EXPECT_EQ(r1->isolation_info().network_anonymization_key(),
              network_anonymization_key1);
    // Run request
    r1->Start();
    d.RunUntilComplete();
    // Verify request started with a full handshake
    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r1->ssl_info().handshake_type);
  }

  // Create a request from third party `kTestSiteB` embedded in `kTestSiteA`.
  // This request should share a key with r1 when NetworkAnonymizationKey is in
  // double keyed scheme and should not share a key with r1 when
  // NetworkAnonymizationKey is triple keyed or in cross site flag scheme.
  {
    TestDelegate d;
    // Create request and create IsolationInfo from
    // `network_anonymization_key2`
    std::unique_ptr<URLRequest> r2(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r2->set_isolation_info_from_network_anonymization_key(
        network_anonymization_key2);
    r2->SetLoadFlags(LOAD_DISABLE_CACHE);
    r2->set_allow_credentials(false);

    // Verify NetworkAnonymizationKey is set correctly.
    EXPECT_TRUE(r2->is_created_from_network_anonymization_key());
    EXPECT_EQ(r2->isolation_info().network_anonymization_key(),
              network_anonymization_key2);
    // Run request
    r2->Start();
    d.RunUntilComplete();

    EXPECT_THAT(d.request_status(), IsOk());
    // Connections where the NetworkAnonymizationKey has different nonces should
    // always be separated regardless of scheme
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, r2->ssl_info().handshake_type);
  }
}

class PatternedExpectBypassCacheNetworkDelegate : public TestNetworkDelegate {
 public:
  explicit PatternedExpectBypassCacheNetworkDelegate(
      std::vector<bool> expectations,
      bool enable_storage_access_header)
      : expectations_(std::move(expectations)) {
    set_is_storage_access_header_enabled(enable_storage_access_header);
  }

  ~PatternedExpectBypassCacheNetworkDelegate() override {
    EXPECT_TRUE(expectations_.empty());
  }

  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override {
    CHECK(!expectations_.empty());
    EXPECT_EQ(!!(request->load_flags() & LOAD_BYPASS_CACHE),
              expectations_.front());
    expectations_.erase(expectations_.begin());

    return TestNetworkDelegate::OnBeforeURLRequest(request, std::move(callback),
                                                   new_url);
  }

 private:
  std::vector<bool> expectations_;
};

class StorageAccessHeaderURLRequestTest : public URLRequestTestHTTP {
 public:
  StorageAccessHeaderURLRequestTest() = default;

  ~StorageAccessHeaderURLRequestTest() override {
    EXPECT_TRUE(http_test_server()->ShutdownAndWaitUntilComplete());
    {
      base::AutoLock auto_lock(lock_);
      EXPECT_TRUE(response_sequence_.empty());
    }
  }

  void SetUp() override {
    URLRequestTestHTTP::SetUp();
    http_test_server()->RegisterRequestHandler(base::BindRepeating(
        &StorageAccessHeaderURLRequestTest::HandleRetryRequest,
        base::Unretained(this)));
    ASSERT_TRUE(http_test_server()->Start());
  }

  enum class ResponseKind {
    kOk,
    kRedirect,
    kAuthChallenge,
    kExpectAuthCredentials,
    kAuthWithoutChallenge,
  };

  void set_response_sequence(const std::initializer_list<ResponseKind>& kinds) {
    base::AutoLock auto_lock(lock_);
    response_sequence_ = kinds;
  }

  void set_activate_storage_access_value(std::string_view value) {
    base::AutoLock auto_lock(lock_);
    activate_storage_access_value_ = value;
  }

 protected:
  static constexpr char kStorageAccessRetryPath[] =
      "/retry-with-storage-access";

 private:
  std::unique_ptr<test_server::HttpResponse> HandleRetryRequest(
      const test_server::HttpRequest& request) {
    if (!base::StartsWith(request.GetURL().path(), kStorageAccessRetryPath)) {
      return nullptr;
    }
    auto http_response = std::make_unique<test_server::BasicHttpResponse>();
    http_response->set_content_type("text/plain");

    // We add this header in all cases (including auth challenges and redirects)
    // in order to verify that it's ignored for auth challenges, and respected
    // for redirects.
    ResponseKind response_kind;
    {
      base::AutoLock auto_lock(lock_);
      http_response->AddCustomHeader("Activate-Storage-Access",
                                     activate_storage_access_value_);

      CHECK(!response_sequence_.empty());
      response_kind = response_sequence_.front();
      response_sequence_.erase(response_sequence_.begin());
    }
    switch (response_kind) {
      case ResponseKind::kOk:
        http_response->set_code(HTTP_OK);
        return http_response;
      case ResponseKind::kRedirect:
        http_response->set_code(HTTP_TEMPORARY_REDIRECT);
        http_response->AddCustomHeader(
            "Location", http_test_server()->GetURL("/echo").spec());
        return http_response;
      case ResponseKind::kAuthChallenge:
        return HandleAuthChallenge(request, std::move(http_response));
      case ResponseKind::kExpectAuthCredentials:
        return HandleExpectAuthCredentials(request, std::move(http_response));
      case ResponseKind::kAuthWithoutChallenge:
        http_response->set_code(HTTP_UNAUTHORIZED);
        return http_response;
    }
  }

  static std::unique_ptr<test_server::HttpResponse> HandleAuthChallenge(
      const test_server::HttpRequest& request,
      std::unique_ptr<test_server::BasicHttpResponse> http_response) {
    EXPECT_FALSE(base::Contains(request.headers, "Authorization"));

    http_response->set_code(HTTP_UNAUTHORIZED);
    http_response->AddCustomHeader("WWW-Authenticate", "Basic realm=\"REALM\"");
    return http_response;
  }

  static std::unique_ptr<test_server::HttpResponse> HandleExpectAuthCredentials(
      const test_server::HttpRequest& request,
      std::unique_ptr<test_server::BasicHttpResponse> http_response) {
    // Logic stolen from
    // //net/test/embedded_test_server/default_handlers.cc
    // (HandleBasicAuth).

    EXPECT_TRUE(base::Contains(request.headers, "Authorization"));
    std::string auth = request.headers.at("Authorization");
    EXPECT_TRUE(base::Contains(auth, "Basic"));
    std::string b64str = auth.substr(std::string("Basic ").size());
    std::string userpass;
    base::Base64Decode(b64str, &userpass);
    size_t delimiter = userpass.find(":");
    EXPECT_NE(delimiter, std::string::npos);
    EXPECT_EQ(userpass.substr(0, delimiter), base::UTF16ToASCII(kUser));
    EXPECT_EQ(userpass.substr(delimiter + 1), base::UTF16ToASCII(kSecret));

    http_response->set_code(HTTP_OK);
    return http_response;
  }

  base::Lock lock_;
  std::vector<ResponseKind> response_sequence_ GUARDED_BY(lock_);

  std::string activate_storage_access_value_ GUARDED_BY(lock_) =
      "retry; allowed-origin=*";
};

struct StorageAccessHeaderRetryData {
  std::optional<url::Origin> origin_header;
  std::string activate_storage_access_value;

  bool expect_retry;
};

class StorageAccessHeaderRetryURLRequestTest
    : public StorageAccessHeaderURLRequestTest,
      public testing::WithParamInterface<StorageAccessHeaderRetryData> {};

TEST_P(StorageAccessHeaderRetryURLRequestTest, StorageAccessHeaderRetry) {
  const StorageAccessHeaderRetryData test = GetParam();
  set_activate_storage_access_value(test.activate_storage_access_value);

  std::vector<bool> pattern;
  if (test.expect_retry) {
    set_response_sequence({ResponseKind::kOk, ResponseKind::kOk});
    pattern = {false, true};
  } else {
    set_response_sequence({ResponseKind::kOk});
    pattern = {false};
  }

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<PatternedExpectBypassCacheNetworkDelegate>(
          pattern,
          /*enable_storage_access_header=*/true));
  auto context = context_builder->Build();
  TestDelegate d;
  base::HistogramTester histogram_tester;

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL(kStorageAccessRetryPath), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  if (test.origin_header) {
    req->SetExtraRequestHeaderByName(HttpRequestHeaders::kOrigin,
                                     test.origin_header->Serialize(),
                                     /*overwrite=*/true);
  }
  req->set_storage_access_status(cookie_util::StorageAccessStatus::kInactive);

  req->Start();
  d.RunUntilComplete();

  if (test.expect_retry) {
    // Expect 4 records for 2 requests, since each request records the overrides
    // in both `OnForcePrivacyMode` and in
    // `OnAnnotateAndMoveUserBlockedCookies`.
    EXPECT_THAT(
        network_delegate.cookie_setting_overrides_records(),
        ElementsAre(
            CookieSettingOverrides(), CookieSettingOverrides(),
            CookieSettingOverrides(
                {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader}),
            CookieSettingOverrides(
                {CookieSettingOverride::
                     kStorageAccessGrantEligibleViaHeader})));
    histogram_tester.ExpectBucketCount(
        "API.StorageAccessHeader.ActivateStorageAccessRetryOutcome",
        /*sample=*/
        net::cookie_util::ActivateStorageAccessRetryOutcome::kSuccess,
        /*expected_count=*/1);
    // We expect this record since the retried response still includes the
    // header, but it doesn't result in a successful retry the second time
    // around.
    histogram_tester.ExpectBucketCount(
        "API.StorageAccessHeader.ActivateStorageAccessRetryOutcome",
        /*sample=*/
        net::cookie_util::ActivateStorageAccessRetryOutcome::
            kFailureIneffectiveRetry,
        /*expected_count=*/1);
  } else {
    // Expect 2 records for 1 request, since the request is not retried.
    EXPECT_THAT(
        network_delegate.cookie_setting_overrides_records(),
        ElementsAre(CookieSettingOverrides(), CookieSettingOverrides()));
  }
}

const StorageAccessHeaderRetryData storage_access_header_retry_tests[] = {
    // No origin header, no item.
    {std::nullopt, "", false},
    // No origin header, empty string param.
    {std::nullopt, "retry; allowed-origin=\"\"", false},
    // No origin header, "null" origin param.
    {std::nullopt, "retry; allowed-origin=\"null\"", false},
    // No origin header, wildcard param.
    {std::nullopt, "retry; allowed-origin=*", true},
    // No origin header, non-wildcard param.
    {std::nullopt, "retry; allowed-origin=\"https://example.test\"", false},
    // Opaque origin header, "null" origin param.
    {url::Origin(), "retry; allowed-origin=\"null\"", true},
    // Origin header, no item.
    {url::Origin::Create(GURL("https://example.test")), "", false},
    // Origin header, wildcard.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed-origin=*", true},
    // Origin header, quoted wildcard.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed-origin=\"*\"", false},
    // Origin header, non-wildcard (matching).
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed-origin=\"https://example.test\"", true},
    // Origin header, non-wildcard (non-matching).
    {url::Origin::Create(GURL("https://example.test:123")),
     "retry; allowed-origin=\"https://example.test\"", false},
    // Origin header, multiple items, first matches.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed-origin=\"https://example.test\", foo, bar", true},
    // Origin header, multiple items, non-first non-last matches.
    {url::Origin::Create(GURL("https://example.test")),
     "foo, retry; allowed-origin=\"https://example.test\", bar", true},
    // Origin header, multiple items, last matches.
    {url::Origin::Create(GURL("https://example.test")),
     "foo, bar, retry; allowed-origin=\"https://example.test\"", true},
    // Origin header, multiple params, first matches.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed-origin=\"https://example.test\"; foo; bar", true},
    // Origin header, multiple params, non-first non-last matches.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; foo; allowed-origin=\"https://example.test\"; bar", true},
    // Origin header, multiple params, last matches.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; foo; bar; allowed-origin=\"https://example.test\"", true},
    // Origin header, multiple params with same key, first matches but is
    // ignored.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed-origin=\"https://example.test\"; "
     "allowed-origin=\"https://foo.test\"",
     false},
    // Origin header, multiple params with same key, last matches and is not
    // ignored.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed-origin=\"https://foo.test\"; "
     "allowed-origin=\"https://example.test\"",
     true},
    // Origin header, matching origin, wrong item.
    {url::Origin::Create(GURL("https://example.test")),
     "bogus; allowed-origin=\"https://example.test\"", false},
    // Origin header, matching origin, wrong param.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; allowed=\"https://example.test\"", false},
    // Origin header, matching origin, malformed param.
    {url::Origin::Create(GURL("https://example.test")),
     "retry; %=\"https://example.test\"", false},
    // Origin header, matching origin, not a token.
    {url::Origin::Create(GURL("https://example.test")),
     "\"retry\"; allowed-origin=\"https://example.test\"", false},
};

INSTANTIATE_TEST_SUITE_P(,
                         StorageAccessHeaderRetryURLRequestTest,
                         testing::ValuesIn(storage_access_header_retry_tests));

TEST_F(StorageAccessHeaderURLRequestTest,
       StorageAccessHeaderRetry_RedirectPrioritizesRetryHeader) {
  set_response_sequence({ResponseKind::kRedirect, ResponseKind::kRedirect});

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<PatternedExpectBypassCacheNetworkDelegate>(
          std::vector({false, true, false}),
          /*enable_storage_access_header=*/true));
  auto context = context_builder->Build();
  TestDelegate d;

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL(kStorageAccessRetryPath), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_storage_access_status(cookie_util::StorageAccessStatus::kInactive);

  req->Start();
  d.RunUntilRedirect();

  EXPECT_EQ(req->url().path(), kStorageAccessRetryPath);
  req->set_storage_access_status(cookie_util::StorageAccessStatus::kActive);
  req->FollowDeferredRedirect(/*removed_headers=*/{}, /*modified_headers=*/{});

  d.RunUntilComplete();

  EXPECT_THAT(
      network_delegate.cookie_setting_overrides_records(),
      ElementsAre(
          // The first request (OnForcePrivacyMode and
          // OnAnnotateAndMoveUserBlockedCookies).
          CookieSettingOverrides(), CookieSettingOverrides(),
          // The first response included the retry header, so the request is
          // retried, this time with the appropriate override.
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader}),
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader}),
          // The second response was a (same-origin) redirect, and no
          // longer carries the override.
          CookieSettingOverrides(), CookieSettingOverrides()));
  EXPECT_EQ(req->url().path(), "/echo");
}

TEST_F(StorageAccessHeaderURLRequestTest,
       StorageAccessHeaderRetry_AuthChallengeIgnoresRetryHeader) {
  set_response_sequence({ResponseKind::kAuthChallenge,
                         ResponseKind::kExpectAuthCredentials,
                         ResponseKind::kOk});

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<PatternedExpectBypassCacheNetworkDelegate>(
          std::vector({false, true}),
          /*enable_storage_access_header=*/true));
  auto context = context_builder->Build();
  TestDelegate d;
  d.set_credentials(AuthCredentials(kUser, kSecret));

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL(kStorageAccessRetryPath), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_storage_access_status(cookie_util::StorageAccessStatus::kInactive);

  req->Start();
  d.RunUntilComplete();

  EXPECT_THAT(
      network_delegate.cookie_setting_overrides_records(),
      ElementsAre(
          // The first request (OnForcePrivacyMode and
          // OnAnnotateAndMoveUserBlockedCookies).
          CookieSettingOverrides(), CookieSettingOverrides(),
          // The first response is an auth challenge, so we respond to
          // the challenge (and record the overrides again in
          // OnAnnotateAndMoveUserBlockedCookies).
          CookieSettingOverrides(),
          // The next response is a 200 but includes the retry response
          // header, so we retry the request and include the appropriate
          // overrides this time.
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader}),
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader})));
  EXPECT_TRUE(d.auth_required_called());
}

TEST_F(StorageAccessHeaderURLRequestTest,
       StorageAccessHeaderRetry_AuthWithoutChallengeHonorsRetryHeader) {
  set_response_sequence(
      {ResponseKind::kAuthWithoutChallenge, ResponseKind::kOk});

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<PatternedExpectBypassCacheNetworkDelegate>(
          std::vector({false, true}),
          /*enable_storage_access_header=*/true));
  auto context = context_builder->Build();
  TestDelegate d;
  d.set_credentials(AuthCredentials(kUser, kSecret));

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL(kStorageAccessRetryPath), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_storage_access_status(cookie_util::StorageAccessStatus::kInactive);

  req->Start();
  d.RunUntilComplete();

  EXPECT_THAT(
      network_delegate.cookie_setting_overrides_records(),
      ElementsAre(
          // The first request (OnForcePrivacyMode and
          // OnAnnotateAndMoveUserBlockedCookies). The first response is a 4XX
          // but does not include a challenge, so we handle the `retry` header.
          CookieSettingOverrides(), CookieSettingOverrides(),
          // The next request should include the overrides due to the `retry`
          // header.
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader}),
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader})));
  EXPECT_FALSE(d.auth_required_called());
}

TEST_F(StorageAccessHeaderURLRequestTest,
       StorageAccessHeaderRetry_SurvivesPostAuthRetries) {
  set_response_sequence({ResponseKind::kOk, ResponseKind::kAuthChallenge,
                         ResponseKind::kExpectAuthCredentials});
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<PatternedExpectBypassCacheNetworkDelegate>(
          std::vector({false, true}),
          /*enable_storage_access_header=*/true));
  auto context = context_builder->Build();
  TestDelegate d;
  d.set_credentials(AuthCredentials(kUser, kSecret));

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL(kStorageAccessRetryPath), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_storage_access_status(cookie_util::StorageAccessStatus::kInactive);

  req->Start();
  d.RunUntilComplete();

  EXPECT_THAT(
      network_delegate.cookie_setting_overrides_records(),
      ElementsAre(
          // The first request (OnForcePrivacyMode and
          // OnAnnotateAndMoveUserBlockedCookies).
          CookieSettingOverrides(), CookieSettingOverrides(),
          // The first response includes the retry header (and is not
          // auth), so we retry with the appropriate overrides).
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader}),
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader}),
          // The next response is an auth challenge, so we respond to
          // the challenge (and record the overrides again in
          // OnAnnotateAndMoveUserBlockedCookies).
          CookieSettingOverrides(
              {CookieSettingOverride::kStorageAccessGrantEligibleViaHeader})));
  EXPECT_TRUE(d.auth_required_called());
}

TEST_F(StorageAccessHeaderURLRequestTest,
       StorageAccessHeaderRetry_NoRetryWhenDisabled) {
  set_response_sequence({ResponseKind::kOk});

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<PatternedExpectBypassCacheNetworkDelegate>(
          std::vector({false}),
          /*enable_storage_access_header=*/false));
  auto context = context_builder->Build();
  TestDelegate d;
  base::HistogramTester histogram_tester;

  std::unique_ptr<URLRequest> req(context->CreateRequest(
      http_test_server()->GetURL(kStorageAccessRetryPath), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  req->Start();
  d.RunUntilComplete();

  // This expects 2 records for 1 request, since it should not have been
  // retried.
  EXPECT_THAT(network_delegate.cookie_setting_overrides_records(),
              ElementsAre(CookieSettingOverrides(), CookieSettingOverrides()));
  histogram_tester.ExpectUniqueSample(
      "API.StorageAccessHeader.ActivateStorageAccessRetryOutcome",
      /*sample=*/
      net::cookie_util::ActivateStorageAccessRetryOutcome::
          kFailureHeaderDisabled,
      /*expected_bucket_count=*/1);
}

}  // namespace net
