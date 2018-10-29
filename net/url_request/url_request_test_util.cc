// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_test_util.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/host_port_pair.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// These constants put the NetworkDelegate events of TestNetworkDelegate
// into an order. They are used in conjunction with
// |TestNetworkDelegate::next_states_| to check that we do not send
// events in the wrong order.
const int kStageBeforeURLRequest = 1 << 0;
const int kStageBeforeStartTransaction = 1 << 1;
const int kStageStartTransaction = 1 << 2;
const int kStageHeadersReceived = 1 << 3;
const int kStageAuthRequired = 1 << 4;
const int kStageBeforeRedirect = 1 << 5;
const int kStageResponseStarted = 1 << 6;
const int kStageCompletedSuccess = 1 << 7;
const int kStageCompletedError = 1 << 8;
const int kStageURLRequestDestroyed = 1 << 9;
const int kStageDestruction = 1 << 10;

}  // namespace

TestURLRequestContext::TestURLRequestContext() : TestURLRequestContext(false) {}

TestURLRequestContext::TestURLRequestContext(bool delay_initialization)
    : context_storage_(this) {
  if (!delay_initialization)
    Init();
}

TestURLRequestContext::~TestURLRequestContext() {
  DCHECK(initialized_);
  AssertNoURLRequests();
}

void TestURLRequestContext::Init() {
  DCHECK(!initialized_);
  initialized_ = true;

  if (!host_resolver())
    context_storage_.set_host_resolver(
        std::unique_ptr<HostResolver>(new MockCachingHostResolver()));
  if (!proxy_resolution_service())
    context_storage_.set_proxy_resolution_service(ProxyResolutionService::CreateDirect());
  if (!cert_verifier())
    context_storage_.set_cert_verifier(CertVerifier::CreateDefault());
  if (!transport_security_state()) {
    context_storage_.set_transport_security_state(
        std::make_unique<TransportSecurityState>());
  }
  if (!cert_transparency_verifier()) {
    context_storage_.set_cert_transparency_verifier(
        std::make_unique<DoNothingCTVerifier>());
  }
  if (!ct_policy_enforcer()) {
    context_storage_.set_ct_policy_enforcer(
        std::make_unique<DefaultCTPolicyEnforcer>());
  }
  if (!ssl_config_service()) {
    context_storage_.set_ssl_config_service(
        std::make_unique<SSLConfigServiceDefaults>());
  }
  if (!http_auth_handler_factory()) {
    context_storage_.set_http_auth_handler_factory(
        HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  }
  if (!http_server_properties()) {
    context_storage_.set_http_server_properties(
        std::unique_ptr<HttpServerProperties>(new HttpServerPropertiesImpl()));
  }
  // In-memory cookie store.
  if (!cookie_store()) {
    context_storage_.set_cookie_store(std::make_unique<CookieMonster>(
        nullptr /* store */, nullptr /* channel_id_service */,
        nullptr /* netlog */));
  }

  // In-memory Channel ID service.  Must be created before the
  // HttpNetworkSession.
  if (!channel_id_service()) {
    context_storage_.set_channel_id_service(
        std::make_unique<ChannelIDService>(new DefaultChannelIDStore(nullptr)));
  }
  if (http_transaction_factory()) {
    // Make sure we haven't been passed an object we're not going to use.
    EXPECT_FALSE(client_socket_factory_);
  } else {
    HttpNetworkSession::Params session_params;
    if (http_network_session_params_)
      session_params = *http_network_session_params_;

    HttpNetworkSession::Context session_context;
    if (http_network_session_context_)
      session_context = *http_network_session_context_;
    session_context.client_socket_factory = client_socket_factory();
    session_context.host_resolver = host_resolver();
    session_context.cert_verifier = cert_verifier();
    session_context.cert_transparency_verifier = cert_transparency_verifier();
    session_context.ct_policy_enforcer = ct_policy_enforcer();
    session_context.transport_security_state = transport_security_state();
    session_context.proxy_resolution_service = proxy_resolution_service();
    session_context.ssl_config_service = ssl_config_service();
    session_context.http_auth_handler_factory = http_auth_handler_factory();
    session_context.http_server_properties = http_server_properties();
    session_context.net_log = net_log();
    session_context.channel_id_service = channel_id_service();
    context_storage_.set_http_network_session(
        std::make_unique<HttpNetworkSession>(session_params, session_context));
    context_storage_.set_http_transaction_factory(std::make_unique<HttpCache>(
        context_storage_.http_network_session(),
        HttpCache::DefaultBackend::InMemory(0), true /* is_main_cache */));
  }
  if (!http_user_agent_settings() && create_default_http_user_agent_settings_) {
    context_storage_.set_http_user_agent_settings(
        std::make_unique<StaticHttpUserAgentSettings>("en-us,fr",
                                                      std::string()));
  }
  if (!job_factory()) {
    context_storage_.set_job_factory(
        std::make_unique<URLRequestJobFactoryImpl>());
  }
}

TestURLRequestContextGetter::TestURLRequestContextGetter(
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : network_task_runner_(network_task_runner) {
  DCHECK(network_task_runner_.get());
}

TestURLRequestContextGetter::TestURLRequestContextGetter(
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
    std::unique_ptr<TestURLRequestContext> context)
    : network_task_runner_(network_task_runner), context_(std::move(context)) {
  DCHECK(network_task_runner_.get());
}

TestURLRequestContextGetter::~TestURLRequestContextGetter() = default;

TestURLRequestContext* TestURLRequestContextGetter::GetURLRequestContext() {
  if (is_shut_down_)
    return nullptr;

  if (!context_.get())
    context_.reset(new TestURLRequestContext);
  return context_.get();
}

void TestURLRequestContextGetter::NotifyContextShuttingDown() {
  // This should happen before call to base NotifyContextShuttingDown() per that
  // method's doc comments.
  is_shut_down_ = true;

  URLRequestContextGetter::NotifyContextShuttingDown();
  context_ = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
TestURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

const int TestDelegate::kBufferSize;

TestDelegate::TestDelegate()
    : buf_(base::MakeRefCounted<IOBuffer>(kBufferSize)) {}

TestDelegate::~TestDelegate() = default;

void TestDelegate::RunUntilComplete() {
  use_legacy_on_complete_ = false;
  base::RunLoop run_loop;
  on_complete_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestDelegate::RunUntilRedirect() {
  use_legacy_on_complete_ = false;
  base::RunLoop run_loop;
  on_redirect_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestDelegate::RunUntilAuthRequired() {
  use_legacy_on_complete_ = false;
  base::RunLoop run_loop;
  on_auth_required_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestDelegate::ClearFullRequestHeaders() {
  full_request_headers_.Clear();
  have_full_request_headers_ = false;
}

void TestDelegate::OnReceivedRedirect(URLRequest* request,
                                      const RedirectInfo& redirect_info,
                                      bool* defer_redirect) {
  EXPECT_TRUE(request->is_redirecting());

  redirect_info_ = redirect_info;

  have_full_request_headers_ =
      request->GetFullRequestHeaders(&full_request_headers_);

  received_redirect_count_++;
  if (on_redirect_) {
    *defer_redirect = true;
    std::move(on_redirect_).Run();
  } else if (cancel_in_rr_) {
    request->Cancel();
  }
}

void TestDelegate::OnAuthRequired(URLRequest* request,
                                  AuthChallengeInfo* auth_info) {
  auth_required_ = true;
  if (on_auth_required_) {
    std::move(on_auth_required_).Run();
    return;
  }
  if (!credentials_.Empty()) {
    request->SetAuth(credentials_);
  } else {
    request->CancelAuth();
  }
}

void TestDelegate::OnSSLCertificateError(URLRequest* request,
                                         const SSLInfo& ssl_info,
                                         bool fatal) {
  // The caller can control whether it needs all SSL requests to go through,
  // independent of any possible errors, or whether it wants SSL errors to
  // cancel the request.
  have_certificate_errors_ = true;
  certificate_errors_are_fatal_ = fatal;
  if (allow_certificate_errors_)
    request->ContinueDespiteLastError();
  else
    request->Cancel();
}

void TestDelegate::OnResponseStarted(URLRequest* request, int net_error) {
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK_NE(ERR_IO_PENDING, net_error);
  EXPECT_FALSE(request->is_redirecting());

  have_full_request_headers_ =
      request->GetFullRequestHeaders(&full_request_headers_);

  response_started_count_++;
  request_status_ = net_error;
  if (cancel_in_rs_) {
    request_status_ = request->Cancel();
    // Canceling |request| will cause OnResponseCompleted() to be called.
  } else if (net_error != OK) {
    request_failed_ = true;
    OnResponseCompleted(request);
  } else {
    // Initiate the first read.
    int bytes_read = request->Read(buf_.get(), kBufferSize);
    if (bytes_read >= 0)
      OnReadCompleted(request, bytes_read);
    else if (bytes_read != ERR_IO_PENDING)
      OnResponseCompleted(request);
  }
}

void TestDelegate::OnReadCompleted(URLRequest* request, int bytes_read) {
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK_NE(bytes_read, ERR_IO_PENDING);

  // If the request was cancelled in a redirect, it should not signal
  // OnReadCompleted. Note that |cancel_in_rs_| may be true due to
  // https://crbug.com/564848.
  EXPECT_FALSE(cancel_in_rr_);

  if (response_started_count_ == 0)
    received_data_before_response_ = true;

  if (bytes_read >= 0) {
    // There is data to read.
    received_bytes_count_ += bytes_read;

    // Consume the data.
    data_received_.append(buf_->data(), bytes_read);

    if (cancel_in_rd_) {
      request_status_ = request->Cancel();
      // If bytes_read is 0, won't get a notification on cancelation.
      if (bytes_read == 0) {
        if (use_legacy_on_complete_)
          base::RunLoop::QuitCurrentWhenIdleDeprecated();
        else
          std::move(on_complete_).Run();
      }
      return;
    }
  }

  // If it was not end of stream, request to read more.
  while (bytes_read > 0) {
    bytes_read = request->Read(buf_.get(), kBufferSize);
    if (bytes_read > 0) {
      data_received_.append(buf_->data(), bytes_read);
      received_bytes_count_ += bytes_read;
    }
  }

  request_status_ = bytes_read;
  if (request_status_ != ERR_IO_PENDING)
    OnResponseCompleted(request);
  else if (cancel_in_rd_pending_)
    request_status_ = request->Cancel();
}

void TestDelegate::OnResponseCompleted(URLRequest* request) {
  response_completed_ = true;
  if (use_legacy_on_complete_)
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  else
    std::move(on_complete_).Run();
}

TestNetworkDelegate::TestNetworkDelegate()
    : last_error_(0),
      error_count_(0),
      created_requests_(0),
      destroyed_requests_(0),
      completed_requests_(0),
      canceled_requests_(0),
      cookie_options_bit_mask_(0),
      blocked_get_cookies_count_(0),
      blocked_set_cookie_count_(0),
      set_cookie_count_(0),
      before_send_headers_with_proxy_count_(0),
      before_start_transaction_count_(0),
      headers_received_count_(0),
      total_network_bytes_received_(0),
      total_network_bytes_sent_(0),
      has_load_timing_info_before_redirect_(false),
      has_load_timing_info_before_auth_(false),
      can_access_files_(true),
      experimental_cookie_features_enabled_(false),
      cancel_request_with_policy_violating_referrer_(false),
      will_be_intercepted_on_next_error_(false),
      before_start_transaction_fails_(false),
      add_header_to_first_response_(false) {}

TestNetworkDelegate::~TestNetworkDelegate() {
  for (auto i = next_states_.begin(); i != next_states_.end(); ++i) {
    event_order_[i->first] += "~TestNetworkDelegate\n";
    EXPECT_TRUE(i->second & kStageDestruction) << event_order_[i->first];
  }
}

bool TestNetworkDelegate::GetLoadTimingInfoBeforeRedirect(
    LoadTimingInfo* load_timing_info_before_redirect) const {
  *load_timing_info_before_redirect = load_timing_info_before_redirect_;
  return has_load_timing_info_before_redirect_;
}

bool TestNetworkDelegate::GetLoadTimingInfoBeforeAuth(
    LoadTimingInfo* load_timing_info_before_auth) const {
  *load_timing_info_before_auth = load_timing_info_before_auth_;
  return has_load_timing_info_before_auth_;
}

void TestNetworkDelegate::InitRequestStatesIfNew(int request_id) {
  if (next_states_.find(request_id) == next_states_.end()) {
    // TODO(davidben): Although the URLRequest documentation does not allow
    // calling Cancel() before Start(), the ResourceLoader does so. URLRequest's
    // destructor also calls Cancel. Either officially support this or fix the
    // ResourceLoader code.
    next_states_[request_id] = kStageBeforeURLRequest | kStageCompletedError;
    event_order_[request_id] = "";
  }
}

int TestNetworkDelegate::OnBeforeURLRequest(URLRequest* request,
                                            CompletionOnceCallback callback,
                                            GURL* new_url) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeURLRequest\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeURLRequest) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeStartTransaction |
      kStageResponseStarted |  // data: URLs do not trigger sending headers
      kStageBeforeRedirect |   // a delegate can trigger a redirection
      kStageCompletedError |   // request canceled by delegate
      kStageAuthRequired;      // Auth can come next for FTP requests
  created_requests_++;
  return OK;
}

int TestNetworkDelegate::OnBeforeStartTransaction(
    URLRequest* request,
    CompletionOnceCallback callback,
    HttpRequestHeaders* headers) {
  if (before_start_transaction_fails_)
    return ERR_FAILED;

  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeStartTransaction\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeStartTransaction)
      << event_order_[req_id];
  next_states_[req_id] = kStageStartTransaction |
                         kStageCompletedError;  // request canceled by delegate
  before_start_transaction_count_++;
  return OK;
}

void TestNetworkDelegate::OnBeforeSendHeaders(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    const ProxyRetryInfoMap& proxy_retry_info,
    HttpRequestHeaders* headers) {
  if (!proxy_info.is_http() && !proxy_info.is_https() && !proxy_info.is_quic())
    return;
  if (!request || request->url().SchemeIs("https") ||
      request->url().SchemeIsWSOrWSS()) {
    return;
  }
  ++before_send_headers_with_proxy_count_;
  last_observed_proxy_ = proxy_info.proxy_server().host_port_pair();
}

void TestNetworkDelegate::OnStartTransaction(
    URLRequest* request,
    const HttpRequestHeaders& headers) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnStartTransaction\n";
  EXPECT_TRUE(next_states_[req_id] & kStageStartTransaction)
      << event_order_[req_id];
  if (!will_be_intercepted_on_next_error_)
    next_states_[req_id] = kStageHeadersReceived | kStageCompletedError;
  else
    next_states_[req_id] = kStageResponseStarted;
  will_be_intercepted_on_next_error_ = false;
}

int TestNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  int req_id = request->identifier();
  bool is_first_response =
      event_order_[req_id].find("OnHeadersReceived\n") == std::string::npos;
  event_order_[req_id] += "OnHeadersReceived\n";
  InitRequestStatesIfNew(req_id);
  EXPECT_TRUE(next_states_[req_id] & kStageHeadersReceived) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeRedirect |
      kStageResponseStarted |
      kStageAuthRequired |
      kStageCompletedError;  // e.g. proxy resolution problem

  // Basic authentication sends a second request from the URLRequestHttpJob
  // layer before the URLRequest reports that a response has started.
  next_states_[req_id] |= kStageBeforeStartTransaction;

  if (!redirect_on_headers_received_url_.is_empty()) {
    *override_response_headers =
        new HttpResponseHeaders(original_response_headers->raw_headers());
    (*override_response_headers)->ReplaceStatusLine("HTTP/1.1 302 Found");
    (*override_response_headers)->RemoveHeader("Location");
    (*override_response_headers)->AddHeader(
        "Location: " + redirect_on_headers_received_url_.spec());

    redirect_on_headers_received_url_ = GURL();

    if (!allowed_unsafe_redirect_url_.is_empty())
      *allowed_unsafe_redirect_url = allowed_unsafe_redirect_url_;
  } else if (add_header_to_first_response_ && is_first_response) {
    *override_response_headers =
        new HttpResponseHeaders(original_response_headers->raw_headers());
    (*override_response_headers)
        ->AddHeader("X-Network-Delegate: Greetings, planet");
  }

  headers_received_count_++;
  return OK;
}

void TestNetworkDelegate::OnBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {
  load_timing_info_before_redirect_ = LoadTimingInfo();
  request->GetLoadTimingInfo(&load_timing_info_before_redirect_);
  has_load_timing_info_before_redirect_ = true;
  EXPECT_FALSE(load_timing_info_before_redirect_.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info_before_redirect_.request_start.is_null());

  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeRedirect\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeRedirect) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeURLRequest |        // HTTP redirects trigger this.
      kStageBeforeStartTransaction |  // Redirects from the network delegate do
                                      // not
                                      // trigger onBeforeURLRequest.
      kStageCompletedError;

  // A redirect can lead to a file or a data URL. In this case, we do not send
  // headers.
  next_states_[req_id] |= kStageResponseStarted;
}

void TestNetworkDelegate::OnResponseStarted(URLRequest* request,
                                            int net_error) {
  DCHECK_NE(ERR_IO_PENDING, net_error);

  LoadTimingInfo load_timing_info;
  request->GetLoadTimingInfo(&load_timing_info);
  EXPECT_FALSE(load_timing_info.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info.request_start.is_null());

  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnResponseStarted\n";
  EXPECT_TRUE(next_states_[req_id] & kStageResponseStarted)
      << event_order_[req_id];
  next_states_[req_id] = kStageCompletedSuccess | kStageCompletedError;
  if (net_error == ERR_ABORTED)
    return;

  if (net_error != OK) {
    error_count_++;
    last_error_ = net_error;
  }
}

void TestNetworkDelegate::OnNetworkBytesReceived(URLRequest* request,
                                                 int64_t bytes_received) {
  event_order_[request->identifier()] += "OnNetworkBytesReceived\n";
  total_network_bytes_received_ += bytes_received;
}

void TestNetworkDelegate::OnNetworkBytesSent(URLRequest* request,
                                             int64_t bytes_sent) {
  event_order_[request->identifier()] += "OnNetworkBytesSent\n";
  total_network_bytes_sent_ += bytes_sent;
}

void TestNetworkDelegate::OnCompleted(URLRequest* request,
                                      bool started,
                                      int net_error) {
  DCHECK_NE(net_error, net::ERR_IO_PENDING);

  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnCompleted\n";
  // Expect "Success -> (next_states_ & kStageCompletedSuccess)"
  // is logically identical to
  // Expect "!(Success) || (next_states_ & kStageCompletedSuccess)"
  EXPECT_TRUE(net_error != OK ||
              (next_states_[req_id] & kStageCompletedSuccess))
      << event_order_[req_id];
  EXPECT_TRUE(net_error == OK || (next_states_[req_id] & kStageCompletedError))
      << event_order_[req_id];
  next_states_[req_id] = kStageURLRequestDestroyed;
  completed_requests_++;
  if (net_error == ERR_ABORTED) {
    canceled_requests_++;
  } else if (net_error != OK) {
    error_count_++;
    last_error_ = net_error;
  } else {
    DCHECK_EQ(OK, net_error);
  }
}

void TestNetworkDelegate::OnURLRequestDestroyed(URLRequest* request) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnURLRequestDestroyed\n";
  EXPECT_TRUE(next_states_[req_id] & kStageURLRequestDestroyed) <<
      event_order_[req_id];
  next_states_[req_id] = kStageDestruction;
  destroyed_requests_++;
}

void TestNetworkDelegate::OnPACScriptError(int line_number,
                                           const base::string16& error) {
}

NetworkDelegate::AuthRequiredResponse TestNetworkDelegate::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    AuthCallback callback,
    AuthCredentials* credentials) {
  load_timing_info_before_auth_ = LoadTimingInfo();
  request->GetLoadTimingInfo(&load_timing_info_before_auth_);
  has_load_timing_info_before_auth_ = true;
  EXPECT_FALSE(load_timing_info_before_auth_.request_start_time.is_null());
  EXPECT_FALSE(load_timing_info_before_auth_.request_start.is_null());

  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnAuthRequired\n";
  EXPECT_TRUE(next_states_[req_id] & kStageAuthRequired) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeStartTransaction |
      kStageAuthRequired |  // For example, proxy auth followed by server auth.
      kStageHeadersReceived |  // Request canceled by delegate simulates empty
                               // response.
      kStageResponseStarted |  // data: URLs do not trigger sending headers
      kStageBeforeRedirect |   // a delegate can trigger a redirection
      kStageCompletedError;    // request cancelled before callback
  return NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool TestNetworkDelegate::OnCanGetCookies(const URLRequest& request,
                                          const CookieList& cookie_list,
                                          bool allowed_from_caller) {
  bool allow = allowed_from_caller;
  if (cookie_options_bit_mask_ & NO_GET_COOKIES)
    allow = false;

  if (!allow) {
    blocked_get_cookies_count_++;
  }

  return allow;
}

bool TestNetworkDelegate::OnCanSetCookie(const URLRequest& request,
                                         const net::CanonicalCookie& cookie,
                                         CookieOptions* options,
                                         bool allowed_from_caller) {
  bool allow = allowed_from_caller;
  if (cookie_options_bit_mask_ & NO_SET_COOKIE)
    allow = false;

  if (!allow) {
    blocked_set_cookie_count_++;
  } else {
    set_cookie_count_++;
  }

  return allow;
}

bool TestNetworkDelegate::OnCanAccessFile(
    const URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return can_access_files_;
}

bool TestNetworkDelegate::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  return cancel_request_with_policy_violating_referrer_;
}

TestJobInterceptor::TestJobInterceptor() = default;

TestJobInterceptor::~TestJobInterceptor() = default;

URLRequestJob* TestJobInterceptor::MaybeCreateJob(
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  return main_intercept_job_.release();
}

void TestJobInterceptor::set_main_intercept_job(
    std::unique_ptr<URLRequestJob> job) {
  main_intercept_job_ = std::move(job);
}

}  // namespace net
