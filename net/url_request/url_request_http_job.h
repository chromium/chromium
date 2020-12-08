// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/http/http_request_info.h"
#include "net/socket/connection_attempts.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_throttler_entry_interface.h"

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;
class HttpResponseInfo;
class HttpTransaction;
class HttpUserAgentSettings;
class SSLPrivateKey;
struct TransportInfo;
class UploadDataStream;

// A URLRequestJob subclass that is built on top of HttpTransaction. It
// provides an implementation for both HTTP and HTTPS.
class NET_EXPORT_PRIVATE URLRequestHttpJob : public URLRequestJob {
 public:
  // Creates URLRequestJob for the specified HTTP, HTTPS, WS, or WSS URL.
  // Returns a job that returns a redirect in the case of HSTS, and returns a
  // job that fails for unencrypted requests if current settings dont allow
  // them. Never returns nullptr.
  static std::unique_ptr<URLRequestJob> Create(URLRequest* request);

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;
  void SetResponseHeadersCallback(ResponseHeadersCallback callback) override;

 protected:
  URLRequestHttpJob(URLRequest* request,
                    const HttpUserAgentSettings* http_user_agent_settings);

  ~URLRequestHttpJob() override;

  // Overridden from URLRequestJob:
  void SetPriority(RequestPriority priority) override;
  void Start() override;
  void Kill() override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void CloseConnectionOnDestruction() override;
  std::unique_ptr<SourceStream> SetUpSourceStream() override;

  RequestPriority priority() const {
    return priority_;
  }

 private:
  // For CookieRequestScheme histogram enum.
  FRIEND_TEST_ALL_PREFIXES(URLRequestHttpJobTest,
                           CookieSchemeRequestSchemeHistogram);

  enum CompletionCause {
    ABORTED,
    FINISHED
  };

  // Used to indicate which kind of cookies are sent on which kind of requests,
  // for use in histograms. A (non)secure set cookie means that the cookie was
  // originally set by a (non)secure url. A (non)secure request means that the
  // request url is (non)secure. An unset cookie scheme means that the cookie's
  // source scheme was marked as "Unset" and thus cannot be compared  with the
  // request.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CookieRequestScheme {
    kUnsetCookieScheme = 0,
    kNonsecureSetNonsecureRequest,
    kSecureSetSecureRequest,
    kNonsecureSetSecureRequest,
    kSecureSetNonsecureRequest,

    kMaxValue = kSecureSetNonsecureRequest  // Keep as the last value.
  };

  typedef base::RefCountedData<bool> SharedBoolean;

  // Shadows URLRequestJob's version of this method so we can grab cookies.
  void NotifyHeadersComplete();

  void DestroyTransaction();

  void AddExtraHeaders();
  void AddCookieHeaderAndStart();
  void SaveCookiesAndNotifyHeadersComplete(int result);

  // Processes the Strict-Transport-Security header, if one exists.
  void ProcessStrictTransportSecurityHeader();

  // Processes the Expect-CT header, if one exists. This header
  // indicates that the server wants the user agent to send a report
  // when a connection violates the Expect CT policy.
  void ProcessExpectCTHeader();

  // |result| should be OK, or the request is canceled.
  void OnHeadersReceivedCallback(int result);
  void OnStartCompleted(int result);
  void OnReadCompleted(int result);
  void NotifyBeforeStartTransactionCallback(int result);
  // This just forwards the call to URLRequestJob::NotifyConnected().
  // We need it because that method is protected and cannot be bound in a
  // callback in this class.
  int NotifyConnectedCallback(const TransportInfo& info);

  void RestartTransactionWithAuth(const AuthCredentials& credentials);

  // Overridden from URLRequestJob:
  void SetUpload(UploadDataStream* upload) override;
  void SetExtraRequestHeaders(const HttpRequestHeaders& headers) override;
  LoadState GetLoadState() const override;
  bool GetMimeType(std::string* mime_type) const override;
  bool GetCharset(std::string* charset) override;
  void GetResponseInfo(HttpResponseInfo* info) override;
  void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  bool GetTransactionRemoteEndpoint(IPEndPoint* endpoint) const override;
  int GetResponseCode() const override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;
  bool CopyFragmentOnRedirect(const GURL& location) const override;
  bool IsSafeRedirect(const GURL& location) override;
  bool NeedsAuth() override;
  std::unique_ptr<AuthChallengeInfo> GetAuthChallengeInfo() override;
  void SetAuth(const AuthCredentials& credentials) override;
  void CancelAuth() override;
  void ContinueWithCertificate(
      scoped_refptr<X509Certificate> client_cert,
      scoped_refptr<SSLPrivateKey> client_private_key) override;
  void ContinueDespiteLastError() override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  void DoneReading() override;
  void DoneReadingRedirectResponse() override;

  IPEndPoint GetResponseRemoteEndpoint() const override;
  void NotifyURLRequestDestroyed() override;

  void RecordTimer();
  void ResetTimer();

  // Starts the transaction if extensions using the webrequest API do not
  // object.
  void StartTransaction();
  // If |result| is OK, calls StartTransactionInternal. Otherwise notifies
  // cancellation.
  void MaybeStartTransactionInternal(int result);
  void StartTransactionInternal();

  void RecordPerfHistograms(CompletionCause reason);
  void DoneWithRequest(CompletionCause reason);

  // Callback functions for Cookie Monster
  void SetCookieHeaderAndStart(const CookieOptions& options,
                               const CookieAccessResultList& cookie_list,
                               const CookieAccessResultList& excluded_list);

  // Another Cookie Monster callback
  void OnSetCookieResult(const CookieOptions& options,
                         base::Optional<CanonicalCookie> cookie,
                         std::string cookie_string,
                         CookieAccessResult access_result);
  int num_cookie_lines_left_;
  CookieAndLineAccessResultList set_cookie_access_result_list_;

  // Some servers send the body compressed, but specify the content length as
  // the uncompressed size. If this is the case, we return true in order
  // to request to work around this non-adherence to the HTTP standard.
  // |rv| is the standard return value of a read function indicating the number
  // of bytes read or, if negative, an error code.
  bool ShouldFixMismatchedContentLength(int rv) const;

  // Returns the effective response headers, considering that they may be
  // overridden by |override_response_headers_|.
  HttpResponseHeaders* GetResponseHeaders() const;

  RequestPriority priority_;

  HttpRequestInfo request_info_;
  const HttpResponseInfo* response_info_;

  // Auth states for proxy and origin server.
  AuthState proxy_auth_state_;
  AuthState server_auth_state_;
  AuthCredentials auth_credentials_;

  bool read_in_progress_;

  std::unique_ptr<HttpTransaction> transaction_;

  // This is used to supervise traffic and enforce exponential
  // back-off. May be NULL.
  scoped_refptr<URLRequestThrottlerEntryInterface> throttling_entry_;

  base::Time request_creation_time_;

  // True when we are done doing work.
  bool done_;

  // The start time for the job, ignoring re-starts.
  base::TimeTicks start_time_;

  // When the transaction finished reading the request headers.
  base::TimeTicks receive_headers_end_;

  // We allow the network delegate to modify a copy of the response headers.
  // This prevents modifications of headers that are shared with the underlying
  // layers of the network stack.
  scoped_refptr<HttpResponseHeaders> override_response_headers_;

  // Ordinarily the original URL's fragment is copied during redirects, unless
  // the destination URL already has one. However, the NetworkDelegate can
  // override this behavior by setting |preserve_fragment_on_redirect_url_|:
  // * If set to base::nullopt, the default behavior is used.
  // * If the final URL in the redirect chain matches
  //     |preserve_fragment_on_redirect_url_|, its fragment unchanged. So this
  //     is basically a way for the embedder to force a redirect not to copy the
  //     original URL's fragment when the original URL had one.
  base::Optional<GURL> preserve_fragment_on_redirect_url_;

  // Flag used to verify that |this| is not deleted while we are awaiting
  // a callback from the NetworkDelegate. Used as a fail-fast mechanism.
  // True if we are waiting a callback and
  // NetworkDelegate::NotifyURLRequestDestroyed has not been called, yet,
  // to inform the NetworkDelegate that it may not call back.
  bool awaiting_callback_;

  const HttpUserAgentSettings* http_user_agent_settings_;

  // Keeps track of total received bytes over the network from transactions used
  // by this job that have already been destroyed.
  int64_t total_received_bytes_from_previous_transactions_;
  // Keeps track of total sent bytes over the network from transactions used by
  // this job that have already been destroyed.
  int64_t total_sent_bytes_from_previous_transactions_;

  RequestHeadersCallback request_headers_callback_;
  ResponseHeadersCallback response_headers_callback_;

  base::WeakPtrFactory<URLRequestHttpJob> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
