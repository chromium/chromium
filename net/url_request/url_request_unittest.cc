// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

// This must be before Windows headers
#include "build/build_config.h"

#if defined(OS_WIN)
#include <objbase.h>
#include <shlobj.h>
#include <windows.h>
#include <wrl/client.h>
#endif

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/directory_listing.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/base/url_util.h"
#include "net/cert/crl_set.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/test_root_certs.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/quic_server_info.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_http_job_histogram.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if defined(OS_FUCHSIA)
#define USE_BUILTIN_CERT_VERIFIER
#endif

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
#include "net/base/filename_util.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_file_dir_job.h"
#endif

#if !BUILDFLAG(DISABLE_FTP_SUPPORT) && !defined(OS_ANDROID)
#include "net/ftp/ftp_network_layer.h"
#include "net/url_request/ftp_protocol_handler.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

#if defined(OS_ANDROID) || defined(USE_BUILTIN_CERT_VERIFIER)
#include "net/cert/cert_net_fetcher.h"
#include "net/cert_net/cert_net_fetcher_impl.h"
#endif

#if defined(USE_NSS_CERTS)
#include "net/cert_net/nss_ocsp.h"
#endif

using net::test::IsError;
using net::test::IsOk;

using base::ASCIIToUTF16;
using base::Time;
using std::string;

namespace net {

namespace {

namespace test_default {
#include "net/http/transport_security_state_static_unittest_default.h"
}

const base::string16 kChrome(ASCIIToUTF16("chrome"));
const base::string16 kSecret(ASCIIToUTF16("secret"));
const base::string16 kUser(ASCIIToUTF16("user"));

const base::FilePath::CharType kTestFilePath[] =
    FILE_PATH_LITERAL("net/data/url_request_unittest");

#if !BUILDFLAG(DISABLE_FTP_SUPPORT) && !defined(OS_ANDROID) && \
    !defined(OS_FUCHSIA)
// Test file used in most FTP tests.
const char kFtpTestFile[] = "BullRunSpeech.txt";
#endif

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
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
}

// Same as above, but with proxy times.
void TestLoadTimingNotReusedWithProxy(
    const LoadTimingInfo& load_timing_info,
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
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);
}

// Same as above, but with a reused socket and proxy times.
void TestLoadTimingReusedWithProxy(
    const LoadTimingInfo& load_timing_info) {
  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  EXPECT_LE(load_timing_info.request_start,
            load_timing_info.proxy_resolve_start);
  EXPECT_LE(load_timing_info.proxy_resolve_start,
            load_timing_info.proxy_resolve_end);
  EXPECT_LE(load_timing_info.proxy_resolve_end,
            load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);
}

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
// Tests load timing information in the case of a cache hit, when no cache
// validation request was sent over the wire.
base::StringPiece TestNetResourceProvider(int key) {
  return "header";
}

void FillBuffer(char* buffer, size_t len) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i])
      buffer[i] = 'g';
  }
}
#endif

void TestLoadTimingCacheHitNoNetwork(
    const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_EQ(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  EXPECT_LE(load_timing_info.request_start, load_timing_info.send_start);
  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);
  EXPECT_LE(load_timing_info.send_end, load_timing_info.receive_headers_end);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
}

#if !BUILDFLAG(DISABLE_FTP_SUPPORT) && !defined(OS_ANDROID) && \
    !defined(OS_FUCHSIA)
// Tests load timing in the case that there is no HTTP response.  This can be
// used to test in the case of errors or non-HTTP requests.
void TestLoadTimingNoHttpResponse(
    const LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_EQ(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  // Only the request times should be non-null.
  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());
  EXPECT_TRUE(load_timing_info.send_start.is_null());
  EXPECT_TRUE(load_timing_info.send_end.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}
#endif

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

// Job that allows monitoring of its priority.
class PriorityMonitoringURLRequestJob : public URLRequestTestJob {
 public:
  // The latest priority of the job is always written to |request_priority_|.
  PriorityMonitoringURLRequestJob(URLRequest* request,
                                  NetworkDelegate* network_delegate,
                                  RequestPriority* request_priority)
      : URLRequestTestJob(request, network_delegate),
        request_priority_(request_priority) {
    *request_priority_ = DEFAULT_PRIORITY;
  }

  void SetPriority(RequestPriority priority) override {
    *request_priority_ = priority;
    URLRequestTestJob::SetPriority(priority);
  }

 private:
  RequestPriority* const request_priority_;
};

// Do a case-insensitive search through |haystack| for |needle|.
bool ContainsString(const std::string& haystack, const char* needle) {
  std::string::const_iterator it = std::search(
      haystack.begin(), haystack.end(), needle, needle + strlen(needle),
      base::CaseInsensitiveCompareASCII<char>());
  return it != haystack.end();
}

std::unique_ptr<UploadDataStream> CreateSimpleUploadData(const char* data) {
  std::unique_ptr<UploadElementReader> reader(
      new UploadBytesElementReader(data, strlen(data)));
  return ElementsUploadDataStream::CreateWithReader(std::move(reader), 0);
}

// Verify that the SSLInfo of a successful SSL connection has valid values.
void CheckSSLInfo(const SSLInfo& ssl_info) {
  // -1 means unknown.  0 means no encryption.
  EXPECT_GT(ssl_info.security_bits, 0);

  // The cipher suite TLS_NULL_WITH_NULL_NULL (0) must not be negotiated.
  uint16_t cipher_suite =
      SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
  EXPECT_NE(0U, cipher_suite);
}

void CheckFullRequestHeaders(const HttpRequestHeaders& headers,
                             const GURL& host_url) {
  std::string sent_value;

  EXPECT_TRUE(headers.GetHeader("Host", &sent_value));
  EXPECT_EQ(GetHostAndOptionalPort(host_url), sent_value);

  EXPECT_TRUE(headers.GetHeader("Connection", &sent_value));
  EXPECT_EQ("keep-alive", sent_value);
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
    ON_AUTH_REQUIRED = 1 << 3
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

  // Runs the message loop until the delegate blocks.
  void RunUntilBlocked();

  // For users to trigger a callback returning |response|.
  // Side-effects: resets |stage_blocked_for_callback_| and stored callbacks.
  // Only call if |block_mode_| == USER_CALLBACK.
  void DoCallback(int response);
  void DoAuthCallback(NetworkDelegate::AuthRequiredResponse response);

  // Setters.
  void set_retval(int retval) {
    ASSERT_NE(USER_CALLBACK, block_mode_);
    ASSERT_NE(ERR_IO_PENDING, retval);
    ASSERT_NE(OK, retval);
    retval_ = retval;
  }

  // If |auth_retval| == AUTH_REQUIRED_RESPONSE_SET_AUTH, then
  // |auth_credentials_| will be passed with the response.
  void set_auth_retval(AuthRequiredResponse auth_retval) {
    ASSERT_NE(USER_CALLBACK, block_mode_);
    ASSERT_NE(AUTH_REQUIRED_RESPONSE_IO_PENDING, auth_retval);
    auth_retval_ = auth_retval;
  }
  void set_auth_credentials(const AuthCredentials& auth_credentials) {
    auth_credentials_ = auth_credentials;
  }

  void set_redirect_url(const GURL& url) {
    redirect_url_ = url;
  }

  void set_block_on(int block_on) {
    block_on_ = block_on;
  }

  // Allows the user to check in which state did we block.
  Stage stage_blocked_for_callback() const {
    EXPECT_EQ(USER_CALLBACK, block_mode_);
    return stage_blocked_for_callback_;
  }

 private:
  void OnBlocked();

  void RunCallback(int response, CompletionOnceCallback callback);
  void RunAuthCallback(AuthRequiredResponse response, AuthCallback callback);

  // TestNetworkDelegate implementation.
  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override;

  int OnBeforeStartTransaction(URLRequest* request,
                               CompletionOnceCallback callback,
                               HttpRequestHeaders* headers) override;

  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;

  NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      AuthCallback callback,
      AuthCredentials* credentials) override;

  // Resets the callbacks and |stage_blocked_for_callback_|.
  void Reset();

  // Checks whether we should block in |stage|. If yes, returns an error code
  // and optionally sets up callback based on |block_mode_|. If no, returns OK.
  int MaybeBlockStage(Stage stage, CompletionOnceCallback callback);

  // Configuration parameters, can be adjusted by public methods:
  const BlockMode block_mode_;

  // Values returned on blocking stages when mode is SYNCHRONOUS or
  // AUTO_CALLBACK. For USER_CALLBACK these are set automatically to IO_PENDING.
  int retval_;  // To be returned in non-auth stages.
  AuthRequiredResponse auth_retval_;

  GURL redirect_url_;  // Used if non-empty during OnBeforeURLRequest.
  int block_on_;  // Bit mask: in which stages to block.

  // |auth_credentials_| will be copied to |*target_auth_credential_| on
  // callback.
  AuthCredentials auth_credentials_;
  AuthCredentials* target_auth_credentials_;

  // Internal variables, not set by not the user:
  // Last blocked stage waiting for user callback (unused if |block_mode_| !=
  // USER_CALLBACK).
  Stage stage_blocked_for_callback_;

  // Callback objects stored during blocking stages.
  CompletionOnceCallback callback_;
  AuthCallback auth_callback_;

  // Closure to run to exit RunUntilBlocked().
  base::OnceClosure on_blocked_;

  base::WeakPtrFactory<BlockingNetworkDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlockingNetworkDelegate);
};

BlockingNetworkDelegate::BlockingNetworkDelegate(BlockMode block_mode)
    : block_mode_(block_mode),
      retval_(OK),
      auth_retval_(AUTH_REQUIRED_RESPONSE_NO_ACTION),
      block_on_(0),
      target_auth_credentials_(NULL),
      stage_blocked_for_callback_(NOT_BLOCKED),
      weak_factory_(this) {
}

void BlockingNetworkDelegate::RunUntilBlocked() {
  base::RunLoop run_loop;
  on_blocked_ = run_loop.QuitClosure();
  run_loop.Run();
}

void BlockingNetworkDelegate::DoCallback(int response) {
  ASSERT_EQ(USER_CALLBACK, block_mode_);
  ASSERT_NE(NOT_BLOCKED, stage_blocked_for_callback_);
  ASSERT_NE(ON_AUTH_REQUIRED, stage_blocked_for_callback_);
  CompletionOnceCallback callback = std::move(callback_);
  Reset();

  // |callback| may trigger completion of a request, so post it as a task, so
  // it will run under a subsequent TestDelegate::RunUntilComplete() loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::RunCallback,
                                weak_factory_.GetWeakPtr(), response,
                                std::move(callback)));
}

void BlockingNetworkDelegate::DoAuthCallback(
    NetworkDelegate::AuthRequiredResponse response) {
  ASSERT_EQ(USER_CALLBACK, block_mode_);
  ASSERT_EQ(ON_AUTH_REQUIRED, stage_blocked_for_callback_);
  AuthCallback auth_callback = std::move(auth_callback_);
  Reset();
  RunAuthCallback(response, std::move(auth_callback));
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

void BlockingNetworkDelegate::RunAuthCallback(AuthRequiredResponse response,
                                              AuthCallback callback) {
  if (auth_retval_ == AUTH_REQUIRED_RESPONSE_SET_AUTH) {
    ASSERT_TRUE(target_auth_credentials_ != NULL);
    *target_auth_credentials_ = auth_credentials_;
  }
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
    CompletionOnceCallback callback,
    HttpRequestHeaders* headers) {
  // TestNetworkDelegate always completes synchronously.
  CHECK_NE(ERR_IO_PENDING, TestNetworkDelegate::OnBeforeStartTransaction(
                               request, base::NullCallback(), headers));

  return MaybeBlockStage(ON_BEFORE_SEND_HEADERS, std::move(callback));
}

int BlockingNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  // TestNetworkDelegate always completes synchronously.
  CHECK_NE(ERR_IO_PENDING,
           TestNetworkDelegate::OnHeadersReceived(
               request, base::NullCallback(), original_response_headers,
               override_response_headers, allowed_unsafe_redirect_url));

  return MaybeBlockStage(ON_HEADERS_RECEIVED, std::move(callback));
}

NetworkDelegate::AuthRequiredResponse BlockingNetworkDelegate::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    AuthCallback callback,
    AuthCredentials* credentials) {
  // TestNetworkDelegate always completes synchronously.
  CHECK_NE(AUTH_REQUIRED_RESPONSE_IO_PENDING,
           TestNetworkDelegate::OnAuthRequired(
               request, auth_info, base::NullCallback(), credentials));
  // Check that the user has provided callback for the previous blocked stage.
  EXPECT_EQ(NOT_BLOCKED, stage_blocked_for_callback_);

  if ((block_on_ & ON_AUTH_REQUIRED) == 0) {
    return AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  target_auth_credentials_ = credentials;

  switch (block_mode_) {
    case SYNCHRONOUS:
      if (auth_retval_ == AUTH_REQUIRED_RESPONSE_SET_AUTH)
        *target_auth_credentials_ = auth_credentials_;
      return auth_retval_;

    case AUTO_CALLBACK:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::RunAuthCallback,
                                    weak_factory_.GetWeakPtr(), auth_retval_,
                                    std::move(callback)));
      return AUTH_REQUIRED_RESPONSE_IO_PENDING;

    case USER_CALLBACK:
      auth_callback_ = std::move(callback);
      stage_blocked_for_callback_ = ON_AUTH_REQUIRED;
      // We may reach here via a callback prior to RunUntilBlocked(), so post
      // a task to fetch and run the |on_blocked_| closure.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::OnBlocked,
                                    weak_factory_.GetWeakPtr()));
      return AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }
  NOTREACHED();
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;  // Dummy value.
}

void BlockingNetworkDelegate::Reset() {
  EXPECT_NE(NOT_BLOCKED, stage_blocked_for_callback_);
  stage_blocked_for_callback_ = NOT_BLOCKED;
  callback_.Reset();
  auth_callback_.Reset();
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
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::RunCallback,
                                    weak_factory_.GetWeakPtr(), retval_,
                                    std::move(callback)));
      return ERR_IO_PENDING;

    case USER_CALLBACK:
      callback_ = std::move(callback);
      stage_blocked_for_callback_ = stage;
      // We may reach here via a callback prior to RunUntilBlocked(), so post
      // a task to fetch and run the |on_blocked_| closure.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&BlockingNetworkDelegate::OnBlocked,
                                    weak_factory_.GetWeakPtr()));
      return ERR_IO_PENDING;
  }
  NOTREACHED();
  return 0;
}

class TestURLRequestContextWithProxy : public TestURLRequestContext {
 public:
  // Does not own |delegate|.
  TestURLRequestContextWithProxy(const std::string& proxy,
                                 NetworkDelegate* delegate)
      : TestURLRequestContext(true) {
    context_storage_.set_proxy_resolution_service(
        ProxyResolutionService::CreateFixed(proxy,
                                            TRAFFIC_ANNOTATION_FOR_TESTS));
    set_network_delegate(delegate);
    Init();
  }
  ~TestURLRequestContextWithProxy() override = default;
};

// A mock ReportSenderInterface that just remembers the latest report
// URI and report to be sent.
class MockCertificateReportSender
    : public TransportSecurityState::ReportSenderInterface {
 public:
  MockCertificateReportSender() = default;
  ~MockCertificateReportSender() override = default;

  void Send(const GURL& report_uri,
            base::StringPiece content_type,
            base::StringPiece report,
            const base::Callback<void()>& success_callback,
            const base::Callback<void(const GURL&, int, int)>& error_callback)
      override {
    latest_report_uri_ = report_uri;
    report.CopyToString(&latest_report_);
    content_type.CopyToString(&latest_content_type_);
  }
  const GURL& latest_report_uri() { return latest_report_uri_; }
  const std::string& latest_report() { return latest_report_; }
  const std::string& latest_content_type() { return latest_content_type_; }

 private:
  GURL latest_report_uri_;
  std::string latest_report_;
  std::string latest_content_type_;
};

// OCSPErrorTestDelegate caches the SSLInfo passed to OnSSLCertificateError.
// This is needed because after the certificate failure, the URLRequest will
// retry the connection, and return a partial SSLInfo with a cached cert status.
// The partial SSLInfo does not have the OCSP information filled out.
class OCSPErrorTestDelegate : public TestDelegate {
 public:
  void OnSSLCertificateError(URLRequest* request,
                             const SSLInfo& ssl_info,
                             bool fatal) override {
    ssl_info_ = ssl_info;
    on_ssl_certificate_error_called_ = true;
    TestDelegate::OnSSLCertificateError(request, ssl_info, fatal);
  }

  bool on_ssl_certificate_error_called() {
    return on_ssl_certificate_error_called_;
  }

  SSLInfo ssl_info() { return ssl_info_; }

 private:
  bool on_ssl_certificate_error_called_ = false;
  SSLInfo ssl_info_;
};

}  // namespace

// Inherit PlatformTest since we require the autorelease pool on Mac OS X.
class URLRequestTest : public PlatformTest, public WithScopedTaskEnvironment {
 public:
  URLRequestTest()
      : default_context_(std::make_unique<TestURLRequestContext>(true)) {
    default_context_->set_network_delegate(&default_network_delegate_);
    default_context_->set_net_log(&net_log_);
    job_factory_impl_ = new URLRequestJobFactoryImpl();
    job_factory_.reset(job_factory_impl_);
  }

  ~URLRequestTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();

    SetTransportSecurityStateSourceForTesting(nullptr);
  }

  void SetUp() override {
    SetUpFactory();
    default_context_->set_job_factory(job_factory_.get());
    default_context_->Init();
    PlatformTest::SetUp();
  }

  void TearDown() override { default_context_.reset(); }

  virtual void SetUpFactory() {
    job_factory_impl_->SetProtocolHandler(
        "data", std::make_unique<DataProtocolHandler>());
#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
    job_factory_impl_->SetProtocolHandler(
        "file", std::make_unique<FileProtocolHandler>(
                    base::ThreadTaskRunnerHandle::Get()));
#endif
  }

  TestNetworkDelegate* default_network_delegate() {
    return &default_network_delegate_;
  }

  TestURLRequestContext& default_context() const { return *default_context_; }

  // Adds the TestJobInterceptor to the default context.
  TestJobInterceptor* AddTestInterceptor() {
    TestJobInterceptor* protocol_handler_ = new TestJobInterceptor();
    job_factory_impl_->SetProtocolHandler("http", nullptr);
    job_factory_impl_->SetProtocolHandler("http",
                                          base::WrapUnique(protocol_handler_));
    return protocol_handler_;
  }

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
    ASSERT_EQ(static_cast<int>(data_size),
              base::WriteFile(*test_file, data, data_size));
  }

 protected:
  TestNetLog net_log_;
  TestNetworkDelegate default_network_delegate_;  // Must outlive URLRequest.
  URLRequestJobFactoryImpl* job_factory_impl_;
  std::unique_ptr<URLRequestJobFactory> job_factory_;
  std::unique_ptr<TestURLRequestContext> default_context_;
  base::ScopedTempDir temp_dir_;
};

// This NetworkDelegate is picky about what files are accessible. Only
// whitelisted files are allowed.
class CookieBlockingNetworkDelegate : public TestNetworkDelegate {
 public:
  CookieBlockingNetworkDelegate() = default;
  ;

  // Adds |directory| to the access white list.
  void AddToWhitelist(const base::FilePath& directory) {
    whitelist_.insert(directory);
  }

 private:
  // Returns true if |path| matches the white list.
  bool OnCanAccessFileInternal(const base::FilePath& path) const {
    for (const auto& directory : whitelist_) {
      if (directory == path || directory.IsParent(path))
        return true;
    }
    return false;
  }

  // Returns true only if both |original_path| and |absolute_path| match the
  // white list.
  bool OnCanAccessFile(const URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override {
    return (OnCanAccessFileInternal(original_path) &&
            OnCanAccessFileInternal(absolute_path));
  }

  std::set<base::FilePath> whitelist_;

  DISALLOW_COPY_AND_ASSIGN(CookieBlockingNetworkDelegate);
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
    EXPECT_EQ("", r->GetSocketAddress().host());
    EXPECT_EQ(0, r->GetSocketAddress().port());

    HttpRequestHeaders headers;
    EXPECT_FALSE(r->GetFullRequestHeaders(&headers));
  }
}

TEST_F(URLRequestTest, DataURLImageTest) {
  TestDelegate d;
  {
    // Use our nice little Chrome logo.
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        GURL("data:image/png;base64,"
             "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAADVklEQVQ4jX2TfUwUB"
             "BjG3w1y+HGcd9dxhXR8T4awOccJGgOSWclHImznLkTlSw0DDQXkrmgYgbUYnlQTqQ"
             "xIEVxitD5UMCATRA1CEEg+Qjw3bWDxIauJv/5oumqs39/P827vnucRmYN0gyF01GI"
             "5MpCVdW0gO7tvNC+vqSEtbZefk5NuLv1jdJ46p/zw0HeH4+PHr3h7c1mjoV2t5rKz"
             "Mx1+fg9bAgK6zHq9cU5z+LpA3xOtx34+vTeT21onRuzssC3zxbbSwC13d/pFuC7Ck"
             "IMDxQpF7r/MWq12UctI1dWWm99ypqSYmRUBdKem8MkrO/kgaTt1O7YzlpzE5GIVd0"
             "WYUqt57yWf2McHTObYPbVD+ZwbtlLTVMZ3BW+TnLyXLaWtmEq6WJVbT3HBh3Svj2H"
             "QQcm43XwmtoYM6vVKleh0uoWvnzW3v3MpidruPTQPf0bia7sJOtBM0ufTWNvus/nk"
             "DFHF9ZS+uYVjRUasMeHUmyLYtcklTvzWGFZnNOXczThvpKIzjcahSqIzkvDLayDq6"
             "D3eOjtBbNUEIZYyqsvj4V4wY92eNJ4IoyhTbxXX1T5xsV9tm9r4TQwHLiZw/pdDZJ"
             "ea8TKmsmR/K0uLh/GwnCHghTja6lPhphezPfO5/5MrVvMzNaI3+ERHfrFzPKQukrQ"
             "GI4d/3EFD/3E2mVNYvi4at7CXWREaxZGD+3hg28zD3gVMd6q5c8GdosynKmSeRuGz"
             "pjyl1/9UDGtPR5HeaKT8Wjo17WXk579BXVUhN64ehF9fhRtq/uxxZKzNiZFGD0wRC"
             "3NFROZ5mwIPL/96K/rKMMLrIzF9uhHr+/sYH7DAbwlgC4J+R2Z7FUx1qLnV7MGF40"
             "smVSoJ/jvHRfYhQeUJd/SnYtGWhPHR0Sz+GE2F2yth0B36Vcz2KpnufBJbsysjjW4"
             "kblBUiIjiURUWqJY65zxbnTy57GQyH58zgy0QBtTQv5gH15XMdKkYu+TGaJMnlm2O"
             "34uI4b9tflqp1+QEFGzoW/ulmcofcpkZCYJhDfSpme7QcrHa+Xfji8paEQkTkSfmm"
             "oRWRNZr/F1KfVMjW+IKEnv2FwZfKdzt0BQR6lClcZR0EfEXEfv/G6W9iLiIyCoReV"
             "5EnhORIBHx+ufPj/gLB/zGI/G4Bk0AAAAASUVORK5CYII="),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_TRUE(!r->is_pending());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 911);
    EXPECT_EQ("", r->GetSocketAddress().host());
    EXPECT_EQ(0, r->GetSocketAddress().port());

    HttpRequestHeaders headers;
    EXPECT_FALSE(r->GetFullRequestHeaders(&headers));
  }
}

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
TEST_F(URLRequestTest, FileTest) {
  const char kTestFileContent[] = "Hello";
  base::FilePath test_file;
  ASSERT_NO_FATAL_FAILURE(
      CreateTestFile(kTestFileContent, sizeof(kTestFileContent), &test_file));

  GURL test_url = FilePathToFileURL(test_file);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_TRUE(!r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(sizeof(kTestFileContent)));
    EXPECT_EQ("", r->GetSocketAddress().host());
    EXPECT_EQ(0, r->GetSocketAddress().port());

    HttpRequestHeaders headers;
    EXPECT_FALSE(r->GetFullRequestHeaders(&headers));
  }
}

TEST_F(URLRequestTest, FileTestCancel) {
  const char kTestFileContent[] = "Hello";
  base::FilePath test_file;
  ASSERT_NO_FATAL_FAILURE(
      CreateTestFile(kTestFileContent, sizeof(kTestFileContent), &test_file));

  GURL test_url = FilePathToFileURL(test_file);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());
    r->Cancel();
  }
  // Async cancellation should be safe even when URLRequest has been already
  // destroyed.
  base::RunLoop().RunUntilIdle();
}

TEST_F(URLRequestTest, FileTestFullSpecifiedRange) {
  const size_t buffer_size = 4000;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  base::FilePath test_file;
  ASSERT_NO_FATAL_FAILURE(
      CreateTestFile(buffer.get(), buffer_size, &test_file));
  GURL temp_url = FilePathToFileURL(test_file);

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - first_byte_position;
  const size_t content_length = last_byte_position - first_byte_position + 1;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        temp_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    HttpRequestHeaders headers;
    headers.SetHeader(
        HttpRequestHeaders::kRange,
        HttpByteRange::Bounded(
            first_byte_position, last_byte_position).GetHeaderValue());
    r->SetExtraRequestHeaders(headers);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_TRUE(!r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(static_cast<int>(content_length), d.bytes_received());
    // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
    EXPECT_TRUE(partial_buffer_string == d.data_received());
  }
}

TEST_F(URLRequestTest, FileTestHalfSpecifiedRange) {
  const size_t buffer_size = 4000;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  base::FilePath test_file;
  ASSERT_NO_FATAL_FAILURE(
      CreateTestFile(buffer.get(), buffer_size, &test_file));
  GURL temp_url = FilePathToFileURL(test_file);

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - 1;
  const size_t content_length = last_byte_position - first_byte_position + 1;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        temp_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kRange,
                      HttpByteRange::RightUnbounded(
                          first_byte_position).GetHeaderValue());
    r->SetExtraRequestHeaders(headers);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    base::RunLoop().Run();
    EXPECT_TRUE(!r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(static_cast<int>(content_length), d.bytes_received());
    // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
    EXPECT_TRUE(partial_buffer_string == d.data_received());
  }
}

TEST_F(URLRequestTest, FileTestMultipleRanges) {
  const size_t buffer_size = 400000;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);

  base::FilePath test_file;
  ASSERT_NO_FATAL_FAILURE(
      CreateTestFile(buffer.get(), buffer_size, &test_file));
  GURL temp_url = FilePathToFileURL(test_file);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        temp_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kRange, "bytes=0-0,10-200,200-300");
    r->SetExtraRequestHeaders(headers);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_TRUE(d.request_failed());
  }
}

TEST_F(URLRequestTest, AllowFileURLs) {
  std::string test_data("monkey");
  base::FilePath test_file;
  ASSERT_NO_FATAL_FAILURE(
      CreateTestFile(test_data.data(), test_data.size(), &test_file));

  // The directory part of the path returned from CreateTemporaryFileInDir()
  // can be slightly different from |absolute_temp_dir| on Windows.
  // Example: C:\\Users\\CHROME~2 -> C:\\Users\\chrome-bot
  // Hence the test should use the directory name of |test_file|, rather than
  // |absolute_temp_dir|, for whitelisting.
  base::FilePath real_temp_dir = test_file.DirName();
  GURL test_file_url = FilePathToFileURL(test_file);
  {
    TestDelegate d;
    CookieBlockingNetworkDelegate network_delegate;
    network_delegate.AddToWhitelist(real_temp_dir);
    default_context().set_network_delegate(&network_delegate);
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_file_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();
    // This should be allowed as the file path is whitelisted.
    EXPECT_FALSE(d.request_failed());
    EXPECT_EQ(test_data, d.data_received());
  }

  {
    TestDelegate d;
    CookieBlockingNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_file_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();
    // This should be rejected as the file path is not whitelisted.
    EXPECT_TRUE(d.request_failed());
    EXPECT_EQ("", d.data_received());
    EXPECT_EQ(ERR_ACCESS_DENIED, d.request_status());
  }
}

#if defined(OS_POSIX)  // Because of symbolic links.

TEST_F(URLRequestTest, SymlinksToFiles) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  // Get an absolute path since temp_dir can contain a symbolic link.
  base::FilePath absolute_temp_dir =
      base::MakeAbsoluteFilePath(temp_dir_.GetPath());

  // Create a good directory (will be whitelisted) and a good file.
  base::FilePath good_dir = absolute_temp_dir.AppendASCII("good");
  ASSERT_TRUE(base::CreateDirectory(good_dir));
  base::FilePath good_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(good_dir, &good_file));
  std::string good_data("good");
  base::WriteFile(good_file, good_data.data(), good_data.size());
  // See the comment in AllowFileURLs() for why this is done.
  base::FilePath real_good_dir = good_file.DirName();

  // Create a bad directory (will not be whitelisted) and a bad file.
  base::FilePath bad_dir = absolute_temp_dir.AppendASCII("bad");
  ASSERT_TRUE(base::CreateDirectory(bad_dir));
  base::FilePath bad_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(bad_dir, &bad_file));
  std::string bad_data("bad");
  base::WriteFile(bad_file, bad_data.data(), bad_data.size());

  // This symlink will point to the good file. Access to the symlink will be
  // allowed as both the symlink and the destination file are in the same
  // good directory.
  base::FilePath good_symlink = good_dir.AppendASCII("good_symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(good_file, good_symlink));
  GURL good_file_url = FilePathToFileURL(good_symlink);
  // This symlink will point to the bad file. Even though the symlink is in
  // the good directory, access to the symlink will be rejected since it
  // points to the bad file.
  base::FilePath bad_symlink = good_dir.AppendASCII("bad_symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(bad_file, bad_symlink));
  GURL bad_file_url = FilePathToFileURL(bad_symlink);

  CookieBlockingNetworkDelegate network_delegate;
  network_delegate.AddToWhitelist(real_good_dir);
  {
    TestDelegate d;
    default_context().set_network_delegate(&network_delegate);
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        good_file_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();
    // good_file_url should be allowed.
    EXPECT_FALSE(d.request_failed());
    EXPECT_EQ(good_data, d.data_received());
  }

  {
    TestDelegate d;
    default_context().set_network_delegate(&network_delegate);
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        bad_file_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();
    // bad_file_url should be rejected.
    EXPECT_TRUE(d.request_failed());
    EXPECT_EQ("", d.data_received());
    EXPECT_EQ(ERR_ACCESS_DENIED, d.request_status());
  }
}

TEST_F(URLRequestTest, SymlinksToDirs) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  // Get an absolute path since temp_dir can contain a symbolic link.
  base::FilePath absolute_temp_dir =
      base::MakeAbsoluteFilePath(temp_dir_.GetPath());

  // Create a good directory (will be whitelisted).
  base::FilePath good_dir = absolute_temp_dir.AppendASCII("good");
  ASSERT_TRUE(base::CreateDirectory(good_dir));

  // Create a bad directory (will not be whitelisted).
  base::FilePath bad_dir = absolute_temp_dir.AppendASCII("bad");
  ASSERT_TRUE(base::CreateDirectory(bad_dir));

  // This symlink will point to the good directory. Access to the symlink
  // will be allowed as the symlink is in the good dir that'll be white
  // listed.
  base::FilePath good_symlink = good_dir.AppendASCII("good_symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(good_dir, good_symlink));
  GURL good_file_url = FilePathToFileURL(good_symlink);
  // This symlink will point to the bad directory. Even though the symlink is
  // in the good directory, access to the symlink will be rejected since it
  // points to the bad directory.
  base::FilePath bad_symlink = good_dir.AppendASCII("bad_symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(bad_dir, bad_symlink));
  GURL bad_file_url = FilePathToFileURL(bad_symlink);

  CookieBlockingNetworkDelegate network_delegate;
  network_delegate.AddToWhitelist(good_dir);
  {
    TestDelegate d;
    default_context().set_network_delegate(&network_delegate);
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        good_file_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();
    // good_file_url should be allowed.
    EXPECT_FALSE(d.request_failed());
    ASSERT_NE(d.data_received().find("good_symlink"), std::string::npos);
  }

  {
    TestDelegate d;
    default_context().set_network_delegate(&network_delegate);
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        bad_file_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();
    // bad_file_url should be rejected.
    EXPECT_TRUE(d.request_failed());
    EXPECT_EQ("", d.data_received());
    EXPECT_EQ(ERR_ACCESS_DENIED, d.request_status());
  }
}

#endif  // defined(OS_POSIX)

TEST_F(URLRequestTest, FileDirCancelTest) {
  // Put in mock resource provider.
  NetModule::SetResourceProvider(TestNetResourceProvider);

  TestDelegate d;
  {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("net"));
    file_path = file_path.Append(FILE_PATH_LITERAL("data"));

    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        FilePathToFileURL(file_path), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    EXPECT_TRUE(req->is_pending());

    d.set_cancel_in_received_data_pending(true);

    d.RunUntilComplete();
  }

  // Take out mock resource provider.
  NetModule::SetResourceProvider(NULL);
}

TEST_F(URLRequestTest, FileDirOutputSanity) {
  // Verify the general sanity of the the output of the file:
  // directory lister by checking for the output of a known existing
  // file.
  const char sentinel_name[] = "filedir-sentinel";

  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(kTestFilePath);

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      default_context().CreateRequest(FilePathToFileURL(path), DEFAULT_PRIORITY,
                                      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  // Generate entry for the sentinel file.
  base::FilePath sentinel_path = path.AppendASCII(sentinel_name);
  base::File::Info info;
  EXPECT_TRUE(base::GetFileInfo(sentinel_path, &info));
  EXPECT_GT(info.size, 0);
  std::string sentinel_output = GetDirectoryListingEntry(
      base::string16(sentinel_name, sentinel_name + strlen(sentinel_name)),
      std::string(sentinel_name), false /* is_dir */, info.size,

      info.last_modified);

  ASSERT_LT(0, d.bytes_received());
  ASSERT_FALSE(d.request_failed());
  EXPECT_EQ(OK, d.request_status());
  // Check for the entry generated for the "sentinel" file.
  const std::string& data = d.data_received();
  ASSERT_NE(data.find(sentinel_output), std::string::npos);
}

TEST_F(URLRequestTest, FileDirRedirectNoCrash) {
  // There is an implicit redirect when loading a file path that matches a
  // directory and does not end with a slash.  Ensure that following such
  // redirects does not crash.  See http://crbug.com/18686.

  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(kTestFilePath);

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      default_context().CreateRequest(FilePathToFileURL(path), DEFAULT_PRIORITY,
                                      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1, d.received_redirect_count());
  ASSERT_LT(0, d.bytes_received());
  ASSERT_FALSE(d.request_failed());
  EXPECT_EQ(OK, d.request_status());
}

#if defined(OS_WIN)
// Don't accept the url "file:///" on windows. See http://crbug.com/1474.
TEST_F(URLRequestTest, FileDirRedirectSingleSlash) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("file:///"), DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1, d.received_redirect_count());
  EXPECT_NE(OK, d.request_status());
}
#endif  // defined(OS_WIN)

#endif  // !BUILDFLAG(DISABLE_FILE_SUPPORT)

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

TEST_F(URLRequestTest, InvalidReferrerTest) {
  TestURLRequestContext context;
  TestNetworkDelegate network_delegate;
  network_delegate.set_cancel_request_with_policy_violating_referrer(true);
  context.set_network_delegate(&network_delegate);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context.CreateRequest(GURL("http://localhost/"), DEFAULT_PRIORITY, &d,
                            TRAFFIC_ANNOTATION_FOR_TESTS));
  req->SetReferrer("https://somewhere.com/");

  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(d.request_failed());
}

#if defined(OS_WIN)
TEST_F(URLRequestTest, ResolveShortcutTest) {
  base::FilePath app_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path = app_path.Append(kTestFilePath);
  app_path = app_path.AppendASCII("with-headers.html");

  std::wstring lnk_path = app_path.value() + L".lnk";

  base::win::ScopedCOMInitializer com_initializer;

  // Temporarily create a shortcut for test
  {
    Microsoft::WRL::ComPtr<IShellLink> shell;
    ASSERT_TRUE(SUCCEEDED(::CoCreateInstance(
        CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell))));
    Microsoft::WRL::ComPtr<IPersistFile> persist;
    ASSERT_TRUE(SUCCEEDED(shell.CopyTo(persist.GetAddressOf())));
    EXPECT_TRUE(SUCCEEDED(shell->SetPath(app_path.value().c_str())));
    EXPECT_TRUE(SUCCEEDED(shell->SetDescription(L"ResolveShortcutTest")));
    EXPECT_TRUE(SUCCEEDED(persist->Save(lnk_path.c_str(), TRUE)));
  }

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        FilePathToFileURL(base::FilePath(lnk_path)), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    WIN32_FILE_ATTRIBUTE_DATA data;
    GetFileAttributesEx(app_path.value().c_str(),
                        GetFileExInfoStandard, &data);
    HANDLE file = CreateFile(app_path.value().c_str(), GENERIC_READ,
                             FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT_NE(INVALID_HANDLE_VALUE, file);
    std::unique_ptr<char[]> buffer(new char[data.nFileSizeLow]);
    DWORD read_size;
    BOOL result;
    result = ReadFile(file, buffer.get(), data.nFileSizeLow,
                      &read_size, NULL);
    std::string content(buffer.get(), read_size);
    CloseHandle(file);

    EXPECT_TRUE(!r->is_pending());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(content, d.data_received());
  }

  // Clean the shortcut
  DeleteFile(lnk_path.c_str());
}
#endif  // defined(OS_WIN)

// Custom URLRequestJobs for use with interceptor tests
class RestartTestJob : public URLRequestTestJob {
 public:
  RestartTestJob(URLRequest* request, NetworkDelegate* network_delegate)
    : URLRequestTestJob(request, network_delegate, true) {}
 protected:
  void StartAsync() override { this->NotifyRestartRequired(); }
 private:
  ~RestartTestJob() override = default;
};

class CancelTestJob : public URLRequestTestJob {
 public:
  explicit CancelTestJob(URLRequest* request, NetworkDelegate* network_delegate)
    : URLRequestTestJob(request, network_delegate, true) {}
 protected:
  void StartAsync() override { request_->Cancel(); }
 private:
  ~CancelTestJob() override = default;
};

class CancelThenRestartTestJob : public URLRequestTestJob {
 public:
  explicit CancelThenRestartTestJob(URLRequest* request,
                                    NetworkDelegate* network_delegate)
      : URLRequestTestJob(request, network_delegate, true) {
  }
 protected:
  void StartAsync() override {
    request_->Cancel();
    this->NotifyRestartRequired();
  }
 private:
  ~CancelThenRestartTestJob() override = default;
};

// An Interceptor for use with interceptor tests.
class MockURLRequestInterceptor : public URLRequestInterceptor {
 public:
  // Static getters for canned response header and data strings.
  static std::string ok_data() {
    return URLRequestTestJob::test_data_1();
  }

  static std::string ok_headers() {
    return URLRequestTestJob::test_headers();
  }

  static std::string redirect_data() {
    return std::string();
  }

  static std::string redirect_headers() {
    return URLRequestTestJob::test_redirect_headers();
  }

  static std::string error_data() {
    return std::string("ohhh nooooo mr. bill!");
  }

  static std::string error_headers() {
    return URLRequestTestJob::test_error_headers();
  }

  MockURLRequestInterceptor()
      : intercept_main_request_(false), restart_main_request_(false),
        cancel_main_request_(false), cancel_then_restart_main_request_(false),
        simulate_main_network_error_(false),
        intercept_redirect_(false), cancel_redirect_request_(false),
        intercept_final_response_(false), cancel_final_request_(false),
        use_url_request_http_job_(false),
        did_intercept_main_(false), did_restart_main_(false),
        did_cancel_main_(false), did_cancel_then_restart_main_(false),
        did_simulate_error_main_(false),
        did_intercept_redirect_(false), did_cancel_redirect_(false),
        did_intercept_final_(false), did_cancel_final_(false) {
  }

  ~MockURLRequestInterceptor() override = default;

  // URLRequestInterceptor implementation:
  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    if (restart_main_request_) {
      restart_main_request_ = false;
      did_restart_main_ = true;
      return new RestartTestJob(request, network_delegate);
    }
    if (cancel_main_request_) {
      cancel_main_request_ = false;
      did_cancel_main_ = true;
      return new CancelTestJob(request, network_delegate);
    }
    if (cancel_then_restart_main_request_) {
      cancel_then_restart_main_request_ = false;
      did_cancel_then_restart_main_ = true;
      return new CancelThenRestartTestJob(request, network_delegate);
    }
    if (simulate_main_network_error_) {
      simulate_main_network_error_ = false;
      did_simulate_error_main_ = true;
      if (use_url_request_http_job_) {
        return URLRequestHttpJob::Factory(request, network_delegate, "http");
      }
      // This job will result in error since the requested URL is not one of the
      // URLs supported by these tests.
      return new URLRequestTestJob(request, network_delegate, true);
    }
    if (!intercept_main_request_)
      return nullptr;
    intercept_main_request_ = false;
    did_intercept_main_ = true;
    URLRequestTestJob* job =  new URLRequestTestJob(request,
                                                    network_delegate,
                                                    main_headers_,
                                                    main_data_,
                                                    true);
    job->set_load_timing_info(main_request_load_timing_info_);
    return job;
  }

  URLRequestJob* MaybeInterceptRedirect(URLRequest* request,
                                        NetworkDelegate* network_delegate,
                                        const GURL& location) const override {
    if (cancel_redirect_request_) {
      cancel_redirect_request_ = false;
      did_cancel_redirect_ = true;
      return new CancelTestJob(request, network_delegate);
    }
    if (!intercept_redirect_)
      return nullptr;
    intercept_redirect_ = false;
    did_intercept_redirect_ = true;
    if (use_url_request_http_job_) {
      return URLRequestHttpJob::Factory(request, network_delegate, "http");
    }
    return new URLRequestTestJob(request,
                                 network_delegate,
                                 redirect_headers_,
                                 redirect_data_,
                                 true);
  }

  URLRequestJob* MaybeInterceptResponse(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    if (cancel_final_request_) {
      cancel_final_request_ = false;
      did_cancel_final_ = true;
      return new CancelTestJob(request, network_delegate);
    }
    if (!intercept_final_response_)
      return nullptr;
    intercept_final_response_ = false;
    did_intercept_final_ = true;
    if (use_url_request_http_job_) {
      return URLRequestHttpJob::Factory(request, network_delegate, "http");
    }
    return new URLRequestTestJob(request,
                                 network_delegate,
                                 final_headers_,
                                 final_data_,
                                 true);
  }

  void set_intercept_main_request(bool intercept_main_request) {
    intercept_main_request_ = intercept_main_request;
  }

  void set_main_headers(const std::string& main_headers) {
    main_headers_ = main_headers;
  }

  void set_main_data(const std::string& main_data) {
    main_data_ = main_data;
  }

  void set_main_request_load_timing_info(
      const LoadTimingInfo& main_request_load_timing_info) {
    main_request_load_timing_info_ = main_request_load_timing_info;
  }

  void set_restart_main_request(bool restart_main_request) {
    restart_main_request_ = restart_main_request;
  }

  void set_cancel_main_request(bool cancel_main_request) {
    cancel_main_request_ = cancel_main_request;
  }

  void set_cancel_then_restart_main_request(
      bool cancel_then_restart_main_request) {
    cancel_then_restart_main_request_ = cancel_then_restart_main_request;
  }

  void set_simulate_main_network_error(bool simulate_main_network_error) {
    simulate_main_network_error_ = simulate_main_network_error;
  }

  void set_intercept_redirect(bool intercept_redirect) {
    intercept_redirect_ = intercept_redirect;
  }

  void set_redirect_headers(const std::string& redirect_headers) {
    redirect_headers_ = redirect_headers;
  }

  void set_redirect_data(const std::string& redirect_data) {
    redirect_data_ = redirect_data;
  }

  void set_cancel_redirect_request(bool cancel_redirect_request) {
    cancel_redirect_request_ = cancel_redirect_request;
  }

  void set_intercept_final_response(bool intercept_final_response) {
    intercept_final_response_ = intercept_final_response;
  }

  void set_final_headers(const std::string& final_headers) {
    final_headers_ = final_headers;
  }

  void set_final_data(const std::string& final_data) {
    final_data_ = final_data;
  }

  void set_cancel_final_request(bool cancel_final_request) {
    cancel_final_request_ = cancel_final_request;
  }

  void set_use_url_request_http_job(bool use_url_request_http_job) {
    use_url_request_http_job_ = use_url_request_http_job;
  }

  bool did_intercept_main() const {
    return did_intercept_main_;
  }

  bool did_restart_main() const {
    return did_restart_main_;
  }

  bool did_cancel_main() const {
    return did_cancel_main_;
  }

  bool did_cancel_then_restart_main() const {
    return did_cancel_then_restart_main_;
  }

  bool did_simulate_error_main() const {
    return did_simulate_error_main_;
  }

  bool did_intercept_redirect() const {
    return did_intercept_redirect_;
  }

  bool did_cancel_redirect() const {
    return did_cancel_redirect_;
  }

  bool did_intercept_final() const {
    return did_intercept_final_;
  }

  bool did_cancel_final() const {
    return did_cancel_final_;
  }

 private:
  // Indicate whether to intercept the main request, and if so specify the
  // response to return and the LoadTimingInfo to use.
  mutable bool intercept_main_request_;
  mutable std::string main_headers_;
  mutable std::string main_data_;
  mutable LoadTimingInfo main_request_load_timing_info_;

  // These indicate actions that can be taken within MaybeInterceptRequest.
  mutable bool restart_main_request_;
  mutable bool cancel_main_request_;
  mutable bool cancel_then_restart_main_request_;
  mutable bool simulate_main_network_error_;

  // Indicate whether to intercept redirects, and if so specify the response to
  // return.
  mutable bool intercept_redirect_;
  mutable std::string redirect_headers_;
  mutable std::string redirect_data_;

  // Cancel the request within MaybeInterceptRedirect.
  mutable bool cancel_redirect_request_;

  // Indicate whether to intercept the final response, and if so specify the
  // response to return.
  mutable bool intercept_final_response_;
  mutable std::string final_headers_;
  mutable std::string final_data_;

  // Cancel the final request within MaybeInterceptResponse.
  mutable bool cancel_final_request_;

  // Instruct the interceptor to use a real URLRequestHTTPJob.
  mutable bool use_url_request_http_job_;

  // These indicate if the interceptor did something or not.
  mutable bool did_intercept_main_;
  mutable bool did_restart_main_;
  mutable bool did_cancel_main_;
  mutable bool did_cancel_then_restart_main_;
  mutable bool did_simulate_error_main_;
  mutable bool did_intercept_redirect_;
  mutable bool did_cancel_redirect_;
  mutable bool did_intercept_final_;
  mutable bool did_cancel_final_;
};

// Inherit PlatformTest since we require the autorelease pool on Mac OS X.
class URLRequestInterceptorTest : public URLRequestTest {
 public:
  URLRequestInterceptorTest() : URLRequestTest(), interceptor_(NULL) {
  }

  ~URLRequestInterceptorTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  void SetUpFactory() override {
    interceptor_ = new MockURLRequestInterceptor();
    job_factory_.reset(new URLRequestInterceptingJobFactory(
        std::move(job_factory_), base::WrapUnique(interceptor_)));
  }

  MockURLRequestInterceptor* interceptor() const {
    return interceptor_;
  }

 private:
  MockURLRequestInterceptor* interceptor_;
};

TEST_F(URLRequestInterceptorTest, Intercept) {
  // Intercept the main request and respond with a simple response.
  interceptor()->set_intercept_main_request(true);
  interceptor()->set_main_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_main_data(MockURLRequestInterceptor::ok_data());
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  base::SupportsUserData::Data* user_data0 = new base::SupportsUserData::Data();
  base::SupportsUserData::Data* user_data1 = new base::SupportsUserData::Data();
  base::SupportsUserData::Data* user_data2 = new base::SupportsUserData::Data();
  req->SetUserData(&user_data0, base::WrapUnique(user_data0));
  req->SetUserData(&user_data1, base::WrapUnique(user_data1));
  req->SetUserData(&user_data2, base::WrapUnique(user_data2));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Make sure we can retrieve our specific user data.
  EXPECT_EQ(user_data0, req->GetUserData(&user_data0));
  EXPECT_EQ(user_data1, req->GetUserData(&user_data1));
  EXPECT_EQ(user_data2, req->GetUserData(&user_data2));

  // Check that we got one good response.
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(200, req->response_headers()->response_code());
  EXPECT_EQ(MockURLRequestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestInterceptorTest, InterceptRedirect) {
  // Intercept the main request and respond with a redirect.
  interceptor()->set_intercept_main_request(true);
  interceptor()->set_main_headers(
      MockURLRequestInterceptor::redirect_headers());
  interceptor()->set_main_data(MockURLRequestInterceptor::redirect_data());

  // Intercept that redirect and respond with a final OK response.
  interceptor()->set_intercept_redirect(true);
  interceptor()->set_redirect_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_redirect_data(MockURLRequestInterceptor::ok_data());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_intercept_main());
  EXPECT_TRUE(interceptor()->did_intercept_redirect());

  // Check that we got one good response.
  int status = d.request_status();
  EXPECT_EQ(OK, status);
  if (status == OK)
    EXPECT_EQ(200, req->response_headers()->response_code());

  EXPECT_EQ(MockURLRequestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestInterceptorTest, InterceptServerError) {
  // Intercept the main request to generate a server error response.
  interceptor()->set_intercept_main_request(true);
  interceptor()->set_main_headers(MockURLRequestInterceptor::error_headers());
  interceptor()->set_main_data(MockURLRequestInterceptor::error_data());

  // Intercept that error and respond with an OK response.
  interceptor()->set_intercept_final_response(true);
  interceptor()->set_final_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_final_data(MockURLRequestInterceptor::ok_data());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_intercept_main());
  EXPECT_TRUE(interceptor()->did_intercept_final());

  // Check that we got one good response.
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(200, req->response_headers()->response_code());
  EXPECT_EQ(MockURLRequestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestInterceptorTest, InterceptNetworkError) {
  // Intercept the main request to simulate a network error.
  interceptor()->set_simulate_main_network_error(true);

  // Intercept that error and respond with an OK response.
  interceptor()->set_intercept_final_response(true);
  interceptor()->set_final_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_final_data(MockURLRequestInterceptor::ok_data());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_simulate_error_main());
  EXPECT_TRUE(interceptor()->did_intercept_final());

  // Check that we received one good response.
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(200, req->response_headers()->response_code());
  EXPECT_EQ(MockURLRequestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestInterceptorTest, InterceptRestartRequired) {
  // Restart the main request.
  interceptor()->set_restart_main_request(true);

  // then intercept the new main request and respond with an OK response
  interceptor()->set_intercept_main_request(true);
  interceptor()->set_main_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_main_data(MockURLRequestInterceptor::ok_data());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_restart_main());
  EXPECT_TRUE(interceptor()->did_intercept_main());

  // Check that we received one good response.
  int status = d.request_status();
  EXPECT_EQ(OK, status);
  if (status == OK)
    EXPECT_EQ(200, req->response_headers()->response_code());

  EXPECT_EQ(MockURLRequestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestInterceptorTest, InterceptRespectsCancelMain) {
  // Intercept the main request and cancel from within the restarted job.
  interceptor()->set_cancel_main_request(true);

  // Set up to intercept the final response and override it with an OK response.
  interceptor()->set_intercept_final_response(true);
  interceptor()->set_final_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_final_data(MockURLRequestInterceptor::ok_data());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_cancel_main());
  EXPECT_FALSE(interceptor()->did_intercept_final());

  // Check that we see a canceled request.
  EXPECT_EQ(ERR_ABORTED, d.request_status());
}

TEST_F(URLRequestInterceptorTest, InterceptRespectsCancelRedirect) {
  // Intercept the main request and respond with a redirect.
  interceptor()->set_intercept_main_request(true);
  interceptor()->set_main_headers(
      MockURLRequestInterceptor::redirect_headers());
  interceptor()->set_main_data(MockURLRequestInterceptor::redirect_data());

  // Intercept the redirect and cancel from within that job.
  interceptor()->set_cancel_redirect_request(true);

  // Set up to intercept the final response and override it with an OK response.
  interceptor()->set_intercept_final_response(true);
  interceptor()->set_final_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_final_data(MockURLRequestInterceptor::ok_data());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_intercept_main());
  EXPECT_TRUE(interceptor()->did_cancel_redirect());
  EXPECT_FALSE(interceptor()->did_intercept_final());

  // Check that we see a canceled request.
  EXPECT_EQ(ERR_ABORTED, d.request_status());
}

TEST_F(URLRequestInterceptorTest, InterceptRespectsCancelFinal) {
  // Intercept the main request to simulate a network error.
  interceptor()->set_simulate_main_network_error(true);

  // Set up to intercept final the response and cancel from within that job.
  interceptor()->set_cancel_final_request(true);

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_simulate_error_main());
  EXPECT_TRUE(interceptor()->did_cancel_final());

  // Check that we see a canceled request.
  EXPECT_EQ(ERR_ABORTED, d.request_status());
}

TEST_F(URLRequestInterceptorTest, InterceptRespectsCancelInRestart) {
  // Intercept the main request and cancel then restart from within that job.
  interceptor()->set_cancel_then_restart_main_request(true);

  // Set up to intercept the final response and override it with an OK response.
  interceptor()->set_intercept_final_response(true);
  interceptor()->set_final_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_final_data(MockURLRequestInterceptor::ok_data());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://test_intercept/foo"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  // Check that the interceptor got called as expected.
  EXPECT_TRUE(interceptor()->did_cancel_then_restart_main());
  EXPECT_FALSE(interceptor()->did_intercept_final());

  // Check that we see a canceled request.
  EXPECT_EQ(ERR_ABORTED, d.request_status());
}

// "Normal" LoadTimingInfo as returned by a job.  Everything is in order, not
// reused.  |connect_time_flags| is used to indicate if there should be dns
// or SSL times, and |used_proxy| is used for proxy times.
LoadTimingInfo NormalLoadTimingInfo(base::TimeTicks now,
                                    int connect_time_flags,
                                    bool used_proxy) {
  LoadTimingInfo load_timing;
  load_timing.socket_log_id = 1;

  if (used_proxy) {
    load_timing.proxy_resolve_start = now + base::TimeDelta::FromDays(1);
    load_timing.proxy_resolve_end = now + base::TimeDelta::FromDays(2);
  }

  LoadTimingInfo::ConnectTiming& connect_timing = load_timing.connect_timing;
  if (connect_time_flags & CONNECT_TIMING_HAS_DNS_TIMES) {
    connect_timing.dns_start = now + base::TimeDelta::FromDays(3);
    connect_timing.dns_end = now + base::TimeDelta::FromDays(4);
  }
  connect_timing.connect_start = now + base::TimeDelta::FromDays(5);
  if (connect_time_flags & CONNECT_TIMING_HAS_SSL_TIMES) {
    connect_timing.ssl_start = now + base::TimeDelta::FromDays(6);
    connect_timing.ssl_end = now + base::TimeDelta::FromDays(7);
  }
  connect_timing.connect_end = now + base::TimeDelta::FromDays(8);

  load_timing.send_start = now + base::TimeDelta::FromDays(9);
  load_timing.send_end = now + base::TimeDelta::FromDays(10);
  load_timing.receive_headers_end = now + base::TimeDelta::FromDays(11);
  return load_timing;
}

// Same as above, but in the case of a reused socket.
LoadTimingInfo NormalLoadTimingInfoReused(base::TimeTicks now,
                                          bool used_proxy) {
  LoadTimingInfo load_timing;
  load_timing.socket_log_id = 1;
  load_timing.socket_reused = true;

  if (used_proxy) {
    load_timing.proxy_resolve_start = now + base::TimeDelta::FromDays(1);
    load_timing.proxy_resolve_end = now + base::TimeDelta::FromDays(2);
  }

  load_timing.send_start = now + base::TimeDelta::FromDays(9);
  load_timing.send_end = now + base::TimeDelta::FromDays(10);
  load_timing.receive_headers_end = now + base::TimeDelta::FromDays(11);
  return load_timing;
}

LoadTimingInfo RunURLRequestInterceptorLoadTimingTest(
    const LoadTimingInfo& job_load_timing,
    const URLRequestContext& context,
    MockURLRequestInterceptor* interceptor) {
  interceptor->set_intercept_main_request(true);
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
  EXPECT_EQ(job_load_timing.receive_headers_end,
            resulting_load_timing.receive_headers_end);
  EXPECT_EQ(job_load_timing.push_start, resulting_load_timing.push_start);
  EXPECT_EQ(job_load_timing.push_end, resulting_load_timing.push_end);

  return resulting_load_timing;
}

// Basic test that the intercept + load timing tests work.
TEST_F(URLRequestInterceptorTest, InterceptLoadTiming) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_DNS_TIMES, false);

  LoadTimingInfo load_timing_result =
      RunURLRequestInterceptorLoadTimingTest(
          job_load_timing, default_context(), interceptor());

  // Nothing should have been changed by the URLRequest.
  EXPECT_EQ(job_load_timing.proxy_resolve_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(job_load_timing.proxy_resolve_end,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(job_load_timing.connect_timing.dns_start,
            load_timing_result.connect_timing.dns_start);
  EXPECT_EQ(job_load_timing.connect_timing.dns_end,
            load_timing_result.connect_timing.dns_end);
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
TEST_F(URLRequestInterceptorTest, InterceptLoadTimingProxy) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_SSL_TIMES, true);

  LoadTimingInfo load_timing_result =
      RunURLRequestInterceptorLoadTimingTest(
          job_load_timing, default_context(), interceptor());

  // Nothing should have been changed by the URLRequest.
  EXPECT_EQ(job_load_timing.proxy_resolve_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(job_load_timing.proxy_resolve_end,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(job_load_timing.connect_timing.dns_start,
            load_timing_result.connect_timing.dns_start);
  EXPECT_EQ(job_load_timing.connect_timing.dns_end,
            load_timing_result.connect_timing.dns_end);
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
TEST_F(URLRequestInterceptorTest, InterceptLoadTimingEarlyProxyResolution) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_DNS_TIMES, true);
  job_load_timing.proxy_resolve_start = now - base::TimeDelta::FromDays(6);
  job_load_timing.proxy_resolve_end = now - base::TimeDelta::FromDays(5);
  job_load_timing.connect_timing.dns_start = now - base::TimeDelta::FromDays(4);
  job_load_timing.connect_timing.dns_end = now - base::TimeDelta::FromDays(3);
  job_load_timing.connect_timing.connect_start =
      now - base::TimeDelta::FromDays(2);
  job_load_timing.connect_timing.connect_end =
      now - base::TimeDelta::FromDays(1);

  LoadTimingInfo load_timing_result =
      RunURLRequestInterceptorLoadTimingTest(
          job_load_timing, default_context(), interceptor());

  // Proxy times, connect times, and DNS times should all be replaced with
  // request_start.
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.proxy_resolve_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.dns_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.dns_end);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_start);
  EXPECT_EQ(load_timing_result.request_start,
            load_timing_result.connect_timing.connect_end);

  // Other times should have been left null.
  TestLoadTimingNotReusedWithProxy(load_timing_result,
                                   CONNECT_TIMING_HAS_DNS_TIMES);
}

// Same as above, but in the reused case.
TEST_F(URLRequestInterceptorTest,
       InterceptLoadTimingEarlyProxyResolutionReused) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing = NormalLoadTimingInfoReused(now, true);
  job_load_timing.proxy_resolve_start = now - base::TimeDelta::FromDays(4);
  job_load_timing.proxy_resolve_end = now - base::TimeDelta::FromDays(3);

  LoadTimingInfo load_timing_result =
      RunURLRequestInterceptorLoadTimingTest(
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
TEST_F(URLRequestInterceptorTest, InterceptLoadTimingEarlyConnect) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_SSL_TIMES, false);
  job_load_timing.connect_timing.connect_start =
      now - base::TimeDelta::FromDays(1);
  job_load_timing.connect_timing.ssl_start = now - base::TimeDelta::FromDays(2);
  job_load_timing.connect_timing.ssl_end = now - base::TimeDelta::FromDays(3);
  job_load_timing.connect_timing.connect_end =
      now - base::TimeDelta::FromDays(4);

  LoadTimingInfo load_timing_result =
      RunURLRequestInterceptorLoadTimingTest(
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
TEST_F(URLRequestInterceptorTest, InterceptLoadTimingEarlyConnectWithProxy) {
  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo job_load_timing =
      NormalLoadTimingInfo(now, CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY, true);
  job_load_timing.connect_timing.connect_start =
      now - base::TimeDelta::FromDays(1);
  job_load_timing.connect_timing.connect_end =
      now - base::TimeDelta::FromDays(2);

  LoadTimingInfo load_timing_result =
      RunURLRequestInterceptorLoadTimingTest(
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

// Check that two different URL requests have different identifiers.
TEST_F(URLRequestTest, Identifiers) {
  TestDelegate d;
  TestURLRequestContext context;
  std::unique_ptr<URLRequest> req(
      context.CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                            TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<URLRequest> other_req(
      context.CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                            TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_NE(req->identifier(), other_req->identifier());
}

#if defined(OS_IOS)
// TODO(droger): Check that a failure to connect to the proxy is reported to
// the network delegate. crbug.com/496743
#define MAYBE_NetworkDelegateProxyError DISABLED_NetworkDelegateProxyError
#else
#define MAYBE_NetworkDelegateProxyError NetworkDelegateProxyError
#endif
TEST_F(URLRequestTest, MAYBE_NetworkDelegateProxyError) {
  MockHostResolver host_resolver;
  host_resolver.rules()->AddSimulatedFailure("*");

  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContextWithProxy context("myproxy:70", &network_delegate);

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context.CreateRequest(GURL("http://example.com"), DEFAULT_PRIORITY, &d,
                            TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");

  req->Start();
  d.RunUntilComplete();

  // Check we see a failed request.
  // The proxy server is not set before failure.
  EXPECT_FALSE(req->proxy_server().is_valid());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, d.request_status());

  EXPECT_EQ(1, network_delegate.error_count());
  EXPECT_THAT(network_delegate.last_error(),
              IsError(ERR_PROXY_CONNECTION_FAILED));
  EXPECT_EQ(1, network_delegate.completed_requests());
}

// Make sure that NetworkDelegate::NotifyCompleted is called if
// content is empty.
TEST_F(URLRequestTest, RequestCompletionForEmptyResponse) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("data:,"), DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ("", d.data_received());
  EXPECT_EQ(1, default_network_delegate_.completed_requests());
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
  std::unique_ptr<URLRequestJob> job(new PriorityMonitoringURLRequestJob(
      req.get(), &default_network_delegate_, &job_priority));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));
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
  std::unique_ptr<URLRequestJob> job(new PriorityMonitoringURLRequestJob(
      req.get(), &default_network_delegate_, &job_priority));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));

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
  std::unique_ptr<URLRequestJob> job(new PriorityMonitoringURLRequestJob(
      req.get(), &default_network_delegate_, &job_priority));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));

  req->SetLoadFlags(LOAD_IGNORE_LIMITS);
  EXPECT_EQ(MAXIMUM_PRIORITY, req->priority());

  req->SetPriority(MAXIMUM_PRIORITY);
  EXPECT_EQ(MAXIMUM_PRIORITY, req->priority());

  req->Start();
  EXPECT_EQ(MAXIMUM_PRIORITY, req->priority());
  EXPECT_EQ(MAXIMUM_PRIORITY, job_priority);
}

namespace {

// Less verbose way of running a simple testserver for the tests below.
class HttpTestServer : public EmbeddedTestServer {
 public:
  explicit HttpTestServer(const base::FilePath& document_root) {
    AddDefaultHandlers(document_root);
  }

  HttpTestServer() { AddDefaultHandlers(base::FilePath()); }
};

}  // namespace

TEST_F(URLRequestTest, DelayedCookieCallback) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestURLRequestContext context;
  std::unique_ptr<DelayedCookieMonster> delayed_cm(new DelayedCookieMonster());
  context.set_cookie_store(delayed_cm.get());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    context.set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotSend=1"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(1, network_delegate.set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    context.set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotSend=1"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent when LOAD_DO_NOT_SEND_COOKIES is set.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetLoadFlags(LOAD_DO_NOT_SEND_COOKIES);
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    // LOAD_DO_NOT_SEND_COOKIES does not trigger OnGetCookies.
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotUpdate=2"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(1, network_delegate.set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetLoadFlags(LOAD_DO_NOT_SAVE_COOKIES);
    req->Start();

    d.RunUntilComplete();

    // LOAD_DO_NOT_SAVE_COOKIES does not trigger OnSetCookie.
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(0, network_delegate.set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(0, network_delegate.set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotSend=1"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    TestNetLogEntry::List entries;
    net_log_.GetEntries(&entries);
    for (const auto& entry : entries) {
      EXPECT_NE(entry.type,
                NetLogEventType::COOKIE_GET_BLOCKED_BY_NETWORK_DELEGATE);
    }
  }

  // Verify that the cookie isn't sent.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_GET_COOKIES);
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    EXPECT_EQ(1, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    TestNetLogEntry::List entries;
    net_log_.GetEntries(&entries);
    ExpectLogContainsSomewhereAfter(
        entries, 0, NetLogEventType::COOKIE_GET_BLOCKED_BY_NETWORK_DELEGATE,
        NetLogEventPhase::NONE);
  }
}

// TODO(crbug.com/564656) This test is flaky on iOS.
#if defined(OS_IOS)
#define MAYBE_DoNotSaveCookies_ViaPolicy FLAKY_DoNotSaveCookies_ViaPolicy
#else
#define MAYBE_DoNotSaveCookies_ViaPolicy DoNotSaveCookies_ViaPolicy
#endif
TEST_F(URLRequestTest, DoNotSaveCookies_ViaPolicy) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotUpdate=2"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    TestNetLogEntry::List entries;
    net_log_.GetEntries(&entries);
    for (const auto& entry : entries) {
      EXPECT_NE(entry.type,
                NetLogEventType::COOKIE_SET_BLOCKED_BY_NETWORK_DELEGATE);
    }
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();

    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(2, network_delegate.blocked_set_cookie_count());
    TestNetLogEntry::List entries;
    net_log_.GetEntries(&entries);
    ExpectLogContainsSomewhereAfter(
        entries, 0, NetLogEventType::COOKIE_SET_BLOCKED_BY_NETWORK_DELEGATE,
        NetLogEventPhase::NONE);
  }

  // Verify the cookies weren't saved or updated.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveEmptyCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up an empty cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(0, network_delegate.set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSendCookies_ViaPolicy_Async) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotSend=1"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSend=1")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie isn't sent.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_GET_COOKIES);
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("Cookie: CookieToNotSend=1")
                == std::string::npos);

    EXPECT_EQ(1, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, DoNotSaveCookies_ViaPolicy_Async) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up a cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotUpdate=2"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Try to set-up another cookie and update the previous cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    network_delegate.set_cookie_options(TestNetworkDelegate::NO_SET_COOKIE);
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/set-cookie?CookieToNotSave=1&CookieToNotUpdate=1"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();

    d.RunUntilComplete();

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(2, network_delegate.blocked_set_cookie_count());
  }

  // Verify the cookies weren't saved or updated.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("CookieToNotSave=1")
                == std::string::npos);
    EXPECT_TRUE(d.data_received().find("CookieToNotUpdate=2")
                != std::string::npos);

    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, SameSiteCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  TestNetworkDelegate network_delegate;
  default_context().set_network_delegate(&network_delegate);

  const std::string kHost = "example.test";
  const std::string kSubHost = "subdomain.example.test";
  const std::string kCrossHost = "cross-origin.test";

  // Set up two 'SameSite' cookies on 'example.test'
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost,
                           "/set-cookie?StrictSameSiteCookie=1;SameSite=Strict&"
                           "LaxSameSiteCookie=1;SameSite=Lax"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
    EXPECT_EQ(2, network_delegate.set_cookie_count());
  }

  // Verify that both cookies are sent for same-site requests.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(test_server.GetURL(kHost, "/"));
    req->set_initiator(url::Origin::Create(test_server.GetURL(kHost, "/")));
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that both cookies are sent when the request has no initiator (can
  // happen for main frame browser-initiated navigations).
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(test_server.GetURL(kHost, "/"));
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that both cookies are sent for same-registrable-domain requests.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(test_server.GetURL(kSubHost, "/"));
    req->set_initiator(url::Origin::Create(test_server.GetURL(kSubHost, "/")));
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that neither cookie is not sent for cross-site requests.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(test_server.GetURL(kCrossHost, "/"));
    req->set_initiator(
        url::Origin::Create(test_server.GetURL(kCrossHost, "/")));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_EQ(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the lax cookie is sent for cross-site initiators when the
  // method is "safe".
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(test_server.GetURL(kHost, "/"));
    req->set_initiator(
        url::Origin::Create(test_server.GetURL(kCrossHost, "/")));
    req->set_method("GET");
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_NE(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that neither cookie is sent for cross-site initiators when the
  // method is unsafe (e.g. POST).
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(kHost, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(test_server.GetURL(kHost, "/"));
    req->set_initiator(
        url::Origin::Create(test_server.GetURL(kCrossHost, "/")));
    req->set_method("POST");
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(std::string::npos,
              d.data_received().find("StrictSameSiteCookie=1"));
    EXPECT_EQ(std::string::npos, d.data_received().find("LaxSameSiteCookie=1"));
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

// Tests that __Secure- cookies can't be set on non-secure origins.
TEST_F(URLRequestTest, SecureCookiePrefixOnNonsecureOrigin) {
  EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  // Try to set a Secure __Secure- cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        http_server.GetURL("/set-cookie?__Secure-nonsecure-origin=1;Secure"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is not set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        https_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(d.data_received().find("__Secure-nonsecure-origin=1"),
              std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, SecureCookiePrefixNonsecure) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(https_server.Start());

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  // Try to set a non-Secure __Secure- cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        https_server.GetURL("/set-cookie?__Secure-foo=1"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.set_cookie_count());
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is not set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        https_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(d.data_received().find("__Secure-foo=1"), std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

TEST_F(URLRequestTest, SecureCookiePrefixSecure) {
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(https_server.Start());

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  // Try to set a Secure __Secure- cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        https_server.GetURL("/set-cookie?__Secure-bar=1;Secure"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        https_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_NE(d.data_received().find("__Secure-bar=1"), std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

// Tests that secure cookies can't be set on non-secure origins if strict secure
// cookies are enabled.
TEST_F(URLRequestTest, StrictSecureCookiesOnNonsecureOrigin) {
  EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  // Try to set a Secure cookie, with experimental features enabled.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        http_server.GetURL("/set-cookie?nonsecure-origin=1;Secure"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }

  // Verify that the cookie is not set.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        https_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_EQ(d.data_received().find("nonsecure-origin=1"), std::string::npos);
    EXPECT_EQ(0, network_delegate.blocked_get_cookies_count());
    EXPECT_EQ(0, network_delegate.blocked_set_cookie_count());
  }
}

// The parameter is true for same-site and false for cross-site requests.
class URLRequestTestParameterizedSameSite
    : public URLRequestTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  URLRequestTestParameterizedSameSite() {
    auto params = std::make_unique<HttpNetworkSession::Params>();
    params->ignore_certificate_errors = true;
    context_.set_http_network_session_params(std::move(params));
    context_.set_network_delegate(&network_delegate_);
    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
    EXPECT_TRUE(https_server_.Start());
  }

  // To be called after configuration of |context_| has been finalized.
  void InitContext() { context_.Init(); }

  const std::string kHost_ = "example.test";
  const std::string kCrossHost_ = "cross-site.test";
  TestURLRequestContext context_{true};
  TestNetworkDelegate network_delegate_;
  base::HistogramTester histograms_;
  EmbeddedTestServer https_server_{EmbeddedTestServer::TYPE_HTTPS};
};

INSTANTIATE_TEST_CASE_P(URLRequestTest,
                        URLRequestTestParameterizedSameSite,
                        ::testing::Bool());

TEST_P(URLRequestTestParameterizedSameSite, CookieAgeMetrics) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;
  InitContext();

  EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(http_server.Start());

  // Set two test cookies.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        http_server.GetURL(kHost_, "/set-cookie?cookie=value&cookie2=value2"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(2, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.AgeForNonSecureCrossSiteRequest", 0);
    histograms_.ExpectTotalCount("Cookie.AgeForNonSecureSameSiteRequest", 0);
    histograms_.ExpectTotalCount("Cookie.AgeForSecureCrossSiteRequest", 0);
    histograms_.ExpectTotalCount("Cookie.AgeForSecureSameSiteRequest", 0);
    histograms_.ExpectTotalCount("Cookie.AllAgesForNonSecureCrossSiteRequest",
                                 0);
    histograms_.ExpectTotalCount("Cookie.AllAgesForNonSecureSameSiteRequest",
                                 0);
    histograms_.ExpectTotalCount("Cookie.AllAgesForSecureCrossSiteRequest", 0);
    histograms_.ExpectTotalCount("Cookie.AllAgesForSecureSameSiteRequest", 0);
  }

  // Make a secure request.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(
        url::Origin::Create(https_server_.GetURL(kInitiatingHost, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.AgeForNonSecureCrossSiteRequest", 0);
    histograms_.ExpectTotalCount("Cookie.AgeForNonSecureSameSiteRequest", 0);
    histograms_.ExpectTotalCount("Cookie.AgeForSecureCrossSiteRequest",
                                 !same_site);
    histograms_.ExpectTotalCount("Cookie.AgeForSecureSameSiteRequest",
                                 same_site);
    histograms_.ExpectTotalCount("Cookie.AllAgesForNonSecureCrossSiteRequest",
                                 0);
    histograms_.ExpectTotalCount("Cookie.AllAgesForNonSecureSameSiteRequest",
                                 0);
    histograms_.ExpectTotalCount("Cookie.AllAgesForSecureCrossSiteRequest",
                                 same_site ? 0 : 2);
    histograms_.ExpectTotalCount("Cookie.AllAgesForSecureSameSiteRequest",
                                 same_site ? 2 : 0);
    EXPECT_TRUE(d.data_received().find("cookie=value") != std::string::npos);
    EXPECT_TRUE(d.data_received().find("cookie2=value2") != std::string::npos);
  }

  // Make a non-secure request.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        http_server.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(http_server.GetURL(kInitiatingHost, "/"));
    req->set_initiator(
        url::Origin::Create(http_server.GetURL(kInitiatingHost, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.AgeForNonSecureCrossSiteRequest",
                                 !same_site);
    histograms_.ExpectTotalCount("Cookie.AgeForNonSecureSameSiteRequest",
                                 same_site);
    histograms_.ExpectTotalCount("Cookie.AgeForSecureCrossSiteRequest",
                                 !same_site);
    histograms_.ExpectTotalCount("Cookie.AgeForSecureSameSiteRequest",
                                 same_site);
    histograms_.ExpectTotalCount("Cookie.AllAgesForNonSecureCrossSiteRequest",
                                 same_site ? 0 : 2);
    histograms_.ExpectTotalCount("Cookie.AllAgesForNonSecureSameSiteRequest",
                                 same_site ? 2 : 0);
    histograms_.ExpectTotalCount("Cookie.AllAgesForSecureCrossSiteRequest",
                                 same_site ? 0 : 2);
    histograms_.ExpectTotalCount("Cookie.AllAgesForSecureSameSiteRequest",
                                 same_site ? 2 : 0);
    EXPECT_TRUE(d.data_received().find("cookie=value") != std::string::npos);
    EXPECT_TRUE(d.data_received().find("cookie2=value2") != std::string::npos);
  }
}

// Cookies with secure attribute (no HSTS) --> k1pSecureAttribute
TEST_P(URLRequestTestParameterizedSameSite,
       CookieNetworkSecurityMetricSecureAttribute) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;
  InitContext();

  // Set cookies.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_,
                             "/set-cookie?session-cookie=value;Secure&"
                             "longlived-cookie=value;Secure;domain=" +
                                 kHost_ + ";Max-Age=360000"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(2, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 0);
  }

  // Verify that the cookies fall into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 2);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pSecureAttribute) |
            static_cast<int>(!same_site),
        2);
  }
}

// Short-lived host cookie --> k1pHSTSHostCookie
TEST_P(URLRequestTestParameterizedSameSite,
       CookieNetworkSecurityMetricShortlivedHostCookie) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;

  TransportSecurityState transport_security_state;
  transport_security_state.AddHSTS(
      kHost_, base::Time::Now() + base::TimeDelta::FromHours(10),
      false /* include_subdomains */);
  context_.set_transport_security_state(&transport_security_state);
  InitContext();

  // Set cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/set-cookie?cookie=value;Max-Age=3600"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(1, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 0);
  }

  // Verify that the cookie falls into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 1);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pHSTSHostCookie) |
            static_cast<int>(!same_site),
        1);
  }
}

// Long-lived (either due to expiry or due to being a session cookie) host
// cookies --> k1pExpiringHSTSHostCookie
TEST_P(URLRequestTestParameterizedSameSite,
       CookieNetworkSecurityMetricLonglivedHostCookie) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;

  TransportSecurityState transport_security_state;
  transport_security_state.AddHSTS(
      kHost_, base::Time::Now() + base::TimeDelta::FromHours(10),
      false /* include_subdomains */);
  context_.set_transport_security_state(&transport_security_state);
  InitContext();

  // Set cookies.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_,
                             "/set-cookie?session-cookie=value&"
                             "longlived-cookie=value;Max-Age=360000"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(2, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 0);
  }

  // Verify that the cookies fall into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 2);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pExpiringHSTSHostCookie) |
            static_cast<int>(!same_site),
        2);
  }
}

// Domain cookie with HSTS subdomains with cookie expiry before HSTS expiry -->
// k1pHSTSSubdomainsIncluded
TEST_P(URLRequestTestParameterizedSameSite,
       CookieNetworkSecurityMetricShortlivedDomainCookie) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;

  TransportSecurityState transport_security_state;
  transport_security_state.AddHSTS(
      kHost_, base::Time::Now() + base::TimeDelta::FromHours(10),
      true /* include_subdomains */);
  context_.set_transport_security_state(&transport_security_state);
  InitContext();

  // Set cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/set-cookie?cookie=value;domain=" +
                                         kHost_ + ";Max-Age=3600"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(1, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 0);
  }

  // Verify that the cookie falls into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 1);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pHSTSSubdomainsIncluded) |
            static_cast<int>(!same_site),
        1);
  }
}

// Long-lived (either due to expiry or due to being a session cookie) domain
// cookies with HSTS subdomains --> k1pExpiringHSTSSubdomainsIncluded
TEST_P(URLRequestTestParameterizedSameSite,
       CookieNetworkSecurityMetricLonglivedDomainCookie) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;

  TransportSecurityState transport_security_state;
  transport_security_state.AddHSTS(
      kHost_, base::Time::Now() + base::TimeDelta::FromHours(10),
      true /* include_subdomains */);
  context_.set_transport_security_state(&transport_security_state);
  InitContext();

  // Set cookies.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(
            kHost_, "/set-cookie?session-cookie=value;domain=" + kHost_ + "&" +
                        "longlived-cookie=value;domain=" + kHost_ +
                        ";Max-Age=360000"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(2, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 0);
  }

  // Verify that the cookies fall into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 2);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(
            CookieNetworkSecurity::k1pExpiringHSTSSubdomainsIncluded) |
            static_cast<int>(!same_site),
        2);
  }
}

// Domain cookie with HSTS subdomains not included --> k1pHSTSSpoofable
TEST_P(URLRequestTestParameterizedSameSite,
       CookieNetworkSecurityMetricSpoofableDomainCookie) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;

  TransportSecurityState transport_security_state;
  transport_security_state.AddHSTS(
      kHost_, base::Time::Now() + base::TimeDelta::FromHours(10),
      false /* include_subdomains */);
  context_.set_transport_security_state(&transport_security_state);
  InitContext();

  // Set cookie.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/set-cookie?cookie=value;domain=" +
                                         kHost_ + ";Max-Age=3600"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(1, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 0);
  }

  // Verify that the cookie falls into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 1);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pHSTSSpoofable) |
            static_cast<int>(!same_site),
        1);
  }
}

// Cookie without HSTS --> k1p(Non)SecureConnection
TEST_P(URLRequestTestParameterizedSameSite, CookieNetworkSecurityMetricNoHSTS) {
  const bool same_site = GetParam();
  const std::string kInitiatingHost = same_site ? kHost_ : kCrossHost_;
  InitContext();

  EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(http_server.Start());

  // Set cookies.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_,
                             "/set-cookie?cookie=value;domain=" + kHost_ +
                                 ";Max-Age=3600&host-cookie=value"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_EQ(2, network_delegate_.set_cookie_count());
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 0);
  }

  // Verify that the cookie falls into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        https_server_.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY,
        &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 2);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pSecureConnection) |
            static_cast<int>(!same_site),
        2);
  }

  // Verify that the cookie falls into the correct metrics bucket.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context_.CreateRequest(
        http_server.GetURL(kHost_, "/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_site_for_cookies(https_server_.GetURL(kInitiatingHost, "/"));
    req->set_initiator(url::Origin::Create(https_server_.GetURL(kHost_, "/")));
    req->Start();
    d.RunUntilComplete();
    histograms_.ExpectTotalCount("Cookie.NetworkSecurity", 4);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pSecureConnection) |
            static_cast<int>(!same_site),
        2);
    // Static cast of boolean required for MSVC 1911.
    histograms_.ExpectBucketCount(
        "Cookie.NetworkSecurity",
        static_cast<int>(CookieNetworkSecurity::k1pNonsecureConnection) |
            static_cast<int>(!same_site),
        2);
  }
}

// Tests that a request is cancelled while entering suspend mode. Uses mocks
// rather than a spawned test server because the connection used to talk to
// the test server is affected by entering suspend mode on Android.
TEST_F(URLRequestTest, CancelOnSuspend) {
  TestPowerMonitorSource* power_monitor_source = new TestPowerMonitorSource();
  base::PowerMonitor power_monitor(base::WrapUnique(power_monitor_source));

  URLRequestFailedJob::AddUrlHandler();

  TestDelegate d;
  // Request that just hangs.
  GURL url(URLRequestFailedJob::GetMockHttpUrl(ERR_IO_PENDING));
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->Start();

  power_monitor_source->Suspend();
  // Wait for the suspend notification to cause the request to fail.
  d.RunUntilComplete();
  EXPECT_EQ(ERR_ABORTED, d.request_status());
  EXPECT_TRUE(d.request_failed());
  EXPECT_EQ(1, default_network_delegate_.completed_requests());

  URLRequestFilter::GetInstance()->ClearHandlers();

  // Shouldn't be needed, but just in case.
  power_monitor_source->Resume();
}

// FixedDateNetworkDelegate swaps out the server's HTTP Date response header
// value for the |fixed_date| argument given to the constructor.
class FixedDateNetworkDelegate : public TestNetworkDelegate {
 public:
  explicit FixedDateNetworkDelegate(const std::string& fixed_date)
      : fixed_date_(fixed_date) {}
  ~FixedDateNetworkDelegate() override = default;

  // NetworkDelegate implementation
  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;

 private:
  std::string fixed_date_;

  DISALLOW_COPY_AND_ASSIGN(FixedDateNetworkDelegate);
};

int FixedDateNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  HttpResponseHeaders* new_response_headers =
      new HttpResponseHeaders(original_response_headers->raw_headers());

  new_response_headers->RemoveHeader("Date");
  new_response_headers->AddHeader("Date: " + fixed_date_);

  *override_response_headers = new_response_headers;
  return TestNetworkDelegate::OnHeadersReceived(
      request, std::move(callback), original_response_headers,
      override_response_headers, allowed_unsafe_redirect_url);
}

// Test that cookie expiration times are adjusted for server/client clock
// skew and that we handle incorrect timezone specifier "UTC" in HTTP Date
// headers by defaulting to GMT. (crbug.com/135131)
TEST_F(URLRequestTest, AcceptClockSkewCookieWithWrongDateTimezone) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  // Set up an expired cookie.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(
            "/set-cookie?StillGood=1;expires=Mon,18-Apr-1977,22:50:13,GMT"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
  }
  // Verify that the cookie is not set.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("StillGood=1") == std::string::npos);
  }
  // Set up a cookie with clock skew and "UTC" HTTP Date timezone specifier.
  {
    FixedDateNetworkDelegate network_delegate("18-Apr-1977 22:49:13 UTC");
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL(
            "/set-cookie?StillGood=1;expires=Mon,18-Apr-1977,22:50:13,GMT"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
  }
  // Verify that the cookie is set.
  {
    TestNetworkDelegate network_delegate;
    default_context().set_network_delegate(&network_delegate);
    TestDelegate d;
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_server.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
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
  URLRequestTestHTTP() : test_server_(base::FilePath(kTestFilePath)) {}

 protected:
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
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        redirect_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_method(request_method);
    if (include_data) {
      req->set_upload(CreateSimpleUploadData(kData));
      HttpRequestHeaders headers;
      headers.SetHeader(HttpRequestHeaders::kContentLength,
                        base::NumberToString(arraysize(kData) - 1));
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
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        redirect_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_method(request_method);
    req->SetExtraRequestHeaderByName(HttpRequestHeaders::kOrigin,
                                     redirect_url.GetOrigin().spec(), false);
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
      std::string origin_header;
      EXPECT_TRUE(req->extra_request_headers().GetHeader(
          HttpRequestHeaders::kOrigin, &origin_header));
      EXPECT_EQ(expected_origin_value, origin_header);
    }
  }

  void HTTPUploadDataOperationTest(const std::string& method) {
    const int kMsgSize = 20000;  // multiple of 10
    const int kIterations = 50;
    char* uploadBytes = new char[kMsgSize+1];
    char* ptr = uploadBytes;
    char marker = 'a';
    for (int idx = 0; idx < kMsgSize/10; idx++) {
      memcpy(ptr, "----------", 10);
      ptr += 10;
      if (idx % 100 == 0) {
        ptr--;
        *ptr++ = marker;
        if (++marker > 'z')
          marker = 'a';
      }
    }
    uploadBytes[kMsgSize] = '\0';

    for (int i = 0; i < kIterations; ++i) {
      TestDelegate d;
      std::unique_ptr<URLRequest> r(default_context().CreateRequest(
          test_server_.GetURL("/echo"), DEFAULT_PRIORITY, &d,
          TRAFFIC_ANNOTATION_FOR_TESTS));
      r->set_method(method.c_str());

      r->set_upload(CreateSimpleUploadData(uploadBytes));

      r->Start();
      EXPECT_TRUE(r->is_pending());

      d.RunUntilComplete();

      ASSERT_EQ(1, d.response_started_count()) << "request failed. Error: "
                                               << d.request_status();

      EXPECT_FALSE(d.received_data_before_response());
      EXPECT_EQ(uploadBytes, d.data_received());
    }
    delete[] uploadBytes;
  }

  bool DoManyCookiesRequest(int num_cookies) {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_server_.GetURL("/set-many-cookies?" +
                            base::IntToString(num_cookies)),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    if (d.request_status() != OK) {
      EXPECT_EQ(ERR_RESPONSE_HEADERS_TOO_BIG, d.request_status());
      return false;
    }

    return true;
  }

  HttpTestServer* http_test_server() { return &test_server_; }

 private:
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

  std::unique_ptr<test_server::BasicHttpResponse> http_response(
      new test_server::BasicHttpResponse);
  http_response->set_code(HTTP_FOUND);
  http_response->AddCustomHeader("Location",
                                 "http://www.destination.com/foo.js");
  return std::move(http_response);
}

class TestSSLConfigService : public SSLConfigService {
 public:
  TestSSLConfigService()
      : min_version_(kDefaultSSLVersionMin),
        max_version_(kDefaultSSLVersionMax) {}
  ~TestSSLConfigService() override = default;

  void set_max_version(uint16_t version) { max_version_ = version; }
  void set_min_version(uint16_t version) { min_version_ = version; }

  // SSLConfigService:
  void GetSSLConfig(SSLConfig* config) override {
    *config = SSLConfig();
    config->version_min = min_version_;
    config->version_max = max_version_;
  }

  bool CanShareConnectionWithClientCerts(
      const std::string& hostname) const override {
    return false;
  }

 private:
  uint16_t min_version_;
  uint16_t max_version_;
};

}  // namespace

// In this unit test, we're using the HTTPTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.redirect.com".
// The EmbeddedTestServer will return a 302 response, which we should not
// follow.
TEST_F(URLRequestTestHTTP, ProxyTunnelRedirectTest) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleRedirectConnect));
  ASSERT_TRUE(http_test_server()->Start());

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContextWithProxy context(
      http_test_server()->host_port_pair().ToString(), &network_delegate);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        GURL("https://www.redirect.com/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The proxy server is not set before failure.
    EXPECT_FALSE(r->proxy_server().is_valid());
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

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContextWithProxy context(
      http_test_server()->host_port_pair().ToString(), &network_delegate);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        GURL("https://www.redirect.com/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The proxy server is not set before failure.
    EXPECT_FALSE(r->proxy_server().is_valid());
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
    BlockingNetworkDelegate::ON_HEADERS_RECEIVED
  };
  static const size_t blocking_stages_length = arraysize(blocking_stages);

  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(
      BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST |
      BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS |
      BlockingNetworkDelegate::ON_HEADERS_RECEIVED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    for (size_t i = 0; i < blocking_stages_length; ++i) {
      network_delegate.RunUntilBlocked();
      EXPECT_EQ(blocking_stages[i],
                network_delegate.stage_blocked_for_callback());
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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  network_delegate.set_retval(ERR_EMPTY_RESPONSE);

  TestURLRequestContextWithProxy context(
      http_test_server()->host_port_pair().ToString(), &network_delegate);

  {
    std::unique_ptr<URLRequest> r(
        context.CreateRequest(http_test_server()->GetURL("/"), DEFAULT_PRIORITY,
                              &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    // The proxy server is not set before cancellation.
    EXPECT_FALSE(r->proxy_server().is_valid());
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
  BlockingNetworkDelegate network_delegate(block_mode);
  network_delegate.set_retval(ERR_BLOCKED_BY_CLIENT);
  network_delegate.set_block_on(stage);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    // The proxy server is not set before cancellation.
    if (stage == BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST ||
        stage == BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS) {
      EXPECT_FALSE(r->proxy_server().is_valid());
    } else if (stage == BlockingNetworkDelegate::ON_HEADERS_RECEIVED) {
      EXPECT_TRUE(r->proxy_server().is_direct());
    } else {
      NOTREACHED();
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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  GURL redirect_url(http_test_server()->GetURL("/simple.html"));
  network_delegate.set_redirect_url(redirect_url);

  TestURLRequestContextWithProxy context(
      http_test_server()->host_port_pair().ToString(), &network_delegate);

  {
    GURL original_url(http_test_server()->GetURL("/defaultresponse"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    // Quit after hitting the redirect, so can check the headers.
    r->Start();
    d.RunUntilRedirect();

    // Check headers from URLRequestJob.
    EXPECT_EQ(307, r->GetResponseCode());
    EXPECT_EQ(307, r->response_headers()->response_code());
    std::string location;
    ASSERT_TRUE(r->response_headers()->EnumerateHeader(NULL, "Location",
                                                       &location));
    EXPECT_EQ(redirect_url, GURL(location));

    // Let the request finish.
    r->FollowDeferredRedirect(base::nullopt /* modified_request_headers */);
    d.RunUntilComplete();
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(ProxyServer(ProxyServer::SCHEME_HTTP,
                          http_test_server()->host_port_pair()),
              r->proxy_server());
    // before_send_headers_with_proxy_count only increments for headers sent
    // through an untunneled proxy.
    EXPECT_EQ(1, network_delegate.before_send_headers_with_proxy_count());
    EXPECT_TRUE(network_delegate.last_observed_proxy().Equals(
        http_test_server()->host_port_pair()));

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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  GURL redirect_url(http_test_server()->GetURL("/simple.html"));
  network_delegate.set_redirect_url(redirect_url);

  TestURLRequestContextWithProxy context(
      http_test_server()->host_port_pair().ToString(), &network_delegate);

  {
    GURL original_url(http_test_server()->GetURL("/defaultresponse"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    // Quit after hitting the redirect, so can check the headers.
    r->Start();
    d.RunUntilRedirect();

    // Check headers from URLRequestJob.
    EXPECT_EQ(307, r->GetResponseCode());
    EXPECT_EQ(307, r->response_headers()->response_code());
    std::string location;
    ASSERT_TRUE(r->response_headers()->EnumerateHeader(NULL, "Location",
                                                       &location));
    EXPECT_EQ(redirect_url, GURL(location));

    // Let the request finish.
    r->FollowDeferredRedirect(base::nullopt /* modified_request_headers */);
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(ProxyServer(ProxyServer::SCHEME_HTTP,
                          http_test_server()->host_port_pair()),
              r->proxy_server());
    // before_send_headers_with_proxy_count only increments for headers sent
    // through an untunneled proxy.
    EXPECT_EQ(1, network_delegate.before_send_headers_with_proxy_count());
    EXPECT_TRUE(network_delegate.last_observed_proxy().Equals(
        http_test_server()->host_port_pair()));
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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);
  GURL redirect_url(http_test_server()->GetURL("/echo"));
  network_delegate.set_redirect_url(redirect_url);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL original_url(http_test_server()->GetURL("/defaultresponse"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method("POST");
    r->set_upload(CreateSimpleUploadData(kData));
    HttpRequestHeaders headers;
    headers.SetHeader(HttpRequestHeaders::kContentLength,
                      base::NumberToString(arraysize(kData) - 1));
    r->SetExtraRequestHeaders(headers);

    // Quit after hitting the redirect, so can check the headers.
    r->Start();
    d.RunUntilRedirect();

    // Check headers from URLRequestJob.
    EXPECT_EQ(307, r->GetResponseCode());
    EXPECT_EQ(307, r->response_headers()->response_code());
    std::string location;
    ASSERT_TRUE(r->response_headers()->EnumerateHeader(NULL, "Location",
                                                       &location));
    EXPECT_EQ(redirect_url, GURL(location));

    // Let the request finish.
    r->FollowDeferredRedirect(base::nullopt /* modified_request_headers */);
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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_HEADERS_RECEIVED);
  GURL redirect_url(http_test_server()->GetURL("/simple.html"));
  network_delegate.set_redirect_on_headers_received_url(redirect_url);

  TestURLRequestContextWithProxy context(
      http_test_server()->host_port_pair().ToString(), &network_delegate);

  {
    GURL original_url(http_test_server()->GetURL("/defaultresponse"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(ProxyServer(ProxyServer::SCHEME_HTTP,
                          http_test_server()->host_port_pair()),
              r->proxy_server());
    // before_send_headers_with_proxy_count only increments for headers sent
    // through an untunneled proxy.
    EXPECT_EQ(2, network_delegate.before_send_headers_with_proxy_count());
    EXPECT_TRUE(network_delegate.last_observed_proxy().Equals(
        http_test_server()->host_port_pair()));

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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
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

TEST_F(URLRequestTestHTTP,
    NetworkDelegateOnAuthRequiredSyncNoAction_GetFullRequestHeaders) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    d.RunUntilComplete();

    {
      HttpRequestHeaders headers;
      EXPECT_TRUE(r->GetFullRequestHeaders(&headers));
      EXPECT_TRUE(headers.HasHeader("Authorization"));
    }

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_TRUE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by setting credentials.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncSetAuth) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  network_delegate.set_auth_credentials(AuthCredentials(kUser, kSecret));

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Same as above, but also tests that GetFullRequestHeaders returns the proper
// headers (for the first or second request) when called at the proper times.
TEST_F(URLRequestTestHTTP,
    NetworkDelegateOnAuthRequiredSyncSetAuth_GetFullRequestHeaders) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  network_delegate.set_auth_credentials(AuthCredentials(kUser, kSecret));

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());

    {
      HttpRequestHeaders headers;
      EXPECT_TRUE(r->GetFullRequestHeaders(&headers));
      EXPECT_TRUE(headers.HasHeader("Authorization"));
    }
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can synchronously complete OnAuthRequired
// by cancelling authentication.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredSyncCancel) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(401, r->GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by taking no action. This indicates that the NetworkDelegate does not want
// to handle the challenge, and is passing the buck along to the
// URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncNoAction) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  d.set_credentials(AuthCredentials(kUser, kSecret));

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
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

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by setting credentials.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncSetAuth) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);

  AuthCredentials auth_credentials(kUser, kSecret);
  network_delegate.set_auth_credentials(auth_credentials);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());
}

// Tests that the network delegate can asynchronously complete OnAuthRequired
// by cancelling authentication.
TEST_F(URLRequestTestHTTP, NetworkDelegateOnAuthRequiredAsyncCancel) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::AUTO_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);
  network_delegate.set_auth_retval(
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    GURL url(http_test_server()->GetURL("/auth-basic"));
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(401, r->GetResponseCode());
    EXPECT_FALSE(d.auth_required_called());
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
  default_network_delegate_.set_add_header_to_first_response(true);

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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_BEFORE_URL_REQUEST);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(
        context.CreateRequest(http_test_server()->GetURL("/"), DEFAULT_PRIORITY,
                              &d, TRAFFIC_ANNOTATION_FOR_TESTS));

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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(
      BlockingNetworkDelegate::ON_BEFORE_SEND_HEADERS);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(
        context.CreateRequest(http_test_server()->GetURL("/"), DEFAULT_PRIORITY,
                              &d, TRAFFIC_ANNOTATION_FOR_TESTS));

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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_HEADERS_RECEIVED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(
        context.CreateRequest(http_test_server()->GetURL("/"), DEFAULT_PRIORITY,
                              &d, TRAFFIC_ANNOTATION_FOR_TESTS));

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

// Tests that we can handle when a network request was canceled while we were
// waiting for the network delegate.
// Part 4: Request is cancelled while waiting for OnAuthRequired callback.
TEST_F(URLRequestTestHTTP, NetworkDelegateCancelWhileWaiting4) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::USER_CALLBACK);
  network_delegate.set_block_on(BlockingNetworkDelegate::ON_AUTH_REQUIRED);

  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    network_delegate.RunUntilBlocked();
    EXPECT_EQ(BlockingNetworkDelegate::ON_AUTH_REQUIRED,
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

  std::unique_ptr<test_server::BasicHttpResponse> http_response(
      new test_server::BasicHttpResponse);
  http_response->set_code(HTTP_UNAUTHORIZED);
  http_response->AddCustomHeader("WWW-Authenticate",
                                 "Basic realm=\"WallyWorld\"");
  return std::move(http_response);
}

}  // namespace

// In this unit test, we're using the EmbeddedTestServer as a proxy server and
// issuing a CONNECT request with the magic host name "www.server-auth.com".
// The EmbeddedTestServer will return a 401 response, which we should balk at.
TEST_F(URLRequestTestHTTP, UnexpectedServerAuthTest) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleServerAuthConnect));
  ASSERT_TRUE(http_test_server()->Start());

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContextWithProxy context(
      http_test_server()->host_port_pair().ToString(), &network_delegate);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        GURL("https://www.server-auth.com/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The proxy server is not set before failure.
    EXPECT_FALSE(r->proxy_server().is_valid());
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
              r->GetSocketAddress().host());
    EXPECT_EQ(http_test_server()->host_port_pair().port(),
              r->GetSocketAddress().port());

    // TODO(eroman): Add back the NetLog tests...
  }
}

// This test has the server send a large number of cookies to the client.
// To ensure that no number of cookies causes a crash, a galloping binary
// search is used to estimate that maximum number of cookies that are accepted
// by the browser. Beyond the maximum number, the request will fail with
// ERR_RESPONSE_HEADERS_TOO_BIG.
#if defined(OS_WIN) || defined(OS_FUCHSIA)
// http://crbug.com/177916
#define MAYBE_GetTest_ManyCookies DISABLED_GetTest_ManyCookies
#else
#define MAYBE_GetTest_ManyCookies GetTest_ManyCookies
#endif  // defined(OS_WIN)
TEST_F(URLRequestTestHTTP, MAYBE_GetTest_ManyCookies) {
  ASSERT_TRUE(http_test_server()->Start());

  int lower_bound = 0;
  int upper_bound = 1;

  // Double the number of cookies until the response header limits are
  // exceeded.
  while (DoManyCookiesRequest(upper_bound)) {
    lower_bound = upper_bound;
    upper_bound *= 2;
    ASSERT_LT(upper_bound, 1000000);
  }

  int tolerance = static_cast<int>(upper_bound * 0.005);
  if (tolerance < 2)
    tolerance = 2;

  // Perform a binary search to find the highest possible number of cookies,
  // within the desired tolerance.
  while (upper_bound - lower_bound >= tolerance) {
    int num_cookies = (lower_bound + upper_bound) / 2;

    if (DoManyCookiesRequest(num_cookies))
      lower_bound = num_cookies;
    else
      upper_bound = num_cookies;
  }
  // Success: the test did not crash.
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
              r->GetSocketAddress().host());
    EXPECT_EQ(http_test_server()->host_port_pair().port(),
              r->GetSocketAddress().port());
  }
}

TEST_F(URLRequestTestHTTP, GetTest_GetFullRequestHeaders) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    GURL test_url(http_test_server()->GetURL("/defaultresponse"));
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    HttpRequestHeaders headers;
    EXPECT_FALSE(r->GetFullRequestHeaders(&headers));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(http_test_server()->host_port_pair().host(),
              r->GetSocketAddress().host());
    EXPECT_EQ(http_test_server()->host_port_pair().port(),
              r->GetSocketAddress().port());

    EXPECT_TRUE(d.have_full_request_headers());
    CheckFullRequestHeaders(d.full_request_headers(), test_url);
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
              r->GetSocketAddress().host());
    EXPECT_EQ(http_test_server()->host_port_pair().port(),
              r->GetSocketAddress().port());
  }
}

// TODO(svaldez): Update tests to use EmbeddedTestServer.
#if !defined(OS_IOS)
TEST_F(URLRequestTestHTTP, GetZippedTest) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTP,
                                base::FilePath(kTestFilePath));

  ASSERT_TRUE(test_server.Start());

  // Parameter that specifies the Content-Length field in the response:
  // C - Compressed length.
  // U - Uncompressed length.
  // L - Large length (larger than both C & U).
  // M - Medium length (between C & U).
  // S - Small length (smaller than both C & U).
  const char test_parameters[] = "CULMS";
  const int num_tests = arraysize(test_parameters)- 1;  // Skip NULL.
  // C & U should be OK.
  // L & M are larger than the data sent, and show an error.
  // S has too little data, but we seem to accept it.
  const bool test_expect_success[num_tests] =
      { true, true, false, false, true };

  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.Append(kTestFilePath);
  file_path = file_path.Append(FILE_PATH_LITERAL("BullRunSpeech.txt"));
  std::string expected_content;
  ASSERT_TRUE(base::ReadFileToString(file_path, &expected_content));

  for (int i = 0; i < num_tests; i++) {
    TestDelegate d;
    {
      std::string test_file = base::StringPrintf(
          "compressedfiles/BullRunSpeech.txt?%c", test_parameters[i]);

      TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
      TestURLRequestContext context(true);
      context.set_network_delegate(&network_delegate);
      context.Init();

      std::unique_ptr<URLRequest> r(
          context.CreateRequest(test_server.GetURL(test_file), DEFAULT_PRIORITY,
                                &d, TRAFFIC_ANNOTATION_FOR_TESTS));
      r->Start();
      EXPECT_TRUE(r->is_pending());

      d.RunUntilComplete();

      EXPECT_EQ(1, d.response_started_count());
      EXPECT_FALSE(d.received_data_before_response());
      VLOG(1) << " Received " << d.bytes_received() << " bytes"
              << " error = " << d.request_status();
      if (test_expect_success[i]) {
        EXPECT_EQ(OK, d.request_status()) << " Parameter = \"" << test_file
                                          << "\"";
        if (test_parameters[i] == 'S') {
          // When content length is smaller than both compressed length and
          // uncompressed length, HttpStreamParser might not read the full
          // response body.
          continue;
        }
        EXPECT_EQ(expected_content, d.data_received());
      } else {
        EXPECT_EQ(ERR_CONTENT_LENGTH_MISMATCH, d.request_status())
            << " Parameter = \"" << test_file << "\"";
      }
    }
  }
}
#endif  // !defined(OS_IOS)

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
  EXPECT_TRUE(default_network_delegate_.GetLoadTimingInfoBeforeRedirect(
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

// First and second pieces of information logged by delegates to URLRequests.
const char kFirstDelegateInfo[] = "Wonderful delegate";
const char kSecondDelegateInfo[] = "Exciting delegate";

// Logs delegate information to a URLRequest.  The first string is logged
// synchronously on Start(), using DELEGATE_INFO_DEBUG_ONLY.  The second is
// logged asynchronously, using DELEGATE_INFO_DISPLAY_TO_USER.  Then
// another asynchronous call is used to clear the delegate information
// before calling a callback.  The object then deletes itself.
class AsyncDelegateLogger : public base::RefCounted<AsyncDelegateLogger> {
 public:
  using Callback = base::OnceCallback<void()>;

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
  static size_t CheckDelegateInfo(const TestNetLogEntry::List& entries,
                                  size_t log_position) {
    // There should be 4 DELEGATE_INFO events: Two begins and two ends.
    if (log_position + 3 >= entries.size()) {
      ADD_FAILURE() << "Not enough log entries";
      return entries.size();
    }
    std::string delegate_info;
    EXPECT_EQ(NetLogEventType::DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::BEGIN, entries[log_position].phase);
    EXPECT_TRUE(entries[log_position].GetStringValue("delegate_blocked_by",
                                                     &delegate_info));
    EXPECT_EQ(kFirstDelegateInfo, delegate_info);

    ++log_position;
    EXPECT_EQ(NetLogEventType::DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);

    ++log_position;
    EXPECT_EQ(NetLogEventType::DELEGATE_INFO, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::BEGIN, entries[log_position].phase);
    EXPECT_TRUE(entries[log_position].GetStringValue("delegate_blocked_by",
                                                     &delegate_info));
    EXPECT_EQ(kSecondDelegateInfo, delegate_info);

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
    EXPECT_NE(ASCIIToUTF16(kFirstDelegateInfo), load_state.param);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AsyncDelegateLogger::LogSecondDelegate, this));
  }

  void LogSecondDelegate() {
    url_request_->LogAndReportBlockedBy(kSecondDelegateInfo);
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_second_load_state_, load_state.state);
    if (expected_second_load_state_ == LOAD_STATE_WAITING_FOR_DELEGATE) {
      EXPECT_EQ(ASCIIToUTF16(kSecondDelegateInfo), load_state.param);
    } else {
      EXPECT_NE(ASCIIToUTF16(kSecondDelegateInfo), load_state.param);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AsyncDelegateLogger::LogComplete, this));
  }

  void LogComplete() {
    url_request_->LogUnblocked();
    LoadStateWithParam load_state = url_request_->GetLoadState();
    EXPECT_EQ(expected_third_load_state_, load_state.state);
    if (expected_second_load_state_ == LOAD_STATE_WAITING_FOR_DELEGATE)
      EXPECT_EQ(base::string16(), load_state.param);
    std::move(callback_).Run();
  }

  URLRequest* url_request_;
  const int expected_first_load_state_;
  const int expected_second_load_state_;
  const int expected_third_load_state_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDelegateLogger);
};

// NetworkDelegate that logs delegate information before a request is started,
// before headers are sent, when headers are read, and when auth information
// is requested.  Uses AsyncDelegateLogger.
class AsyncLoggingNetworkDelegate : public TestNetworkDelegate {
 public:
  AsyncLoggingNetworkDelegate() = default;
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

  int OnBeforeStartTransaction(URLRequest* request,
                               CompletionOnceCallback callback,
                               HttpRequestHeaders* headers) override {
    // TestNetworkDelegate always completes synchronously.
    CHECK_NE(ERR_IO_PENDING, TestNetworkDelegate::OnBeforeStartTransaction(
                                 request, base::NullCallback(), headers));
    return RunCallbackAsynchronously(request, std::move(callback));
  }

  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override {
    // TestNetworkDelegate always completes synchronously.
    CHECK_NE(ERR_IO_PENDING,
             TestNetworkDelegate::OnHeadersReceived(
                 request, base::NullCallback(), original_response_headers,
                 override_response_headers, allowed_unsafe_redirect_url));
    return RunCallbackAsynchronously(request, std::move(callback));
  }

  NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      AuthCallback callback,
      AuthCredentials* credentials) override {
    AsyncDelegateLogger::Run(
        request, LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE, LOAD_STATE_WAITING_FOR_DELEGATE,
        base::BindOnce(&AsyncLoggingNetworkDelegate::SetAuthAndResume,
                       std::move(callback), credentials));
    return AUTH_REQUIRED_RESPONSE_IO_PENDING;
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

  static void SetAuthAndResume(AuthCallback callback,
                               AuthCredentials* credentials) {
    *credentials = AuthCredentials(kUser, kSecret);
    std::move(callback).Run(NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);
  }

  DISALLOW_COPY_AND_ASSIGN(AsyncLoggingNetworkDelegate);
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
  ~AsyncLoggingUrlRequestDelegate() override = default;

  // URLRequest::Delegate implementation:
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    *defer_redirect = true;
    AsyncDelegateLogger::Run(
        request,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        base::Bind(
            &AsyncLoggingUrlRequestDelegate::OnReceivedRedirectLoggingComplete,
            base::Unretained(this), request, redirect_info));
  }

  void OnResponseStarted(URLRequest* request, int net_error) override {
    AsyncDelegateLogger::Run(
        request, LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE, LOAD_STATE_WAITING_FOR_DELEGATE,
        base::Bind(
            &AsyncLoggingUrlRequestDelegate::OnResponseStartedLoggingComplete,
            base::Unretained(this), request, net_error));
  }

  void OnReadCompleted(URLRequest* request, int bytes_read) override {
    AsyncDelegateLogger::Run(
        request,
        LOAD_STATE_IDLE,
        LOAD_STATE_IDLE,
        LOAD_STATE_IDLE,
        base::Bind(
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
    if (!defer_redirect)
      request->FollowDeferredRedirect(
          base::nullopt /* modified_request_headers */);
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

  DISALLOW_COPY_AND_ASSIGN(AsyncLoggingUrlRequestDelegate);
};

// Tests handling of delegate info before a request starts.
TEST_F(URLRequestTestHTTP, DelegateInfoBeforeStart) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate request_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(NULL);
  context.set_net_log(&net_log_);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY,
        &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    AsyncDelegateLogger::Run(
        r.get(),
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_WAITING_FOR_DELEGATE,
        LOAD_STATE_IDLE,
        base::Bind(&URLRequest::Start, base::Unretained(r.get())));

    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
  }

  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
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
  AsyncLoggingNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_net_log(&net_log_);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        http_test_server()->GetURL("/simple.html"), DEFAULT_PRIORITY,
        &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
  static const NetLogEventType kExpectedEvents[] = {
      NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST,
      NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION,
      NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED,
  };
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLog::EventTypeToString(event));
    log_position = ExpectLogContainsSomewhereAfter(
        entries, log_position + 1, event, NetLogEventPhase::BEGIN);

    log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                          log_position + 1);

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
  AsyncLoggingNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_net_log(&net_log_);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        http_test_server()->GetURL("/server-redirect?simple.html"),
        DEFAULT_PRIORITY, &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
    EXPECT_EQ(2, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
  static const NetLogEventType kExpectedEvents[] = {
      NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST,
      NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION,
      NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED,
  };
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLog::EventTypeToString(event));
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
    SCOPED_TRACE(NetLog::EventTypeToString(event));
    log_position = ExpectLogContainsSomewhereAfter(
        entries, log_position + 1, event, NetLogEventPhase::BEGIN);

    log_position = AsyncDelegateLogger::CheckDelegateInfo(entries,
                                                          log_position + 1);

    ASSERT_LT(log_position, entries.size());
    EXPECT_EQ(event, entries[log_position].type);
    EXPECT_EQ(NetLogEventPhase::END, entries[log_position].phase);
  }

  EXPECT_FALSE(LogContainsEntryWithTypeAfter(entries, log_position + 1,
                                             NetLogEventType::DELEGATE_INFO));
}

// Tests handling of delegate info from a network delegate in the case of HTTP
// AUTH.
TEST_F(URLRequestTestHTTP, NetworkDelegateInfoAuth) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate request_delegate;
  AsyncLoggingNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_net_log(&net_log_);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY,
        &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    EXPECT_EQ(LOAD_STATE_IDLE, load_state.state);
    EXPECT_EQ(base::string16(), load_state.param);

    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
    EXPECT_EQ(1, network_delegate.created_requests());
    EXPECT_EQ(0, network_delegate.destroyed_requests());
  }
  EXPECT_EQ(1, network_delegate.destroyed_requests());

  size_t log_position = 0;
  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
  static const NetLogEventType kExpectedEvents[] = {
      NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST,
      NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION,
      NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED,
      NetLogEventType::NETWORK_DELEGATE_AUTH_REQUIRED,
      NetLogEventType::NETWORK_DELEGATE_BEFORE_START_TRANSACTION,
      NetLogEventType::NETWORK_DELEGATE_HEADERS_RECEIVED,
  };
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLog::EventTypeToString(event));
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

// TODO(svaldez): Update tests to use EmbeddedTestServer.
#if !defined(OS_IOS)
// Tests handling of delegate info from a URLRequest::Delegate.
TEST_F(URLRequestTestHTTP, URLRequestDelegateInfo) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTP,
                                base::FilePath(kTestFilePath));

  ASSERT_TRUE(test_server.Start());

  AsyncLoggingUrlRequestDelegate request_delegate(
      AsyncLoggingUrlRequestDelegate::NO_CANCEL);
  TestURLRequestContext context(true);
  context.set_network_delegate(NULL);
  context.set_net_log(&net_log_);
  context.Init();

  {
    // A chunked response with delays between chunks is used to make sure that
    // attempts by the URLRequest delegate to log information while reading the
    // body are ignored.  Since they are ignored, this test is robust against
    // the possibility of multiple reads being combined in the unlikely event
    // that it occurs.
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        test_server.GetURL("/chunked?waitBetweenChunks=20"), DEFAULT_PRIORITY,
        &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
  }

  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);

  size_t log_position = 0;

  // The delegate info should only have been logged on header complete.  Other
  // times it should silently be ignored.
  EXPECT_FALSE(LogContainsEntryWithTypeAfter(
      entries, 0, NetLogEventType::NETWORK_DELEGATE_BEFORE_URL_REQUEST));
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
#endif  // !defined(OS_IOS)

// Tests handling of delegate info from a URLRequest::Delegate in the case of
// an HTTP redirect.
TEST_F(URLRequestTestHTTP, URLRequestDelegateInfoOnRedirect) {
  ASSERT_TRUE(http_test_server()->Start());

  AsyncLoggingUrlRequestDelegate request_delegate(
      AsyncLoggingUrlRequestDelegate::NO_CANCEL);
  TestURLRequestContext context(true);
  context.set_network_delegate(NULL);
  context.set_net_log(&net_log_);
  context.Init();

  {
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        http_test_server()->GetURL("/server-redirect?simple.html"),
        DEFAULT_PRIORITY, &request_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    LoadStateWithParam load_state = r->GetLoadState();
    r->Start();
    request_delegate.RunUntilComplete();

    EXPECT_EQ(200, r->GetResponseCode());
    EXPECT_EQ(OK, request_delegate.request_status());
  }

  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);

  // Delegate info should only have been logged in OnReceivedRedirect and
  // OnResponseStarted.
  size_t log_position = 0;
  static const NetLogEventType kExpectedEvents[] = {
      NetLogEventType::URL_REQUEST_DELEGATE_RECEIVED_REDIRECT,
      NetLogEventType::URL_REQUEST_DELEGATE_RESPONSE_STARTED,
  };
  for (NetLogEventType event : kExpectedEvents) {
    SCOPED_TRACE(NetLog::EventTypeToString(event));
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
    TestNetLog net_log;
    TestURLRequestContext context(true);
    context.set_network_delegate(NULL);
    context.set_net_log(&net_log);
    context.Init();

    {
      std::unique_ptr<URLRequest> r(context.CreateRequest(
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

    TestNetLogEntry::List entries;
    net_log.GetEntries(&entries);

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
      SCOPED_TRACE(NetLog::EventTypeToString(event));
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

  std::string value;
  const HttpRequestHeaders& headers = req->extra_request_headers();
  EXPECT_TRUE(headers.GetHeader(kExtraHeader, &value));
  EXPECT_EQ(kExtraValue, value);
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

  std::string value;
  const HttpRequestHeaders& headers = req->extra_request_headers();
  EXPECT_FALSE(headers.GetHeader(kExtraHeaderToRemove, &value));
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

    ASSERT_EQ(1, d.response_started_count()) << "request failed. Error: "
                                             << d.request_status();

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
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(kTestFilePath);
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));
    element_readers.push_back(std::make_unique<UploadFileElementReader>(
        base::ThreadTaskRunnerHandle::Get().get(), path, 0,
        std::numeric_limits<uint64_t>::max(), base::Time()));
    r->set_upload(std::make_unique<ElementsUploadDataStream>(
        std::move(element_readers), 0));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    int64_t size64 = 0;
    ASSERT_EQ(true, base::GetFileSize(path, &size64));
    ASSERT_LE(size64, std::numeric_limits<int>::max());
    int size = static_cast<int>(size64);
    std::unique_ptr<char[]> buf(new char[size]);

    ASSERT_EQ(size, base::ReadFile(path, buf.get(), size));

    ASSERT_EQ(1, d.response_started_count()) << "request failed. Error: "
                                             << d.request_status();

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
        base::ThreadTaskRunnerHandle::Get().get(),
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
  writer->AppendData("a", 1, false);
  writer->AppendData("bcd", 3, false);
  writer->AppendData("this is a longer chunk than before.", 35, false);
  writer->AppendData("\r\n\r\n", 4, false);
  writer->AppendData("0", 1, false);
  writer->AppendData("2323", 4, true);
}

// Checks that the upload data added in AddChunksToUpload() was echoed back from
// the server.
void VerifyReceivedDataMatchesChunks(URLRequest* r, TestDelegate* d) {
  // This should match the chunks sent by AddChunksToUpload().
  const std::string expected_data =
      "abcdthis is a longer chunk than before.\r\n\r\n02323";

  ASSERT_EQ(1, d->response_started_count()) << "request failed. Error: "
                                            << d->request_status();

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
    std::unique_ptr<ChunkedUploadDataStream> upload_data_stream(
        new ChunkedUploadDataStream(0));
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
    std::unique_ptr<ChunkedUploadDataStream> upload_data_stream(
        new ChunkedUploadDataStream(0));
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
    std::unique_ptr<ChunkedUploadDataStream> upload_data_stream(
        new ChunkedUploadDataStream(0));
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
#if !defined(OS_IOS)
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
#if defined(OS_ANDROID)
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

namespace {
const char kExpectCTStaticHostname[] = "expect-ct.preloaded.test";
const char kPKPReportUri[] = "http://report-uri.preloaded.test/pkp";
const char kPKPHost[] = "with-report-uri-pkp.preloaded.test";
}  // namespace

// Tests that reports get sent on PKP violations when a report-uri is set.
TEST_F(URLRequestTestHTTP, ProcessPKPAndSendReport) {
  GURL report_uri(kPKPReportUri);
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  std::string test_server_hostname = kPKPHost;

  // Set up a pin for |test_server_hostname|.
  TransportSecurityState security_state;
  security_state.EnableStaticPinsForTesting();
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);

  MockCertificateReportSender mock_report_sender;
  security_state.SetReportSender(&mock_report_sender);

  // Set up a MockCertVerifier to trigger a violation of the previously
  // set pin.
  scoped_refptr<X509Certificate> cert = https_test_server.GetCertificate();
  ASSERT_TRUE(cert);

  MockCertVerifier cert_verifier;
  CertVerifyResult verify_result;
  verify_result.verified_cert = cert;
  verify_result.is_issued_by_known_root = true;
  HashValue hash3;
  ASSERT_TRUE(
      hash3.FromString("sha256/3333333333333333333333333333333333333333333="));
  verify_result.public_key_hashes.push_back(hash3);
  cert_verifier.AddResultForCert(cert.get(), verify_result, OK);

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_transport_security_state(&security_state);
  context.set_network_delegate(&network_delegate);
  context.set_cert_verifier(&cert_verifier);
  context.Init();

  // Now send a request to trigger the violation.
  TestDelegate d;
  std::unique_ptr<URLRequest> violating_request(context.CreateRequest(
      https_test_server.GetURL(test_server_hostname, "/simple.html"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  violating_request->Start();
  d.RunUntilComplete();

  // Check that a report was sent.
  EXPECT_EQ(report_uri, mock_report_sender.latest_report_uri());
  ASSERT_FALSE(mock_report_sender.latest_report().empty());
  EXPECT_EQ("application/json; charset=utf-8",
            mock_report_sender.latest_content_type());
  std::unique_ptr<base::Value> value(
      base::JSONReader::Read(mock_report_sender.latest_report()));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  base::DictionaryValue* report_dict;
  ASSERT_TRUE(value->GetAsDictionary(&report_dict));
  std::string report_hostname;
  EXPECT_TRUE(report_dict->GetString("hostname", &report_hostname));
  EXPECT_EQ(test_server_hostname, report_hostname);
}

// Tests that reports do not get sent on requests to static pkp hosts that
// don't have pin violations.
TEST_F(URLRequestTestHTTP, ProcessPKPWithNoViolation) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  std::string test_server_hostname = kPKPHost;

  TransportSecurityState security_state;
  security_state.EnableStaticPinsForTesting();
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);
  MockCertificateReportSender mock_report_sender;
  security_state.SetReportSender(&mock_report_sender);

  scoped_refptr<X509Certificate> cert = https_test_server.GetCertificate();
  ASSERT_TRUE(cert);
  MockCertVerifier mock_cert_verifier;
  CertVerifyResult verify_result;
  verify_result.verified_cert = cert;
  verify_result.is_issued_by_known_root = true;
  HashValue hash;
  // The expected value of GoodPin1 used by |test_default::kHSTSSource|.
  ASSERT_TRUE(
      hash.FromString("sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  verify_result.public_key_hashes.push_back(hash);
  mock_cert_verifier.AddResultForCert(cert.get(), verify_result, OK);

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_transport_security_state(&security_state);
  context.set_network_delegate(&network_delegate);
  context.set_cert_verifier(&mock_cert_verifier);
  context.Init();

  // Now send a request that does not trigger the violation.
  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      https_test_server.GetURL(test_server_hostname, "/simple.html"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  // Check that the request succeeded, a report was not sent and the pkp was
  // not bypassed.
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(security_state.GetStaticDomainState(test_server_hostname,
                                                  &sts_state, &pkp_state));
  EXPECT_TRUE(pkp_state.HasPublicKeyPins());
  EXPECT_FALSE(request->ssl_info().pkp_bypassed);
}

TEST_F(URLRequestTestHTTP, PKPBypassRecorded) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  // Set up a MockCertVerifier to be a local root that violates the pin
  scoped_refptr<X509Certificate> cert = https_test_server.GetCertificate();
  ASSERT_TRUE(cert);

  MockCertVerifier cert_verifier;
  CertVerifyResult verify_result;
  verify_result.verified_cert = cert;
  verify_result.is_issued_by_known_root = false;
  HashValue hash;
  ASSERT_TRUE(
      hash.FromString("sha256/1111111111111111111111111111111111111111111="));
  verify_result.public_key_hashes.push_back(hash);
  cert_verifier.AddResultForCert(cert.get(), verify_result, OK);

  std::string test_server_hostname = kPKPHost;

  // Set up PKP
  TransportSecurityState security_state;
  security_state.EnableStaticPinsForTesting();
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);
  MockCertificateReportSender mock_report_sender;
  security_state.SetReportSender(&mock_report_sender);

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_transport_security_state(&security_state);
  context.set_network_delegate(&network_delegate);
  context.set_cert_verifier(&cert_verifier);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      https_test_server.GetURL(test_server_hostname, "/simple.html"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  // Check that the request succeeded, a report was not sent and the PKP was
  // bypassed.
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(GURL(), mock_report_sender.latest_report_uri());
  EXPECT_EQ(std::string(), mock_report_sender.latest_report());
  TransportSecurityState::STSState sts_state;
  TransportSecurityState::PKPState pkp_state;
  EXPECT_TRUE(security_state.GetStaticDomainState(test_server_hostname,
                                                  &sts_state, &pkp_state));
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

// An ExpectCTReporter that records the number of times OnExpectCTFailed() was
// called.
class MockExpectCTReporter : public TransportSecurityState::ExpectCTReporter {
 public:
  MockExpectCTReporter() : num_failures_(0) {}
  ~MockExpectCTReporter() override = default;

  void OnExpectCTFailed(const HostPortPair& host_port_pair,
                        const GURL& report_uri,
                        base::Time expiration,
                        const X509Certificate* validated_certificate_chain,
                        const X509Certificate* served_certificate_chain,
                        const SignedCertificateTimestampAndStatusList&
                            signed_certificate_timestamps) override {
    num_failures_++;
  }

  uint32_t num_failures() { return num_failures_; }

 private:
  uint32_t num_failures_;
};

// A CTPolicyEnforcer that returns a default CTPolicyCompliance value
// for every certificate.
class MockCTPolicyEnforcer : public CTPolicyEnforcer {
 public:
  MockCTPolicyEnforcer()
      : default_result_(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS) {}
  ~MockCTPolicyEnforcer() override = default;

  ct::CTPolicyCompliance CheckCompliance(
      X509Certificate* cert,
      const ct::SCTList& verified_scts,
      const NetLogWithSource& net_log) override {
    return default_result_;
  }

  void set_default_result(ct::CTPolicyCompliance default_result) {
    default_result_ = default_result;
  }

 private:
  ct::CTPolicyCompliance default_result_;
};

// Tests that Expect CT headers for the preload list are processed correctly.
TEST_F(URLRequestTestHTTP, PreloadExpectCTHeader) {
  SetTransportSecurityStateSourceForTesting(&test_default::kHSTSSource);

  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  MockExpectCTReporter reporter;
  TransportSecurityState transport_security_state;
  transport_security_state.enable_static_expect_ct_ = true;
  transport_security_state.SetExpectCTReporter(&reporter);

  // Set up a MockCertVerifier to accept the certificate that the server sends.
  scoped_refptr<X509Certificate> cert = https_test_server.GetCertificate();
  ASSERT_TRUE(cert);
  MockCertVerifier cert_verifier;
  CertVerifyResult verify_result;
  verify_result.verified_cert = cert;
  verify_result.is_issued_by_known_root = true;
  cert_verifier.AddResultForCert(cert.get(), verify_result, OK);

  // Set up a DoNothingCTVerifier and MockCTPolicyEnforcer to trigger an Expect
  // CT violation.
  DoNothingCTVerifier ct_verifier;
  MockCTPolicyEnforcer ct_policy_enforcer;
  ct_policy_enforcer.set_default_result(
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS);

  TestNetworkDelegate network_delegate;
  // Use a MockHostResolver (which by default maps all hosts to
  // 127.0.0.1) so that the request can be sent to a site on the Expect
  // CT preload list.
  MockHostResolver host_resolver;
  TestURLRequestContext context(true);
  context.set_host_resolver(&host_resolver);
  context.set_transport_security_state(&transport_security_state);
  context.set_network_delegate(&network_delegate);
  context.set_cert_verifier(&cert_verifier);
  context.set_cert_transparency_verifier(&ct_verifier);
  context.set_ct_policy_enforcer(&ct_policy_enforcer);
  context.Init();

  // Now send a request to trigger the violation.
  TestDelegate d;
  GURL url = https_test_server.GetURL("/expect-ct-header-preload.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr(kExpectCTStaticHostname);
  url = url.ReplaceComponents(replace_host);
  std::unique_ptr<URLRequest> violating_request(context.CreateRequest(
      url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  violating_request->Start();
  d.RunUntilComplete();

  EXPECT_EQ(1u, reporter.num_failures());
}

// Tests that Expect CT HTTP headers are processed correctly.
TEST_F(URLRequestTestHTTP, ExpectCTHeader) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  MockExpectCTReporter reporter;
  TransportSecurityState transport_security_state;
  transport_security_state.SetExpectCTReporter(&reporter);

  // Set up a MockCertVerifier to accept the certificate that the server sends.
  scoped_refptr<X509Certificate> cert = https_test_server.GetCertificate();
  ASSERT_TRUE(cert);
  MockCertVerifier cert_verifier;
  CertVerifyResult verify_result;
  verify_result.verified_cert = cert;
  verify_result.is_issued_by_known_root = true;
  cert_verifier.AddResultForCert(cert.get(), verify_result, OK);

  // Set up a DoNothingCTVerifier and MockCTPolicyEnforcer to simulate CT
  // compliance.
  DoNothingCTVerifier ct_verifier;
  MockCTPolicyEnforcer ct_policy_enforcer;
  ct_policy_enforcer.set_default_result(
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS);

  TestNetworkDelegate network_delegate;
  // Use a MockHostResolver (which by default maps all hosts to
  // 127.0.0.1).
  MockHostResolver host_resolver;
  TestURLRequestContext context(true);
  context.set_host_resolver(&host_resolver);
  context.set_transport_security_state(&transport_security_state);
  context.set_network_delegate(&network_delegate);
  context.set_cert_verifier(&cert_verifier);
  context.set_cert_transparency_verifier(&ct_verifier);
  context.set_ct_policy_enforcer(&ct_policy_enforcer);
  context.Init();

  // Now send a request to trigger the header processing.
  TestDelegate d;
  GURL url = https_test_server.GetURL("/expect-ct-header.html");
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  TransportSecurityState::ExpectCTState state;
  ASSERT_TRUE(
      transport_security_state.GetDynamicExpectCTState(url.host(), &state));
  EXPECT_TRUE(state.enforce);
  EXPECT_EQ(GURL("https://example.test"), state.report_uri);
}

// Tests that if multiple Expect CT HTTP headers are sent, they are all
// processed.
TEST_F(URLRequestTestHTTP, MultipleExpectCTHeaders) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestFilePath));
  ASSERT_TRUE(https_test_server.Start());

  MockExpectCTReporter reporter;
  TransportSecurityState transport_security_state;
  transport_security_state.SetExpectCTReporter(&reporter);

  // Set up a MockCertVerifier to accept the certificate that the server sends.
  scoped_refptr<X509Certificate> cert = https_test_server.GetCertificate();
  ASSERT_TRUE(cert);
  MockCertVerifier cert_verifier;
  CertVerifyResult verify_result;
  verify_result.verified_cert = cert;
  verify_result.is_issued_by_known_root = true;
  cert_verifier.AddResultForCert(cert.get(), verify_result, OK);

  // Set up a DoNothingCTVerifier and MockCTPolicyEnforcer to simulate CT
  // compliance.
  DoNothingCTVerifier ct_verifier;
  MockCTPolicyEnforcer ct_policy_enforcer;
  ct_policy_enforcer.set_default_result(
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS);

  TestNetworkDelegate network_delegate;
  // Use a MockHostResolver (which by default maps all hosts to
  // 127.0.0.1).
  MockHostResolver host_resolver;
  TestURLRequestContext context(true);
  context.set_host_resolver(&host_resolver);
  context.set_transport_security_state(&transport_security_state);
  context.set_network_delegate(&network_delegate);
  context.set_cert_verifier(&cert_verifier);
  context.set_cert_transparency_verifier(&ct_verifier);
  context.set_ct_policy_enforcer(&ct_policy_enforcer);
  context.Init();

  // Now send a request to trigger the header processing.
  TestDelegate d;
  GURL url = https_test_server.GetURL("/expect-ct-header-multiple.html");
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  TransportSecurityState::ExpectCTState state;
  ASSERT_TRUE(
      transport_security_state.GetDynamicExpectCTState(url.host(), &state));
  EXPECT_TRUE(state.enforce);
  EXPECT_EQ(GURL("https://example.test"), state.report_uri);
}

#endif  // !defined(OS_IOS)

#if BUILDFLAG(ENABLE_REPORTING)
namespace {

class TestReportingService : public ReportingService {
 public:
  struct Header {
    GURL url;
    std::string header_value;
  };

  const std::vector<Header>& headers() { return headers_; }

  // ReportingService implementation:

  ~TestReportingService() override = default;

  void QueueReport(const GURL& url,
                   const std::string& user_agent,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body,
                   int depth) override {
    NOTIMPLEMENTED();
  }

  void ProcessHeader(const GURL& url,
                     const std::string& header_value) override {
    headers_.push_back({url, header_value});
  }

  void RemoveBrowsingData(int data_type_mask,
                          const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    NOTIMPLEMENTED();
  }

  void RemoveAllBrowsingData(int data_type_mask) override { NOTIMPLEMENTED(); }

  int GetUploadDepth(const URLRequest& request) override {
    NOTIMPLEMENTED();
    return 0;
  }

  const ReportingPolicy& GetPolicy() const override {
    static ReportingPolicy dummy_policy_;
    NOTIMPLEMENTED();
    return dummy_policy_;
  }

 private:
  std::vector<Header> headers_;
};

std::unique_ptr<test_server::HttpResponse> SendReportToHeader(
    const test_server::HttpRequest& request) {
  std::unique_ptr<test_server::BasicHttpResponse> http_response(
      new test_server::BasicHttpResponse);
  http_response->set_code(HTTP_OK);
  http_response->AddCustomHeader("Report-To", "foo");
  http_response->AddCustomHeader("Report-To", "bar");
  return std::move(http_response);
}

}  // namespace

TEST_F(URLRequestTestHTTP, DontProcessReportToHeaderNoService) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&SendReportToHeader));
  ASSERT_TRUE(http_test_server()->Start());
  GURL request_url = http_test_server()->GetURL("/");

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();
}

TEST_F(URLRequestTestHTTP, DontProcessReportToHeaderHTTP) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&SendReportToHeader));
  ASSERT_TRUE(http_test_server()->Start());
  GURL request_url = http_test_server()->GetURL("/");

  TestNetworkDelegate network_delegate;
  TestReportingService reporting_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_reporting_service(&reporting_service);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(reporting_service.headers().empty());
}

TEST_F(URLRequestTestHTTP, ProcessReportToHeaderHTTPS) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.RegisterRequestHandler(
      base::BindRepeating(&SendReportToHeader));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/");

  TestNetworkDelegate network_delegate;
  TestReportingService reporting_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_reporting_service(&reporting_service);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, reporting_service.headers().size());
  EXPECT_EQ(request_url, reporting_service.headers()[0].url);
  EXPECT_EQ("foo, bar", reporting_service.headers()[0].header_value);
}

TEST_F(URLRequestTestHTTP, DontProcessReportToHeaderInvalidHttps) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_test_server.RegisterRequestHandler(
      base::BindRepeating(&SendReportToHeader));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/");

  TestNetworkDelegate network_delegate;
  TestReportingService reporting_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_reporting_service(&reporting_service);
  context.Init();

  TestDelegate d;
  d.set_allow_certificate_errors(true);
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(d.have_certificate_errors());
  EXPECT_TRUE(IsCertStatusError(request->ssl_info().cert_status));
  EXPECT_TRUE(reporting_service.headers().empty());
}

// Network Error Logging is dependent on the Reporting API, so only run NEL
// tests if Reporting is enabled in the build.

namespace {

class TestNetworkErrorLoggingService : public NetworkErrorLoggingService {
 public:
  struct Header {
    Header() = default;
    ~Header() = default;

    // Returns whether the |received_ip_address| field matches any of the
    // addresses in |address_list|.
    bool MatchesAddressList(const AddressList& address_list) const {
      return std::any_of(address_list.begin(), address_list.end(),
                         [this](const IPEndPoint& endpoint) {
                           return endpoint.address() == received_ip_address;
                         });
    }

    url::Origin origin;
    IPAddress received_ip_address;
    std::string value;
  };

  const std::vector<Header>& headers() { return headers_; }
  const std::vector<RequestDetails>& errors() { return errors_; }

  // NetworkErrorLoggingService implementation:

  ~TestNetworkErrorLoggingService() override = default;

  void OnHeader(const url::Origin& origin,
                const IPAddress& received_ip_address,
                const std::string& value) override {
    Header header;
    header.origin = origin;
    header.received_ip_address = received_ip_address;
    header.value = value;
    headers_.push_back(header);
  }

  void OnRequest(RequestDetails details) override {
    errors_.push_back(std::move(details));
  }

  void RemoveBrowsingData(const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    NOTREACHED();
  }

  void RemoveAllBrowsingData() override { NOTREACHED(); }

 private:
  std::vector<Header> headers_;
  std::vector<RequestDetails> errors_;
};

std::unique_ptr<test_server::HttpResponse> SendNelHeader(
    const test_server::HttpRequest& request) {
  std::unique_ptr<test_server::BasicHttpResponse> http_response(
      new test_server::BasicHttpResponse);
  http_response->set_code(HTTP_OK);
  http_response->AddCustomHeader(NetworkErrorLoggingService::kHeaderName,
                                 "foo");
  return std::move(http_response);
}

std::unique_ptr<test_server::HttpResponse> SendEmptyResponse(
    const test_server::HttpRequest& request) {
  return std::make_unique<test_server::RawHttpResponse>("", "");
}

// Distinct User-Agent header values that we use to ensure that URLRequest
// passes along user agents into NEL reports correctly.
constexpr char kHeaderUserAgent[] = "MozillaFromHeader/1.0";
constexpr char kSettingsUserAgent[] = "MozillaFromSettings/1.0";

}  // namespace

TEST_F(URLRequestTestHTTP, DontProcessNelHeaderNoDelegate) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&SendNelHeader));
  ASSERT_TRUE(http_test_server()->Start());
  GURL request_url = http_test_server()->GetURL("/");

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();
}

TEST_F(URLRequestTestHTTP, DontProcessNelHeaderHttp) {
  http_test_server()->RegisterRequestHandler(
      base::BindRepeating(&SendNelHeader));
  ASSERT_TRUE(http_test_server()->Start());
  GURL request_url = http_test_server()->GetURL("/");

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(nel_service.headers().empty());
}

TEST_F(URLRequestTestHTTP, ProcessNelHeaderHttps) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.RegisterRequestHandler(base::BindRepeating(&SendNelHeader));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/");

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.headers().size());
  EXPECT_EQ(url::Origin::Create(request_url), nel_service.headers()[0].origin);
  AddressList address_list;
  EXPECT_TRUE(https_test_server.GetAddressList(&address_list));
  EXPECT_TRUE(nel_service.headers()[0].MatchesAddressList(address_list));
  EXPECT_EQ("foo", nel_service.headers()[0].value);
}

TEST_F(URLRequestTestHTTP, DontProcessNelHeaderInvalidHttps) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_test_server.RegisterRequestHandler(base::BindRepeating(&SendNelHeader));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/");

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.Init();

  TestDelegate d;
  d.set_allow_certificate_errors(true);
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(d.have_certificate_errors());
  EXPECT_TRUE(IsCertStatusError(request->ssl_info().cert_status));
  EXPECT_TRUE(nel_service.headers().empty());
}

TEST_F(URLRequestTestHTTP, DontForwardErrorToNelNoDelegate) {
  URLRequestFailedJob::AddUrlHandler();

  GURL request_url =
      URLRequestFailedJob::GetMockHttpsUrl(ERR_CONNECTION_REFUSED);

  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  URLRequestFilter::GetInstance()->ClearHandlers();
}

// TODO(juliatuttle): Figure out whether this restriction should be in place,
// and either implement it or remove this test.
TEST_F(URLRequestTestHTTP, DISABLED_DontForwardErrorToNelHttp) {
  URLRequestFailedJob::AddUrlHandler();

  GURL request_url =
      URLRequestFailedJob::GetMockHttpUrl(ERR_CONNECTION_REFUSED);

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(nel_service.errors().empty());

  URLRequestFilter::GetInstance()->ClearHandlers();
}

TEST_F(URLRequestTestHTTP, ForwardErrorToNelHttps_Mock) {
  URLRequestFailedJob::AddUrlHandler();

  GURL request_url =
      URLRequestFailedJob::GetMockHttpsUrl(ERR_CONNECTION_REFUSED);

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  EXPECT_EQ(request_url, nel_service.errors()[0].uri);
  EXPECT_EQ(0, nel_service.errors()[0].status_code);
  EXPECT_EQ(ERR_CONNECTION_REFUSED, nel_service.errors()[0].type);

  URLRequestFilter::GetInstance()->ClearHandlers();
}

// Also test with a real server, to exercise interactions with
// URLRequestHttpJob.
TEST_F(URLRequestTestHTTP, ForwardErrorToNelHttps_Real) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.RegisterRequestHandler(
      base::BindRepeating(&SendEmptyResponse));
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.GetURL("/");

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  EXPECT_EQ(request_url, nel_service.errors()[0].uri);
  EXPECT_EQ(0, nel_service.errors()[0].status_code);
  EXPECT_EQ(ERR_EMPTY_RESPONSE, nel_service.errors()[0].type);
}

TEST_F(URLRequestTestHTTP, NelReportUserAgentWithHeaderWithSettings) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.base_url();

  StaticHttpUserAgentSettings settings("en", kSettingsUserAgent);
  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.set_http_user_agent_settings(&settings);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetExtraRequestHeaderByName("User-Agent", kHeaderUserAgent, true);
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  EXPECT_EQ(kHeaderUserAgent, nel_service.errors()[0].user_agent);
}

TEST_F(URLRequestTestHTTP, NelReportUserAgentWithHeaderWithoutSettings) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.base_url();

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.set_create_default_http_user_agent_settings(false);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetExtraRequestHeaderByName("User-Agent", kHeaderUserAgent, true);
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  EXPECT_EQ(kHeaderUserAgent, nel_service.errors()[0].user_agent);
}

TEST_F(URLRequestTestHTTP, NelReportUserAgentWithoutHeaderWithSettings) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.base_url();

  StaticHttpUserAgentSettings settings("en", kSettingsUserAgent);
  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.set_http_user_agent_settings(&settings);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  EXPECT_EQ(kSettingsUserAgent, nel_service.errors()[0].user_agent);
}

TEST_F(URLRequestTestHTTP, NelReportUserAgentWithoutHeaderWithoutSettings) {
  EmbeddedTestServer https_test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(https_test_server.Start());
  GURL request_url = https_test_server.base_url();

  TestNetworkDelegate network_delegate;
  TestNetworkErrorLoggingService nel_service;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_network_error_logging_service(&nel_service);
  context.set_create_default_http_user_agent_settings(false);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> request(context.CreateRequest(
      request_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  d.RunUntilComplete();

  ASSERT_EQ(1u, nel_service.errors().size());
  EXPECT_EQ("", nel_service.errors()[0].user_agent);
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

TEST_F(URLRequestTestHTTP, ProtocolHandlerAndFactoryRestrictDataRedirects) {
  // Test URLRequestJobFactory::ProtocolHandler::IsSafeRedirectTarget().
  GURL data_url("data:,foo");
  DataProtocolHandler data_protocol_handler;
  EXPECT_FALSE(data_protocol_handler.IsSafeRedirectTarget(data_url));

  // Test URLRequestJobFactoryImpl::IsSafeRedirectTarget().
  EXPECT_FALSE(job_factory_->IsSafeRedirectTarget(data_url));
}

#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
TEST_F(URLRequestTestHTTP, ProtocolHandlerAndFactoryRestrictFileRedirects) {
  // Test URLRequestJobFactory::ProtocolHandler::IsSafeRedirectTarget().
  GURL file_url("file:///foo.txt");
  FileProtocolHandler file_protocol_handler(
      base::ThreadTaskRunnerHandle::Get());
  EXPECT_FALSE(file_protocol_handler.IsSafeRedirectTarget(file_url));

  // Test URLRequestJobFactoryImpl::IsSafeRedirectTarget().
  EXPECT_FALSE(job_factory_->IsSafeRedirectTarget(file_url));
}

TEST_F(URLRequestTestHTTP, RestrictFileRedirects) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/redirect-to-file.html"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(ERR_UNSAFE_REDIRECT, d.request_status());

  // The redirect should have been rejected before reporting it to the caller.
  EXPECT_EQ(0, d.received_redirect_count());
}
#endif  // !BUILDFLAG(DISABLE_FILE_SUPPORT)

TEST_F(URLRequestTestHTTP, RestrictDataRedirects) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/redirect-to-data.html"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
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
    req->Start();
    d.RunUntilRedirect();

    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(0, d.response_started_count());
    EXPECT_TRUE(req->was_cached());

    req->FollowDeferredRedirect(base::nullopt /* modified_request_headers */);
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
    default_network_delegate_.set_redirect_on_headers_received_url(
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

// Tests that redirection to an unsafe URL is allowed when it has been marked as
// safe.
TEST_F(URLRequestTestHTTP, UnsafeRedirectToWhitelistedUnsafeURL) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL unsafe_url("data:text/html,this-is-considered-an-unsafe-url");
  default_network_delegate_.set_redirect_on_headers_received_url(unsafe_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(unsafe_url);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/whatever"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(unsafe_url, r->url());
    EXPECT_EQ("this-is-considered-an-unsafe-url", d.data_received());
  }
}

// Tests that a redirect to a different unsafe URL is blocked, even after adding
// some other URL to the whitelist.
TEST_F(URLRequestTestHTTP, UnsafeRedirectToDifferentUnsafeURL) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL unsafe_url("data:text/html,something");
  GURL different_unsafe_url("data:text/html,something-else");
  default_network_delegate_.set_redirect_on_headers_received_url(unsafe_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(
      different_unsafe_url);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/whatever"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(ERR_UNSAFE_REDIRECT, d.request_status());

    // The redirect should have been rejected before reporting it to the caller.
    EXPECT_EQ(0, d.received_redirect_count());
  }
}

// Redirects from an URL with fragment to an unsafe URL with fragment should
// be allowed, and the reference fragment of the target URL should be preserved.
TEST_F(URLRequestTestHTTP, UnsafeRedirectWithDifferentReferenceFragment) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL original_url(http_test_server()->GetURL("/original#fragment1"));
  GURL unsafe_url("data:,url-marked-safe-and-used-in-redirect#fragment2");
  GURL expected_url("data:,url-marked-safe-and-used-in-redirect#fragment2");

  default_network_delegate_.set_redirect_on_headers_received_url(unsafe_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(unsafe_url);

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

// When a delegate has specified a safe redirect URL, but it does not match the
// redirect target, then do not prevent the reference fragment from being added.
TEST_F(URLRequestTestHTTP, RedirectWithReferenceFragmentAndUnrelatedUnsafeUrl) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL original_url(http_test_server()->GetURL("/original#expected-fragment"));
  GURL unsafe_url("data:text/html,this-url-does-not-match-redirect-url");
  GURL redirect_url(http_test_server()->GetURL("/target"));
  GURL expected_redirect_url(
      http_test_server()->GetURL("/target#expected-fragment"));

  default_network_delegate_.set_redirect_on_headers_received_url(redirect_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(unsafe_url);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(expected_redirect_url, r->url());
  }
}

// When a delegate has specified a safe redirect URL, assume that the redirect
// URL should not be changed. In particular, the reference fragment should not
// be modified.
TEST_F(URLRequestTestHTTP, RedirectWithReferenceFragment) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL original_url(
      http_test_server()->GetURL("/original#should-not-be-appended"));
  GURL redirect_url("data:text/html,expect-no-reference-fragment");

  default_network_delegate_.set_redirect_on_headers_received_url(redirect_url);
  default_network_delegate_.set_allowed_unsafe_redirect_url(redirect_url);

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        original_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(original_url, r->original_url());
    EXPECT_EQ(redirect_url, r->url());
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

  std::unique_ptr<URLRequestRedirectJob> job(new URLRequestRedirectJob(
      r.get(), &default_network_delegate_, redirect_url,
      URLRequestRedirectJob::REDIRECT_302_FOUND, "Very Good Reason"));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));

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
    EXPECT_TRUE(d.have_full_request_headers());
    CheckFullRequestHeaders(d.full_request_headers(), test_url);
    d.ClearFullRequestHeaders();

    req->FollowDeferredRedirect(base::nullopt /* modified_request_headers */);
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(OK, d.request_status());

    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(kTestFilePath);
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));

    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect_GetFullRequestHeaders) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  {
    GURL test_url(http_test_server()->GetURL("/redirect-test.html"));
    std::unique_ptr<URLRequest> req(default_context().CreateRequest(
        test_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    EXPECT_FALSE(d.have_full_request_headers());

    req->Start();
    d.RunUntilRedirect();

    EXPECT_EQ(1, d.received_redirect_count());

    req->FollowDeferredRedirect(base::nullopt /* modified_request_headers */);
    d.RunUntilComplete();

    GURL target_url(http_test_server()->GetURL("/with-headers.html"));
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_TRUE(d.have_full_request_headers());
    CheckFullRequestHeaders(d.full_request_headers(), target_url);
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(OK, d.request_status());

    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append(kTestFilePath);
    path = path.Append(FILE_PATH_LITERAL("with-headers.html"));

    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, d.data_received());
  }
}

TEST_F(URLRequestTestHTTP, DeferredRedirect_ModifiedRequestHeaders) {
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
    EXPECT_TRUE(d.have_full_request_headers());
    const HttpRequestHeaders& sent_headers1 = d.full_request_headers();
    std::string sent_value;
    EXPECT_TRUE(sent_headers1.GetHeader("Header1", &sent_value));
    EXPECT_EQ("Value1", sent_value);
    EXPECT_TRUE(sent_headers1.GetHeader("Header2", &sent_value));
    EXPECT_EQ("Value2", sent_value);
    EXPECT_FALSE(sent_headers1.GetHeader("Header3", &sent_value));
    d.ClearFullRequestHeaders();

    // Overwrite Header2 and add Header3.
    net::HttpRequestHeaders modified_request_headers;
    modified_request_headers.SetHeader("Header2", "");
    modified_request_headers.SetHeader("Header3", "Value3");

    req->FollowDeferredRedirect(modified_request_headers);
    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(OK, d.request_status());

    // Redirected request should also have modified headers.
    EXPECT_TRUE(d.have_full_request_headers());
    const HttpRequestHeaders& sent_headers2 = d.full_request_headers();
    EXPECT_TRUE(sent_headers2.GetHeader("Header1", &sent_value));
    EXPECT_EQ("Value1", sent_value);
    EXPECT_TRUE(sent_headers2.GetHeader("Header2", &sent_value));
    EXPECT_EQ("", sent_value);
    EXPECT_TRUE(sent_headers2.GetHeader("Header3", &sent_value));
    EXPECT_EQ("Value3", sent_value);
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
  {
    TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
    TestURLRequestContext context(true);
    context.set_network_delegate(&network_delegate);
    context.Init();

    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    std::unique_ptr<URLRequest> r(
        context.CreateRequest(url_requiring_auth, DEFAULT_PRIORITY, &d,
                              TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Make sure we sent the cookie in the restarted transaction.
    EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true")
        != std::string::npos);
  }

  // Same test as above, except this time the restart is initiated earlier
  // (without user intervention since identity is embedded in the URL).
  {
    TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
    TestURLRequestContext context(true);
    context.set_network_delegate(&network_delegate);
    context.Init();

    TestDelegate d;

    GURL::Replacements replacements;
    replacements.SetUsernameStr("user2");
    replacements.SetPasswordStr("secret");
    GURL url_with_identity = url_requiring_auth.ReplaceComponents(replacements);

    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url_with_identity, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("user2/secret") != std::string::npos);

    // Make sure we sent the cookie in the restarted transaction.
    EXPECT_TRUE(d.data_received().find("Cookie: got_challenged=true")
        != std::string::npos);
  }
}

// Tests that load timing works as expected with auth and the cache.
TEST_F(URLRequestTestHTTP, BasicAuthLoadTiming) {
  ASSERT_TRUE(http_test_server()->Start());

  // populate the cache
  {
    TestDelegate d;
    d.set_credentials(AuthCredentials(kUser, kSecret));

    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        http_test_server()->GetURL("/auth-basic"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    d.RunUntilComplete();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    LoadTimingInfo load_timing_info_before_auth;
    EXPECT_TRUE(default_network_delegate_.GetLoadTimingInfoBeforeAuth(
        &load_timing_info_before_auth));
    TestLoadTimingNotReused(load_timing_info_before_auth,
                            CONNECT_TIMING_HAS_DNS_TIMES);

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
  req->set_upload(CreateSimpleUploadData(kData));

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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT", url.GetOrigin().spec());
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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT", url.GetOrigin().spec());
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

  HTTPRedirectOriginHeaderTest(url, "CONNECT", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "CONNECT", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "DELETE", "GET", std::string());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "DELETE", "GET",
                               std::string());
  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "HEAD", "HEAD", url.GetOrigin().spec());
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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "POST", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null");
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT", url.GetOrigin().spec());
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

  HTTPRedirectOriginHeaderTest(url, "GET", "GET", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "GET", "GET", "null");
  HTTPRedirectOriginHeaderTest(url, "POST", "POST", url.GetOrigin().spec());
  HTTPRedirectOriginHeaderTest(https_redirect_url, "POST", "POST", "null");
  HTTPRedirectOriginHeaderTest(url, "PUT", "PUT", url.GetOrigin().spec());
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

TEST_F(URLRequestTestHTTP, RedirectPreserveFirstPartyURL) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL url(http_test_server()->GetURL("/redirect302-to-echo"));
  GURL first_party_url("http://example.com");

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_site_for_cookies(first_party_url);

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(first_party_url, r->site_for_cookies());
  }
}

TEST_F(URLRequestTestHTTP, RedirectUpdateFirstPartyURL) {
  ASSERT_TRUE(http_test_server()->Start());

  GURL url(http_test_server()->GetURL("/redirect302-to-echo"));
  GURL original_first_party_url("http://example.com");
  GURL expected_first_party_url(http_test_server()->GetURL("/echo"));

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_site_for_cookies(original_first_party_url);
    r->set_first_party_url_policy(
        URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);

    r->Start();
    d.RunUntilComplete();

    EXPECT_EQ(2U, r->url_chain().size());
    EXPECT_EQ(OK, d.request_status());
    EXPECT_EQ(expected_first_party_url, r->site_for_cookies());
  }
}

TEST_F(URLRequestTestHTTP, InterceptPost302RedirectGet) {
  ASSERT_TRUE(http_test_server()->Start());

  const char kData[] = "hello world";

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("POST");
  req->set_upload(CreateSimpleUploadData(kData));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::NumberToString(arraysize(kData) - 1));
  req->SetExtraRequestHeaders(headers);

  std::unique_ptr<URLRequestRedirectJob> job(new URLRequestRedirectJob(
      req.get(), &default_network_delegate_,
      http_test_server()->GetURL("/echo"),
      URLRequestRedirectJob::REDIRECT_302_FOUND, "Very Good Reason"));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));

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
  req->set_upload(CreateSimpleUploadData(kData));
  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentLength,
                    base::NumberToString(arraysize(kData) - 1));
  req->SetExtraRequestHeaders(headers);

  std::unique_ptr<URLRequestRedirectJob> job(new URLRequestRedirectJob(
      req.get(), &default_network_delegate_,
      http_test_server()->GetURL("/echo"),
      URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT,
      "Very Good Reason"));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));

  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ("POST", req->method());
  EXPECT_EQ(kData, d.data_received());
}

// Check that default A-L header is sent.
TEST_F(URLRequestTestHTTP, DefaultAcceptLanguage) {
  ASSERT_TRUE(http_test_server()->Start());

  StaticHttpUserAgentSettings settings("en", std::string());
  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_http_user_agent_settings(&settings);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context.CreateRequest(
      http_test_server()->GetURL("/echoheader?Accept-Language"),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_EQ("en", d.data_received());
}

// Check that an empty A-L header is not sent. http://crbug.com/77365.
TEST_F(URLRequestTestHTTP, EmptyAcceptLanguage) {
  ASSERT_TRUE(http_test_server()->Start());

  std::string empty_string;  // Avoid most vexing parse on line below.
  StaticHttpUserAgentSettings settings(empty_string, empty_string);
  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();
  // We override the language after initialization because empty entries
  // get overridden by Init().
  context.set_http_user_agent_settings(&settings);

  TestDelegate d;
  std::unique_ptr<URLRequest> req(context.CreateRequest(
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
// TODO(crbug.com/564656) This test is flaky on iOS.
#if defined(OS_IOS)
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

  TestNetworkDelegate network_delegate;  // Must outlive URLRequests.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.Init();
  // We override the HttpUserAgentSettings after initialization because empty
  // entries get overridden by Init().
  context.set_http_user_agent_settings(NULL);

  struct {
    const char* request;
    const char* expected_response;
  } tests[] = {{"/echoheader?Accept-Language", "None"},
               {"/echoheader?Accept-Charset", "None"},
               {"/echoheader?User-Agent", ""}};

  for (size_t i = 0; i < arraysize(tests); i++) {
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context.CreateRequest(
        http_test_server()->GetURL(tests[i].request), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    EXPECT_EQ(tests[i].expected_response, d.data_received())
        << " Request = \"" << tests[i].request << "\"";
  }
}

// Make sure that URLRequest passes on its priority updates to
// newly-created jobs after the first one.
TEST_F(URLRequestTestHTTP, SetSubsequentJobPriority) {
  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/defaultresponse"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(DEFAULT_PRIORITY, req->priority());

  std::unique_ptr<URLRequestRedirectJob> redirect_job(new URLRequestRedirectJob(
      req.get(), &default_network_delegate_,
      http_test_server()->GetURL("/echo"),
      URLRequestRedirectJob::REDIRECT_302_FOUND, "Very Good Reason"));
  AddTestInterceptor()->set_main_intercept_job(std::move(redirect_job));

  req->SetPriority(LOW);
  req->Start();
  EXPECT_TRUE(req->is_pending());

  RequestPriority job_priority;
  std::unique_ptr<URLRequestJob> job(new PriorityMonitoringURLRequestJob(
      req.get(), &default_network_delegate_, &job_priority));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));

  // Should trigger |job| to be started.
  d.RunUntilComplete();
  EXPECT_EQ(LOW, job_priority);
}

// Check that creating a network request while entering/exiting suspend mode
// fails as it should.  This is the only case where an HttpTransactionFactory
// does not return an HttpTransaction.
TEST_F(URLRequestTestHTTP, NetworkSuspendTest) {
  // Create a new HttpNetworkLayer that thinks it's suspended.
  std::unique_ptr<HttpNetworkLayer> network_layer(new HttpNetworkLayer(
      default_context().http_transaction_factory()->GetSession()));
  network_layer->OnSuspend();

  HttpCache http_cache(std::move(network_layer),
                       HttpCache::DefaultBackend::InMemory(0),
                       false /* is_main_cache */);

  TestURLRequestContext context(true);
  context.set_http_transaction_factory(&http_cache);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context.CreateRequest(GURL("http://127.0.0.1/"), DEFAULT_PRIORITY, &d,
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

  ~FailingHttpTransactionFactory() override = default;

  // HttpTransactionFactory methods:
  int CreateTransaction(RequestPriority priority,
                        std::unique_ptr<HttpTransaction>* trans) override {
    return ERR_FAILED;
  }

  HttpCache* GetCache() override { return nullptr; }

  HttpNetworkSession* GetSession() override { return network_session_; }

 private:
  HttpNetworkSession* network_session_;

  DISALLOW_COPY_AND_ASSIGN(FailingHttpTransactionFactory);
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
  FailingHttpTransactionFactory http_transaction_factory(
      default_context().http_transaction_factory()->GetSession());
  TestURLRequestContext context(true);
  context.set_http_transaction_factory(&http_transaction_factory);
  context.set_network_delegate(default_network_delegate());
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context.CreateRequest(GURL("http://127.0.0.1/"), DEFAULT_PRIORITY, &d,
                            TRAFFIC_ANNOTATION_FOR_TESTS));
  // Don't send cookies (Collecting cookies is asynchronous, and need request to
  // try to create an HttpNetworkTransaction synchronously on start).
  req->SetLoadFlags(LOAD_DO_NOT_SEND_COOKIES);
  req->Start();
  req->Cancel();
  d.RunUntilComplete();
  // Run pending error task, if there is one.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(d.request_failed());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(ERR_ABORTED, d.request_status());

  // NetworkDelegate should see the cancellation, but not the error.
  EXPECT_EQ(1, default_network_delegate()->canceled_requests());
  EXPECT_EQ(0, default_network_delegate()->error_count());
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
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(OK, d.request_status());
  EXPECT_TRUE(req->response_info().network_accessed);
  EXPECT_FALSE(req->response_info().was_cached);

  req = default_context().CreateRequest(
      http_test_server()->GetURL("/cachetime"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS);
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

  EXPECT_TRUE(req->status().is_success());
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
  default_network_delegate_.set_before_start_transaction_fails();

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

class URLRequestInterceptorTestHTTP : public URLRequestTestHTTP {
 public:
  // TODO(bengr): Merge this with the URLRequestInterceptorHTTPTest fixture,
  // ideally remove the dependency on URLRequestTestJob, and maybe move these
  // tests into the factory tests.
  URLRequestInterceptorTestHTTP() : URLRequestTestHTTP(), interceptor_(NULL) {
  }

  void SetUpFactory() override {
    interceptor_ = new MockURLRequestInterceptor();
    job_factory_.reset(new URLRequestInterceptingJobFactory(
        std::move(job_factory_), base::WrapUnique(interceptor_)));
  }

  MockURLRequestInterceptor* interceptor() const {
    return interceptor_;
  }

 private:
  MockURLRequestInterceptor* interceptor_;
};

TEST_F(URLRequestInterceptorTestHTTP,
       NetworkDelegateNotificationOnRedirectIntercept) {
  interceptor()->set_intercept_redirect(true);
  interceptor()->set_redirect_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_redirect_data(MockURLRequestInterceptor::ok_data());

  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/redirect-test.html"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(interceptor()->did_intercept_redirect());
  // Check we got one good response
  int status = d.request_status();
  EXPECT_EQ(OK, status);
  if (status == OK)
    EXPECT_EQ(200, req->response_headers()->response_code());

  EXPECT_EQ(MockURLRequestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());

  EXPECT_EQ(1, default_network_delegate()->created_requests());
  EXPECT_EQ(1, default_network_delegate()->before_start_transaction_count());
  EXPECT_EQ(1, default_network_delegate()->headers_received_count());
}

TEST_F(URLRequestInterceptorTestHTTP,
       NetworkDelegateNotificationOnErrorIntercept) {
  // Intercept that error and respond with an OK response.
  interceptor()->set_intercept_final_response(true);
  interceptor()->set_final_headers(MockURLRequestInterceptor::ok_headers());
  interceptor()->set_final_data(MockURLRequestInterceptor::ok_data());
  default_network_delegate()->set_can_be_intercepted_on_error(true);

  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/two-content-lengths.html"), DEFAULT_PRIORITY,
      &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(interceptor()->did_intercept_final());

  // Check we received one good response.
  int status = d.request_status();
  EXPECT_EQ(OK, status);
  if (status == OK)
    EXPECT_EQ(200, req->response_headers()->response_code());
  EXPECT_EQ(MockURLRequestInterceptor::ok_data(), d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());

  EXPECT_EQ(1, default_network_delegate()->created_requests());
  EXPECT_EQ(1, default_network_delegate()->before_start_transaction_count());
  EXPECT_EQ(0, default_network_delegate()->headers_received_count());
}

TEST_F(URLRequestInterceptorTestHTTP,
       NetworkDelegateNotificationOnResponseIntercept) {
  // Intercept that error and respond with an OK response.
  interceptor()->set_intercept_final_response(true);

  // Intercept with a real URLRequestHttpJob.
  interceptor()->set_use_url_request_http_job(true);

  ASSERT_TRUE(http_test_server()->Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      http_test_server()->GetURL("/simple.html"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("GET");
  req->Start();
  d.RunUntilComplete();

  EXPECT_TRUE(interceptor()->did_intercept_final());

  // Check we received one good response.
  int status = d.request_status();
  EXPECT_EQ(OK, status);
  if (status == OK)
    EXPECT_EQ(200, req->response_headers()->response_code());
  EXPECT_EQ("hello", d.data_received());
  EXPECT_EQ(1, d.response_started_count());
  EXPECT_EQ(0, d.received_redirect_count());

  EXPECT_EQ(1, default_network_delegate()->created_requests());
  EXPECT_EQ(2, default_network_delegate()->before_start_transaction_count());
  EXPECT_EQ(2, default_network_delegate()->headers_received_count());
}

class URLRequestTestReferrerPolicy : public URLRequestTest {
 public:
  URLRequestTestReferrerPolicy() = default;

  void InstantiateSameOriginServers(net::EmbeddedTestServer::Type type) {
    origin_server_.reset(new EmbeddedTestServer(type));
    if (type == net::EmbeddedTestServer::TYPE_HTTPS) {
      origin_server_->AddDefaultHandlers(
          base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
    } else {
      origin_server_->AddDefaultHandlers(base::FilePath(kTestFilePath));
    }
    ASSERT_TRUE(origin_server_->Start());
  }

  void InstantiateCrossOriginServers(net::EmbeddedTestServer::Type origin_type,
                                     net::EmbeddedTestServer::Type dest_type) {
    origin_server_.reset(new EmbeddedTestServer(origin_type));
    if (origin_type == net::EmbeddedTestServer::TYPE_HTTPS) {
      origin_server_->AddDefaultHandlers(
          base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
    } else {
      origin_server_->AddDefaultHandlers(base::FilePath(kTestFilePath));
    }
    ASSERT_TRUE(origin_server_->Start());

    destination_server_.reset(new EmbeddedTestServer(dest_type));
    if (dest_type == net::EmbeddedTestServer::TYPE_HTTPS) {
      destination_server_->AddDefaultHandlers(
          base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
    } else {
      destination_server_->AddDefaultHandlers(base::FilePath(kTestFilePath));
    }
    ASSERT_TRUE(destination_server_->Start());
  }

  void VerifyReferrerAfterRedirect(URLRequest::ReferrerPolicy policy,
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
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer, referrer);

  VerifyReferrerAfterRedirect(
      URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      referrer, referrer);

  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer, referrer);

  VerifyReferrerAfterRedirect(URLRequest::NEVER_CLEAR_REFERRER, referrer,
                              referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(URLRequest::ORIGIN, referrer.GetOrigin(),
                              referrer.GetOrigin());

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.GetOrigin(), referrer.GetOrigin());

  VerifyReferrerAfterRedirect(URLRequest::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPToCrossOriginHTTP) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTP,
                                net::EmbeddedTestServer::TYPE_HTTP);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer, referrer);

  VerifyReferrerAfterRedirect(
      URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      referrer, referrer.GetOrigin());

  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer.GetOrigin());

  VerifyReferrerAfterRedirect(URLRequest::NEVER_CLEAR_REFERRER, referrer,
                              referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(URLRequest::ORIGIN, referrer.GetOrigin(),
                              referrer.GetOrigin());

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN, referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.GetOrigin(), referrer.GetOrigin());

  VerifyReferrerAfterRedirect(URLRequest::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPSToSameOriginHTTPS) {
  InstantiateSameOriginServers(net::EmbeddedTestServer::TYPE_HTTPS);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer, referrer);

  VerifyReferrerAfterRedirect(
      URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      referrer, referrer);

  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer, referrer);

  VerifyReferrerAfterRedirect(URLRequest::NEVER_CLEAR_REFERRER, referrer,
                              referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(URLRequest::ORIGIN, referrer.GetOrigin(),
                              referrer.GetOrigin());

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN, referrer,
      referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.GetOrigin(), referrer.GetOrigin());

  VerifyReferrerAfterRedirect(URLRequest::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPSToCrossOriginHTTPS) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTPS,
                                net::EmbeddedTestServer::TYPE_HTTPS);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer, referrer);

  VerifyReferrerAfterRedirect(
      URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      referrer, origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(URLRequest::NEVER_CLEAR_REFERRER, referrer,
                              referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(URLRequest::ORIGIN, referrer.GetOrigin(),
                              referrer.GetOrigin());

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN, referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.GetOrigin(), referrer.GetOrigin());

  VerifyReferrerAfterRedirect(URLRequest::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPToHTTPS) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTP,
                                net::EmbeddedTestServer::TYPE_HTTPS);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer, referrer);

  VerifyReferrerAfterRedirect(
      URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      referrer, origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(URLRequest::NEVER_CLEAR_REFERRER, referrer,
                              referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(URLRequest::ORIGIN, referrer.GetOrigin(),
                              referrer.GetOrigin());

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN, referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.GetOrigin(), referrer.GetOrigin());

  VerifyReferrerAfterRedirect(URLRequest::NO_REFERRER, GURL(), GURL());
}

TEST_F(URLRequestTestReferrerPolicy, HTTPSToHTTP) {
  InstantiateCrossOriginServers(net::EmbeddedTestServer::TYPE_HTTPS,
                                net::EmbeddedTestServer::TYPE_HTTP);
  GURL referrer = origin_server()->GetURL("/path/to/file.html");

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer, GURL());

  VerifyReferrerAfterRedirect(
      URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      referrer, GURL());

  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN, referrer,
      origin_server()->GetURL("/"));

  VerifyReferrerAfterRedirect(URLRequest::NEVER_CLEAR_REFERRER, referrer,
                              referrer);

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin; thus this test case just
  // checks that this policy doesn't cause the referrer to change when following
  // a redirect.
  VerifyReferrerAfterRedirect(URLRequest::ORIGIN, referrer.GetOrigin(),
                              referrer.GetOrigin());

  VerifyReferrerAfterRedirect(
      URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN, referrer, GURL());

  // The original referrer set on the request is expected to obey the referrer
  // policy and already be stripped to the origin, though it should be
  // subsequently cleared during the downgrading redirect.
  VerifyReferrerAfterRedirect(
      URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      referrer.GetOrigin(), GURL());

  VerifyReferrerAfterRedirect(URLRequest::NO_REFERRER, GURL(), GURL());
}

class HTTPSRequestTest : public TestWithScopedTaskEnvironment {
 public:
  HTTPSRequestTest() : default_context_(true) {
    default_context_.set_network_delegate(&default_network_delegate_);
    default_context_.Init();
  }
  ~HTTPSRequestTest() override {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }

 protected:
  TestNetworkDelegate default_network_delegate_;  // Must outlive URLRequest.
  TestURLRequestContext default_context_;
};

TEST_F(HTTPSRequestTest, HTTPSGetTest) {
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
              r->GetSocketAddress().host());
    EXPECT_EQ(test_server.host_port_pair().port(),
              r->GetSocketAddress().port());
  }
}

TEST_F(HTTPSRequestTest, HTTPSMismatchedTest) {
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  bool err_allowed = true;
  for (int i = 0; i < 2 ; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // Iterate from false to true, just so that we do the opposite of the
  // previous test in order to increase test coverage.
  bool err_allowed = false;
  for (int i = 0; i < 2 ; i++, err_allowed = !err_allowed) {
    TestDelegate d;
    {
      d.set_allow_certificate_errors(err_allowed);
      std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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

// TODO(svaldez): iOS tests are flaky with EmbeddedTestServer and transport
// security state. (see http://crbug.com/550977).
#if !defined(OS_IOS)
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
  // hsts-hpkp-preloaded.test to the testserver. By default, MockHostResolver
  // maps all hosts to 127.0.0.1.

  MockHostResolver host_resolver;
  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_host_resolver(&host_resolver);
  TransportSecurityState transport_security_state;
  context.set_transport_security_state(&transport_security_state);
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> r(context.CreateRequest(
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
  // hsts-hpkp-preloaded.test to the testserver. By default, MockHostResolver
  // maps all hosts to 127.0.0.1.

  MockHostResolver host_resolver;
  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_host_resolver(&host_resolver);
  TransportSecurityState transport_security_state;

  TransportSecurityState::STSState static_sts_state;
  TransportSecurityState::PKPState static_pkp_state;
  EXPECT_TRUE(transport_security_state.GetStaticDomainState(
      "hsts-hpkp-preloaded.test", &static_sts_state, &static_pkp_state));
  context.set_transport_security_state(&transport_security_state);
  context.Init();

  TransportSecurityState::STSState dynamic_sts_state;
  TransportSecurityState::PKPState dynamic_pkp_state;
  EXPECT_FALSE(transport_security_state.GetDynamicSTSState(
      "hsts-hpkp-preloaded.test", &dynamic_sts_state));
  EXPECT_FALSE(transport_security_state.GetDynamicPKPState(
      "hsts-hpkp-preloaded.test", &dynamic_pkp_state));

  TestDelegate d;
  std::unique_ptr<URLRequest> r(context.CreateRequest(
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
  EXPECT_TRUE(transport_security_state.GetStaticDomainState(
      "hsts-hpkp-preloaded.test", &new_static_sts_state,
      &new_static_pkp_state));
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
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());


  // Per spec, TransportSecurityState expects a domain name, rather than an IP
  // address, so a MockHostResolver is needed to redirect www.somewhere.com to
  // the EmbeddedTestServer.  By default, MockHostResolver maps all hosts
  // to 127.0.0.1.
  MockHostResolver host_resolver;

  // Force https for www.somewhere.com.
  TransportSecurityState transport_security_state;
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  bool include_subdomains = false;
  transport_security_state.AddHSTS("www.somewhere.com", expiry,
                                   include_subdomains);

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.

  TestURLRequestContext context(true);
  context.set_host_resolver(&host_resolver);
  context.set_transport_security_state(&transport_security_state);
  context.set_network_delegate(&network_delegate);
  context.Init();

  TestDelegate d;
  // Navigating to https://www.somewhere.com instead of https://127.0.0.1 will
  // cause a certificate error.  Ignore the error.
  d.set_allow_certificate_errors(true);

  std::unique_ptr<URLRequest> req(context.CreateRequest(
      GURL(base::StringPrintf("http://www.somewhere.com:%d/echo",
                              test_server.host_port_pair().port())),
      DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->set_method("POST");
  req->set_upload(CreateSimpleUploadData(kData));

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

  // Per spec, TransportSecurityState expects a domain name, rather than an IP
  // address, so a MockHostResolver is needed to redirect example.net to the
  // EmbeddedTestServer. MockHostResolver maps all hosts to 127.0.0.1 by
  // default.
  MockHostResolver host_resolver;

  TransportSecurityState transport_security_state;
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1);
  bool include_subdomains = false;
  transport_security_state.AddHSTS("example.net", expiry, include_subdomains);

  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.

  MockCertVerifier cert_verifier;
  cert_verifier.set_default_result(OK);

  TestURLRequestContext context(true);
  context.set_host_resolver(&host_resolver);
  context.set_transport_security_state(&transport_security_state);
  context.set_network_delegate(&network_delegate);
  context.set_cert_verifier(&cert_verifier);
  context.Init();

  GURL hsts_http_url(base::StringPrintf("http://example.net:%d/somehstssite",
                                        test_server.host_port_pair().port()));
  url::Replacements<char> replacements;
  const char kNewScheme[] = "https";
  replacements.SetScheme(kNewScheme, url::Component(0, strlen(kNewScheme)));
  GURL hsts_https_url = hsts_http_url.ReplaceComponents(replacements);

  TestDelegate d;

  std::unique_ptr<URLRequest> req(context.CreateRequest(
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
  EXPECT_TRUE(headers->EnumerateHeader(NULL, "Location", &redirect_location));
  EXPECT_EQ(hsts_https_url.spec(), redirect_location);

  std::string received_cors_header;
  EXPECT_TRUE(headers->EnumerateHeader(NULL, "Access-Control-Allow-Origin",
                                       &received_cors_header));
  EXPECT_EQ(kOriginHeaderValue, received_cors_header);
}

namespace {

class SSLClientAuthTestDelegate : public TestDelegate {
 public:
  SSLClientAuthTestDelegate() : on_certificate_requested_count_(0) {
    set_on_complete(base::DoNothing());
  }
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
  int on_certificate_requested_count_;
  base::OnceClosure on_certificate_requested_;
};

class TestSSLPrivateKey : public SSLPrivateKey {
 public:
  explicit TestSSLPrivateKey(scoped_refptr<SSLPrivateKey> key)
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
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientAuthTestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  default_context_.http_transaction_factory()
      ->GetSession()
      ->CloseAllConnections();
  SSLClientSocket::ClearSessionCache();

  // Connecting again should not call OnCertificateRequested. The identity is
  // taken from the client auth cache.
  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  default_context_.http_transaction_factory()
      ->GetSession()
      ->CloseAllConnections();
  SSLClientSocket::ClearSessionCache();

  // The bad identity should have been evicted from the cache, so connecting
  // again should call OnCertificateRequested again.
  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // Connect with a client certificate to put it in the client auth cache.
  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  default_context_.http_transaction_factory()
      ->GetSession()
      ->CloseAllConnections();
  SSLClientSocket::ClearSessionCache();

  // Cause the private key to fail. Connecting again should attempt to use it,
  // notice the failure, and then request a new identity via
  // OnCertificateRequested.
  private_key->set_fail_signing(true);

  {
    SSLClientAuthTestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
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
  // Test that we attempt a session resume when making two connections to the
  // same host.
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.record_resume = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientSocket::ClearSessionCache();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
        test_server.GetURL("ssl-session-cache"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
  }

  reinterpret_cast<HttpCache*>(default_context_.http_transaction_factory())->
    CloseAllConnections();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
        test_server.GetURL("ssl-session-cache"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The response will look like;
    //   insert abc
    //   lookup abc
    //   insert xyz
    //
    // With a newline at the end which makes the split think that there are
    // four lines.

    EXPECT_EQ(1, d.response_started_count());
    std::vector<std::string> lines = base::SplitString(
        d.data_received(), "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(4u, lines.size()) << d.data_received();

    std::string session_id;

    for (size_t i = 0; i < 2; i++) {
      std::vector<std::string> parts = base::SplitString(
          lines[i], "\t", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      ASSERT_EQ(2u, parts.size());
      if (i == 0) {
        EXPECT_EQ("insert", parts[0]);
        session_id = parts[1];
      } else {
        EXPECT_EQ("lookup", parts[0]);
        EXPECT_EQ(session_id, parts[1]);
      }
    }
  }
}

// AssertTwoDistinctSessionsInserted checks that |session_info|, which must be
// the result of fetching "ssl-session-cache" from the test server, indicates
// that exactly two different sessions were inserted, with no lookups etc.
static void AssertTwoDistinctSessionsInserted(const string& session_info) {
  std::vector<std::string> lines = base::SplitString(
      session_info, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(3u, lines.size()) << session_info;

  std::string session_id;
  for (size_t i = 0; i < 2; i++) {
    std::vector<std::string> parts = base::SplitString(
        lines[i], "\t", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(2u, parts.size());
    EXPECT_EQ("insert", parts[0]);
    if (i == 0) {
      session_id = parts[1];
    } else {
      EXPECT_NE(session_id, parts[1]);
    }
  }
}

TEST_F(HTTPSRequestTest, SSLSessionCacheShardTest) {
  // Test that sessions aren't resumed when the value of ssl_session_cache_shard
  // differs.
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.record_resume = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientSocket::ClearSessionCache();

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
        test_server.GetURL("ssl-session-cache"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
  }

  // Now create a new HttpCache with a different ssl_session_cache_shard value.
  HttpNetworkSession::Context session_context;
  session_context.host_resolver = default_context_.host_resolver();
  session_context.cert_verifier = default_context_.cert_verifier();
  session_context.transport_security_state =
      default_context_.transport_security_state();
  session_context.cert_transparency_verifier =
      default_context_.cert_transparency_verifier();
  session_context.ct_policy_enforcer = default_context_.ct_policy_enforcer();
  session_context.proxy_resolution_service = default_context_.proxy_resolution_service();
  session_context.ssl_config_service = default_context_.ssl_config_service();
  session_context.http_auth_handler_factory =
      default_context_.http_auth_handler_factory();
  session_context.http_server_properties =
      default_context_.http_server_properties();

  HttpNetworkSession network_session(HttpNetworkSession::Params(),
                                     session_context);
  std::unique_ptr<HttpCache> cache(
      new HttpCache(&network_session, HttpCache::DefaultBackend::InMemory(0),
                    false /* is_main_cache */));

  default_context_.set_http_transaction_factory(cache.get());

  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
        test_server.GetURL("ssl-session-cache"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The response will look like;
    //   insert abc
    //   insert xyz
    //
    // With a newline at the end which makes the split think that there are
    // three lines.

    EXPECT_EQ(1, d.response_started_count());
    AssertTwoDistinctSessionsInserted(d.data_received());
  }
}

class HTTPSFallbackTest : public TestWithScopedTaskEnvironment {
 public:
  HTTPSFallbackTest() : context_(true) {
    ssl_config_service_ = std::make_unique<TestSSLConfigService>();
    context_.set_ssl_config_service(ssl_config_service_.get());
  }
  ~HTTPSFallbackTest() override = default;

 protected:
  TestSSLConfigService* ssl_config_service() {
    return ssl_config_service_.get();
  }

  void DoFallbackTest(const SpawnedTestServer::SSLOptions& ssl_options) {
    DCHECK(!request_);
    context_.Init();
    delegate_.set_allow_certificate_errors(true);

    SpawnedTestServer test_server(
        SpawnedTestServer::TYPE_HTTPS,
        ssl_options,
        base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
    ASSERT_TRUE(test_server.Start());

    request_ = context_.CreateRequest(test_server.GetURL("/"), DEFAULT_PRIORITY,
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
  std::unique_ptr<TestSSLConfigService> ssl_config_service_;
  TestDelegate delegate_;
  TestURLRequestContext context_;
  std::unique_ptr<URLRequest> request_;
};

// Tests the TLS 1.0 fallback doesn't happen.
TEST_F(HTTPSFallbackTest, TLSv1NoFallback) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  ssl_options.tls_intolerant =
      SpawnedTestServer::SSLOptions::TLS_INTOLERANT_TLS1_1;

  ASSERT_NO_FATAL_FAILURE(DoFallbackTest(ssl_options));
  ExpectFailure(ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
}

// Tests the TLS 1.1 fallback doesn't happen.
TEST_F(HTTPSFallbackTest, TLSv1_1NoFallback) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  ssl_options.tls_intolerant =
      SpawnedTestServer::SSLOptions::TLS_INTOLERANT_TLS1_2;

  ASSERT_NO_FATAL_FAILURE(DoFallbackTest(ssl_options));
  ExpectFailure(ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
}

// Tests that TLS 1.3 interference results in a dedicated error code.
TEST_F(HTTPSFallbackTest, TLSv1_3Interference) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  ssl_options.tls_intolerant =
      SpawnedTestServer::SSLOptions::TLS_INTOLERANT_TLS1_3;
  ssl_config_service()->set_max_version(SSL_PROTOCOL_VERSION_TLS1_3);

  ASSERT_NO_FATAL_FAILURE(DoFallbackTest(ssl_options));
  ExpectFailure(ERR_SSL_VERSION_INTERFERENCE);
}

// Tests that disabling TLS 1.3 leaves TLS 1.3 interference unnoticed.
TEST_F(HTTPSFallbackTest, TLSv1_3InterferenceDisableVersion) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_OK);
  ssl_options.tls_intolerant =
      SpawnedTestServer::SSLOptions::TLS_INTOLERANT_TLS1_3;
  ssl_config_service()->set_max_version(SSL_PROTOCOL_VERSION_TLS1_2);

  ASSERT_NO_FATAL_FAILURE(DoFallbackTest(ssl_options));
  ExpectConnection(SSL_CONNECTION_VERSION_TLS1_2);
}

class HTTPSSessionTest : public TestWithScopedTaskEnvironment {
 public:
  HTTPSSessionTest() : default_context_(true) {
    cert_verifier_.set_default_result(OK);

    default_context_.set_network_delegate(&default_network_delegate_);
    default_context_.set_cert_verifier(&cert_verifier_);
    default_context_.Init();
  }
  ~HTTPSSessionTest() override = default;

 protected:
  MockCertVerifier cert_verifier_;
  TestNetworkDelegate default_network_delegate_;  // Must outlive URLRequest.
  TestURLRequestContext default_context_;
};

// Tests that session resumption is not attempted if an invalid certificate
// is presented.
TEST_F(HTTPSSessionTest, DontResumeSessionsForInvalidCertificates) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.record_resume = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  SSLClientSocket::ClearSessionCache();

  // Simulate the certificate being expired and attempt a connection.
  cert_verifier_.set_default_result(ERR_CERT_DATE_INVALID);
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
        test_server.GetURL("ssl-session-cache"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_EQ(1, d.response_started_count());
  }

  reinterpret_cast<HttpCache*>(default_context_.http_transaction_factory())->
    CloseAllConnections();

  // Now change the certificate to be acceptable (so that the response is
  // loaded), and ensure that no session id is presented to the peer.
  cert_verifier_.set_default_result(OK);
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r(default_context_.CreateRequest(
        test_server.GetURL("ssl-session-cache"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    // The response will look like;
    //   insert abc
    //   insert xyz
    //
    // With a newline at the end which makes the split think that there are
    // three lines.
    //
    // If a session was presented (eg: a bug), then the response would look
    // like;
    //   insert abc
    //   lookup abc
    //   insert xyz

    EXPECT_EQ(1, d.response_started_count());
    AssertTwoDistinctSessionsInserted(d.data_received());
  }
}

// This the fingerprint of the "Testing CA" certificate used by the testserver.
// See net/data/ssl/certificates/ocsp-test-root.pem.
static const SHA256HashValue kOCSPTestCertFingerprint = {{
    0x0c, 0xa9, 0x05, 0x11, 0xb0, 0xa2, 0xc0, 0x1d, 0x40, 0x6a, 0x99,
    0x04, 0x21, 0x36, 0x45, 0x3f, 0x59, 0x12, 0x5c, 0x80, 0x64, 0x2d,
    0x46, 0x6a, 0x3b, 0x78, 0x9e, 0x84, 0xea, 0x54, 0x0f, 0x8b,
}};

// This is the SHA256, SPKI hash of the "Testing CA" certificate used by the
// testserver.
static const SHA256HashValue kOCSPTestCertSPKI = {{
    0x05, 0xa8, 0xf6, 0xfd, 0x8e, 0x10, 0xfe, 0x92, 0x2f, 0x22, 0x75,
    0x46, 0x40, 0xf4, 0xc4, 0x57, 0x06, 0x0d, 0x95, 0xfd, 0x60, 0x31,
    0x3b, 0xf3, 0xfc, 0x12, 0x47, 0xe7, 0x66, 0x1a, 0x82, 0xa3,
}};

// This is the policy OID contained in the certificates that testserver
// generates.
static const char kOCSPTestCertPolicy[] = "1.3.6.1.4.1.11129.2.4.1";

class HTTPSOCSPTest : public HTTPSRequestTest {
 public:
  HTTPSOCSPTest()
      : context_(true),
        ev_test_policy_(
            new ScopedTestEVPolicy(EVRootCAMetadata::GetInstance(),
                                   kOCSPTestCertFingerprint,
                                   kOCSPTestCertPolicy)) {
  }

  void SetUp() override {
    context_.SetCTPolicyEnforcer(std::make_unique<DefaultCTPolicyEnforcer>());
    context_.Init();

    context_.cert_verifier()->SetConfig(GetCertVerifierConfig());

    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(GetTestCertsDirectory(), "ocsp-test-root.pem");
    CHECK_NE(static_cast<X509Certificate*>(NULL), root_cert.get());
    test_root_.reset(new ScopedTestRoot(root_cert.get()));

#if defined(OS_ANDROID) || defined(USE_BUILTIN_CERT_VERIFIER)
    SetGlobalCertNetFetcherForTesting(net::CreateCertNetFetcher(&context_));
#endif

#if defined(USE_NSS_CERTS)
    SetURLRequestContextForNSSHttpIO(&context_);
#endif
  }

  void DoConnectionWithDelegate(
      const SpawnedTestServer::SSLOptions& ssl_options,
      TestDelegate* delegate,
      SSLInfo* out_ssl_info) {
    // Always overwrite |out_ssl_info|.
    out_ssl_info->Reset();

    SpawnedTestServer test_server(
        SpawnedTestServer::TYPE_HTTPS,
        ssl_options,
        base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
    ASSERT_TRUE(test_server.Start());

    delegate->set_allow_certificate_errors(true);
    std::unique_ptr<URLRequest> r(
        context_.CreateRequest(test_server.GetURL("/"), DEFAULT_PRIORITY,
                               delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();

    delegate->RunUntilComplete();
    EXPECT_EQ(1, delegate->response_started_count());

    *out_ssl_info = r->ssl_info();
  }

  void DoConnection(const SpawnedTestServer::SSLOptions& ssl_options,
                    CertStatus* out_cert_status) {
    // Always overwrite |out_cert_status|.
    *out_cert_status = 0;

    TestDelegate d;
    SSLInfo ssl_info;
    ASSERT_NO_FATAL_FAILURE(
        DoConnectionWithDelegate(ssl_options, &d, &ssl_info));

    *out_cert_status = ssl_info.cert_status;
  }

  ~HTTPSOCSPTest() override {
#if defined(OS_ANDROID) || defined(USE_BUILTIN_CERT_VERIFIER)
    ShutdownGlobalCertNetFetcher();
#endif

#if defined(USE_NSS_CERTS)
    SetURLRequestContextForNSSHttpIO(nullptr);
#endif
  }

 protected:
  // GetCertVerifierConfig() configures the URLRequestContext that will be used
  // for making connections to the testserver. This can be overridden in test
  // subclasses for different behaviour.
  virtual CertVerifier::Config GetCertVerifierConfig() {
    CertVerifier::Config config;
    config.enable_rev_checking = true;
    return config;
  }

  std::unique_ptr<ScopedTestRoot> test_root_;
  std::unique_ptr<TestSSLConfigService> ssl_config_service_;
  TestURLRequestContext context_;
  std::unique_ptr<ScopedTestEVPolicy> ev_test_policy_;
};

static CertStatus ExpectedCertStatusForFailedOnlineRevocationCheck() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Windows can return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION but we don't
  // have that ability on other platforms.
  // TODO(eroman): Should this also be the return value for
  //               CertVerifyProcBuiltin?
  return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
#else
  return 0;
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
#if defined(OS_WIN) || defined(USE_NSS_CERTS) || \
    defined(USE_BUILTIN_CERT_VERIFIER)
  return true;
#else
  return false;
#endif
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

// Returns the expected CertStatus for tests that expect an online revocation
// check failure as a result of checking a test EV cert, which will not
// actually trigger an online revocation check on some platforms.
static CertStatus ExpectedCertStatusForFailedOnlineEVRevocationCheck() {
  if (SystemUsesChromiumEVMetadata()) {
    return ExpectedCertStatusForFailedOnlineRevocationCheck();
  } else {
    // If SystemUsesChromiumEVMetadata is false, revocation checking will not
    // be enabled, and thus there will not be a revocation check to fail.
    return 0u;
  }
}

static bool SystemSupportsOCSP() {
#if defined(OS_ANDROID)
  // TODO(jnd): http://crbug.com/117478 - EV verification is not yet supported.
  return false;
#else
  return true;
#endif
}

static bool SystemSupportsOCSPStapling() {
#if defined(USE_NSS_CERTS) || defined(OS_WIN) || \
    defined(USE_BUILTIN_CERT_VERIFIER)
  return true;
#else
  return false;
#endif
}

TEST_F(HTTPSOCSPTest, Valid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));

  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, Revoked) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_REVOKED;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, Invalid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status =
      SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

#if defined(USE_BUILTIN_CERT_VERIFIER)
  // TODO(649017): This test uses soft-fail revocation checking, but returns an
  // invalid OCSP response (can't parse). CertVerifyProcBuiltin currently
  // doesn't consider this a candidate for soft-fail (only considers
  // network-level failures as skippable).
  EXPECT_EQ(CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
            cert_status & CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
#else
  EXPECT_EQ(ExpectedCertStatusForFailedOnlineRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);
#endif

  // Without a positive OCSP response, we shouldn't show the EV status.
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateValid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO_WITH_INTERMEDIATE);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.ocsp_intermediate_status = SpawnedTestServer::SSLOptions::OCSP_OK;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));

  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateResponseOldButStillValid) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO_WITH_INTERMEDIATE);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.ocsp_intermediate_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  // Use an OCSP response for the intermediate that would be too old for a leaf
  // cert, but is still valid for an intermediate.
  ssl_options.ocsp_intermediate_date =
      SpawnedTestServer::SSLOptions::OCSP_DATE_LONG;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));

  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateResponseTooOld) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO_WITH_INTERMEDIATE);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.ocsp_intermediate_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.ocsp_intermediate_date =
      SpawnedTestServer::SSLOptions::OCSP_DATE_LONGER;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

#if defined(USE_BUILTIN_CERT_VERIFIER)
  // The builtin verifier enforces the baseline requirements for max age of an
  // intermediate's OCSP response.
  EXPECT_EQ(CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
            cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(0u, cert_status & CERT_STATUS_IS_EV);
#else
  // The platform verifiers are more lenient.
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));
#endif
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, IntermediateRevoked) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO_WITH_INTERMEDIATE);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.ocsp_intermediate_status =
      SpawnedTestServer::SSLOptions::OCSP_REVOKED;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

#if defined(OS_WIN)
  // TODO(mattm): why does CertVerifyProcWin accept this?
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
#else
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
#endif
  EXPECT_EQ(0u, cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSOCSPTest, ValidStapled) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.staple_ocsp_response = true;
  ssl_options.ocsp_server_unavailable = true;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));

  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// Disabled on NSS ports. See https://crbug.com/431716.
#if defined(USE_NSS_CERTS)
#define MAYBE_RevokedStapled DISABLED_RevokedStapled
#else
#define MAYBE_RevokedStapled RevokedStapled
#endif
TEST_F(HTTPSOCSPTest, MAYBE_RevokedStapled) {
  if (!SystemSupportsOCSPStapling()) {
    LOG(WARNING)
        << "Skipping test because system doesn't support OCSP stapling";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_REVOKED;
  ssl_options.staple_ocsp_response = true;
  ssl_options.ocsp_server_unavailable = true;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

static const struct OCSPVerifyTestData {
  std::vector<SpawnedTestServer::SSLOptions::OCSPSingleResponse> ocsp_responses;
  SpawnedTestServer::SSLOptions::OCSPProduced ocsp_produced;
  OCSPVerifyResult::ResponseStatus response_status;
  bool has_revocation_status;
  OCSPRevocationStatus cert_status;
} kOCSPVerifyData[] = {
    // 0
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::GOOD},

    // 1
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_OLD}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 2
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_EARLY}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 3
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_LONG}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 4
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_LONG}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 5
    {{{SpawnedTestServer::SSLOptions::OCSP_TRY_LATER,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::ERROR_RESPONSE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 6
    {{{SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PARSE_RESPONSE_ERROR,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 7
    {{{SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE_DATA,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 8
    {{{SpawnedTestServer::SSLOptions::OCSP_REVOKED,
       SpawnedTestServer::SSLOptions::OCSP_DATE_EARLY}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 9
    {{{SpawnedTestServer::SSLOptions::OCSP_UNKNOWN,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::UNKNOWN},

    // 10
    {{{SpawnedTestServer::SSLOptions::OCSP_UNKNOWN,
       SpawnedTestServer::SSLOptions::OCSP_DATE_OLD}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 11
    {{{SpawnedTestServer::SSLOptions::OCSP_UNKNOWN,
       SpawnedTestServer::SSLOptions::OCSP_DATE_EARLY}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 12
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_BEFORE_CERT,
     OCSPVerifyResult::BAD_PRODUCED_AT,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 13
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_AFTER_CERT,
     OCSPVerifyResult::BAD_PRODUCED_AT,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 14
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_AFTER_CERT,
     OCSPVerifyResult::BAD_PRODUCED_AT,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 15
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::GOOD},

    // 16
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_OLD},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::GOOD},

    // 17
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_EARLY},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::GOOD},

    // 18
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_LONG},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::GOOD},

    // 19
    {{{SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_EARLY},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_OLD},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_LONG}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 20
    {{{SpawnedTestServer::SSLOptions::OCSP_UNKNOWN,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID},
      {SpawnedTestServer::SSLOptions::OCSP_REVOKED,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::REVOKED},

    // 21
    {{{SpawnedTestServer::SSLOptions::OCSP_UNKNOWN,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::UNKNOWN},

    // 22
    {{{SpawnedTestServer::SSLOptions::OCSP_UNKNOWN,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID},
      {SpawnedTestServer::SSLOptions::OCSP_REVOKED,
       SpawnedTestServer::SSLOptions::OCSP_DATE_LONG},
      {SpawnedTestServer::SSLOptions::OCSP_OK,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::UNKNOWN},

    // 23
    {{{SpawnedTestServer::SSLOptions::OCSP_MISMATCHED_SERIAL,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::NO_MATCHING_RESPONSE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 24
    {{{SpawnedTestServer::SSLOptions::OCSP_MISMATCHED_SERIAL,
       SpawnedTestServer::SSLOptions::OCSP_DATE_EARLY}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::NO_MATCHING_RESPONSE,
     false,
     OCSPRevocationStatus::UNKNOWN},

// These tests fail when using NSS for certificate verification, as NSS fails
// and doesn't return the partial path. As a result the OCSP checks being done
// at the CertVerifyProc layer cannot access the issuer certificate.
#if !defined(USE_NSS_CERTS)
    // 25
    {{{SpawnedTestServer::SSLOptions::OCSP_REVOKED,
       SpawnedTestServer::SSLOptions::OCSP_DATE_VALID}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::PROVIDED,
     true,
     OCSPRevocationStatus::REVOKED},

    // 26
    {{{SpawnedTestServer::SSLOptions::OCSP_REVOKED,
       SpawnedTestServer::SSLOptions::OCSP_DATE_OLD}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},

    // 27
    {{{SpawnedTestServer::SSLOptions::OCSP_REVOKED,
       SpawnedTestServer::SSLOptions::OCSP_DATE_LONG}},
     SpawnedTestServer::SSLOptions::OCSP_PRODUCED_VALID,
     OCSPVerifyResult::INVALID_DATE,
     false,
     OCSPRevocationStatus::UNKNOWN},
#endif
};

class HTTPSOCSPVerifyTest
    : public HTTPSOCSPTest,
      public testing::WithParamInterface<OCSPVerifyTestData> {};

TEST_P(HTTPSOCSPVerifyTest, VerifyResult) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  OCSPVerifyTestData test = GetParam();

  ssl_options.ocsp_responses = test.ocsp_responses;
  ssl_options.ocsp_produced = test.ocsp_produced;
  ssl_options.staple_ocsp_response = true;

  SSLInfo ssl_info;
  OCSPErrorTestDelegate delegate;
  ASSERT_NO_FATAL_FAILURE(
      DoConnectionWithDelegate(ssl_options, &delegate, &ssl_info));

  // The SSLInfo must be extracted from |delegate| on error, due to how
  // URLRequest caches certificate errors.
  if (delegate.have_certificate_errors()) {
    ASSERT_TRUE(delegate.on_ssl_certificate_error_called());
    ssl_info = delegate.ssl_info();
  }

  EXPECT_EQ(test.response_status, ssl_info.ocsp_result.response_status);

  if (test.has_revocation_status)
    EXPECT_EQ(test.cert_status, ssl_info.ocsp_result.revocation_status);
}

INSTANTIATE_TEST_CASE_P(OCSPVerify,
                        HTTPSOCSPVerifyTest,
                        testing::ValuesIn(kOCSPVerifyData));

class HTTPSAIATest : public HTTPSOCSPTest {
 public:
  CertVerifier::Config GetCertVerifierConfig() override {
    CertVerifier::Config config;
    return config;
  }
};

TEST_F(HTTPSAIATest, AIAFetching) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO_AIA_INTERMEDIATE);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  // Unmark the certificate's OID as EV, which will disable revocation
  // checking.
  ev_test_policy_.reset();

  TestDelegate d;
  d.set_allow_certificate_errors(true);
  std::unique_ptr<URLRequest> r(context_.CreateRequest(
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

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status =
      SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

#if defined(USE_BUILTIN_CERT_VERIFIER)
  // TODO(crbug.com/649017): Should we consider invalid response as
  //                         affirmatively revoked?
  EXPECT_EQ(CERT_STATUS_UNABLE_TO_CHECK_REVOCATION,
            cert_status & CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
#else
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_REVOKED);
#endif

  // Without a positive OCSP response, we shouldn't show the EV status.
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

class HTTPSEVCRLSetTest : public HTTPSOCSPTest {
 protected:
  CertVerifier::Config GetCertVerifierConfig() override {
    CertVerifier::Config config;
    return config;
  }
};

TEST_F(HTTPSEVCRLSetTest, MissingCRLSetAndInvalidOCSP) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status =
      SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(ExpectedCertStatusForFailedOnlineEVRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, MissingCRLSetAndRevokedOCSP) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_REVOKED;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

// Currently only works for Windows and OS X. When using NSS, it's not
// possible to determine whether the check failed because of actual
// revocation or because there was an OCSP failure.
#if defined(OS_WIN) || defined(OS_MACOSX)
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
#else
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
#endif

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, MissingCRLSetAndGoodOCSP) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, ExpiredCRLSet) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status =
      SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE;
  CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
  cert_verifier_config.crl_set = CRLSet::ExpiredCRLSetForTesting();
  context_.cert_verifier()->SetConfig(cert_verifier_config);

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(ExpectedCertStatusForFailedOnlineEVRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, FreshCRLSetCovered) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status =
      SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE;
  CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
  cert_verifier_config.crl_set =
      CRLSet::ForTesting(false, &kOCSPTestCertSPKI, "", "", {});
  context_.cert_verifier()->SetConfig(cert_verifier_config);

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  // With a fresh CRLSet that covers the issuing certificate, we shouldn't do a
  // revocation check for EV.
  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_IS_EV));
  EXPECT_FALSE(
      static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

TEST_F(HTTPSEVCRLSetTest, FreshCRLSetNotCovered) {
  if (!SystemSupportsOCSP()) {
    LOG(WARNING) << "Skipping test because system doesn't support OCSP";
    return;
  }

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status =
      SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE;
  CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
  cert_verifier_config.crl_set = CRLSet::EmptyCRLSetForTesting();
  context_.cert_verifier()->SetConfig(cert_verifier_config);

  CertStatus cert_status = 0;
  DoConnection(ssl_options, &cert_status);

  // Even with a fresh CRLSet, we should still do online revocation checks when
  // the certificate chain isn't covered by the CRLSet, which it isn't in this
  // test.
  EXPECT_EQ(ExpectedCertStatusForFailedOnlineEVRevocationCheck(),
            cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_EQ(SystemUsesChromiumEVMetadata(),
            static_cast<bool>(cert_status & CERT_STATUS_REV_CHECKING_ENABLED));
}

class HTTPSCRLSetTest : public HTTPSOCSPTest {
 protected:
  CertVerifier::Config GetCertVerifierConfig() override {
    CertVerifier::Config config;
    return config;
  }

  void SetUp() override {
    HTTPSOCSPTest::SetUp();

    // Unmark the certificate's OID as EV, which should disable revocation
    // checking (as per the user preference).
    ev_test_policy_.reset();
  }
};

TEST_F(HTTPSCRLSetTest, ExpiredCRLSet) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status =
      SpawnedTestServer::SSLOptions::OCSP_INVALID_RESPONSE;
  CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
  cert_verifier_config.crl_set = CRLSet::ExpiredCRLSetForTesting();
  context_.cert_verifier()->SetConfig(cert_verifier_config);

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

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

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_REVOKED;

  CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
  cert_verifier_config.crl_set = CRLSet::ExpiredCRLSetForTesting();
  context_.cert_verifier()->SetConfig(cert_verifier_config);

  CertStatus cert_status;
  DoConnection(ssl_options, &cert_status);

  EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);

  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSCRLSetTest, CRLSetRevoked) {
#if defined(OS_ANDROID)
  LOG(WARNING) << "Skipping test because system doesn't support CRLSets";
  return;
#endif

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  ssl_options.cert_serial = 10;

  CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
  cert_verifier_config.crl_set =
      CRLSet::ForTesting(false, &kOCSPTestCertSPKI, "\x0a", "", {});
  context_.cert_verifier()->SetConfig(cert_verifier_config);

  CertStatus cert_status = 0;
  DoConnection(ssl_options, &cert_status);

  // If the certificate is recorded as revoked in the CRLSet, that should be
  // reflected without online revocation checking.
  EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
  EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_F(HTTPSCRLSetTest, CRLSetRevokedBySubject) {
#if defined(OS_ANDROID)
  LOG(WARNING) << "Skipping test because system doesn't support CRLSets";
  return;
#endif

  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = SpawnedTestServer::SSLOptions::OCSP_OK;
  static const char kCommonName[] = "Test CN";
  ssl_options.cert_common_name = kCommonName;

  {
    CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
    cert_verifier_config.crl_set =
        CRLSet::ForTesting(false, nullptr, "", kCommonName, {});
    context_.cert_verifier()->SetConfig(cert_verifier_config);

    CertStatus cert_status = 0;
    DoConnection(ssl_options, &cert_status);

    // If the certificate is recorded as revoked in the CRLSet, that should be
    // reflected without online revocation checking.
    EXPECT_EQ(CERT_STATUS_REVOKED, cert_status & CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(cert_status & CERT_STATUS_IS_EV);
    EXPECT_FALSE(cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
  }

  const uint8_t kTestServerSPKISHA256[32] = {
      0xb3, 0x91, 0xac, 0x73, 0x32, 0x54, 0x7f, 0x7b, 0x8a, 0x62, 0x77,
      0x73, 0x1d, 0x45, 0x7b, 0x23, 0x46, 0x69, 0xef, 0x6f, 0x05, 0x3d,
      0x07, 0x22, 0x15, 0x18, 0xd6, 0x10, 0x8b, 0xa1, 0x49, 0x33,
  };
  const std::string spki_hash(
      reinterpret_cast<const char*>(kTestServerSPKISHA256),
      sizeof(kTestServerSPKISHA256));

  {
    CertVerifier::Config cert_verifier_config = GetCertVerifierConfig();
    cert_verifier_config.crl_set =
        CRLSet::ForTesting(false, nullptr, "", kCommonName, {spki_hash});
    context_.cert_verifier()->SetConfig(cert_verifier_config);

    CertStatus cert_status = 0;
    DoConnection(ssl_options, &cert_status);

    // When the correct SPKI hash is specified, the connection should succeed
    // even though the subject is listed in the CRLSet.
    EXPECT_EQ(0u, cert_status & CERT_STATUS_ALL_ERRORS);
  }
}
#endif  // !defined(OS_IOS)

#if !BUILDFLAG(DISABLE_FTP_SUPPORT) && !defined(OS_ANDROID) && \
    !defined(OS_FUCHSIA)
// FTP uses a second TCP connection with the port number allocated dynamically
// on the server side, so it would be hard to make RemoteTestServer proxy FTP
// connections reliably. FTP tests are disabled on platforms that use
// RemoteTestServer. See http://crbug.com/495220
class URLRequestTestFTP : public URLRequestTest {
 public:
  URLRequestTestFTP()
      : ftp_test_server_(SpawnedTestServer::TYPE_FTP,
                         base::FilePath(kTestFilePath)) {
    // Can't use |default_context_|'s HostResolver to set up the
    // FTPTransactionFactory because it hasn't been created yet.
    default_context().set_host_resolver(&host_resolver_);
  }

  // URLRequestTest interface:
  void SetUpFactory() override {
    // Add FTP support to the default URLRequestContext.
    job_factory_impl_->SetProtocolHandler(
        "ftp", FtpProtocolHandler::Create(&host_resolver_));
  }

  std::string GetTestFileContents() {
    base::FilePath path;
    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &path));
    path = path.Append(kTestFilePath);
    path = path.AppendASCII(kFtpTestFile);
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    return contents;
  }

 protected:
  MockHostResolver host_resolver_;

  SpawnedTestServer ftp_test_server_;
};

// Make sure an FTP request using an unsafe ports fails.
TEST_F(URLRequestTestFTP, UnsafePort) {
  GURL url("ftp://127.0.0.1:7");

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(ERR_UNSAFE_PORT, d.request_status());
  }
}

TEST_F(URLRequestTestFTP, FTPDirectoryListing) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURL("/"), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_LT(0, d.bytes_received());
    EXPECT_EQ(ftp_test_server_.host_port_pair().host(),
              r->GetSocketAddress().host());
    EXPECT_EQ(ftp_test_server_.host_port_pair().port(),
              r->GetSocketAddress().port());
  }
}

TEST_F(URLRequestTestFTP, FTPGetTestAnonymous) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURL(kFtpTestFile), DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d.data_received());
    EXPECT_EQ(ftp_test_server_.host_port_pair().host(),
              r->GetSocketAddress().host());
    EXPECT_EQ(ftp_test_server_.host_port_pair().port(),
              r->GetSocketAddress().port());
  }
}

TEST_F(URLRequestTestFTP, FTPGetTest) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURLWithUserAndPassword(kFtpTestFile, "chrome",
                                                   "chrome"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d.data_received());
    EXPECT_EQ(ftp_test_server_.host_port_pair().host(),
              r->GetSocketAddress().host());
    EXPECT_EQ(ftp_test_server_.host_port_pair().port(),
              r->GetSocketAddress().port());

    LoadTimingInfo load_timing_info;
    r->GetLoadTimingInfo(&load_timing_info);
    TestLoadTimingNoHttpResponse(load_timing_info);
  }
}

TEST_F(URLRequestTestFTP, FTPCheckWrongPassword) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURLWithUserAndPassword(kFtpTestFile, "chrome",
                                                   "wrong_password"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
}

TEST_F(URLRequestTestFTP, FTPCheckWrongPasswordRestart) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d.set_credentials(AuthCredentials(kChrome, kChrome));
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURLWithUserAndPassword(kFtpTestFile, "chrome",
                                                   "wrong_password"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d.data_received());
  }
}

TEST_F(URLRequestTestFTP, FTPCheckWrongUser) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURLWithUserAndPassword(kFtpTestFile, "wrong_user",
                                                   "chrome"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(0, d.bytes_received());
  }
}

TEST_F(URLRequestTestFTP, FTPCheckWrongUserRestart) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d.set_credentials(AuthCredentials(kChrome, kChrome));
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURLWithUserAndPassword(kFtpTestFile, "wrong_user",
                                                   "chrome"),
        DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d.data_received());
  }
}

TEST_F(URLRequestTestFTP, FTPCacheURLCredentials) {
  ASSERT_TRUE(ftp_test_server_.Start());

  std::unique_ptr<TestDelegate> d(new TestDelegate);
  {
    // Pass correct login identity in the URL.
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURLWithUserAndPassword(kFtpTestFile, "chrome",
                                                   "chrome"),
        DEFAULT_PRIORITY, d.get(), TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d->RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d->data_received());
  }

  d.reset(new TestDelegate);
  {
    // This request should use cached identity from previous request.
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURL(kFtpTestFile), DEFAULT_PRIORITY, d.get(),
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d->RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d->data_received());
  }
}

TEST_F(URLRequestTestFTP, FTPCacheLoginBoxCredentials) {
  ASSERT_TRUE(ftp_test_server_.Start());

  std::unique_ptr<TestDelegate> d(new TestDelegate);
  // Set correct login credentials. The delegate will be asked for them when
  // the initial login with wrong credentials will fail.
  d->set_credentials(AuthCredentials(kChrome, kChrome));
  {
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURLWithUserAndPassword(kFtpTestFile, "chrome",
                                                   "wrong_password"),
        DEFAULT_PRIORITY, d.get(), TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d->RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d->data_received());
  }

  // Use a new delegate without explicit credentials. The cached ones should be
  // used.
  d.reset(new TestDelegate);
  {
    // Don't pass wrong credentials in the URL, they would override valid cached
    // ones.
    std::unique_ptr<URLRequest> r(default_context().CreateRequest(
        ftp_test_server_.GetURL(kFtpTestFile), DEFAULT_PRIORITY, d.get(),
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d->RunUntilComplete();

    EXPECT_FALSE(r->is_pending());
    EXPECT_EQ(1, d->response_started_count());
    EXPECT_FALSE(d->received_data_before_response());
    EXPECT_EQ(GetTestFileContents(), d->data_received());
  }
}

TEST_F(URLRequestTestFTP, RawBodyBytes) {
  ASSERT_TRUE(ftp_test_server_.Start());

  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      ftp_test_server_.GetURL("simple.html"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(6, req->GetRawBodyBytes());
}

#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

TEST_F(URLRequestTest, NetworkAccessedClearOnDataRequest) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("data:,"), DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_FALSE(req->response_info().network_accessed);

  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ(1, default_network_delegate_.completed_requests());
  EXPECT_FALSE(req->response_info().network_accessed);
}

TEST_F(URLRequestTest, NetworkAccessedSetOnHostResolutionFailure) {
  MockHostResolver host_resolver;
  TestNetworkDelegate network_delegate;  // Must outlive URLRequest.
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_host_resolver(&host_resolver);
  host_resolver.rules()->AddSimulatedFailure("*");
  context.Init();

  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context.CreateRequest(GURL("http://test_intercept/foo"), DEFAULT_PRIORITY,
                            &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_FALSE(req->response_info().network_accessed);

  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(req->response_info().network_accessed);
}

// Test that URLRequest is canceled correctly.
// See http://crbug.com/508900
TEST_F(URLRequestTest, URLRequestRedirectJobCancelRequest) {
  TestDelegate d;
  std::unique_ptr<URLRequest> req(default_context().CreateRequest(
      GURL("http://not-a-real-domain/"), DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  std::unique_ptr<URLRequestRedirectJob> job(new URLRequestRedirectJob(
      req.get(), &default_network_delegate_,
      GURL("http://this-should-never-be-navigated-to/"),
      URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT, "Jumbo shrimp"));
  AddTestInterceptor()->set_main_intercept_job(std::move(job));

  req->Start();
  req->Cancel();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ERR_ABORTED, d.request_status());
  EXPECT_EQ(0, d.received_redirect_count());
}

TEST_F(URLRequestTestHTTP, HeadersCallbacks) {
  ASSERT_TRUE(http_test_server()->Start());
  TestURLRequestContext context;
  GURL url(http_test_server()->GetURL("/cachetime"));
  TestDelegate delegate;
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");

  {
    HttpRawRequestHeaders raw_req_headers;
    scoped_refptr<const HttpResponseHeaders> raw_resp_headers;

    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetExtraRequestHeaders(extra_headers);
    r->SetRequestHeadersCallback(base::Bind(
        &HttpRawRequestHeaders::Assign, base::Unretained(&raw_req_headers)));
    r->SetResponseHeadersCallback(base::Bind(
        [](scoped_refptr<const HttpResponseHeaders>* left,
           scoped_refptr<const HttpResponseHeaders> right) { *left = right; },
        base::Unretained(&raw_resp_headers)));
    r->Start();
    while (!delegate.response_started_count())
      base::RunLoop().RunUntilIdle();
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
    std::unique_ptr<URLRequest> r(context.CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetExtraRequestHeaders(extra_headers);
    r->SetRequestHeadersCallback(base::Bind([](HttpRawRequestHeaders) {
      FAIL() << "Callback should not be called unless request is sent";
    }));
    r->SetResponseHeadersCallback(
        base::Bind([](scoped_refptr<const HttpResponseHeaders>) {
          FAIL() << "Callback should not be called unless request is sent";
        }));
    r->Start();
    delegate.RunUntilComplete();
    EXPECT_TRUE(r->was_cached());
  }
}

TEST_F(URLRequestTestHTTP, HeadersCallbacksWithRedirect) {
  ASSERT_TRUE(http_test_server()->Start());
  HttpRawRequestHeaders raw_req_headers;
  scoped_refptr<const HttpResponseHeaders> raw_resp_headers;

  TestURLRequestContext context;
  TestDelegate delegate;
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");
  GURL url(http_test_server()->GetURL("/redirect-test.html"));
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->SetExtraRequestHeaders(extra_headers);
  r->SetRequestHeadersCallback(base::Bind(&HttpRawRequestHeaders::Assign,
                                          base::Unretained(&raw_req_headers)));
  r->SetResponseHeadersCallback(base::Bind(
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
  r->FollowDeferredRedirect(base::nullopt /* modified_request_headers */);
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
  r->SetRequestHeadersCallback(base::Bind([](net::HttpRawRequestHeaders) {
    FAIL() << "Callback should not be called unless request is sent";
  }));
  r->SetResponseHeadersCallback(
      base::Bind([](scoped_refptr<const net::HttpResponseHeaders>) {
        FAIL() << "Callback should not be called unless request is sent";
      }));
  r->Start();
  request_delegate.RunUntilComplete();
  EXPECT_FALSE(r->is_pending());
}

TEST_F(URLRequestTestHTTP, HeadersCallbacksAuthRetry) {
  ASSERT_TRUE(http_test_server()->Start());
  GURL url(http_test_server()->GetURL("/auth-basic"));

  TestURLRequestContext context;
  TestDelegate delegate;

  delegate.set_credentials(AuthCredentials(kUser, kSecret));
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");

  using ReqHeadersVector = std::vector<std::unique_ptr<HttpRawRequestHeaders>>;
  ReqHeadersVector raw_req_headers;

  using RespHeadersVector =
      std::vector<scoped_refptr<const HttpResponseHeaders>>;
  RespHeadersVector raw_resp_headers;

  auto req_headers_callback = base::Bind(
      [](ReqHeadersVector* vec, HttpRawRequestHeaders headers) {
        vec->emplace_back(new HttpRawRequestHeaders(std::move(headers)));
      },
      &raw_req_headers);
  auto resp_headers_callback = base::Bind(
      [](RespHeadersVector* vec,
         scoped_refptr<const HttpResponseHeaders> headers) {
        vec->push_back(headers);
      },
      &raw_resp_headers);
  std::unique_ptr<URLRequest> r(context.CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->SetExtraRequestHeaders(extra_headers);
  r->SetRequestHeadersCallback(req_headers_callback);
  r->SetResponseHeadersCallback(resp_headers_callback);
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

  std::unique_ptr<URLRequest> r2(context.CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  r2->SetExtraRequestHeaders(extra_headers);
  r2->SetRequestHeadersCallback(req_headers_callback);
  r2->SetResponseHeadersCallback(resp_headers_callback);
  r2->SetLoadFlags(LOAD_VALIDATE_CACHE);
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

TEST_F(URLRequestTest, HeadersCallbacksNonHTTP) {
  GURL data_url("data:text/html,<html><body>Hello!</body></html>");
  TestDelegate d;
  std::unique_ptr<URLRequest> r(default_context().CreateRequest(
      data_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  r->SetRequestHeadersCallback(base::Bind([](net::HttpRawRequestHeaders) {
    FAIL() << "Callback should not be called for non-HTTP schemes";
  }));
  r->SetResponseHeadersCallback(
      base::Bind([](scoped_refptr<const net::HttpResponseHeaders>) {
        FAIL() << "Callback should not be called for non-HTTP schemes";
      }));
  r->Start();
  d.RunUntilComplete();
  EXPECT_FALSE(r->is_pending());
}

TEST_F(URLRequestTest, UpgradeIfInsecureFlagSet) {
  TestDelegate d;
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test");
  network_delegate.set_redirect_url(kRedirectUrl);
  TestURLRequestContext context(true /* delay_initialization */);
  context.set_network_delegate(&network_delegate);
  context.Init();

  std::unique_ptr<URLRequest> r(context.CreateRequest(
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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test:80");
  network_delegate.set_redirect_url(kRedirectUrl);
  TestURLRequestContext context(true /* delay_initialization */);
  context.set_network_delegate(&network_delegate);
  context.Init();

  std::unique_ptr<URLRequest> r(context.CreateRequest(
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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test:1234");
  network_delegate.set_redirect_url(kRedirectUrl);
  TestURLRequestContext context(true /* delay_initialization */);
  context.set_network_delegate(&network_delegate);
  context.Init();

  std::unique_ptr<URLRequest> r(context.CreateRequest(
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
  BlockingNetworkDelegate network_delegate(
      BlockingNetworkDelegate::SYNCHRONOUS);
  const GURL kOriginalUrl("https://original.test");
  const GURL kRedirectUrl("http://redirect.test");
  network_delegate.set_redirect_url(kRedirectUrl);
  TestURLRequestContext context(true /* delay_initialization */);
  context.set_network_delegate(&network_delegate);
  context.Init();
  std::unique_ptr<URLRequest> r(context.CreateRequest(
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
#if defined(OS_ANDROID)
TEST_F(URLRequestTestHTTP, TestTagging) {
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

}  // namespace net
