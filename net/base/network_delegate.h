// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_DELEGATE_H_
#define NET_BASE_NETWORK_DELEGATE_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/threading/thread_checker.h"
#include "net/base/auth.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/proxy_resolution/proxy_retry_info.h"

class GURL;

namespace url {
class Origin;
}

namespace net {

// NOTE: Layering violations!
// We decided to accept these violations (depending
// on other net/ submodules from net/base/), because otherwise NetworkDelegate
// would have to be broken up into too many smaller interfaces targeted to each
// submodule. Also, since the lower levels in net/ may callback into higher
// levels, we may encounter dangerous casting issues.
//
// NOTE: It is not okay to add any compile-time dependencies on symbols outside
// of net/base here, because we have a net_base library. Forward declarations
// are ok.
class CookieOptions;
class CookieInclusionStatus;
class HttpRequestHeaders;
class HttpResponseHeaders;
class IPEndPoint;
class URLRequest;

class NET_EXPORT NetworkDelegate {
 public:
  virtual ~NetworkDelegate();

  // Notification interface called by the network stack. Note that these
  // functions mostly forward to the private virtuals. They also add some sanity
  // checking on parameters. See the corresponding virtuals for explanations of
  // the methods and their arguments.
  int NotifyBeforeURLRequest(URLRequest* request,
                             CompletionOnceCallback callback,
                             GURL* new_url);
  using OnBeforeStartTransactionCallback =
      base::OnceCallback<void(int, const std::optional<HttpRequestHeaders>&)>;
  int NotifyBeforeStartTransaction(URLRequest* request,
                                   const HttpRequestHeaders& headers,
                                   OnBeforeStartTransactionCallback callback);
  int NotifyHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& remote_endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url);
  void NotifyBeforeRedirect(URLRequest* request,
                            const GURL& new_location);
  void NotifyBeforeRetry(URLRequest* request);
  void NotifyResponseStarted(URLRequest* request, int net_error);
  void NotifyCompleted(URLRequest* request, bool started, int net_error);
  void NotifyURLRequestDestroyed(URLRequest* request);
  void NotifyPACScriptError(int line_number, const std::u16string& error);
  bool AnnotateAndMoveUserBlockedCookies(
      const URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      CookieAccessResultList& maybe_included_cookies,
      CookieAccessResultList& excluded_cookies);
  bool CanSetCookie(const URLRequest& request,
                    const net::CanonicalCookie& cookie,
                    CookieOptions* options,
                    const net::FirstPartySetMetadata& first_party_set_metadata,
                    CookieInclusionStatus* inclusion_status);

  std::optional<cookie_util::StorageAccessStatus> GetStorageAccessStatus(
      const URLRequest& request) const;

  // Returns true if the `Sec-Fetch-Storage-Access` request header flow is
  // enabled in the given context.
  bool IsStorageAccessHeaderEnabled(const url::Origin* top_frame_origin,
                                    const GURL& url) const;

  // PrivacySetting is kStateDisallowed iff the given |url| has to be
  // requested over connection that is not tracked by the server.
  //
  // Usually PrivacySetting is kStateAllowed, unless user privacy settings
  // block cookies from being get or set.
  //
  // It may be set to kPartitionedStateAllowedOnly if the request allows
  // partitioned state to be sent over the connection, but unpartitioned
  // state should be blocked.
  enum class PrivacySetting {
    kStateAllowed,
    kStateDisallowed,
    // First-party requests will never have this setting.
    kPartitionedStateAllowedOnly,
  };
  PrivacySetting ForcePrivacyMode(const URLRequest& request) const;

  bool CancelURLRequestWithPolicyViolatingReferrerHeader(
      const URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const;

  bool CanQueueReportingReport(const url::Origin& origin) const;
  void CanSendReportingReports(
      std::set<url::Origin> origins,
      base::OnceCallback<void(std::set<url::Origin>)> result_callback) const;
  bool CanSetReportingClient(const url::Origin& origin,
                             const GURL& endpoint) const;
  bool CanUseReportingClient(const url::Origin& origin,
                             const GURL& endpoint) const;

 protected:
  // Adds the given ExclusionReason to all cookies in
  // `mayble_included_cookies`, and moves the contents of
  // `maybe_included_cookies` to `excluded_cookies`.
  static void ExcludeAllCookies(
      net::CookieInclusionStatus::ExclusionReason reason,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies);

  // Does the same as ExcludeAllCookies but will still include
  // cookies that are partitioned if cookies are not disabled
  // globally.
  static void ExcludeAllCookiesExceptPartitioned(
      net::CookieInclusionStatus::ExclusionReason reason,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies);

  // Moves any cookie in `maybe_included_cookies` that has an ExclusionReason
  // into `excluded_cookies`.
  static void MoveExcludedCookies(
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies);

  THREAD_CHECKER(thread_checker_);

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkDelegateTest, ExcludeAllCookies);
  FRIEND_TEST_ALL_PREFIXES(NetworkDelegateTest, MoveExcludedCookies);
  // This is the interface for subclasses of NetworkDelegate to implement. These
  // member functions will be called by the respective public notification
  // member function, which will perform basic sanity checking.
  //
  // Note that these member functions refer to URLRequests which may be canceled
  // or destroyed at any time. Implementations which return ERR_IO_PENDING must
  // also implement OnURLRequestDestroyed and OnCompleted to handle cancelation.
  // See below for details.
  //
  // (NetworkDelegateImpl has default implementations of these member functions.
  // NetworkDelegate implementations should consider subclassing
  // NetworkDelegateImpl.)

  // Called before a request is sent. Allows the delegate to rewrite the URL
  // being fetched by modifying |new_url|. If set, the URL must be valid. The
  // reference fragment from the original URL is not automatically appended to
  // |new_url|; callers are responsible for copying the reference fragment if
  // desired.
  //
  // Returns OK to continue with the request, ERR_IO_PENDING if the result is
  // not ready yet, and any other status code to cancel the request.  If
  // returning ERR_IO_PENDING, call |callback| when the result is ready. Note,
  // however, that a pending operation may be cancelled by
  // OnURLRequestDestroyed. Once cancelled, |request| and |new_url| become
  // invalid and |callback| may not be called.
  //
  // The default implementation returns OK (continue with request).
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 CompletionOnceCallback callback,
                                 GURL* new_url) = 0;

  // Called right before the network transaction starts. Allows the delegate to
  // read |headers| and modify them by passing a new copy to |callback| before
  // they get sent out.
  //
  // Returns OK to continue with the request, ERR_IO_PENDING if the result is
  // not ready yet, and any other status code to cancel the request. If
  // returning ERR_IO_PENDING, call |callback| when the result is ready. Note,
  // however, that a pending operation may be cancelled by OnURLRequestDestroyed
  // or OnCompleted. Once cancelled, |request| and |headers| become invalid and
  // |callback| may not be called.
  //
  // The default implementation returns OK (continue with request).
  virtual int OnBeforeStartTransaction(
      URLRequest* request,
      const HttpRequestHeaders& headers,
      OnBeforeStartTransactionCallback callback) = 0;

  // Called for HTTP requests when the headers have been received.
  // |original_response_headers| contains the headers as received over the
  // network, these must not be modified. |override_response_headers| can be set
  // to new values, that should be considered as overriding
  // |original_response_headers|.
  // If the response is a redirect, and the Location response header value is
  // identical to |preserve_fragment_on_redirect_url|, then the redirect is
  // never blocked and the reference fragment is not copied from the original
  // URL to the redirection target.
  //
  // Returns OK to continue with the request, ERR_IO_PENDING if the result is
  // not ready yet, and any other status code to cancel the request. If
  // returning ERR_IO_PENDING, call |callback| when the result is ready. Note,
  // however, that a pending operation may be cancelled by
  // OnURLRequestDestroyed. Once cancelled, |request|,
  // |original_response_headers|, |override_response_headers|, and
  // |preserve_fragment_on_redirect_url| become invalid and |callback| may not
  // be called.
  virtual int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& remote_endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url) = 0;

  // Called right after a redirect response code was received. |new_location| is
  // only valid for the duration of the call.
  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) = 0;

  virtual void OnBeforeRetry(URLRequest* request) = 0;

  // This corresponds to URLRequestDelegate::OnResponseStarted.
  virtual void OnResponseStarted(URLRequest* request, int net_error) = 0;

  // Indicates that the URL request has been completed or failed.
  // |started| indicates whether the request has been started. If false,
  // some information like the socket address is not available.
  virtual void OnCompleted(URLRequest* request,
                           bool started,
                           int net_error) = 0;

  // Called when an URLRequest is being destroyed. Note that the request is
  // being deleted, so it's not safe to call any methods that may result in
  // a virtual method call.
  virtual void OnURLRequestDestroyed(URLRequest* request) = 0;

  // Corresponds to ProxyResolverJSBindings::OnError.
  virtual void OnPACScriptError(int line_number,
                                const std::u16string& error) = 0;

  // Called when reading cookies to allow the network delegate to block access
  // to individual cookies, by adding the appropriate ExclusionReason and moving
  // them to the `excluded_cookies` list.  This method will never be invoked
  // when LOAD_DO_NOT_SEND_COOKIES is specified.
  //
  // Returns false if the delegate has blocked access to all cookies; true
  // otherwise.
  virtual bool OnAnnotateAndMoveUserBlockedCookies(
      const URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) = 0;

  // Called when a cookie is set to allow the network delegate to block access
  // to the cookie. If the cookie is allowed, `inclusion_status` may be updated
  // to include reason to warn about the given cookie according to the user
  // cookie-blocking settings; Otherwise, `inclusion_status` may be updated with
  // the proper exclusion reasons, if not then proper reasons need to be
  // manually added in the caller. This method will never be invoked when
  // LOAD_DO_NOT_SAVE_COOKIES is specified.
  virtual bool OnCanSetCookie(
      const URLRequest& request,
      const CanonicalCookie& cookie,
      CookieOptions* options,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      CookieInclusionStatus* inclusion_status) = 0;

  virtual PrivacySetting OnForcePrivacyMode(
      const URLRequest& request) const = 0;

  // Called when the |referrer_url| for requesting |target_url| during handling
  // of the |request| is does not comply with the referrer policy (e.g. a
  // secure referrer for an insecure initial target).
  // Returns true if the request should be cancelled. Otherwise, the referrer
  // header is stripped from the request.
  virtual bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const = 0;

  virtual bool OnCanQueueReportingReport(const url::Origin& origin) const = 0;

  virtual void OnCanSendReportingReports(
      std::set<url::Origin> origins,
      base::OnceCallback<void(std::set<url::Origin>)> result_callback)
      const = 0;

  virtual bool OnCanSetReportingClient(const url::Origin& origin,
                                       const GURL& endpoint) const = 0;

  virtual bool OnCanUseReportingClient(const url::Origin& origin,
                                       const GURL& endpoint) const = 0;

  virtual std::optional<cookie_util::StorageAccessStatus>
  OnGetStorageAccessStatus(const URLRequest& request) const = 0;

  virtual bool OnIsStorageAccessHeaderEnabled(
      const url::Origin* top_frame_origin,
      const GURL& url) const = 0;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_DELEGATE_H_
