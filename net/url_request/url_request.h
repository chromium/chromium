// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_H_
#define NET_URL_REQUEST_URL_REQUEST_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/idempotency.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_delegate.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/filter/source_stream.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "net/shared_dictionary/shared_dictionary.h"
#include "net/shared_dictionary/shared_dictionary_getter.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_tag.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/referrer_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class CookieOptions;
class CookieInclusionStatus;
class IOBuffer;
struct LoadTimingInfo;
struct RedirectInfo;
class SSLCertRequestInfo;
class SSLInfo;
class SSLPrivateKey;
struct TransportInfo;
class UploadDataStream;
class URLRequestContext;
class URLRequestJob;
class X509Certificate;

//-----------------------------------------------------------------------------
// A class  representing the asynchronous load of a data stream from an URL.
//
// The lifetime of an instance of this class is completely controlled by the
// consumer, and the instance is not required to live on the heap or be
// allocated in any special way.  It is also valid to delete an URLRequest
// object during the handling of a callback to its delegate.  Of course, once
// the URLRequest is deleted, no further callbacks to its delegate will occur.
//
// NOTE: All usage of all instances of this class should be on the same thread.
//
class NET_EXPORT URLRequest : public base::SupportsUserData {
 public:
  // Max number of http redirects to follow. The Fetch spec says: "If
  // request's redirect count is twenty, return a network error."
  // https://fetch.spec.whatwg.org/#http-redirect-fetch
  static constexpr int kMaxRedirects = 20;

  // The delegate's methods are called from the message loop of the thread
  // on which the request's Start() method is called. See above for the
  // ordering of callbacks.
  //
  // The callbacks will be called in the following order:
  //   Start()
  //    - OnConnected* (zero or more calls, see method comment)
  //    - OnCertificateRequested* (zero or more calls, if the SSL server and/or
  //      SSL proxy requests a client certificate for authentication)
  //    - OnSSLCertificateError* (zero or one call, if the SSL server's
  //      certificate has an error)
  //    - OnReceivedRedirect* (zero or more calls, for the number of redirects)
  //    - OnAuthRequired* (zero or more calls, for the number of
  //      authentication failures)
  //    - OnResponseStarted
  //   Read() initiated by delegate
  //    - OnReadCompleted* (zero or more calls until all data is read)
  //
  // Read() must be called at least once. Read() returns bytes read when it
  // completes immediately, and a negative error value if an IO is pending or if
  // there is an error.
  class NET_EXPORT Delegate {
   public:
    Delegate() = default;

    // Forbid copy and assign to prevent slicing.
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called each time a connection is obtained, before any data is sent.
    //
    // |request| is never nullptr. Caller retains ownership.
    //
    // |info| describes the newly-obtained connection.
    //
    // This may be called several times if the request creates multiple HTTP
    // transactions, e.g. if the request is redirected. It may also be called
    // several times per transaction, e.g. if the connection is retried, after
    // each HTTP auth challenge, or for split HTTP range requests.
    //
    // If this returns an error, the transaction will stop. The transaction
    // will continue when the |callback| is run. If run with an error, the
    // transaction will fail.
    virtual int OnConnected(URLRequest* request,
                            const TransportInfo& info,
                            CompletionOnceCallback callback);

    // Called upon receiving a redirect.  The delegate may call the request's
    // Cancel method to prevent the redirect from being followed.  Since there
    // may be multiple chained redirects, there may also be more than one
    // redirect call.
    //
    // When this function is called, the request will still contain the
    // original URL, the destination of the redirect is provided in
    // |redirect_info.new_url|.  If the delegate does not cancel the request
    // and |*defer_redirect| is false, then the redirect will be followed, and
    // the request's URL will be changed to the new URL.  Otherwise if the
    // delegate does not cancel the request and |*defer_redirect| is true, then
    // the redirect will be followed once FollowDeferredRedirect is called
    // on the URLRequest.
    //
    // The caller must set |*defer_redirect| to false, so that delegates do not
    // need to set it if they are happy with the default behavior of not
    // deferring redirect.
    virtual void OnReceivedRedirect(URLRequest* request,
                                    const RedirectInfo& redirect_info,
                                    bool* defer_redirect);

    // Called when we receive an authentication failure.  The delegate should
    // call request->SetAuth() with the user's credentials once it obtains them,
    // or request->CancelAuth() to cancel the login and display the error page.
    // When it does so, the request will be reissued, restarting the sequence
    // of On* callbacks.
    //
    // NOTE: If auth_info.scheme is AUTH_SCHEME_NEGOTIATE on ChromeOS, this
    // method should not call SetAuth(). Instead, it should show ChromeOS
    // specific UI and cancel the request. (See b/260522530).
    virtual void OnAuthRequired(URLRequest* request,
                                const AuthChallengeInfo& auth_info);

    // Called when we receive an SSL CertificateRequest message for client
    // authentication.  The delegate should call
    // request->ContinueWithCertificate() with the client certificate the user
    // selected and its private key, or request->ContinueWithCertificate(NULL,
    // NULL)
    // to continue the SSL handshake without a client certificate.
    virtual void OnCertificateRequested(URLRequest* request,
                                        SSLCertRequestInfo* cert_request_info);

    // Called when using SSL and the server responds with a certificate with
    // an error, for example, whose common name does not match the common name
    // we were expecting for that host.  The delegate should either do the
    // safe thing and Cancel() the request or decide to proceed by calling
    // ContinueDespiteLastError().  cert_error is a ERR_* error code
    // indicating what's wrong with the certificate.
    // If |fatal| is true then the host in question demands a higher level
    // of security (due e.g. to HTTP Strict Transport Security, user
    // preference, or built-in policy). In this case, errors must not be
    // bypassable by the user.
    virtual void OnSSLCertificateError(URLRequest* request,
                                       int net_error,
                                       const SSLInfo& ssl_info,
                                       bool fatal);

    // After calling Start(), the delegate will receive an OnResponseStarted
    // callback when the request has completed. |net_error| will be set to OK
    // or an actual net error.  On success, all redirects have been
    // followed and the final response is beginning to arrive.  At this point,
    // meta data about the response is available, including for example HTTP
    // response headers if this is a request for a HTTP resource.
    virtual void OnResponseStarted(URLRequest* request, int net_error);

    // Called when the a Read of the response body is completed after an
    // IO_PENDING status from a Read() call.
    // The data read is filled into the buffer which the caller passed
    // to Read() previously.
    //
    // If an error occurred, |bytes_read| will be set to the error.
    virtual void OnReadCompleted(URLRequest* request, int bytes_read) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // URLRequests are always created by calling URLRequestContext::CreateRequest.
  URLRequest(base::PassKey<URLRequestContext> pass_key,
             const GURL& url,
             RequestPriority priority,
             Delegate* delegate,
             const URLRequestContext* context,
             NetworkTrafficAnnotationTag traffic_annotation,
             bool is_for_websockets,
             std::optional<net::NetLogSource> net_log_source);

  URLRequest(const URLRequest&) = delete;
  URLRequest& operator=(const URLRequest&) = delete;

  // If destroyed after Start() has been called but while IO is pending,
  // then the request will be effectively canceled and the delegate
  // will not have any more of its methods called.
  ~URLRequest() override;

  // Changes the default cookie policy from allowing all cookies to blocking all
  // cookies. Embedders that want to implement a more flexible policy should
  // change the default to blocking all cookies, and provide a NetworkDelegate
  // with the URLRequestContext that maintains the CookieStore.
  // The cookie policy default has to be set before the first URLRequest is
  // started. Once it was set to block all cookies, it cannot be changed back.
  static void SetDefaultCookiePolicyToBlock();

  // The original url is the url used to initialize the request, and it may
  // differ from the url if the request was redirected.
  const GURL& original_url() const { return url_chain_.front(); }
  // The chain of urls traversed by this request.  If the request had no
  // redirects, this vector will contain one element.
  const std::vector<GURL>& url_chain() const { return url_chain_; }
  const GURL& url() const { return url_chain_.back(); }

  // Explicitly set the URL chain for this request.  This can be used to
  // indicate a chain of redirects that happen at a layer above the network
  // service; e.g. navigation redirects.
  //
  // Note, the last entry in the new `url_chain` will be ignored.  Instead
  // the request will preserve its current URL.  This is done since the higher
  // layer providing the explicit `url_chain` may not be aware of modifications
  // to the request URL by throttles.
  //
  // This method should only be called on new requests that have a single
  // entry in their existing `url_chain_`.
  void SetURLChain(const std::vector<GURL>& url_chain);

  // The URL that should be consulted for the third-party cookie blocking
  // policy, as defined in Section 2.1.1 and 2.1.2 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  //
  // WARNING: This URL must only be used for the third-party cookie blocking
  //          policy. It MUST NEVER be used for any kind of SECURITY check.
  //
  //          For example, if a top-level navigation is redirected, the
  //          first-party for cookies will be the URL of the first URL in the
  //          redirect chain throughout the whole redirect. If it was used for
  //          a security check, an attacker might try to get around this check
  //          by starting from some page that redirects to the
  //          host-to-be-attacked.
  //
  const SiteForCookies& site_for_cookies() const { return site_for_cookies_; }
  // This method may only be called before Start().
  void set_site_for_cookies(const SiteForCookies& site_for_cookies);

  // Sets IsolationInfo for the request, which affects whether SameSite cookies
  // are sent, what NetworkAnonymizationKey is used for cached resources, and
  // how that behavior changes when following redirects. This may only be
  // changed before Start() is called. Setting this value causes the
  // cookie_partition_key_ to be recalculated. When the isolation information is
  // set through a redirect, the request_site used to create the partition key
  // should come from the new_url associated with the redirect_info object
  // associated with the redirect to ensure the cookie partition key's ancestor
  // chain bit is set correctly.
  //
  // TODO(crbug.com/40093296): This isn't actually used yet for SameSite
  // cookies. Update consumers and fix that.
  void set_isolation_info(
      const IsolationInfo& isolation_info,
      std::optional<GURL> redirect_info_new_url = std::nullopt);

  // This will convert the passed NetworkAnonymizationKey to an IsolationInfo.
  // This IsolationInfo mmay be assigned an inaccurate frame origin because the
  // NetworkAnonymizationKey might not contain all the information to populate
  // it. Additionally the NetworkAnonymizationKey uses sites which will be
  // converted to origins when set on the IsolationInfo. If using this method it
  // is required to skip the cache and not use credentials. Before starting the
  // request, it must have the LoadFlag LOAD_DISABLE_CACHE set, and must be set
  // to not allow credentials, to ensure that the inaccurate frame origin has no
  // impact. The request will DCHECK otherwise.
  void set_isolation_info_from_network_anonymization_key(
      const NetworkAnonymizationKey& network_anonymization_key);

  const IsolationInfo& isolation_info() const { return isolation_info_; }

  const std::optional<CookiePartitionKey>& cookie_partition_key() const {
    return cookie_partition_key_;
  }

  // Indicate whether SameSite cookies should be attached even though the
  // request is cross-site.
  bool force_ignore_site_for_cookies() const {
    return force_ignore_site_for_cookies_;
  }
  void set_force_ignore_site_for_cookies(bool attach) {
    force_ignore_site_for_cookies_ = attach;
  }

  // Indicates if the request should be treated as a main frame navigation for
  // SameSite cookie computations.  This flag overrides the IsolationInfo
  // request type associated with fetches from a service worker context.
  bool force_main_frame_for_same_site_cookies() const {
    return force_main_frame_for_same_site_cookies_;
  }
  void set_force_main_frame_for_same_site_cookies(bool value) {
    force_main_frame_for_same_site_cookies_ = value;
  }

  // Overrides pertaining to cookie settings for this particular request.
  CookieSettingOverrides& cookie_setting_overrides() {
    return cookie_setting_overrides_;
  }
  const CookieSettingOverrides& cookie_setting_overrides() const {
    return cookie_setting_overrides_;
  }

  // The first-party URL policy to apply when updating the first party URL
  // during redirects. The first-party URL policy may only be changed before
  // Start() is called.
  RedirectInfo::FirstPartyURLPolicy first_party_url_policy() const {
    return first_party_url_policy_;
  }
  void set_first_party_url_policy(
      RedirectInfo::FirstPartyURLPolicy first_party_url_policy);

  // The origin of the context which initiated the request. This is distinct
  // from the "first party for cookies" discussed above in a number of ways:
  //
  // 1. The request's initiator does not change during a redirect. If a form
  //    submission from `https://example.com/` redirects through a number of
  //    sites before landing on `https://not-example.com/`, the initiator for
  //    each of those requests will be `https://example.com/`.
  //
  // 2. The request's initiator is the origin of the frame or worker which made
  //    the request, even for top-level navigations. That is, if
  //    `https://example.com/`'s form submission is made in the top-level frame,
  //    the first party for cookies would be the target URL's origin. The
  //    initiator remains `https://example.com/`.
  //
  // This value is used to perform the cross-origin check specified in Section
  // 4.3 of https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  //
  // Note: the initiator can be null for browser-initiated top level
  // navigations. This is different from a unique Origin (e.g. in sandboxed
  // iframes).
  const std::optional<url::Origin>& initiator() const { return initiator_; }
  // This method may only be called before Start().
  void set_initiator(const std::optional<url::Origin>& initiator);

  // The request method.  "GET" is the default value. The request method may
  // only be changed before Start() is called. Request methods are
  // case-sensitive, so standard HTTP methods like GET or POST should be
  // specified in uppercase.
  const std::string& method() const { return method_; }
  void set_method(std::string_view method);

#if BUILDFLAG(ENABLE_REPORTING)
  // Reporting upload nesting depth of this request.
  //
  // If the request is not a Reporting upload, the depth is 0.
  //
  // If the request is a Reporting upload, the depth is the max of the depth
  // of the requests reported within it plus 1. (Non-NEL reports are
  // considered to have depth 0.)
  int reporting_upload_depth() const { return reporting_upload_depth_; }
  void set_reporting_upload_depth(int reporting_upload_depth);
#endif

  // The referrer URL for the request
  const std::string& referrer() const { return referrer_; }
  // Sets the referrer URL for the request. Can only be changed before Start()
  // is called. |referrer| is sanitized to remove URL fragment, user name and
  // password. If a referrer policy is set via set_referrer_policy(), then
  // |referrer| should obey the policy; if it doesn't, it will be cleared when
  // the request is started. The referrer URL may be suppressed or changed
  // during the course of the request, for example because of a referrer policy
  // set with set_referrer_policy().
  void SetReferrer(std::string_view referrer);

  // The referrer policy to apply when updating the referrer during redirects.
  // The referrer policy may only be changed before Start() is called. Any
  // referrer set via SetReferrer() is expected to obey the policy set via
  // set_referrer_policy(); otherwise the referrer will be cleared when the
  // request is started.
  ReferrerPolicy referrer_policy() const { return referrer_policy_; }
  void set_referrer_policy(ReferrerPolicy referrer_policy);

  // Sets whether credentials are allowed.
  // If credentials are allowed, the request will send and save HTTP
  // cookies, as well as authentication to the origin server. If not,
  // they will not be sent, however proxy-level authentication will
  // still occur. Setting this will force the LOAD_DO_NOT_SAVE_COOKIES field to
  // be set in |load_flags_|. See https://crbug.com/799935.
  void set_allow_credentials(bool allow_credentials);
  bool allow_credentials() const { return allow_credentials_; }

  // Sets the upload data.
  void set_upload(std::unique_ptr<UploadDataStream> upload);

  // Gets the upload data.
  const UploadDataStream* get_upload_for_testing() const;

  // Returns true if the request has a non-empty message body to upload.
  bool has_upload() const;

  // Set or remove a extra request header.  These methods may only be called
  // before Start() is called, or between receiving a redirect and trying to
  // follow it.
  void SetExtraRequestHeaderByName(std::string_view name,
                                   std::string_view value,
                                   bool overwrite);
  void RemoveRequestHeaderByName(std::string_view name);

  // Sets all extra request headers.  Any extra request headers set by other
  // methods are overwritten by this method.  This method may only be called
  // before Start() is called.  It is an error to call it later.
  void SetExtraRequestHeaders(const HttpRequestHeaders& headers);

  const HttpRequestHeaders& extra_request_headers() const {
    return extra_request_headers_;
  }

  // Gets the total amount of data received from network after SSL decoding and
  // proxy handling. Pertains only to the last URLRequestJob issued by this
  // URLRequest, i.e. reset on redirects, but not reset when multiple roundtrips
  // are used for range requests or auth.
  int64_t GetTotalReceivedBytes() const;

  // Gets the total amount of data sent over the network before SSL encoding and
  // proxy handling. Pertains only to the last URLRequestJob issued by this
  // URLRequest, i.e. reset on redirects, but not reset when multiple roundtrips
  // are used for range requests or auth.
  int64_t GetTotalSentBytes() const;

  // The size of the response body before removing any content encodings.
  // Does not include redirects or sub-requests issued at lower levels (range
  // requests or auth). Only includes bytes which have been read so far,
  // including bytes from the cache.
  int64_t GetRawBodyBytes() const;

  // Returns the current load state for the request. The returned value's
  // |param| field is an optional parameter describing details related to the
  // load state. Not all load states have a parameter.
  LoadStateWithParam GetLoadState() const;

  // Returns a partial representation of the request's state as a value, for
  // debugging.
  base::Value::Dict GetStateAsValue() const;

  // Logs information about the what external object currently blocking the
  // request.  LogUnblocked must be called before resuming the request.  This
  // can be called multiple times in a row either with or without calling
  // LogUnblocked between calls.  |blocked_by| must not be empty.
  void LogBlockedBy(std::string_view blocked_by);

  // Just like LogBlockedBy, but also makes GetLoadState return source as the
  // |param| in the value returned by GetLoadState.  Calling LogUnblocked or
  // LogBlockedBy will clear the load param.  |blocked_by| must not be empty.
  void LogAndReportBlockedBy(std::string_view blocked_by);

  // Logs that the request is no longer blocked by the last caller to
  // LogBlockedBy.
  void LogUnblocked();

  // Returns the current upload progress in bytes. When the upload data is
  // chunked, size is set to zero, but position will not be.
  UploadProgress GetUploadProgress() const;

  // Get response header(s) by name.  This method may only be called
  // once the delegate's OnResponseStarted method has been called.  Headers
  // that appear more than once in the response are coalesced, with values
  // separated by commas (per RFC 2616). This will not work with cookies since
  // comma can be used in cookie values.
  void GetResponseHeaderByName(std::string_view name, std::string* value) const;

  // The time when |this| was constructed.
  base::TimeTicks creation_time() const { return creation_time_; }

  // The time at which the returned response was requested.  For cached
  // responses, this is the last time the cache entry was validated.
  const base::Time& request_time() const { return response_info_.request_time; }

  // The time at which the returned response was generated.  For cached
  // responses, this is the last time the cache entry was validated.
  const base::Time& response_time() const {
    return response_info_.response_time;
  }

  // Indicate if this response was fetched from disk cache.
  bool was_cached() const { return response_info_.was_cached; }

  // Returns true if the URLRequest was delivered over SPDY.
  bool was_fetched_via_spdy() const {
    return response_info_.was_fetched_via_spdy;
  }

  // Returns the host and port that the content was fetched from.  See
  // http_response_info.h for caveats relating to cached content.
  IPEndPoint GetResponseRemoteEndpoint() const;

  // Get all response headers, as a HttpResponseHeaders object.  See comments
  // in HttpResponseHeaders class as to the format of the data.
  HttpResponseHeaders* response_headers() const;

  // Get the SSL connection info.
  const SSLInfo& ssl_info() const { return response_info_.ssl_info; }

  const std::optional<AuthChallengeInfo>& auth_challenge_info() const;

  // Gets timing information related to the request.  Events that have not yet
  // occurred are left uninitialized.  After a second request starts, due to
  // a redirect or authentication, values will be reset.
  //
  // LoadTimingInfo only contains ConnectTiming information and socket IDs for
  // non-cached HTTP responses.
  void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const;

  // Gets the networkd error details of the most recent origin that the network
  // stack makes the request to.
  void PopulateNetErrorDetails(NetErrorDetails* details) const;

  // Gets the remote endpoint of the most recent socket that the network stack
  // used to make this request.
  //
  // Note that GetResponseRemoteEndpoint returns the |socket_address| field from
  // HttpResponseInfo, which is only populated once the response headers are
  // received, and can return cached values for cache revalidation requests.
  // GetTransactionRemoteEndpoint will only return addresses from the current
  // request.
  //
  // Returns true and fills in |endpoint| if the endpoint is available; returns
  // false and leaves |endpoint| unchanged if it is unavailable.
  bool GetTransactionRemoteEndpoint(IPEndPoint* endpoint) const;

  // Get the mime type.  This method may only be called once the delegate's
  // OnResponseStarted method has been called.
  void GetMimeType(std::string* mime_type) const;

  // Get the charset (character encoding).  This method may only be called once
  // the delegate's OnResponseStarted method has been called.
  void GetCharset(std::string* charset) const;

  // Returns the HTTP response code (e.g., 200, 404, and so on).  This method
  // may only be called once the delegate's OnResponseStarted method has been
  // called.  For non-HTTP requests, this method returns -1.
  int GetResponseCode() const;

  // Get the HTTP response info in its entirety.
  const HttpResponseInfo& response_info() const { return response_info_; }

  // Access the LOAD_* flags modifying this request (see load_flags.h).
  int load_flags() const {
    if (cookie_setting_overrides().Has(
            CookieSettingOverride::kStorageAccessGrantEligibleViaHeader)) {
      return partial_load_flags_ | LOAD_BYPASS_CACHE;
    }
    return partial_load_flags_;
  }

  bool is_created_from_network_anonymization_key() const {
    return is_created_from_network_anonymization_key_;
  }

  // Returns the Secure DNS Policy for the request.
  SecureDnsPolicy secure_dns_policy() const { return secure_dns_policy_; }

  void set_maybe_sent_cookies(CookieAccessResultList cookies);
  void set_maybe_stored_cookies(CookieAndLineAccessResultList cookies);

  // These lists contain a list of cookies that are associated with the given
  // request, both those that were sent and accepted, and those that were
  // removed or flagged from the request before use. The status indicates
  // whether they were actually used (INCLUDE), or the reason they were removed
  // or flagged. They are cleared on redirects and other request restarts that
  // cause sent cookies to be recomputed / new cookies to potentially be
  // received (such as calling SetAuth() to send HTTP auth credentials, but not
  // calling ContinueWithCertification() to respond to client cert challenges),
  // and only contain the cookies relevant to the most recent roundtrip.

  // Populated while the http request is being built.
  const CookieAccessResultList& maybe_sent_cookies() const {
    return maybe_sent_cookies_;
  }
  // Populated after the response headers are received.
  const CookieAndLineAccessResultList& maybe_stored_cookies() const {
    return maybe_stored_cookies_;
  }

  // The new flags may change the IGNORE_LIMITS flag only when called
  // before Start() is called, it must only set the flag, and if set,
  // the priority of this request must already be MAXIMUM_PRIORITY.
  void SetLoadFlags(int flags);

  // Controls the Secure DNS behavior to use when creating the socket for this
  // request.
  void SetSecureDnsPolicy(SecureDnsPolicy secure_dns_policy);

  // Returns true if the request is "pending" (i.e., if Start() has been called,
  // and the response has not yet been called).
  bool is_pending() const { return is_pending_; }

  // Returns true if the request is in the process of redirecting to a new
  // URL but has not yet initiated the new request.
  bool is_redirecting() const { return is_redirecting_; }

  // This method is called to start the request.  The delegate will receive
  // a OnResponseStarted callback when the request is started.  The request
  // must have a delegate set before this method is called.
  void Start();

  // This method may be called at any time after Start() has been called to
  // cancel the request.  This method may be called many times, and it has
  // no effect once the response has completed.  It is guaranteed that no
  // methods of the delegate will be called after the request has been
  // cancelled, except that this may call the delegate's OnReadCompleted()
  // during the call to Cancel itself. Returns |ERR_ABORTED| or other net error
  // if there was one.
  int Cancel();

  // Cancels the request and sets the error to |error|, unless the request
  // already failed with another error code (see net_error_list.h). Returns
  // final network error code.
  int CancelWithError(int error);

  // Cancels the request and sets the error to |error| (see net_error_list.h
  // for values) and attaches |ssl_info| as the SSLInfo for that request.  This
  // is useful to attach a certificate and certificate error to a canceled
  // request.
  void CancelWithSSLError(int error, const SSLInfo& ssl_info);

  // Read initiates an asynchronous read from the response, and must only be
  // called after the OnResponseStarted callback is received with a net::OK. If
  // data is available, length and the data will be returned immediately. If the
  // request has failed, an error code will be returned. If data is not yet
  // available, Read returns net::ERR_IO_PENDING, and the Delegate's
  // OnReadComplete method will be called asynchronously with the result of the
  // read, unless the URLRequest is canceled.
  //
  // The |buf| parameter is a buffer to receive the data. If the operation
  // completes asynchronously, the implementation will reference the buffer
  // until OnReadComplete is called. The buffer must be at least |max_bytes| in
  // length.
  //
  // The |max_bytes| parameter is the maximum number of bytes to read.
  int Read(IOBuffer* buf, int max_bytes);

  // This method may be called to follow a redirect that was deferred in
  // response to an OnReceivedRedirect call. If non-null,
  // |modified_headers| are changes applied to the request headers after
  // updating them for the redirect.
  void FollowDeferredRedirect(
      const std::optional<std::vector<std::string>>& removed_headers,
      const std::optional<net::HttpRequestHeaders>& modified_headers);

  // One of the following two methods should be called in response to an
  // OnAuthRequired() callback (and only then).
  // SetAuth will reissue the request with the given credentials.
  // CancelAuth will give up and display the error page.
  void SetAuth(const AuthCredentials& credentials);
  void CancelAuth();

  // This method can be called after the user selects a client certificate to
  // instruct this URLRequest to continue with the request with the
  // certificate.  Pass NULL if the user doesn't have a client certificate.
  void ContinueWithCertificate(scoped_refptr<X509Certificate> client_cert,
                               scoped_refptr<SSLPrivateKey> client_private_key);

  // This method can be called after some error notifications to instruct this
  // URLRequest to ignore the current error and continue with the request.  To
  // cancel the request instead, call Cancel().
  void ContinueDespiteLastError();

  // Aborts the request (without invoking any completion callbacks) and closes
  // the current connection, rather than returning it to the socket pool. Only
  // affects HTTP/1.1 connections and tunnels.
  //
  // Intended to be used in cases where socket reuse can potentially leak data
  // across sites.
  //
  // May only be called after Delegate::OnResponseStarted() has been invoked
  // with net::OK, but before the body has been completely read. After the last
  // body has been read, the socket may have already been handed off to another
  // consumer.
  //
  // Due to transactions potentially being shared by multiple URLRequests in
  // some cases, it is possible the socket may not be immediately closed, but
  // will instead be closed when all URLRequests sharing the socket have been
  // destroyed.
  void AbortAndCloseConnection();

  // Used to specify the context (cookie store, cache) for this request.
  const URLRequestContext* context() const;

  // Returns context()->network_delegate().
  NetworkDelegate* network_delegate() const;

  const NetLogWithSource& net_log() const { return net_log_; }

  // Returns the expected content size if available
  int64_t GetExpectedContentSize() const;

  // Returns the priority level for this request.
  RequestPriority priority() const { return priority_; }

  // Returns the incremental loading priority flag for this request.
  bool priority_incremental() const { return priority_incremental_; }

  // Sets the priority level for this request and any related
  // jobs. Must not change the priority to anything other than
  // MAXIMUM_PRIORITY if the IGNORE_LIMITS load flag is set.
  void SetPriority(RequestPriority priority);

  // Sets the incremental priority flag for this request.
  void SetPriorityIncremental(bool priority_incremental);

  void set_received_response_content_length(int64_t received_content_length) {
    received_response_content_length_ = received_content_length;
  }

  // The number of bytes in the raw response body (before any decompression,
  // etc.). This is only available after the final Read completes.
  int64_t received_response_content_length() const {
    return received_response_content_length_;
  }

  // Available when the request headers are sent, which is before the more
  // general response_info() is available.
  const ProxyChain& proxy_chain() const { return proxy_chain_; }

  // Gets the connection attempts made in the process of servicing this
  // URLRequest. Only guaranteed to be valid if called after the request fails
  // or after the response headers are received.
  ConnectionAttempts GetConnectionAttempts() const;

  const NetworkTrafficAnnotationTag& traffic_annotation() const {
    return traffic_annotation_;
  }

  const std::optional<base::flat_set<net::SourceStream::SourceType>>&
  accepted_stream_types() const {
    return accepted_stream_types_;
  }

  void set_accepted_stream_types(
      const std::optional<base::flat_set<net::SourceStream::SourceType>>&
          types) {
    if (types) {
      DCHECK(!types->contains(net::SourceStream::SourceType::TYPE_NONE));
      DCHECK(!types->contains(net::SourceStream::SourceType::TYPE_UNKNOWN));
    }
    accepted_stream_types_ = types;
  }

  // Sets a callback that will be invoked each time the request is about to
  // be actually sent and will receive actual request headers that are about
  // to hit the wire, including SPDY/QUIC internal headers.
  //
  // Can only be set once before the request is started.
  void SetRequestHeadersCallback(RequestHeadersCallback callback);

  // Sets a callback that will be invoked each time the response is received
  // from the remote party with the actual response headers received. Note this
  // is different from response_headers() getter in that in case of revalidation
  // request, the latter will return cached headers, while the callback will be
  // called with a response from the server.
  void SetResponseHeadersCallback(ResponseHeadersCallback callback);

  // Sets a callback that will be invoked each time a 103 Early Hints response
  // is received from the remote party.
  void SetEarlyResponseHeadersCallback(ResponseHeadersCallback callback);

  // Set a callback that will be invoked when a matching shared dictionary is
  // available to determine whether it is allowed to use the dictionary.
  void SetIsSharedDictionaryReadAllowedCallback(
      base::RepeatingCallback<bool()> callback);

  // Sets socket tag to be applied to all sockets used to execute this request.
  // Must be set before Start() is called.  Only currently supported for HTTP
  // and HTTPS requests on Android; UID tagging requires
  // MODIFY_NETWORK_ACCOUNTING permission.
  // NOTE(pauljensen): Setting a tag disallows sharing of sockets with requests
  // with other tags, which may adversely effect performance by prohibiting
  // connection sharing. In other words use of multiplexed sockets (e.g. HTTP/2
  // and QUIC) will only be allowed if all requests have the same socket tag.
  void set_socket_tag(const SocketTag& socket_tag);
  const SocketTag& socket_tag() const { return socket_tag_; }

  // |upgrade_if_insecure| should be set to true if this request (including
  // redirects) should be upgraded to HTTPS due to an Upgrade-Insecure-Requests
  // requirement.
  void set_upgrade_if_insecure(bool upgrade_if_insecure) {
    upgrade_if_insecure_ = upgrade_if_insecure;
  }
  bool upgrade_if_insecure() const { return upgrade_if_insecure_; }

  // `ad_tagged` should be set to true if the request is thought to be related
  // to advertising.
  void set_ad_tagged(bool ad_tagged) { ad_tagged_ = ad_tagged; }
  bool ad_tagged() const { return ad_tagged_; }

  // By default, client certs will be sent (provided via
  // Delegate::OnCertificateRequested) when cookies are disabled
  // (LOAD_DO_NOT_SEND_COOKIES / LOAD_DO_NOT_SAVE_COOKIES). As described at
  // https://crbug.com/775438, this is not the desired behavior. When
  // |send_client_certs| is set to false, this will suppress the
  // Delegate::OnCertificateRequested callback when cookies/credentials are also
  // suppressed. This method has no effect if credentials are enabled (cookies
  // saved and sent).
  // TODO(crbug.com/40089326): Remove this when the underlying
  // issue is fixed.
  void set_send_client_certs(bool send_client_certs) {
    send_client_certs_ = send_client_certs;
  }
  bool send_client_certs() const { return send_client_certs_; }

  bool is_for_websockets() const { return is_for_websockets_; }

  void SetIdempotency(Idempotency idempotency) { idempotency_ = idempotency; }
  Idempotency GetIdempotency() const { return idempotency_; }

  // Set a SharedDictionaryGetter which will be used to get a shared dictionary
  // for this request. This must not be called after Start() is called.
  void SetSharedDictionaryGetter(
      SharedDictionaryGetter shared_dictionary_getter);

  void set_storage_access_status(
      std::optional<cookie_util::StorageAccessStatus> status) {
    storage_access_status_ = status;
  }

  // Returns the StorageAccessStatus for this request.
  // TODO(https://crbug.com/366284840): move this state out of //net (into
  // network::URLLoader) to respect layering rules.
  std::optional<cookie_util::StorageAccessStatus> storage_access_status()
      const {
    return storage_access_status_;
  }

  static bool DefaultCanUseCookies();

  // Calculates the StorageAccessStatus for this request, according to the
  // NetworkDelegate. Also records metrics.
  // TODO(https://crbug.com/366284840): Move this to URLLoader once the
  // "Activate-Storage-Access: retry" header is handled in URLLoader.
  std::optional<net::cookie_util::StorageAccessStatus>
  CalculateStorageAccessStatus() const;

  base::WeakPtr<URLRequest> GetWeakPtr();

 protected:
  // Allow the URLRequestJob class to control the is_pending() flag.
  void set_is_pending(bool value) { is_pending_ = value; }

  // Setter / getter for the status of the request. Status is represented as a
  // net::Error code. See |status_|.
  int status() const { return status_; }
  void set_status(int status);

  // Returns true if the request failed or was cancelled.
  bool failed() const;

  // Returns the error status of the request.

  // Allow the URLRequestJob to redirect this request. If non-null,
  // |removed_headers| and |modified_headers| are changes
  // applied to the request headers after updating them for the redirect.
  void Redirect(const RedirectInfo& redirect_info,
                const std::optional<std::vector<std::string>>& removed_headers,
                const std::optional<net::HttpRequestHeaders>& modified_headers);

  // Allow the URLRequestJob to retry this request, after having activated
  // Storage Access (if possible).
  void RetryWithStorageAccess();

  // Called by URLRequestJob to allow interception when a redirect occurs.
  void NotifyReceivedRedirect(const RedirectInfo& redirect_info,
                              bool* defer_redirect);

 private:
  friend class URLRequestJob;

  // For testing purposes.
  // TODO(maksims): Remove this.
  friend class TestNetworkDelegate;

  // Resumes or blocks a request paused by the NetworkDelegate::OnBeforeRequest
  // handler. If |blocked| is true, the request is blocked and an error page is
  // returned indicating so. This should only be called after Start is called
  // and OnBeforeRequest returns true (signalling that the request should be
  // paused).
  void BeforeRequestComplete(int error);

  void StartJob(std::unique_ptr<URLRequestJob> job);

  // Restarting involves replacing the current job with a new one such as what
  // happens when following a HTTP redirect.
  void RestartWithJob(std::unique_ptr<URLRequestJob> job);
  void PrepareToRestart();

  // Cancels the request and set the error and ssl info for this request to the
  // passed values. Returns the error that was set.
  int DoCancel(int error, const SSLInfo& ssl_info);

  // Called by the URLRequestJob when the headers are received, before any other
  // method, to allow caching of load timing information.
  void OnHeadersComplete();

  // Notifies the network delegate that the request has been completed.
  // This does not imply a successful completion. Also a canceled request is
  // considered completed.
  void NotifyRequestCompleted();

  // Called by URLRequestJob to allow interception when the final response
  // occurs.
  void NotifyResponseStarted(int net_error);

  // These functions delegate to |delegate_|.  See URLRequest::Delegate for the
  // meaning of these functions.
  int NotifyConnected(const TransportInfo& info,
                      CompletionOnceCallback callback);
  void NotifyAuthRequired(std::unique_ptr<AuthChallengeInfo> auth_info);
  void NotifyCertificateRequested(SSLCertRequestInfo* cert_request_info);
  void NotifySSLCertificateError(int net_error,
                                 const SSLInfo& ssl_info,
                                 bool fatal);
  void NotifyReadCompleted(int bytes_read);

  // This function delegates to the NetworkDelegate if it is not nullptr.
  // Otherwise, cookies can be used unless SetDefaultCookiePolicyToBlock() has
  // been called.
  bool CanSetCookie(const net::CanonicalCookie& cookie,
                    CookieOptions* options,
                    const net::FirstPartySetMetadata& first_party_set_metadata,
                    CookieInclusionStatus* inclusion_status) const;

  // Called just before calling a delegate that may block a request. |type|
  // should be the delegate's event type,
  // e.g. NetLogEventType::NETWORK_DELEGATE_AUTH_REQUIRED.
  void OnCallToDelegate(NetLogEventType type);
  // Called when the delegate lets a request continue.  Also called on
  // cancellation. `error` is an optional error code associated with
  // completion. It's only for logging purposes, and will not directly cancel
  // the request if it's a value other than OK.
  void OnCallToDelegateComplete(int error = OK);

  // Records the referrer policy of the given request, bucketed by
  // whether the request is same-origin or not. To save computation,
  // takes this fact as a boolean parameter rather than dynamically
  // checking.
  void RecordReferrerGranularityMetrics(bool request_is_same_origin) const;

  // Creates a partial IsolationInfo with the information accessible from the
  // NetworkAnonymiationKey.
  net::IsolationInfo CreateIsolationInfoFromNetworkAnonymizationKey(
      const NetworkAnonymizationKey& network_anonymization_key);

  // Contextual information used for this request. Cannot be NULL. This contains
  // most of the dependencies which are shared between requests (disk cache,
  // cookie store, socket pool, etc.)
  raw_ptr<const URLRequestContext> context_;

  // Tracks the time spent in various load states throughout this request.
  NetLogWithSource net_log_;

  std::unique_ptr<URLRequestJob> job_;
  std::unique_ptr<UploadDataStream> upload_data_stream_;

  std::vector<GURL> url_chain_;
  SiteForCookies site_for_cookies_;

  IsolationInfo isolation_info_;
  // The cookie partition key for the request. Partitioned cookies should be set
  // using this key and only partitioned cookies with this partition key should
  // be sent. The cookie partition key is optional(nullopt) if cookie
  // partitioning is not enabled, or if the NIK has no top-frame site.
  //
  // Unpartitioned cookies are unaffected by this field.
  std::optional<CookiePartitionKey> cookie_partition_key_ = std::nullopt;

  bool force_ignore_site_for_cookies_ = false;
  bool force_main_frame_for_same_site_cookies_ = false;
  CookieSettingOverrides cookie_setting_overrides_;

  std::optional<url::Origin> initiator_;
  GURL delegate_redirect_url_;
  std::string method_;  // "GET", "POST", etc. Case-sensitive.
  std::string referrer_;
  ReferrerPolicy referrer_policy_ =
      ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  RedirectInfo::FirstPartyURLPolicy first_party_url_policy_ =
      RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL;
  HttpRequestHeaders extra_request_headers_;
  // Flags indicating the request type for the load. Expected values are LOAD_*
  // enums above.
  int partial_load_flags_ = LOAD_NORMAL;
  // Whether the request is allowed to send credentials in general. Set by
  // caller.
  bool allow_credentials_ = true;
  SecureDnsPolicy secure_dns_policy_ = SecureDnsPolicy::kAllow;

  CookieAccessResultList maybe_sent_cookies_;
  CookieAndLineAccessResultList maybe_stored_cookies_;

#if BUILDFLAG(ENABLE_REPORTING)
  int reporting_upload_depth_ = 0;
#endif

  // Never access methods of the |delegate_| directly. Always use the
  // Notify... methods for this.
  raw_ptr<Delegate> delegate_;

  const bool is_for_websockets_;

  // Current error status of the job, as a net::Error code. When the job is
  // busy, it is ERR_IO_PENDING. When the job is idle (either completed, or
  // awaiting a call from the URLRequestDelegate before continuing the request),
  // it is OK. If the request has been cancelled without a specific error, it is
  // ERR_ABORTED. And on failure, it's the corresponding error code for that
  // error.
  //
  // |status_| may bounce between ERR_IO_PENDING and OK as a request proceeds,
  // but once an error is encountered or the request is canceled, it will take
  // the appropriate error code and never change again. If multiple failures
  // have been encountered, this will be the first error encountered.
  int status_ = OK;

  bool is_created_from_network_anonymization_key_ = false;

  // The HTTP response info, lazily initialized.
  HttpResponseInfo response_info_;

  // Tells us whether the job is outstanding. This is true from the time
  // Start() is called to the time we dispatch RequestComplete and indicates
  // whether the job is active.
  bool is_pending_ = false;

  // Indicates if the request is in the process of redirecting to a new
  // location.  It is true from the time the headers complete until a
  // new request begins.
  bool is_redirecting_ = false;

  // Number of times we're willing to redirect.  Used to guard against
  // infinite redirects.
  int redirect_limit_;

  // Cached value for use after we've orphaned the job handling the
  // first transaction in a request involving redirects.
  UploadProgress final_upload_progress_;

  // The priority level for this request.  Objects like
  // ClientSocketPool use this to determine which URLRequest to
  // allocate sockets to first.
  RequestPriority priority_;

  // The incremental flag for this request that indicates if it should be
  // loaded concurrently with other resources of the same priority for
  // protocols that support HTTP extensible priorities (RFC 9218).
  // Currently only used in HTTP/3.
  bool priority_incremental_ = kDefaultPriorityIncremental;

  // If |calling_delegate_| is true, the event type of the delegate being
  // called.
  NetLogEventType delegate_event_type_ = NetLogEventType::FAILED;

  // True if this request is currently calling a delegate, or is blocked waiting
  // for the URL request or network delegate to resume it.
  bool calling_delegate_ = false;

  // An optional parameter that provides additional information about what
  // |this| is currently being blocked by.
  std::string blocked_by_;
  bool use_blocked_by_as_load_param_ = false;

  // Safe-guard to ensure that we do not send multiple "I am completed"
  // messages to network delegate.
  // TODO(battre): Remove this. http://crbug.com/89049
  bool has_notified_completion_ = false;

  int64_t received_response_content_length_ = 0;

  base::TimeTicks creation_time_;

  // Timing information for the most recent request.  Its start times are
  // populated during Start(), and the rest are populated in OnResponseReceived.
  LoadTimingInfo load_timing_info_;

  // The proxy chain used for this request, if any.
  ProxyChain proxy_chain_;

  // If not null, the network service will not advertise any stream types
  // (via Accept-Encoding) that are not listed. Also, it will not attempt
  // decoding any non-listed stream types.
  std::optional<base::flat_set<net::SourceStream::SourceType>>
      accepted_stream_types_;

  const NetworkTrafficAnnotationTag traffic_annotation_;

  SocketTag socket_tag_;

  // See Set{Request|Response,EarlyResponse}HeadersCallback() above for details.
  RequestHeadersCallback request_headers_callback_;
  ResponseHeadersCallback early_response_headers_callback_;
  ResponseHeadersCallback response_headers_callback_;

  // See SetIsSharedDictionaryReadAllowedCallback() above for details.
  base::RepeatingCallback<bool()> is_shared_dictionary_read_allowed_callback_;

  bool upgrade_if_insecure_ = false;

  bool ad_tagged_ = false;

  bool send_client_certs_ = true;

  // Idempotency of the request.
  Idempotency idempotency_ = DEFAULT_IDEMPOTENCY;

  SharedDictionaryGetter shared_dictionary_getter_;

  // The storage access status for this request. If this is nullopt, this
  // request will not include the Sec-Fetch-Storage-Access header.
  std::optional<net::cookie_util::StorageAccessStatus> storage_access_status_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<URLRequest> weak_factory_{this};
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_H_
