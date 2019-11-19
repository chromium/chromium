// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_H_
#define NET_URL_REQUEST_URL_REQUEST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/debug/leak_tracker.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/network_delegate.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace net {

class CookieOptions;
class IOBuffer;
struct LoadTimingInfo;
struct RedirectInfo;
class SSLCertRequestInfo;
class SSLInfo;
class SSLPrivateKey;
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
  // Callback function implemented by protocol handlers to create new jobs.
  // The factory may return NULL to indicate an error, which will cause other
  // factories to be queried.  If no factory handles the request, then the
  // default job will be used.
  typedef URLRequestJob*(ProtocolFactory)(URLRequest* request,
                                          NetworkDelegate* network_delegate,
                                          const std::string& scheme);

  // A ReferrerPolicy for the request can be set with
  // set_referrer_policy() and controls the contents of the Referer
  // header when URLRequest follows server redirects. Note that setting
  // a ReferrerPolicy on the request has no effect on the Referer header
  // of the initial leg of the request; the caller is responsible for
  // setting the initial Referer, and the ReferrerPolicy only controls
  // what happens to the Referer while following redirects.
  //
  // NOTE: This enum is persisted to histograms. Do not change or reorder
  // values.
  // TODO(~M82): Once the Net.URLRequest.ReferrerPolicyForRequest
  // metric is retired, remove this notice.
  enum ReferrerPolicy {
    // Clear the referrer header if the header value is HTTPS but the request
    // destination is HTTP. This is the default behavior of URLRequest.
    CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE = 0,
    // A slight variant on CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
    // If the request destination is HTTP, an HTTPS referrer will be cleared. If
    // the request's destination is cross-origin with the referrer (but does not
    // downgrade), the referrer's granularity will be stripped down to an origin
    // rather than a full URL. Same-origin requests will send the full referrer.
    REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN = 1,
    // Strip the referrer down to an origin when the origin of the referrer is
    // different from the destination's origin.
    ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN = 2,
    // Never change the referrer.
    NEVER_CLEAR_REFERRER = 3,
    // Strip the referrer down to the origin regardless of the redirect
    // location.
    ORIGIN = 4,
    // Clear the referrer when the request's referrer is cross-origin with
    // the request's destination.
    CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN = 5,
    // Strip the referrer down to the origin, but clear it entirely if the
    // referrer value is HTTPS and the destination is HTTP.
    ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE = 6,
    // Always clear the referrer regardless of the request destination.
    NO_REFERRER = 7,
    MAX_REFERRER_POLICY = NO_REFERRER
  };

  // First-party URL redirect policy: During server redirects, the first-party
  // URL for cookies normally doesn't change. However, if the request is a
  // top-level first-party request, the first-party URL should be updated to the
  // URL on every redirect.
  enum FirstPartyURLPolicy {
    NEVER_CHANGE_FIRST_PARTY_URL,
    UPDATE_FIRST_PARTY_URL_ON_REDIRECT,
  };

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
    virtual ~Delegate() {}
  };

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
  // TODO(mkwst): Convert this to a 'url::Origin'. Several callsites are using
  // this value as a proxy for the "top-level frame URL", which is simply
  // incorrect and fragile. We don't need the full URL for any //net checks,
  // so we should drop the pieces we don't need. https://crbug.com/577565
  const GURL& site_for_cookies() const { return site_for_cookies_; }
  // This method may only be called before Start().
  void set_site_for_cookies(const GURL& site_for_cookies);

  // This key is used to isolate requests from different contexts in accessing
  // shared network resources like the cache.
  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }
  void set_network_isolation_key(const NetworkIsolationKey& key) {
    network_isolation_key_ = key;
  }

  // Indicate whether SameSite cookies should be attached even though the
  // request is cross-site.
  bool attach_same_site_cookies() const { return attach_same_site_cookies_; }
  void set_attach_same_site_cookies(bool attach) {
    attach_same_site_cookies_ = attach;
  }

  // The first-party URL policy to apply when updating the first party URL
  // during redirects. The first-party URL policy may only be changed before
  // Start() is called.
  FirstPartyURLPolicy first_party_url_policy() const {
    return first_party_url_policy_;
  }
  void set_first_party_url_policy(FirstPartyURLPolicy first_party_url_policy);

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
  const base::Optional<url::Origin>& initiator() const { return initiator_; }
  // This method may only be called before Start().
  void set_initiator(const base::Optional<url::Origin>& initiator);

  // The request method, as an uppercase string.  "GET" is the default value.
  // The request method may only be changed before Start() is called and
  // should only be assigned an uppercase value.
  const std::string& method() const { return method_; }
  void set_method(const std::string& method);

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
  void SetReferrer(const std::string& referrer);

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
  // still occur.
  // Setting this to false is equivalent to setting the
  // LOAD_DO_NOT_SAVE_COOKIES, LOAD_DO_NOT_SEND_COOKIES, and
  // LOAD_DO_NOT_SEND_AUTH_DATA flags. See https://crbug.com/799935.
  void set_allow_credentials(bool allow_credentials);

  // Sets the upload data.
  void set_upload(std::unique_ptr<UploadDataStream> upload);

  // Gets the upload data.
  const UploadDataStream* get_upload_for_testing() const;

  // Returns true if the request has a non-empty message body to upload.
  bool has_upload() const;

  // Set or remove a extra request header.  These methods may only be called
  // before Start() is called, or between receiving a redirect and trying to
  // follow it.
  void SetExtraRequestHeaderByName(const std::string& name,
                                   const std::string& value,
                                   bool overwrite);
  void RemoveRequestHeaderByName(const std::string& name);

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
  base::Value GetStateAsValue() const;

  // Logs information about the what external object currently blocking the
  // request.  LogUnblocked must be called before resuming the request.  This
  // can be called multiple times in a row either with or without calling
  // LogUnblocked between calls.  |blocked_by| must not be NULL or have length
  // 0.
  void LogBlockedBy(const char* blocked_by);

  // Just like LogBlockedBy, but also makes GetLoadState return source as the
  // |param| in the value returned by GetLoadState.  Calling LogUnblocked or
  // LogBlockedBy will clear the load param.  |blocked_by| must not be NULL or
  // have length 0.
  void LogAndReportBlockedBy(const char* blocked_by);

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
  void GetResponseHeaderByName(const std::string& name,
                               std::string* value) const;

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

  const base::Optional<AuthChallengeInfo>& auth_challenge_info() const;

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
  int load_flags() const { return load_flags_; }

  // Returns PrivacyMode that should be used for the request. Updated every time
  // the request is redirected.
  PrivacyMode privacy_mode() { return privacy_mode_; }

  // Returns whether secure DNS should be disabled for the request.
  bool disable_secure_dns() { return disable_secure_dns_; }

  void set_maybe_sent_cookies(CookieStatusList cookies);
  void set_maybe_stored_cookies(CookieAndLineStatusList cookies);

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
  const CookieStatusList& maybe_sent_cookies() const {
    return maybe_sent_cookies_;
  }
  // Populated after the response headers are received.
  const CookieAndLineStatusList& maybe_stored_cookies() const {
    return maybe_stored_cookies_;
  }

  // The new flags may change the IGNORE_LIMITS flag only when called
  // before Start() is called, it must only set the flag, and if set,
  // the priority of this request must already be MAXIMUM_PRIORITY.
  void SetLoadFlags(int flags);

  // Sets whether secure DNS should be disabled for the request.
  void SetDisableSecureDns(bool disable_secure_dns);

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
      const base::Optional<std::vector<std::string>>& removed_headers,
      const base::Optional<net::HttpRequestHeaders>& modified_headers);

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

  // Used to specify the context (cookie store, cache) for this request.
  const URLRequestContext* context() const;

  const NetLogWithSource& net_log() const { return net_log_; }

  // Returns the expected content size if available
  int64_t GetExpectedContentSize() const;

  // Returns the priority level for this request.
  RequestPriority priority() const { return priority_; }

  // Sets the priority level for this request and any related
  // jobs. Must not change the priority to anything other than
  // MAXIMUM_PRIORITY if the IGNORE_LIMITS load flag is set.
  void SetPriority(RequestPriority priority);

  void set_received_response_content_length(int64_t received_content_length) {
    received_response_content_length_ = received_content_length;
  }

  // The number of bytes in the raw response body (before any decompression,
  // etc.). This is only available after the final Read completes. Not available
  // for FTP responses.
  int64_t received_response_content_length() const {
    return received_response_content_length_;
  }

  // Available when the request headers are sent, which is before the more
  // general response_info() is available.
  const ProxyServer& proxy_server() const { return proxy_server_; }

  // Gets the connection attempts made in the process of servicing this
  // URLRequest. Only guaranteed to be valid if called after the request fails
  // or after the response headers are received.
  void GetConnectionAttempts(ConnectionAttempts* out) const;

  // Gets the over the wire raw header size of the response after https
  // encryption, 0 for cached responses.
  int raw_header_size() const { return raw_header_size_; }

  // Returns the error status of the request.
  // Do not use! Going to be protected!
  const URLRequestStatus& status() const { return status_; }

  const NetworkTrafficAnnotationTag& traffic_annotation() const {
    return traffic_annotation_;
  }

  // Sets a callback that will be invoked each time the request is about to
  // be actually sent and will receive actual request headers that are about
  // to hit the wire, including SPDY/QUIC internal headers and any additional
  // request headers set via BeforeSendHeaders hooks. Can only be set once
  // before the request is started.
  void SetRequestHeadersCallback(RequestHeadersCallback callback);

  // Sets a callback that will be invoked each time the response is received
  // from the remote party with the actual response headers recieved. Note this
  // is different from response_headers() getter in that in case of revalidation
  // request, the latter will return cached headers, while the callback will be
  // called with a response from the server.
  void SetResponseHeadersCallback(ResponseHeadersCallback callback);

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

  base::WeakPtr<URLRequest> GetWeakPtr();

 protected:
  // Allow the URLRequestJob class to control the is_pending() flag.
  void set_is_pending(bool value) { is_pending_ = value; }

  // Allow the URLRequestJob class to set our status too.
  void set_status(URLRequestStatus status);

  // Allow the URLRequestJob to redirect this request. If non-null,
  // |removed_headers| and |modified_headers| are changes
  // applied to the request headers after updating them for the redirect.
  void Redirect(
      const RedirectInfo& redirect_info,
      const base::Optional<std::vector<std::string>>& removed_headers,
      const base::Optional<net::HttpRequestHeaders>& modified_headers);

  // Called by URLRequestJob to allow interception when a redirect occurs.
  void NotifyReceivedRedirect(const RedirectInfo& redirect_info,
                              bool* defer_redirect);

 private:
  friend class URLRequestJob;
  friend class URLRequestContext;

  // For testing purposes.
  // TODO(maksims): Remove this.
  friend class TestNetworkDelegate;

  // URLRequests are always created by calling URLRequestContext::CreateRequest.
  //
  // If no network delegate is passed in, will use the ones from the
  // URLRequestContext.
  URLRequest(const GURL& url,
             RequestPriority priority,
             Delegate* delegate,
             const URLRequestContext* context,
             NetworkDelegate* network_delegate,
             NetworkTrafficAnnotationTag traffic_annotation);

  // Resumes or blocks a request paused by the NetworkDelegate::OnBeforeRequest
  // handler. If |blocked| is true, the request is blocked and an error page is
  // returned indicating so. This should only be called after Start is called
  // and OnBeforeRequest returns true (signalling that the request should be
  // paused).
  void BeforeRequestComplete(int error);

  // TODO(mmenke):  Make this take a scoped_ptr.
  void StartJob(URLRequestJob* job);

  // Restarting involves replacing the current job with a new one such as what
  // happens when following a HTTP redirect.
  void RestartWithJob(URLRequestJob* job);
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
  void NotifyResponseStarted(const URLRequestStatus& status);

  // These functions delegate to |delegate_|.  See URLRequest::Delegate for the
  // meaning of these functions.
  void NotifyAuthRequired(std::unique_ptr<AuthChallengeInfo> auth_info);
  void NotifyCertificateRequested(SSLCertRequestInfo* cert_request_info);
  void NotifySSLCertificateError(int net_error,
                                 const SSLInfo& ssl_info,
                                 bool fatal);
  void NotifyReadCompleted(int bytes_read);

  // These functions delegate to |network_delegate_| if it is not NULL.
  // If |network_delegate_| is NULL, cookies can be used unless
  // SetDefaultCookiePolicyToBlock() has been called.
  bool CanGetCookies(const CookieList& cookie_list) const;
  bool CanSetCookie(const net::CanonicalCookie& cookie,
                    CookieOptions* options) const;
  PrivacyMode DeterminePrivacyMode() const;

  // Called just before calling a delegate that may block a request. |type|
  // should be the delegate's event type,
  // e.g. NetLogEventType::NETWORK_DELEGATE_AUTH_REQUIRED.
  void OnCallToDelegate(NetLogEventType type);
  // Called when the delegate lets a request continue.  Also called on
  // cancellation.
  void OnCallToDelegateComplete();

  // Records the referrer policy of the given request, bucketed by
  // whether the request is same-origin or not. To save computation,
  // takes this fact as a boolean parameter rather than dynamically
  // checking.
  void RecordReferrerGranularityMetrics(bool request_is_same_origin) const;

  // Contextual information used for this request. Cannot be NULL. This contains
  // most of the dependencies which are shared between requests (disk cache,
  // cookie store, socket pool, etc.)
  const URLRequestContext* context_;

  NetworkDelegate* network_delegate_;

  // Tracks the time spent in various load states throughout this request.
  NetLogWithSource net_log_;

  std::unique_ptr<URLRequestJob> job_;
  std::unique_ptr<UploadDataStream> upload_data_stream_;

  std::vector<GURL> url_chain_;
  GURL site_for_cookies_;

  NetworkIsolationKey network_isolation_key_;

  bool attach_same_site_cookies_;
  base::Optional<url::Origin> initiator_;
  GURL delegate_redirect_url_;
  std::string method_;  // "GET", "POST", etc. Should be all uppercase.
  std::string referrer_;
  ReferrerPolicy referrer_policy_;
  FirstPartyURLPolicy first_party_url_policy_;
  HttpRequestHeaders extra_request_headers_;
  int load_flags_;  // Flags indicating the request type for the load;
                    // expected values are LOAD_* enums above.
  PrivacyMode privacy_mode_;
  bool disable_secure_dns_;

  CookieStatusList maybe_sent_cookies_;
  CookieAndLineStatusList maybe_stored_cookies_;

#if BUILDFLAG(ENABLE_REPORTING)
  int reporting_upload_depth_;
#endif

  // Never access methods of the |delegate_| directly. Always use the
  // Notify... methods for this.
  Delegate* delegate_;

  // Current error status of the job. When no error has been encountered, this
  // will be SUCCESS. If multiple errors have been encountered, this will be
  // the first non-SUCCESS status seen.
  URLRequestStatus status_;

  // The HTTP response info, lazily initialized.
  HttpResponseInfo response_info_;

  // Tells us whether the job is outstanding. This is true from the time
  // Start() is called to the time we dispatch RequestComplete and indicates
  // whether the job is active.
  bool is_pending_;

  // Indicates if the request is in the process of redirecting to a new
  // location.  It is true from the time the headers complete until a
  // new request begins.
  bool is_redirecting_;

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

  // If |calling_delegate_| is true, the event type of the delegate being
  // called.
  NetLogEventType delegate_event_type_;

  // True if this request is currently calling a delegate, or is blocked waiting
  // for the URL request or network delegate to resume it.
  bool calling_delegate_;

  // An optional parameter that provides additional information about what
  // |this| is currently being blocked by.
  std::string blocked_by_;
  bool use_blocked_by_as_load_param_;

  base::debug::LeakTracker<URLRequest> leak_tracker_;

  // Safe-guard to ensure that we do not send multiple "I am completed"
  // messages to network delegate.
  // TODO(battre): Remove this. http://crbug.com/89049
  bool has_notified_completion_;

  int64_t received_response_content_length_;

  base::TimeTicks creation_time_;

  // Timing information for the most recent request.  Its start times are
  // populated during Start(), and the rest are populated in OnResponseReceived.
  LoadTimingInfo load_timing_info_;

  // The proxy server used for this request, if any.
  ProxyServer proxy_server_;

  // The raw header size of the response.
  int raw_header_size_;

  const NetworkTrafficAnnotationTag traffic_annotation_;

  SocketTag socket_tag_;

  // See Set{Request|Response}HeadersCallback() above for details.
  RequestHeadersCallback request_headers_callback_;
  ResponseHeadersCallback response_headers_callback_;

  bool upgrade_if_insecure_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<URLRequest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLRequest);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_H_
