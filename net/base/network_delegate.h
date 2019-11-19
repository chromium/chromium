// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_DELEGATE_H_
#define NET_BASE_NETWORK_DELEGATE_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "net/base/auth.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
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
class HttpRequestHeaders;
class HttpResponseHeaders;
class IPEndPoint;
class ProxyInfo;
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
  int NotifyBeforeStartTransaction(URLRequest* request,
                                   CompletionOnceCallback callback,
                                   HttpRequestHeaders* headers);
  void NotifyBeforeSendHeaders(URLRequest* request,
                               const ProxyInfo& proxy_info,
                               const ProxyRetryInfoMap& proxy_retry_info,
                               HttpRequestHeaders* headers);
  int NotifyHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& remote_endpoint,
      base::Optional<GURL>* preserve_fragment_on_redirect_url);
  void NotifyBeforeRedirect(URLRequest* request,
                            const GURL& new_location);
  void NotifyResponseStarted(URLRequest* request, int net_error);
  void NotifyCompleted(URLRequest* request, bool started, int net_error);
  void NotifyURLRequestDestroyed(URLRequest* request);
  void NotifyPACScriptError(int line_number, const base::string16& error);
  bool CanGetCookies(const URLRequest& request,
                     const CookieList& cookie_list,
                     bool allowed_from_caller);
  bool CanSetCookie(const URLRequest& request,
                    const net::CanonicalCookie& cookie,
                    CookieOptions* options,
                    bool allowed_from_caller);
  bool ForcePrivacyMode(
      const GURL& url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin) const;

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
  THREAD_CHECKER(thread_checker_);

 private:
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
  // read/write |headers| before they get sent out.
  //
  // Returns OK to continue with the request, ERR_IO_PENDING if the result is
  // not ready yet, and any other status code to cancel the request. If
  // returning ERR_IO_PENDING, call |callback| when the result is ready. Note,
  // however, that a pending operation may be cancelled by OnURLRequestDestroyed
  // or OnCompleted. Once cancelled, |request| and |headers| become invalid and
  // |callback| may not be called.
  //
  // The default implementation returns OK (continue with request) without
  // modifying |headers|.
  virtual int OnBeforeStartTransaction(URLRequest* request,
                                       CompletionOnceCallback callback,
                                       HttpRequestHeaders* headers) = 0;

  // Called after a connection is established , and just before headers are sent
  // to the destination server (i.e., not called for HTTP CONNECT requests). For
  // non-tunneled requests using HTTP proxies, |headers| will include any
  // proxy-specific headers as well. Allows the delegate to read/write |headers|
  // before they get sent out. |headers| is valid only for the duration of the
  // call.
  virtual void OnBeforeSendHeaders(URLRequest* request,
                                   const ProxyInfo& proxy_info,
                                   const ProxyRetryInfoMap& proxy_retry_info,
                                   HttpRequestHeaders* headers) = 0;

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
      base::Optional<GURL>* preserve_fragment_on_redirect_url) = 0;

  // Called right after a redirect response code was received. |new_location| is
  // only valid for the duration of the call.
  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) = 0;

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
                                const base::string16& error) = 0;

  // Called when reading cookies to allow the network delegate to block access
  // to the cookie. This method will never be invoked when
  // LOAD_DO_NOT_SEND_COOKIES is specified.
  // The |allowed_from_caller| param is used to pass whether this operation is
  // allowed from any higher level delegates (for example, in a
  // LayeredNetworkDelegate). Any custom logic should be ANDed with this bool.
  virtual bool OnCanGetCookies(const URLRequest& request,
                               const CookieList& cookie_list,
                               bool allowed_from_caller) = 0;

  // Called when a cookie is set to allow the network delegate to block access
  // to the cookie. This method will never be invoked when
  // LOAD_DO_NOT_SAVE_COOKIES is specified.
  // The |allowed_from_caller| param is used to pass whether this operation is
  // allowed from any higher level delegates (for example, in a
  // LayeredNetworkDelegate). Any custom logic should be ANDed with this bool.
  virtual bool OnCanSetCookie(const URLRequest& request,
                              const CanonicalCookie& cookie,
                              CookieOptions* options,
                              bool allowed_from_caller) = 0;

  // Returns true if the given |url| has to be requested over connection that
  // is not tracked by the server. Usually is false, unless user privacy
  // settings block cookies from being get or set.
  virtual bool OnForcePrivacyMode(
      const GURL& url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin) const = 0;

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
};

}  // namespace net

#endif  // NET_BASE_NETWORK_DELEGATE_H_
