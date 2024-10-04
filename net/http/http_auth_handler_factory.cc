// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_factory.h"

#include <optional>
#include <set>
#include <string_view>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_auth_handler_digest.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/log/net_log_values.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(USE_KERBEROS)
#include "net/http/http_auth_handler_negotiate.h"
#endif

namespace net {

namespace {

base::Value::Dict NetLogParamsForCreateAuth(
    std::string_view scheme,
    std::string_view challenge,
    const int net_error,
    const url::SchemeHostPort& scheme_host_port,
    const std::optional<bool>& allows_default_credentials,
    NetLogCaptureMode capture_mode) {
  base::Value::Dict dict;
  dict.Set("scheme", NetLogStringValue(scheme));
  if (NetLogCaptureIncludesSensitive(capture_mode)) {
    dict.Set("challenge", NetLogStringValue(challenge));
  }
  dict.Set("origin", scheme_host_port.Serialize());
  if (allows_default_credentials)
    dict.Set("allows_default_credentials", *allows_default_credentials);
  if (net_error < 0)
    dict.Set("net_error", net_error);
  return dict;
}

}  // namespace

int HttpAuthHandlerFactory::CreateAuthHandlerFromString(
    std::string_view challenge,
    HttpAuth::Target target,
    const SSLInfo& ssl_info,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::SchemeHostPort& scheme_host_port,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
  HttpAuthChallengeTokenizer props(challenge);
  return CreateAuthHandler(&props, target, ssl_info, network_anonymization_key,
                           scheme_host_port, CREATE_CHALLENGE, 1, net_log,
                           host_resolver, handler);
}

int HttpAuthHandlerFactory::CreatePreemptiveAuthHandlerFromString(
    const std::string& challenge,
    HttpAuth::Target target,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::SchemeHostPort& scheme_host_port,
    int digest_nonce_count,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
  HttpAuthChallengeTokenizer props(challenge);
  SSLInfo null_ssl_info;
  return CreateAuthHandler(&props, target, null_ssl_info,
                           network_anonymization_key, scheme_host_port,
                           CREATE_PREEMPTIVE, digest_nonce_count, net_log,
                           host_resolver, handler);
}

HttpAuthHandlerRegistryFactory::HttpAuthHandlerRegistryFactory(
    const HttpAuthPreferences* http_auth_preferences) {
  set_http_auth_preferences(http_auth_preferences);
}

HttpAuthHandlerRegistryFactory::~HttpAuthHandlerRegistryFactory() = default;

void HttpAuthHandlerRegistryFactory::SetHttpAuthPreferences(
    const std::string& scheme,
    const HttpAuthPreferences* prefs) {
  HttpAuthHandlerFactory* factory = GetSchemeFactory(scheme);
  if (factory)
    factory->set_http_auth_preferences(prefs);
}

void HttpAuthHandlerRegistryFactory::RegisterSchemeFactory(
    const std::string& scheme,
    std::unique_ptr<HttpAuthHandlerFactory> factory) {
  std::string lower_scheme = base::ToLowerASCII(scheme);
  if (factory) {
    factory->set_http_auth_preferences(http_auth_preferences());
    factory_map_[lower_scheme] = std::move(factory);
  } else {
    factory_map_.erase(lower_scheme);
  }
}

// static
std::unique_ptr<HttpAuthHandlerRegistryFactory>
HttpAuthHandlerFactory::CreateDefault(
    const HttpAuthPreferences* prefs
#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
    ,
    const std::string& gssapi_library_name
#endif
#if BUILDFLAG(USE_KERBEROS)
    ,
    HttpAuthMechanismFactory negotiate_auth_system_factory
#endif
) {
  return HttpAuthHandlerRegistryFactory::Create(prefs
#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
                                                ,
                                                gssapi_library_name
#endif
#if BUILDFLAG(USE_KERBEROS)
                                                ,
                                                negotiate_auth_system_factory
#endif
  );
}

// static
std::unique_ptr<HttpAuthHandlerRegistryFactory>
HttpAuthHandlerRegistryFactory::Create(
    const HttpAuthPreferences* prefs
#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
    ,
    const std::string& gssapi_library_name
#endif
#if BUILDFLAG(USE_KERBEROS)
    ,
    HttpAuthMechanismFactory negotiate_auth_system_factory
#endif
) {
  auto registry_factory =
      std::make_unique<HttpAuthHandlerRegistryFactory>(prefs);

  registry_factory->RegisterSchemeFactory(
      kBasicAuthScheme, std::make_unique<HttpAuthHandlerBasic::Factory>());

  registry_factory->RegisterSchemeFactory(
      kDigestAuthScheme, std::make_unique<HttpAuthHandlerDigest::Factory>());

  auto ntlm_factory = std::make_unique<HttpAuthHandlerNTLM::Factory>();
#if BUILDFLAG(IS_WIN)
  ntlm_factory->set_sspi_library(
      std::make_unique<SSPILibraryDefault>(NTLMSP_NAME));
#endif  // BUILDFLAG(IS_WIN)
  registry_factory->RegisterSchemeFactory(kNtlmAuthScheme,
                                          std::move(ntlm_factory));

#if BUILDFLAG(USE_KERBEROS)
  auto negotiate_factory = std::make_unique<HttpAuthHandlerNegotiate::Factory>(
      negotiate_auth_system_factory);
#if BUILDFLAG(IS_WIN)
  negotiate_factory->set_library(
      std::make_unique<SSPILibraryDefault>(NEGOSSP_NAME));
#elif BUILDFLAG(USE_EXTERNAL_GSSAPI)
  negotiate_factory->set_library(
      std::make_unique<GSSAPISharedLibrary>(gssapi_library_name));
#endif
  registry_factory->RegisterSchemeFactory(kNegotiateAuthScheme,
                                          std::move(negotiate_factory));
#endif  // BUILDFLAG(USE_KERBEROS)

  if (prefs) {
    registry_factory->set_http_auth_preferences(prefs);
    for (auto& factory_entry : registry_factory->factory_map_) {
      factory_entry.second->set_http_auth_preferences(prefs);
    }
  }
  return registry_factory;
}

int HttpAuthHandlerRegistryFactory::CreateAuthHandler(
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
  auto scheme = challenge->auth_scheme();

  int net_error;
  if (scheme.empty()) {
    handler->reset();
    net_error = ERR_INVALID_RESPONSE;
  } else {
    bool all_schemes_allowed_for_origin =
        http_auth_preferences() &&
        http_auth_preferences()->IsAllowedToUseAllHttpAuthSchemes(
            scheme_host_port);
    auto* factory = all_schemes_allowed_for_origin || IsSchemeAllowed(scheme)
                        ? GetSchemeFactory(scheme)
                        : nullptr;
    if (!factory) {
      handler->reset();
      net_error = ERR_UNSUPPORTED_AUTH_SCHEME;
    } else {
      net_error = factory->CreateAuthHandler(
          challenge, target, ssl_info, network_anonymization_key,
          scheme_host_port, reason, digest_nonce_count, net_log, host_resolver,
          handler);
    }
  }

  net_log.AddEvent(
      NetLogEventType::AUTH_HANDLER_CREATE_RESULT,
      [&](NetLogCaptureMode capture_mode) {
        return NetLogParamsForCreateAuth(
            scheme, challenge->challenge_text(), net_error, scheme_host_port,
            *handler
                ? std::make_optional((*handler)->AllowsDefaultCredentials())
                : std::nullopt,
            capture_mode);
      });
  return net_error;
}

bool HttpAuthHandlerRegistryFactory::IsSchemeAllowedForTesting(
    const std::string& scheme) const {
  return IsSchemeAllowed(scheme);
}

bool HttpAuthHandlerRegistryFactory::IsSchemeAllowed(
    const std::string& scheme) const {
  const std::set<std::string>& allowed_schemes =
      http_auth_preferences() && http_auth_preferences()->allowed_schemes()
          ? *http_auth_preferences()->allowed_schemes()
          : default_auth_schemes_;
  return allowed_schemes.find(scheme) != allowed_schemes.end();
}

#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_POSIX)
std::optional<std::string>
HttpAuthHandlerRegistryFactory::GetNegotiateLibraryNameForTesting() const {
  if (!IsSchemeAllowed(kNegotiateAuthScheme))
    return std::nullopt;

  return reinterpret_cast<HttpAuthHandlerNegotiate::Factory*>(
             GetSchemeFactory(kNegotiateAuthScheme))
      ->GetLibraryNameForTesting();  // IN-TEST
}
#endif

HttpAuthHandlerFactory* HttpAuthHandlerRegistryFactory::GetSchemeFactory(
    const std::string& scheme) const {
  std::string lower_scheme = base::ToLowerASCII(scheme);
  auto it = factory_map_.find(lower_scheme);
  if (it == factory_map_.end()) {
    return nullptr;  // |scheme| is not registered.
  }
  return it->second.get();
}

}  // namespace net
