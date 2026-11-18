// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/x509_util.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_request_info.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_info.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"

namespace net {

using DelegationType = HttpAuth::DelegationType;

namespace {

base::DictValue NetLogParameterChannelBindings(
    const std::string& channel_binding_token,
    NetLogCaptureMode capture_mode) {
  base::DictValue dict;
  if (!NetLogCaptureIncludesSocketBytes(capture_mode))
    return dict;

  dict.Set("token", base::HexEncode(channel_binding_token));
  return dict;
}

// Normalizes a candidate hostname for `url::SchemeHostPort` comparison and
// policy evaluation, rejecting empty strings, embedded NULs, `'%'` (so URL
// canonicalization cannot unescape `%2e` into `'.'`), non-ASCII input (so IDNA
// mapping cannot turn fullwidth characters into ASCII), bracketed literals
// unless `allow_bracketed_ipv6` is true, and any non-canonical host forms
// (such as integer/octal IPv4 or non-RFC-5952 IPv6).
std::string NormalizeHostForValidation(std::string_view host,
                                       bool allow_bracketed_ipv6) {
  if (host.empty() || host.contains('\0') || host.contains('%') ||
      !base::IsStringASCII(host)) {
    return std::string();
  }
  std::string trimmed_host = TrimEndingDot(host);
  if (trimmed_host.empty() || trimmed_host.ends_with('.') ||
      (!allow_bracketed_ipv6 && trimmed_host.starts_with('['))) {
    return std::string();
  }
  url::CanonHostInfo host_info;
  std::string canonicalized_host =
      trimmed_host.starts_with('[')
          ? CanonicalizeHost(trimmed_host, &host_info)
          : CanonicalizeHostSupportsBareIPV6(trimmed_host, &host_info);
  if (host_info.family == url::CanonHostInfo::BROKEN ||
      canonicalized_host.empty()) {
    return std::string();
  }
  // Require the input to already be in canonical form (modulo ASCII case, a
  // single trailing dot, and optional IPv6 brackets when allowed) because
  // `CreateSPN()` uses the raw canonical host string directly (e.g.,
  // `"167772162"` vs `"10.0.0.2"`, or `"2001:0db8::1"` vs `"2001:db8::1"`).
  std::string expected_host = base::ToLowerASCII(trimmed_host);
  if (host_info.family == url::CanonHostInfo::IPV6 &&
      !expected_host.starts_with('[')) {
    expected_host = base::StrCat({"[", expected_host, "]"});
  }
  if (canonicalized_host != expected_host) {
    return std::string();
  }
  return canonicalized_host;
}

// Uses |negotiate_auth_system_factory| to create the auth system, otherwise
// creates the default auth system for each platform.
std::unique_ptr<HttpAuthMechanism> CreateAuthSystem(
#if !BUILDFLAG(IS_ANDROID)
    scoped_refptr<HttpAuthHandlerNegotiate::AuthLibrary> auth_library,
#endif
    const HttpAuthPreferences* prefs,
    HttpAuthMechanismFactory negotiate_auth_system_factory) {
  if (negotiate_auth_system_factory)
    return negotiate_auth_system_factory.Run(prefs);
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<android::HttpAuthNegotiateAndroid>(prefs);
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<HttpAuthSSPI>(std::move(auth_library),
                                        HttpAuth::AUTH_SCHEME_NEGOTIATE);
#elif BUILDFLAG(IS_POSIX)
  return std::make_unique<HttpAuthGSSAPI>(std::move(auth_library),
                                          CHROME_GSS_SPNEGO_MECH_OID_DESC);
#endif
}

// Returns `std::nullopt` if no target principal was reported.
std::optional<HttpAuthHandlerNegotiate::TargetHostValidationResult>
ToTargetHostValidationResult(HttpAuthMechanism::TargetPrincipalError error) {
  using Error = HttpAuthMechanism::TargetPrincipalError;
  using Result = HttpAuthHandlerNegotiate::TargetHostValidationResult;
  switch (error) {
    case Error::kNotReported:
      return std::nullopt;
    case Error::kQueryFailed:
      return Result::kPrincipalQueryFailed;
    case Error::kNonHttpServicePrincipal:
      return Result::kNonHttpServicePrincipal;
    case Error::kUnparsableHost:
      return Result::kUnparsableHost;
  }
  NOTREACHED();
}

std::string_view TargetHostValidationFailureReason(
    HttpAuthHandlerNegotiate::TargetHostValidationResult result) {
  using Result = HttpAuthHandlerNegotiate::TargetHostValidationResult;
  switch (result) {
    case Result::kSameHost:
    case Result::kAllowedDifferentHost:
    case Result::kAllowedWithClampedDelegation:
      NOTREACHED();
    case Result::kPrincipalQueryFailed:
      return "principal_query_failed";
    case Result::kNonHttpServicePrincipal:
      return "non_http_service_principal";
    case Result::kUnparsableHost:
      return "unparsable_host";
    case Result::kMissingPreferences:
      return "missing_preferences";
    case Result::kDisallowedByServerAllowlist:
      return "disallowed_by_server_allowlist";
    case Result::kDisallowedByDelegateAllowlist:
      return "disallowed_by_delegate_allowlist";
  }
  NOTREACHED();
}

}  // namespace

struct HttpAuthHandlerNegotiate::TargetHostValidation {
  TargetHostValidationResult result =
      TargetHostValidationResult::kUnparsableHost;
  HttpAuth::DelegationType allowed_delegation = HttpAuth::DelegationType::kNone;
  bool used_windows_dotless_intranet = false;
};

HttpAuthHandlerNegotiate::Factory::Factory(
    HttpAuthMechanismFactory negotiate_auth_system_factory)
    : negotiate_auth_system_factory_(negotiate_auth_system_factory) {}

HttpAuthHandlerNegotiate::Factory::~Factory() = default;

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_POSIX)
const std::string& HttpAuthHandlerNegotiate::Factory::GetLibraryNameForTesting()
    const {
  return auth_library_->GetLibraryNameForTesting();
}
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_POSIX)

int HttpAuthHandlerNegotiate::Factory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const SSLInfo& ssl_info,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::SchemeHostPort& scheme_host_port,
    CreateReason reason,
    int digest_nonce_count,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
#if BUILDFLAG(IS_WIN)
  if (is_unsupported_ || reason == CREATE_PREEMPTIVE)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  std::unique_ptr<HttpAuthHandler> tmp_handler(
      std::make_unique<HttpAuthHandlerNegotiate>(
          CreateAuthSystem(auth_library_, http_auth_preferences(),
                           negotiate_auth_system_factory_),
          http_auth_preferences(), host_resolver));
#elif BUILDFLAG(IS_ANDROID)
  if (is_unsupported_ || !http_auth_preferences() ||
      http_auth_preferences()->AuthAndroidNegotiateAccountType().empty() ||
      reason == CREATE_PREEMPTIVE)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  std::unique_ptr<HttpAuthHandler> tmp_handler(
      std::make_unique<HttpAuthHandlerNegotiate>(
          CreateAuthSystem(http_auth_preferences(),
                           negotiate_auth_system_factory_),
          http_auth_preferences(), host_resolver));
#elif BUILDFLAG(IS_POSIX)
  if (is_unsupported_)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // Note: Don't set is_unsupported_ = true here. AllowGssapiLibraryLoad()
  // might change to true during a session.
  if (!http_auth_preferences() ||
      !http_auth_preferences()->AllowGssapiLibraryLoad()) {
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  if (!auth_library_->Init(net_log)) {
    is_unsupported_ = true;
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  }
  // TODO(ahendrickson): Move towards model of parsing in the factory
  //                     method and only constructing when valid.
  std::unique_ptr<HttpAuthHandler> tmp_handler(
      std::make_unique<HttpAuthHandlerNegotiate>(
          CreateAuthSystem(auth_library_.get(), http_auth_preferences(),
                           negotiate_auth_system_factory_),
          http_auth_preferences(), host_resolver));
#endif
  if (!tmp_handler->InitFromChallenge(challenge, target, ssl_info,
                                      network_anonymization_key,
                                      scheme_host_port, net_log)) {
    return ERR_INVALID_RESPONSE;
  }
  handler->swap(tmp_handler);
  return OK;
}

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate(
    std::unique_ptr<HttpAuthMechanism> auth_system,
    const HttpAuthPreferences* prefs,
    HostResolver* resolver)
    : auth_system_(std::move(auth_system)),
      resolver_(resolver),
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
  return http_auth_preferences_->CanUseDefaultCredentials(scheme_host_port_);
}

bool HttpAuthHandlerNegotiate::AllowsExplicitCredentials() {
  return auth_system_->AllowsExplicitCredentials();
}

// The Negotiate challenge header looks like:
//   WWW-Authenticate: NEGOTIATE auth-data
bool HttpAuthHandlerNegotiate::Init(
    HttpAuthChallengeTokenizer* challenge,
    const SSLInfo& ssl_info,
    const NetworkAnonymizationKey& network_anonymization_key) {
  network_anonymization_key_ = network_anonymization_key;
#if BUILDFLAG(IS_POSIX)
  if (!auth_system_->Init(net_log())) {
    VLOG(1) << "can't initialize GSSAPI library";
    return false;
  }
  // GSSAPI does not provide a way to enter username/password to obtain a TGT,
  // however ChromesOS provides the user an opportunity to enter their
  // credentials and generate a new TGT on OS level (see b/260522530). If the
  // default credentials are not allowed for a particular site
  // (based on allowlist), fall back to a different scheme.
  if (!AllowsDefaultCredentials()) {
    return false;
  }
#endif
  delegation_type_ = GetDelegationType();
  auth_system_->SetDelegation(delegation_type_);
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
  target_network_ = request->target_network;
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

std::string HttpAuthHandlerNegotiate::CreateSPN(
    const std::string& server,
    const url::SchemeHostPort& scheme_host_port) {
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
  int port = scheme_host_port.port();
#if BUILDFLAG(IS_WIN)
  static const char kSpnSeparator = '/';
#elif BUILDFLAG(IS_POSIX)
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
  resolve_host_request_ =
      resolver_->CreateRequest(scheme_host_port_, network_anonymization_key_,
                               target_network_, net_log(), parameters);
  return resolve_host_request_->Start(base::BindOnce(
      &HttpAuthHandlerNegotiate::OnIOComplete, base::Unretained(this)));
}

int HttpAuthHandlerNegotiate::DoResolveCanonicalNameComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  std::string server = scheme_host_port_.host();
  if (resolve_host_request_) {
    if (rv == OK) {
      // Expect at most a single DNS alias representing the canonical name
      // because the `HostResolver` request was made with
      // `include_canonical_name`.
      DCHECK_LE(resolve_host_request_->GetDnsAliasResults().size(), 1u);
      if (!resolve_host_request_->GetDnsAliasResults().empty()) {
        const std::string& canonical_host =
            *resolve_host_request_->GetDnsAliasResults().begin();
        if (ShouldUseCanonicalNameForSpn(canonical_host)) {
          server = canonical_host;
        }
      }
    } else {
      // Even in the error case, try to use origin_.host instead of
      // passing the failure on to the caller.
      VLOG(1) << "Problem finding canonical name for SPN for host "
              << scheme_host_port_.host() << ": " << ErrorToString(rv);
      rv = OK;
    }
  }

  next_state_ = STATE_GENERATE_AUTH_TOKEN;
  spn_ = CreateSPN(server, scheme_host_port_);
  resolve_host_request_ = nullptr;
  return rv;
}

bool HttpAuthHandlerNegotiate::ShouldUseCanonicalNameForSpn(
    const std::string& canonical_host) {
  // The server and delegation allowlists only apply to servers, so proxy auth
  // keeps using the canonical name unvalidated. Disabling
  // `kValidateNegotiateCnameAllowlist` skips validation altogether.
  if (target_ == HttpAuth::AUTH_PROXY ||
      !base::FeatureList::IsEnabled(
          features::kValidateNegotiateCnameAllowlist)) {
    return !canonical_host.empty();
  }

  TargetHostValidation validation =
      ValidateTargetHost(canonical_host, /*allow_delegation_clamping=*/true,
                         /*allow_bracketed_ipv6=*/false);
  CnameValidationResult cname_result =
      CnameValidationResult::kInvalidCanonicalHost;
  bool allowed = false;
  switch (validation.result) {
    case TargetHostValidationResult::kSameHost:
      cname_result = CnameValidationResult::kSameHost;
      allowed = true;
      break;
    case TargetHostValidationResult::kAllowedDifferentHost:
      allowed = true;
      // Delegation may also be clamped for Windows dotless intranet hosts
      // (see `AUTH_CNAME_DELEGATION_CLAMPED`).
      if (validation.used_windows_dotless_intranet) {
        cname_result = CnameValidationResult::kAllowedWindowsDotlessIntranet;
      } else if (validation.allowed_delegation < delegation_type_) {
        cname_result = CnameValidationResult::kAllowedWithClampedDelegation;
      } else {
        cname_result = CnameValidationResult::kAllowedDifferentHost;
      }
      break;
    case TargetHostValidationResult::kUnparsableHost:
      cname_result = CnameValidationResult::kInvalidCanonicalHost;
      break;
    case TargetHostValidationResult::kMissingPreferences:
      cname_result = CnameValidationResult::kMissingPreferences;
      break;
    case TargetHostValidationResult::kDisallowedByServerAllowlist:
      cname_result = CnameValidationResult::kDisallowedByServerAllowlist;
      break;
    case TargetHostValidationResult::kPrincipalQueryFailed:
    case TargetHostValidationResult::kNonHttpServicePrincipal:
    case TargetHostValidationResult::kDisallowedByDelegateAllowlist:
    case TargetHostValidationResult::kAllowedWithClampedDelegation:
      // Only reported for target principals or without delegation clamping.
      NOTREACHED();
  }
  base::UmaHistogramEnumeration("Net.HttpAuth.NegotiateCnameValidationResult",
                                cname_result);

  // When the canonical name is rejected, the SPN falls back to the requested
  // host, which GSSAPI libraries may still canonicalize through DNS back to
  // `canonical_host`, so never delegate credentials in that case.
  DelegationType allowed_delegation =
      allowed ? validation.allowed_delegation : DelegationType::kNone;
  if (!allowed) {
    net_log().AddEvent(NetLogEventType::AUTH_CNAME_REJECTED, [&] {
      base::DictValue params;
      params.Set("requested_host", scheme_host_port_.host());
      params.Set("canonical_host", NetLogStringValue(canonical_host));
      params.Set("reason",
                 TargetHostValidationFailureReason(validation.result));
      return params;
    });
    VLOG(1) << "Canonical name " << canonical_host
            << " for SPN rejected by auth allowlist check";
  }
  if (allowed_delegation < delegation_type_) {
    net_log().AddEvent(NetLogEventType::AUTH_CNAME_DELEGATION_CLAMPED, [&] {
      base::DictValue params;
      params.Set("requested_host", scheme_host_port_.host());
      params.Set("canonical_host", NetLogStringValue(canonical_host));
      return params;
    });
    delegation_type_ = allowed_delegation;
    auth_system_->SetDelegation(delegation_type_);
  }
  return allowed;
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
  // The server and delegation allowlists only apply to servers:
  // `AllowsDefaultCredentials()` always allows proxies and
  // `GetDelegationType()` never delegates to them. So for
  // `HttpAuth::AUTH_PROXY`, a different principal host cannot receive anything
  // those policies would withhold, and principal validation is skipped to leave
  // proxy auth unchanged.
  // Disabling `kValidateNegotiateTargetPrincipalHost` skips querying the
  // target principal altogether.
  if (rv == OK && target_ == HttpAuth::AUTH_SERVER &&
      base::FeatureList::IsEnabled(
          features::kValidateNegotiateTargetPrincipalHost)) {
    base::expected<std::string, HttpAuthMechanism::TargetPrincipalError>
        principal_host = auth_system_->GetTargetPrincipalHost();
    std::optional<TargetHostValidationResult> validation_result;
    if (principal_host.has_value()) {
      validation_result =
          ValidateTargetHost(*principal_host,
                             /*allow_delegation_clamping=*/false,
                             /*allow_bracketed_ipv6=*/true)
              .result;
    } else {
      validation_result = ToTargetHostValidationResult(principal_host.error());
    }
    // `std::nullopt` means the mechanism does not report a Kerberos-style
    // target principal (Android, SSPI for now, negotiated NTLMSSP under SPNEGO,
    // or an incomplete context with `GSS_C_NO_NAME` or `GSS_S_NO_CONTEXT`).
    if (validation_result.has_value()) {
      // `AuthServerAllowlist` governs whether to authenticate and
      // `AuthNegotiateDelegateAllowlist` governs whether to delegate. So if
      // only delegation is disallowed for the target host and no token has
      // been returned yet, withhold delegation instead of failing.
      if (*validation_result ==
              TargetHostValidationResult::kDisallowedByDelegateAllowlist &&
          !auth_token_generated_) {
        validation_result =
            TargetHostValidationResult::kAllowedWithClampedDelegation;
      }
      base::UmaHistogramEnumeration(
          "Net.HttpAuth.NegotiateTargetPrincipalValidationResult",
          *validation_result);
      if (*validation_result ==
          TargetHostValidationResult::kAllowedWithClampedDelegation) {
        net_log().AddEvent(
            NetLogEventType::AUTH_TARGET_PRINCIPAL_DELEGATION_CLAMPED, [&] {
              base::DictValue params;
              params.Set("requested_host", scheme_host_port_.host());
              params.Set("principal_host", *principal_host);
              return params;
            });
        // Discard the unsent round-1 token, which may carry a delegated
        // credential, and regenerate it with the delegation allowed for the
        // target host. Since that is strictly weaker each time, this
        // terminates.
        if (auth_token_) {
          auth_token_->clear();
        }
        if (auth_system_->ResetSecurityContext()) {
          delegation_type_ =
              http_auth_preferences_->GetDelegationType(url::SchemeHostPort(
                  scheme_host_port_.scheme(), TrimEndingDot(*principal_host),
                  scheme_host_port_.port()));
          auth_system_->SetDelegation(delegation_type_);
          next_state_ = STATE_GENERATE_AUTH_TOKEN;
          return OK;
        }
        rv = ERR_MISCONFIGURED_AUTH_ENVIRONMENT;
      } else if (*validation_result != TargetHostValidationResult::kSameHost &&
                 *validation_result !=
                     TargetHostValidationResult::kAllowedDifferentHost) {
        net_log().AddEvent(
            NetLogEventType::AUTH_TARGET_PRINCIPAL_REJECTED, [&] {
              base::DictValue params;
              params.Set("requested_host", scheme_host_port_.host());
              if (principal_host.has_value()) {
                params.Set("principal_host", *principal_host);
              }
              params.Set("reason",
                         TargetHostValidationFailureReason(*validation_result));
              return params;
            });
        // Disable Negotiate for this transaction so `HttpAuthController`
        // falls back to other offered schemes instead of prompting for
        // credentials (for example, on ChromeOS where
        // `AllowsExplicitCredentials()` is true).
        rv = ERR_MISCONFIGURED_AUTH_ENVIRONMENT;
        if (auth_token_) {
          auth_token_->clear();
        }
      }
    }
  }
  if (rv == OK) {
    auth_token_generated_ = true;
  }
  auth_token_ = nullptr;
  return rv;
}

HttpAuthHandlerNegotiate::TargetHostValidation
HttpAuthHandlerNegotiate::ValidateTargetHost(std::string_view target_host,
                                             bool allow_delegation_clamping,
                                             bool allow_bracketed_ipv6) const {
  TargetHostValidation validation;
  validation.allowed_delegation = delegation_type_;

  std::string normalized_target_host =
      NormalizeHostForValidation(target_host, allow_bracketed_ipv6);
  if (normalized_target_host.empty()) {
    validation.result = TargetHostValidationResult::kUnparsableHost;
    return validation;
  }
  // Build a `url::SchemeHostPort` with the request's scheme and port to
  // evaluate the same per-origin policies against `normalized_target_host`.
  url::SchemeHostPort target_scheme_host_port(scheme_host_port_.scheme(),
                                              normalized_target_host,
                                              scheme_host_port_.port());
  if (!target_scheme_host_port.IsValid()) {
    validation.result = TargetHostValidationResult::kUnparsableHost;
    return validation;
  }
  // Compare after normalization so trailing-dot URLs and bare IPv6 literals
  // match their canonical `url::SchemeHostPort` forms.
  std::string normalized_requested_host = NormalizeHostForValidation(
      scheme_host_port_.host(), /*allow_bracketed_ipv6=*/true);
  if (target_scheme_host_port.host() ==
      (normalized_requested_host.empty()
           ? TrimEndingDot(scheme_host_port_.host())
           : normalized_requested_host)) {
    validation.result = TargetHostValidationResult::kSameHost;
    return validation;
  }
  // On POSIX, where GSSAPI reports a principal host, `Init()` already rejects
  // server auth without preferences (`AllowsDefaultCredentials()` returns
  // false), so fail closed here too.
  if (!http_auth_preferences_) {
    validation.result = TargetHostValidationResult::kMissingPreferences;
    return validation;
  }
#if BUILDFLAG(IS_POSIX)
  // POSIX Negotiate mechanisms (`HttpAuthGSSAPI` and
  // `HttpAuthNegotiateAndroid`) ignore `credentials_` and always use ambient OS
  // credentials, matching `Init()`.
  const bool uses_default_credentials = true;
#else
  // The server allowlist only governs ambient credentials.
  const bool uses_default_credentials = !has_credentials_;
#endif
  if (uses_default_credentials &&
      !http_auth_preferences_->CanUseDefaultCredentials(
          target_scheme_host_port)) {
#if BUILDFLAG(IS_WIN)
    // On Windows when `AuthServerAllowlist` is unset, `URLSecurityManagerWin`
    // maps dotless intranet URLs (e.g. `http://intranet`) to `URLZONE_INTRANET`
    // while their DNS-expanded FQDNs contain dots and map to
    // `URLZONE_INTERNET`. Preserve default Windows zone behavior for dotless
    // intranet hosts when no explicit allowlist is configured.
    if (!http_auth_preferences_->HasDefaultAllowlist() &&
        !normalized_requested_host.empty() &&
        !normalized_requested_host.contains('.') &&
        !normalized_requested_host.starts_with('[') &&
        http_auth_preferences_->CanUseDefaultCredentials(scheme_host_port_)) {
      validation.used_windows_dotless_intranet = true;
    } else {
      validation.result =
          TargetHostValidationResult::kDisallowedByServerAllowlist;
      return validation;
    }
#else
    validation.result =
        TargetHostValidationResult::kDisallowedByServerAllowlist;
    return validation;
#endif
  }
  // The delegation configured on `auth_system_` in `Init()` must also be
  // allowed for the target host, since that is who receives the delegated
  // credentials (or clamped beforehand when validating a DNS CNAME).
  if (delegation_type_ != DelegationType::kNone) {
    DelegationType target_delegation =
        http_auth_preferences_->GetDelegationType(target_scheme_host_port);
    if (target_delegation < delegation_type_) {
      if (!allow_delegation_clamping) {
        validation.result =
            TargetHostValidationResult::kDisallowedByDelegateAllowlist;
        return validation;
      }
      validation.allowed_delegation = target_delegation;
    }
  }
  validation.result = TargetHostValidationResult::kAllowedDifferentHost;
  return validation;
}

DelegationType HttpAuthHandlerNegotiate::GetDelegationType() const {
  if (!http_auth_preferences_)
    return DelegationType::kNone;

  // TODO(cbentzel): Should delegation be allowed on proxies?
  if (target_ == HttpAuth::AUTH_PROXY)
    return DelegationType::kNone;

  return http_auth_preferences_->GetDelegationType(scheme_host_port_);
}

}  // namespace net
