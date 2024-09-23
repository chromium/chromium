// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_CONTROLLER_H_
#define NET_HTTP_HTTP_AUTH_CONTROLLER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_preferences.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

class AuthChallengeInfo;
class AuthCredentials;
class HttpAuthHandler;
class HttpAuthHandlerFactory;
class HttpAuthCache;
class HttpRequestHeaders;
class HostResolver;
class NetLogWithSource;
struct HttpRequestInfo;
class SSLInfo;

// HttpAuthController is the main entry point for external callers into the HTTP
// authentication stack. A single instance of an HttpAuthController can be used
// to handle authentication to a single "target", where "target" is a HTTP
// server or a proxy. During its lifetime, the HttpAuthController can make use
// of multiple authentication handlers (implemented as HttpAuthHandler
// subclasses), and respond to multiple challenges.
//
// Individual HTTP authentication schemes can have additional requirements other
// than what's prescribed in RFC 7235. See HandleAuthChallenge() for details.
class NET_EXPORT_PRIVATE HttpAuthController
    : public base::RefCounted<HttpAuthController> {
 public:
  // Construct a new HttpAuthController.
  //
  // * |target| is either PROXY or SERVER and determines the authentication
  //       headers to use ("WWW-Authenticate"/"Authorization" vs.
  //       "Proxy-Authenticate"/"Proxy-Authorization") and how ambient
  //       credentials are used.
  //
  // * |auth_url| specifies the target URL. The origin of the URL identifies the
  //       target host. The path (hierarchical part defined in RFC 3986 section
  //       3.3) of the URL is used by HTTP basic authentication to determine
  //       cached credentials can be used to preemptively send an authorization
  //       header. See RFC 7617 section 2.2 (Reusing Credentials) for details.
  //       If |target| is PROXY, then |auth_url| should have no hierarchical
  //       part since that is meaningless.
  //
  // * |network_anonymization_key| specifies the NetworkAnonymizationKey
  //       associated with the resource load. Depending on settings, credentials
  //       may be scoped to a single NetworkAnonymizationKey.
  //
  // * |http_auth_cache| specifies the credentials cache to use. During
  //       authentication if explicit (user-provided) credentials are used and
  //       they can be cached to respond to authentication challenges in the
  //       future, they are stored in the cache. In addition, the HTTP Digest
  //       authentication is stateful across requests. So the |http_auth_cache|
  //       is also used to maintain state for this authentication scheme.
  //
  // * |http_auth_handler_factory| is used to construct instances of
  //       HttpAuthHandler subclasses to handle scheme-specific authentication
  //       logic. The |http_auth_handler_factory| is also responsible for
  //       determining whether the authentication stack should use a specific
  //       authentication scheme or not.
  //
  // * |host_resolver| is used for determining the canonical hostname given a
  //       possibly non-canonical host name. Name canonicalization is used for
  //       NTLM and Negotiate HTTP authentication schemes.
  //
  // * |allow_default_credentials| is used for determining if the current
  //       context allows ambient authentication using default credentials.
  HttpAuthController(HttpAuth::Target target,
                     const GURL& auth_url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     HttpAuthCache* http_auth_cache,
                     HttpAuthHandlerFactory* http_auth_handler_factory,
                     HostResolver* host_resolver);

  // Generate an authentication token for |target| if necessary. The return
  // value is a net error code. |OK| will be returned both in the case that
  // a token is correctly generated synchronously, as well as when no tokens
  // were necessary.
  int MaybeGenerateAuthToken(const HttpRequestInfo* request,
                             CompletionOnceCallback callback,
                             const NetLogWithSource& net_log);

  // Adds either the proxy auth header, or the origin server auth header,
  // as specified by |target_|.
  void AddAuthorizationHeader(HttpRequestHeaders* authorization_headers);

  // Checks for and handles HTTP status code 401 or 407.
  // |HandleAuthChallenge()| returns OK on success, or a network error code
  // otherwise. It may also populate |auth_info_|.
  int HandleAuthChallenge(scoped_refptr<HttpResponseHeaders> headers,
                          const SSLInfo& ssl_info,
                          bool do_not_send_server_auth,
                          bool establishing_tunnel,
                          const NetLogWithSource& net_log);

  // Store the supplied credentials and prepare to restart the auth.
  void ResetAuth(const AuthCredentials& credentials);

  bool HaveAuthHandler() const;

  bool HaveAuth() const;

  // Return whether the authentication scheme is incompatible with HTTP/2
  // and thus the server would presumably reject a request on HTTP/2 anyway.
  bool NeedsHTTP11() const;

  // Swaps the authentication challenge info into |other|.
  void TakeAuthInfo(std::optional<AuthChallengeInfo>* other);

  bool IsAuthSchemeDisabled(HttpAuth::Scheme scheme) const;
  void DisableAuthScheme(HttpAuth::Scheme scheme);
  void DisableEmbeddedIdentity();

  // Called when the connection has been closed, so the current handler (which
  // contains state bound to the connection) should be dropped. If retrying on a
  // new connection, the next call to MaybeGenerateAuthToken will retry the
  // current auth scheme.
  void OnConnectionClosed();

 private:
  // Actions for InvalidateCurrentHandler()
  enum InvalidateHandlerAction {
    INVALIDATE_HANDLER_AND_CACHED_CREDENTIALS,
    INVALIDATE_HANDLER_AND_DISABLE_SCHEME,
    INVALIDATE_HANDLER
  };

  // So that we can mock this object.
  friend class base::RefCounted<HttpAuthController>;

  ~HttpAuthController();

  // If this controller's NetLog hasn't been created yet, creates it and
  // associates it with |caller_net_log|. Does nothing after the first
  // invocation.
  void BindToCallingNetLog(const NetLogWithSource& caller_net_log);

  // Searches the auth cache for an entry that encompasses the request's path.
  // If such an entry is found, updates |identity_| and |handler_| with the
  // cache entry's data and returns true.
  bool SelectPreemptiveAuth(const NetLogWithSource& caller_net_log);

  // Invalidates the current handler. If |action| is
  // INVALIDATE_HANDLER_AND_CACHED_CREDENTIALS, then also invalidate
  // the cached credentials used by the handler.
  void InvalidateCurrentHandler(InvalidateHandlerAction action);

  // Invalidates any auth cache entries after authentication has failed.
  // The identity that was rejected is |identity_|.
  void InvalidateRejectedAuthFromCache();

  // Allows reusing last used identity source. If the authentication handshake
  // breaks down halfway, then the controller needs to restart it from the
  // beginning and resue the same identity.
  void PrepareIdentityForReuse();

  // Sets |identity_| to the next identity that the transaction should try. It
  // chooses candidates by searching the auth cache and the URL for a
  // username:password. Returns true if an identity was found.
  bool SelectNextAuthIdentityToTry();

  // Populates auth_info_ with the challenge information, so that
  // URLRequestHttpJob can prompt for credentials.
  void PopulateAuthChallenge();

  // Handle the result of calling GenerateAuthToken on an HttpAuthHandler. The
  // return value of this function should be used as the return value of the
  // GenerateAuthToken operation.
  int HandleGenerateTokenResult(int result);

  void OnGenerateAuthTokenDone(int result);

  // Indicates if this handler is for Proxy auth or Server auth.
  HttpAuth::Target target_;

  // Holds the {scheme, host, port, path} for the authentication target.
  const GURL auth_url_;

  // Holds the {scheme, host, port} for the authentication target.
  const url::SchemeHostPort auth_scheme_host_port_;

  // The absolute path of the resource needing authentication.
  // For proxy authentication, the path is empty.
  const std::string auth_path_;

  // NetworkAnonymizationKey associated with the request.
  const NetworkAnonymizationKey network_anonymization_key_;

  // |handler_| encapsulates the logic for the particular auth-scheme.
  // This includes the challenge's parameters. If nullptr, then there is no
  // associated auth handler.
  std::unique_ptr<HttpAuthHandler> handler_;

  // |identity_| holds the credentials that should be used by the handler_ to
  // generate challenge responses. This identity can come from a number of
  // places (url, cache, prompt).
  HttpAuth::Identity identity_;

  // |auth_token_| contains the opaque string to pass to the proxy or
  // server to authenticate the client.
  std::string auth_token_;

  // Contains information about the auth challenge.
  std::optional<AuthChallengeInfo> auth_info_;

  // True if we've used the username:password embedded in the URL.  This
  // makes sure we use the embedded identity only once for the transaction,
  // preventing an infinite auth restart loop.
  bool embedded_identity_used_ = false;

  // True if default credentials have already been tried for this transaction
  // in response to an HTTP authentication challenge.
  bool default_credentials_used_ = false;

  // These two are owned by the HttpNetworkSession/IOThread, which own the
  // objects which reference |this|. Therefore, these raw pointers are valid
  // for the lifetime of this object.
  const raw_ptr<HttpAuthCache> http_auth_cache_;
  const raw_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  const raw_ptr<HostResolver> host_resolver_;

  std::set<HttpAuth::Scheme> disabled_schemes_;

  CompletionOnceCallback callback_;

  // NetLog to be used for logging in this controller.
  NetLogWithSource net_log_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_CONTROLLER_H_
