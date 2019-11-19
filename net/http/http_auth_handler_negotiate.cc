// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_util.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_preferences.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_info.h"

namespace net {

using DelegationType = HttpAuth::DelegationType;

namespace {

base::Value NetLogParameterChannelBindings(
    const std::string& channel_binding_token,
    NetLogCaptureMode capture_mode) {
  base::DictionaryValue dict;
  if (!NetLogCaptureIncludesSocketBytes(capture_mode))
    return std::move(dict);

  dict.Clear();
  dict.SetString("token", base::HexEncode(channel_binding_token.data(),
                                          channel_binding_token.size()));
  return std::move(dict);
}

// Uses |negotiate_auth_system_factory| to create the auth system, otherwise
// creates the default auth system for each platform.
std::unique_ptr<HttpAuthMechanism> CreateAuthSystem(
#if !defined(OS_ANDROID)
    HttpAuthHandlerNegotiate::AuthLibrary* auth_library,
#endif
    const HttpAuthPreferences* prefs,
    HttpAuthMechanismFactory negotiate_auth_system_factory) {
  if (negotiate_auth_system_factory)
    return negotiate_auth_system_factory.Run(prefs);
#if defined(OS_ANDROID)
  return std::make_unique<net::android::HttpAuthNegotiateAndroid>(prefs);
#elif defined(OS_WIN)
  return std::make_unique<HttpAuthSSPI>(auth_library,
                                        HttpAuth::AUTH_SCHEME_NEGOTIATE);
#elif defined(OS_POSIX)
  return std::make_unique<HttpAuthGSSAPI>(auth_library,
                                          CHROME_GSS_SPNEGO_MECH_OID_DESC);
#endif
}

}  // namespace

HttpAuthHandlerNegotiate::Factory::Factory(
    HttpAuthMechanismFactory negotiate_auth_system_factory)
    : negotiate_auth_system_factory_(negotiate_auth_system_factory) {}

HttpAuthHandlerNegotiate::Factory::~Factory() = default;

#if !defined(OS_ANDROID) && defined(OS_POSIX)
const std::string& HttpAuthHandlerNegotiate::Factory::GetLibraryNameForTesting()
    const {
  return auth_library_->GetLibraryNameForTesting();
}
#endif  // !defined(OS_ANDROID) && defined(OS_POSIX)

int HttpAuthHandlerNegotiate::Factory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const SSLInfo& ssl_info,
    const GURL& origin,
    CreateReason reason,
    int digest_nonce_count,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
#if defined(OS_WIN)
  if (is_unsupported_ || reason == CREATE_PREEMPTIVE)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  std::unique_ptr<HttpAuthHandler> tmp_handler(new HttpAuthHandlerNegotiate(
      CreateAuthSystem(auth_library_.get(), http_auth_preferences(),
                       negotiate_auth_system_factory_),
      http_auth_preferences(), host_resolver));
#elif defined(OS_ANDROID)
  if (is_unsupported_ || !http_auth_preferences() ||
      http_auth_preferences()->AuthAndroidNegotiateAccountType().empty() ||
      reason == CREATE_PREEMPTIVE)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  std::unique_ptr<HttpAuthHandler> tmp_handler(new HttpAuthHandlerNegotiate(
      CreateAuthSystem(http_auth_preferences(), negotiate_auth_system_factory_),
      http_auth_preferences(), host_resolver));
#elif defined(OS_POSIX)
  if (is_unsupported_)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
#if defined(OS_CHROMEOS)
  // Note: Don't set is_unsupported_ = true here. AllowGssapiLibraryLoad()
  // might change to true during a session.
  if (!http_auth_preferences()->AllowGssapiLibraryLoad())
    return ERR_UNSUPPORTED_AUTH_SCHEME;
#endif
  if (!auth_library_->Init(net_log)) {
    is_unsupported_ = true;
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  }
  // TODO(ahendrickson): Move towards model of parsing in the factory
  //                     method and only constructing when valid.
  std::unique_ptr<HttpAuthHandler> tmp_handler(new HttpAuthHandlerNegotiate(
      CreateAuthSystem(auth_library_.get(), http_auth_preferences(),
                       negotiate_auth_system_factory_),
      http_auth_preferences(), host_resolver));
#endif
  if (!tmp_handler->InitFromChallenge(challenge, target, ssl_info, origin,
                                      net_log))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate(
    std::unique_ptr<HttpAuthMechanism> auth_system,
    const HttpAuthPreferences* prefs,
    HostResolver* resolver)
    : auth_system_(std::move(auth_system)),
      resolver_(resolver),
      already_called_(false),
      has_credentials_(false),
      auth_token_(nullptr),
      next_state_(STATE_NONE),
      http_auth_preferences_(prefs) {}

HttpAuthHandlerNegotiate::~HttpAuthHandlerNegotiate() = default;

// Require identity on first pass instead of second.
bool HttpAuthHandlerNegotiate::NeedsIdentity() {
  return auth_system_->NeedsIdentity();
}

bool HttpAuthHandlerNegotiate::AllowsDefaultCredentials() {
  if (target_ == HttpAuth::AUTH_PROXY)
    return true;
  if (!http_auth_preferences_)
    return false;
  return http_auth_preferences_->CanUseDefaultCredentials(origin_);
}

bool HttpAuthHandlerNegotiate::AllowsExplicitCredentials() {
  return auth_system_->AllowsExplicitCredentials();
}

// The Negotiate challenge header looks like:
//   WWW-Authenticate: NEGOTIATE auth-data
bool HttpAuthHandlerNegotiate::Init(HttpAuthChallengeTokenizer* challenge,
                                    const SSLInfo& ssl_info) {
#if defined(OS_POSIX)
  if (!auth_system_->Init(net_log())) {
    VLOG(1) << "can't initialize GSSAPI library";
    return false;
  }
  // GSSAPI does not provide a way to enter username/password to
  // obtain a TGT. If the default credentials are not allowed for
  // a particular site (based on allowlist), fall back to a
  // different scheme.
  if (!AllowsDefaultCredentials())
    return false;
#endif
  auth_system_->SetDelegation(GetDelegationType());
  auth_scheme_ = HttpAuth::AUTH_SCHEME_NEGOTIATE;
  score_ = 4;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;

  HttpAuth::AuthorizationResult auth_result =
      auth_system_->ParseChallenge(challenge);
  if (auth_result != HttpAuth::AUTHORIZATION_RESULT_ACCEPT)
    return false;

  // Try to extract channel bindings.
  if (ssl_info.is_valid())
    x509_util::GetTLSServerEndPointChannelBinding(*ssl_info.cert,
                                                  &channel_bindings_);
  if (!channel_bindings_.empty())
    net_log().AddEvent(NetLogEventType::AUTH_CHANNEL_BINDINGS,
                       [&](NetLogCaptureMode capture_mode) {
                         return NetLogParameterChannelBindings(
                             channel_bindings_, capture_mode);
                       });
  return true;
}

int HttpAuthHandlerNegotiate::GenerateAuthTokenImpl(
    const AuthCredentials* credentials,
    const HttpRequestInfo* request,
    CompletionOnceCallback callback,
    std::string* auth_token) {
  DCHECK(callback_.is_null());
  DCHECK(auth_token_ == nullptr);
  auth_token_ = auth_token;
  if (already_called_) {
    DCHECK((!has_credentials_ && credentials == nullptr) ||
           (has_credentials_ && credentials->Equals(credentials_)));
    next_state_ = STATE_GENERATE_AUTH_TOKEN;
  } else {
    already_called_ = true;
    if (credentials) {
      has_credentials_ = true;
      credentials_ = *credentials;
    }
    next_state_ = STATE_RESOLVE_CANONICAL_NAME;
  }
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  return rv;
}

HttpAuth::AuthorizationResult
HttpAuthHandlerNegotiate::HandleAnotherChallengeImpl(
    HttpAuthChallengeTokenizer* challenge) {
  return auth_system_->ParseChallenge(challenge);
}

std::string HttpAuthHandlerNegotiate::CreateSPN(const std::string& server,
                                                const GURL& origin) {
  // Kerberos Web Server SPNs are in the form HTTP/<host>:<port> through SSPI,
  // and in the form HTTP@<host>:<port> through GSSAPI
  //   http://msdn.microsoft.com/en-us/library/ms677601%28VS.85%29.aspx
  //
  // However, reality differs from the specification. A good description of
  // the problems can be found here:
  //   http://blog.michelbarneveld.nl/michel/archive/2009/11/14/the-reason-why-kb911149-and-kb908209-are-not-the-soluton.aspx
  //
  // Typically the <host> portion should be the canonical FQDN for the service.
  // If this could not be resolved, the original hostname in the URL will be
  // attempted instead. However, some intranets register SPNs using aliases
  // for the same canonical DNS name to allow multiple web services to reside
  // on the same host machine without requiring different ports. IE6 and IE7
  // have hotpatches that allow the default behavior to be overridden.
  //   http://support.microsoft.com/kb/911149
  //   http://support.microsoft.com/kb/938305
  //
  // According to the spec, the <port> option should be included if it is a
  // non-standard port (i.e. not 80 or 443 in the HTTP case). However,
  // historically browsers have not included the port, even on non-standard
  // ports. IE6 required a hotpatch and a registry setting to enable
  // including non-standard ports, and IE7 and IE8 also require the same
  // registry setting, but no hotpatch. Firefox does not appear to have an
  // option to include non-standard ports as of 3.6.
  //   http://support.microsoft.com/kb/908209
  //
  // Without any command-line flags, Chrome matches the behavior of Firefox
  // and IE. Users can override the behavior so aliases are allowed and
  // non-standard ports are included.
  int port = origin.EffectiveIntPort();
#if defined(OS_WIN)
  static const char kSpnSeparator = '/';
#elif defined(OS_POSIX)
  static const char kSpnSeparator = '@';
#endif
  if (port != 80 && port != 443 &&
      (http_auth_preferences_ &&
       http_auth_preferences_->NegotiateEnablePort())) {
    return base::StringPrintf("HTTP%c%s:%d", kSpnSeparator, server.c_str(),
                              port);
  } else {
    return base::StringPrintf("HTTP%c%s", kSpnSeparator, server.c_str());
  }
}

void HttpAuthHandlerNegotiate::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

void HttpAuthHandlerNegotiate::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(rv);
}

int HttpAuthHandlerNegotiate::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_CANONICAL_NAME:
        DCHECK_EQ(OK, rv);
        rv = DoResolveCanonicalName();
        break;
      case STATE_RESOLVE_CANONICAL_NAME_COMPLETE:
        rv = DoResolveCanonicalNameComplete(rv);
        break;
      case STATE_GENERATE_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateAuthToken();
        break;
      case STATE_GENERATE_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateAuthTokenComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpAuthHandlerNegotiate::DoResolveCanonicalName() {
  next_state_ = STATE_RESOLVE_CANONICAL_NAME_COMPLETE;
  if ((http_auth_preferences_ &&
       http_auth_preferences_->NegotiateDisableCnameLookup()) ||
      !resolver_)
    return OK;

  // TODO(cbentzel): Add reverse DNS lookup for numeric addresses.
  HostResolver::ResolveHostParameters parameters;
  parameters.include_canonical_name = true;
  resolve_host_request_ = resolver_->CreateRequest(
      HostPortPair(origin_.host(), 0), net_log(), parameters);
  return resolve_host_request_->Start(base::BindOnce(
      &HttpAuthHandlerNegotiate::OnIOComplete, base::Unretained(this)));
}

int HttpAuthHandlerNegotiate::DoResolveCanonicalNameComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  std::string server = origin_.host();
  if (resolve_host_request_) {
    if (rv == OK) {
      DCHECK(resolve_host_request_->GetAddressResults());
      const std::string& canonical_name =
          resolve_host_request_->GetAddressResults().value().canonical_name();
      if (!canonical_name.empty())
        server = canonical_name;
    } else {
      // Even in the error case, try to use origin_.host instead of
      // passing the failure on to the caller.
      VLOG(1) << "Problem finding canonical name for SPN for host "
              << origin_.host() << ": " << ErrorToString(rv);
      rv = OK;
    }
  }

  next_state_ = STATE_GENERATE_AUTH_TOKEN;
  spn_ = CreateSPN(server, origin_);
  resolve_host_request_ = nullptr;
  return rv;
}

int HttpAuthHandlerNegotiate::DoGenerateAuthToken() {
  next_state_ = STATE_GENERATE_AUTH_TOKEN_COMPLETE;
  AuthCredentials* credentials = has_credentials_ ? &credentials_ : nullptr;
  return auth_system_->GenerateAuthToken(
      credentials, spn_, channel_bindings_, auth_token_, net_log(),
      base::BindOnce(&HttpAuthHandlerNegotiate::OnIOComplete,
                     base::Unretained(this)));
}

int HttpAuthHandlerNegotiate::DoGenerateAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  auth_token_ = nullptr;
  return rv;
}

DelegationType HttpAuthHandlerNegotiate::GetDelegationType() const {
  if (!http_auth_preferences_)
    return DelegationType::kNone;

  // TODO(cbentzel): Should delegation be allowed on proxies?
  if (target_ == HttpAuth::AUTH_PROXY)
    return DelegationType::kNone;

  return http_auth_preferences_->GetDelegationType(origin_);
}

}  // namespace net
