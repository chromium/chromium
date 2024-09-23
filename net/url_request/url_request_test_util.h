// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
#define NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_

#include <stdint.h>
#include <stdlib.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/request_priority.h"
#include "net/base/transport_info.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_interceptor.h"
#include "url/url_util.h"

namespace net {

class URLRequestContextBuilder;

//-----------------------------------------------------------------------------

// Creates a URLRequestContextBuilder with some members configured for the
// testing purpose.
std::unique_ptr<URLRequestContextBuilder> CreateTestURLRequestContextBuilder();

//-----------------------------------------------------------------------------
// Used to return a dummy context, which lives on the message loop
// given in the constructor.
class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  // |network_task_runner| must not be NULL.
  explicit TestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  // Use to pass a pre-initialized |context|.
  TestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
      std::unique_ptr<URLRequestContext> context);

  // URLRequestContextGetter implementation.
  URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // see NotifyContextShuttingDown() in the base class.
  void NotifyContextShuttingDown();

 protected:
  ~TestURLRequestContextGetter() override;

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<URLRequestContext> context_;
  bool is_shut_down_ = false;
};

//-----------------------------------------------------------------------------

class TestDelegate : public URLRequest::Delegate {
 public:
  TestDelegate();
  ~TestDelegate() override;

  // Helpers to create a RunLoop, set |on_<event>| from it, then Run() it.
  void RunUntilComplete();
  void RunUntilRedirect();
  // Enables quitting the message loop in response to auth requests, as opposed
  // to returning credentials or cancelling the request.
  void RunUntilAuthRequired();

  // Sets the closure to be run on completion, for tests which need more fine-
  // grained control than RunUntilComplete().
  void set_on_complete(base::OnceClosure on_complete) {
    on_complete_ = std::move(on_complete);
  }

  // Sets the result returned by subsequent calls to OnConnected().
  void set_on_connected_result(int result) { on_connected_result_ = result; }

  void set_cancel_in_received_redirect(bool val) { cancel_in_rr_ = val; }
  void set_cancel_in_response_started(bool val) { cancel_in_rs_ = val; }
  void set_cancel_in_received_data(bool val) { cancel_in_rd_ = val; }
  void set_cancel_in_received_data_pending(bool val) {
    cancel_in_rd_pending_ = val;
  }

  void set_allow_certificate_errors(bool val) {
    allow_certificate_errors_ = val;
  }
  void set_credentials(const AuthCredentials& credentials) {
    credentials_ = credentials;
  }

  // If true, the delegate will asynchronously run the callback passed in from
  // URLRequest with `on_connected_result_`
  void set_on_connected_run_callback(bool run_callback) {
    on_connected_run_callback_ = run_callback;
  }

  // Returns the list of arguments with which OnConnected() was called.
  // The arguments are listed in the same order as the calls were received.
  const std::vector<TransportInfo>& transports() const { return transports_; }

  // query state
  const std::string& data_received() const { return data_received_; }
  int bytes_received() const { return static_cast<int>(data_received_.size()); }
  int response_started_count() const { return response_started_count_; }
  int received_bytes_count() const { return received_bytes_count_; }
  int received_redirect_count() const { return received_redirect_count_; }
  bool received_data_before_response() const {
    return received_data_before_response_;
  }
  RedirectInfo redirect_info() { return redirect_info_; }
  bool request_failed() const { return request_failed_; }
  bool have_certificate_errors() const { return have_certificate_errors_; }
  bool certificate_errors_are_fatal() const {
    return certificate_errors_are_fatal_;
  }
  int certificate_net_error() const { return certificate_net_error_; }
  bool auth_required_called() const { return auth_required_; }
  bool response_completed() const { return response_completed_; }
  int request_status() const { return request_status_; }

  // URLRequest::Delegate:
  int OnConnected(URLRequest* request,
                  const TransportInfo& info,
                  CompletionOnceCallback callback) override;
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(URLRequest* request,
                      const AuthChallengeInfo& auth_info) override;
  // NOTE: |fatal| causes |certificate_errors_are_fatal_| to be set to true.
  // (Unit tests use this as a post-condition.) But for policy, this method
  // consults |allow_certificate_errors_|.
  void OnSSLCertificateError(URLRequest* request,
                             int net_error,
                             const SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int bytes_read) override;

 private:
  static const int kBufferSize = 4096;

  virtual void OnResponseCompleted(URLRequest* request);

  // options for controlling behavior
  int on_connected_result_ = OK;
  bool cancel_in_rr_ = false;
  bool cancel_in_rs_ = false;
  bool cancel_in_rd_ = false;
  bool cancel_in_rd_pending_ = false;
  bool allow_certificate_errors_ = false;
  AuthCredentials credentials_;

  // Used to register RunLoop quit closures, to implement the Until*() closures.
  base::OnceClosure on_complete_;
  base::OnceClosure on_redirect_;
  base::OnceClosure on_auth_required_;

  // tracks status of callbacks
  std::vector<TransportInfo> transports_;
  int response_started_count_ = 0;
  int received_bytes_count_ = 0;
  int received_redirect_count_ = 0;
  bool received_data_before_response_ = false;
  bool request_failed_ = false;
  bool have_certificate_errors_ = false;
  bool certificate_errors_are_fatal_ = false;
  int certificate_net_error_ = 0;
  bool auth_required_ = false;
  std::string data_received_;
  bool response_completed_ = false;

  // tracks status of request
  int request_status_ = ERR_IO_PENDING;

  // our read buffer
  scoped_refptr<IOBuffer> buf_;

  RedirectInfo redirect_info_;

  bool on_connected_run_callback_ = false;
};

//-----------------------------------------------------------------------------

class TestNetworkDelegate : public NetworkDelegateImpl {
 public:
  enum Options {
    NO_GET_COOKIES = 1 << 0,
    NO_SET_COOKIE  = 1 << 1,
  };

  TestNetworkDelegate();
  ~TestNetworkDelegate() override;

  // Writes the LoadTimingInfo during the most recent call to OnBeforeRedirect.
  bool GetLoadTimingInfoBeforeRedirect(
      LoadTimingInfo* load_timing_info_before_redirect) const;

  // Will redirect once to the given URL when the next set of headers are
  // received.
  void set_redirect_on_headers_received_url(
      GURL redirect_on_headers_received_url) {
    redirect_on_headers_received_url_ = redirect_on_headers_received_url;
  }

  // Adds a X-Network-Delegate header to the first OnHeadersReceived call, but
  // not subsequent ones.
  void set_add_header_to_first_response(bool add_header_to_first_response) {
    add_header_to_first_response_ = add_header_to_first_response;
  }

  void set_preserve_fragment_on_redirect_url(
      const std::optional<GURL>& preserve_fragment_on_redirect_url) {
    preserve_fragment_on_redirect_url_ = preserve_fragment_on_redirect_url;
  }

  void set_cookie_options(int o) {cookie_options_bit_mask_ = o; }

  int last_error() const { return last_error_; }
  int error_count() const { return error_count_; }
  int created_requests() const { return created_requests_; }
  int destroyed_requests() const { return destroyed_requests_; }
  int completed_requests() const { return completed_requests_; }
  int canceled_requests() const { return canceled_requests_; }
  int blocked_annotate_cookies_count() const {
    return blocked_annotate_cookies_count_;
  }
  int blocked_set_cookie_count() const { return blocked_set_cookie_count_; }
  int set_cookie_count() const { return set_cookie_count_; }

  void set_cancel_request_with_policy_violating_referrer(bool val) {
    cancel_request_with_policy_violating_referrer_ = val;
  }

  int before_start_transaction_count() const {
    return before_start_transaction_count_;
  }

  int headers_received_count() const { return headers_received_count_; }

  void set_before_start_transaction_fails() {
    before_start_transaction_fails_ = true;
  }

  const std::vector<CookieSettingOverrides>& cookie_setting_overrides_records()
      const {
    return cookie_setting_overrides_records_;
  }

  void set_storage_access_status(
      std::optional<cookie_util::StorageAccessStatus> status) {
    storage_access_status_ = status;
  }

  void set_is_storage_access_header_enabled(bool enabled) {
    is_storage_access_header_enabled_ = enabled;
  }

 protected:
  // NetworkDelegate:
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
  void OnBeforeRedirect(URLRequest* request, const GURL& new_location) override;
  void OnBeforeRetry(URLRequest* request) override;
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnCompleted(URLRequest* request, bool started, int net_error) override;
  void OnURLRequestDestroyed(URLRequest* request) override;
  bool OnAnnotateAndMoveUserBlockedCookies(
      const URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) override;
  NetworkDelegate::PrivacySetting OnForcePrivacyMode(
      const URLRequest& request) const override;
  bool OnCanSetCookie(
      const URLRequest& request,
      const net::CanonicalCookie& cookie,
      CookieOptions* options,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      CookieInclusionStatus* inclusion_status) override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;
  std::optional<cookie_util::StorageAccessStatus> OnGetStorageAccessStatus(
      const URLRequest& request) const override;
  bool OnIsStorageAccessHeaderEnabled(const url::Origin* top_frame_origin,
                                      const GURL& url) const override;

  void InitRequestStatesIfNew(int request_id);

  // Gets a request ID if it already has one, assigns a new one and returns that
  // if not.
  int GetRequestId(URLRequest* request);

  void RecordCookieSettingOverrides(CookieSettingOverrides overrides) const {
    cookie_setting_overrides_records_.push_back(overrides);
  }

  GURL redirect_on_headers_received_url_;
  // URL to mark as retaining its fragment if redirected to at the
  // OnHeadersReceived() stage.
  std::optional<GURL> preserve_fragment_on_redirect_url_;

  int last_error_ = 0;
  int error_count_ = 0;
  int created_requests_ = 0;
  int destroyed_requests_ = 0;
  int completed_requests_ = 0;
  int canceled_requests_ = 0;
  int cookie_options_bit_mask_ = 0;
  int blocked_annotate_cookies_count_ = 0;
  int blocked_set_cookie_count_ = 0;
  int set_cookie_count_ = 0;
  int before_start_transaction_count_ = 0;
  int headers_received_count_ = 0;

  // NetworkDelegate callbacks happen in a particular order (e.g.
  // OnBeforeURLRequest is always called before OnBeforeStartTransaction).
  // This bit-set indicates for each request id (key) what events may be sent
  // next.
  std::map<int, int> next_states_;

  // A log that records for each request id (key) the order in which On...
  // functions were called.
  std::map<int, std::string> event_order_;

  LoadTimingInfo load_timing_info_before_redirect_;
  bool has_load_timing_info_before_redirect_ = false;

  bool cancel_request_with_policy_violating_referrer_ =
      false;  // false by default
  bool before_start_transaction_fails_ = false;
  bool add_header_to_first_response_ = false;
  int next_request_id_ = 0;

  mutable std::vector<CookieSettingOverrides> cookie_setting_overrides_records_;

  std::optional<cookie_util::StorageAccessStatus> storage_access_status_ =
      std::nullopt;

  bool is_storage_access_header_enabled_ = false;
};

// ----------------------------------------------------------------------------

class FilteringTestNetworkDelegate : public TestNetworkDelegate {
 public:
  FilteringTestNetworkDelegate();
  ~FilteringTestNetworkDelegate() override;

  bool OnCanSetCookie(
      const URLRequest& request,
      const net::CanonicalCookie& cookie,
      CookieOptions* options,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      CookieInclusionStatus* inclusion_status) override;

  void SetCookieFilter(std::string filter) {
    cookie_name_filter_ = std::move(filter);
  }

  int set_cookie_called_count() { return set_cookie_called_count_; }

  int blocked_set_cookie_count() { return blocked_set_cookie_count_; }

  void ResetSetCookieCalledCount() { set_cookie_called_count_ = 0; }

  void ResetBlockedSetCookieCount() { blocked_set_cookie_count_ = 0; }

  bool OnAnnotateAndMoveUserBlockedCookies(
      const URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) override;

  NetworkDelegate::PrivacySetting OnForcePrivacyMode(
      const URLRequest& request) const override;

  void set_block_annotate_cookies() { block_annotate_cookies_ = true; }

  void unset_block_annotate_cookies() { block_annotate_cookies_ = false; }

  int annotate_cookies_called_count() const {
    return annotate_cookies_called_count_;
  }

  int blocked_annotate_cookies_count() const {
    return blocked_annotate_cookies_count_;
  }

  void ResetAnnotateCookiesCalledCount() { annotate_cookies_called_count_ = 0; }

  void ResetBlockedAnnotateCookiesCount() {
    blocked_annotate_cookies_count_ = 0;
  }

  void set_block_get_cookies_by_name(bool block) {
    block_get_cookies_by_name_ = block;
  }

  void set_force_privacy_mode(bool enabled) { force_privacy_mode_ = enabled; }

  void set_partitioned_state_allowed(bool allowed) {
    partitioned_state_allowed_ = allowed;
  }

 private:
  std::string cookie_name_filter_ = "";
  int set_cookie_called_count_ = 0;
  int blocked_set_cookie_count_ = 0;

  bool block_annotate_cookies_ = false;
  int annotate_cookies_called_count_ = 0;
  int blocked_annotate_cookies_count_ = 0;
  bool block_get_cookies_by_name_ = false;

  bool force_privacy_mode_ = false;
  bool partitioned_state_allowed_ = false;
};

// ----------------------------------------------------------------------------

// Less verbose way of running a simple testserver.
class HttpTestServer : public EmbeddedTestServer {
 public:
  explicit HttpTestServer(const base::FilePath& document_root) {
    AddDefaultHandlers(document_root);
  }

  HttpTestServer() { RegisterDefaultHandlers(this); }
};
//-----------------------------------------------------------------------------

class TestScopedURLInterceptor {
 public:
  // Sets up a URLRequestInterceptor that intercepts a single request for |url|,
  // returning the provided job.
  //
  // On destruction, cleans makes sure the job was removed, and cleans up the
  // interceptor. Other interceptors for the same URL may not be created until
  // the interceptor is deleted.
  TestScopedURLInterceptor(const GURL& url,
                           std::unique_ptr<URLRequestJob> intercept_job);
  ~TestScopedURLInterceptor();

 private:
  class TestRequestInterceptor;

  GURL url_;

  // This is owned by the URLFilter.
  raw_ptr<TestRequestInterceptor> interceptor_ = nullptr;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
