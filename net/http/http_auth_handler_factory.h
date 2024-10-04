// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_mechanism.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/url_security_manager.h"
#include "net/net_buildflags.h"

namespace url {
class SchemeHostPort;
}

namespace net {

class HostResolver;
class HttpAuthChallengeTokenizer;
class HttpAuthHandler;
class HttpAuthHandlerRegistryFactory;
class HttpAuthPreferences;
class NetLogWithSource;
class NetworkAnonymizationKey;

// An HttpAuthHandlerFactory is used to create HttpAuthHandler objects.
// The HttpAuthHandlerFactory object _must_ outlive any of the HttpAuthHandler
// objects that it creates.
class NET_EXPORT HttpAuthHandlerFactory {
 public:
  enum CreateReason {
    CREATE_CHALLENGE,   // Create a handler in response to a challenge.
    CREATE_PREEMPTIVE,  // Create a handler preemptively.
  };

  HttpAuthHandlerFactory() = default;

  HttpAuthHandlerFactory(const HttpAuthHandlerFactory&) = delete;
  HttpAuthHandlerFactory& operator=(const HttpAuthHandlerFactory&) = delete;

  virtual ~HttpAuthHandlerFactory() = default;

  // Sets the source of the HTTP authentication preferences.
  // HttpAuthHandlerFactory doesn't own the preferences, and the
  // HttpAuthPreferences object should outlive the factory and any handlers it
  // creates.
  void set_http_auth_preferences(
      const HttpAuthPreferences* http_auth_preferences) {
    http_auth_preferences_ = http_auth_preferences;
  }

  // Retrieves the associated URL security manager.
  const HttpAuthPreferences* http_auth_preferences() const {
    return http_auth_preferences_;
  }

  // Creates an HttpAuthHandler object based on the authentication challenge
  // specified by |*challenge|. |challenge| must point to a valid tokenizer.
  //
  // If an HttpAuthHandler object is successfully created it is passed back to
  // the caller through |*handler| and OK is returned.
  //
  // If |*challenge| specifies an unsupported authentication scheme, |*handler|
  // is set to nullptr and ERR_UNSUPPORTED_AUTH_SCHEME is returned.
  //
  // If |*challenge| is improperly formed, |*handler| is set to nullptr and
  // ERR_INVALID_RESPONSE is returned.
  //
  // |create_reason| indicates why the handler is being created. This is used
  // since NTLM and Negotiate schemes do not support preemptive creation.
  //
  // |digest_nonce_count| is specifically intended for the Digest authentication
  // scheme, and indicates the number of handlers generated for a particular
  // server nonce challenge.
  //
  // |ssl_info| is valid if the authentication session is being established over
  // a secure connection.
  //
  // For the NTLM and Negotiate handlers:
  // If |origin| does not match the authentication method's filters for
  // the specified |target|, ERR_INVALID_AUTH_CREDENTIALS is returned.
  // NOTE: This will apply to ALL |origin| values if the filters are empty.
  //
  // |*challenge| should not be reused after a call to |CreateAuthHandler()|,
  //
  // |host_resolver| is used by the Negotiate authentication handler to perform
  // CNAME lookups to generate a Kerberos SPN for the server. If the "negotiate"
  // scheme is used and the factory was created with
  // |negotiate_disable_cname_lookup| false, |host_resolver| must not be null,
  // and it must remain valid for the lifetime of the created |handler|.
  virtual int CreateAuthHandler(
      HttpAuthChallengeTokenizer* challenge,
      HttpAuth::Target target,
      const SSLInfo& ssl_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::SchemeHostPort& scheme_host_port,
      CreateReason create_reason,
      int digest_nonce_count,
      const NetLogWithSource& net_log,
      HostResolver* host_resolver,
      std::unique_ptr<HttpAuthHandler>* handler) = 0;

  // Creates an HTTP authentication handler based on the authentication
  // challenge string |challenge|.
  // This is a convenience function which creates a ChallengeTokenizer for
  // |challenge| and calls |CreateAuthHandler|. See |CreateAuthHandler| for
  // more details on return values.
  int CreateAuthHandlerFromString(
      std::string_view challenge,
      HttpAuth::Target target,
      const SSLInfo& ssl_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::SchemeHostPort& scheme_host_port,
      const NetLogWithSource& net_log,
      HostResolver* host_resolver,
      std::unique_ptr<HttpAuthHandler>* handler);

  // Creates an HTTP authentication handler based on the authentication
  // challenge string |challenge|.
  // This is a convenience function which creates a ChallengeTokenizer for
  // |challenge| and calls |CreateAuthHandler|. See |CreateAuthHandler| for
  // more details on return values.
  int CreatePreemptiveAuthHandlerFromString(
      const std::string& challenge,
      HttpAuth::Target target,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::SchemeHostPort& scheme_host_port,
      int digest_nonce_count,
      const NetLogWithSource& net_log,
      HostResolver* host_resolver,
      std::unique_ptr<HttpAuthHandler>* handler);

  // Creates a standard HttpAuthHandlerRegistryFactory. The caller is
  // responsible for deleting the factory.
  // The default factory supports Basic, Digest, NTLM, and Negotiate schemes.
  //
  // |negotiate_auth_system_factory| is used to override the default auth system
  // used by the Negotiate authentication handler.
  static std::unique_ptr<HttpAuthHandlerRegistryFactory> CreateDefault(
      const HttpAuthPreferences* prefs = nullptr
#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
      ,
      const std::string& gssapi_library_name = ""
#endif
#if BUILDFLAG(USE_KERBEROS)
      ,
      HttpAuthMechanismFactory negotiate_auth_system_factory =
          HttpAuthMechanismFactory()
#endif
  );

 private:
  // The preferences for HTTP authentication.
  raw_ptr<const HttpAuthPreferences> http_auth_preferences_ = nullptr;
};

// The HttpAuthHandlerRegistryFactory dispatches create requests out
// to other factories based on the auth scheme.
class NET_EXPORT HttpAuthHandlerRegistryFactory
    : public HttpAuthHandlerFactory {
 public:
  explicit HttpAuthHandlerRegistryFactory(
      const HttpAuthPreferences* http_auth_preferences);

  HttpAuthHandlerRegistryFactory(const HttpAuthHandlerRegistryFactory&) =
      delete;
  HttpAuthHandlerRegistryFactory& operator=(
      const HttpAuthHandlerRegistryFactory&) = delete;

  ~HttpAuthHandlerRegistryFactory() override;

  // Sets the preferences into the factory associated with |scheme|.
  void SetHttpAuthPreferences(const std::string& scheme,
                              const HttpAuthPreferences* prefs);

  // Registers a |factory| that will be used for a particular HTTP
  // authentication scheme such as Basic, Digest, or Negotiate.
  // The |*factory| object is assumed to be new-allocated, and its lifetime
  // will be managed by this HttpAuthHandlerRegistryFactory object (including
  // deleting it when it is no longer used.
  // A nullptr |factory| value means that HttpAuthHandlers's will not be created
  // for |scheme|. If a factory object used to exist for |scheme|, it will be
  // deleted.
  void RegisterSchemeFactory(const std::string& scheme,
                             std::unique_ptr<HttpAuthHandlerFactory> factory);

  // Creates an HttpAuthHandlerRegistryFactory.
  //
  // |prefs| is a pointer to the (single) authentication preferences object.
  // That object tracks preference, and hence policy, updates relevant to HTTP
  // authentication, and provides the current values of the preferences.
  //
  // |negotiate_auth_system_factory| is used to override the default auth system
  // used by the Negotiate authentication handler.
  static std::unique_ptr<HttpAuthHandlerRegistryFactory> Create(
      const HttpAuthPreferences* prefs
#if BUILDFLAG(USE_EXTERNAL_GSSAPI)
      ,
      const std::string& gssapi_library_name = ""
#endif
#if BUILDFLAG(USE_KERBEROS)
      ,
      HttpAuthMechanismFactory negotiate_auth_system_factory =
          HttpAuthMechanismFactory()
#endif
  );

  // Creates an auth handler by dispatching out to the registered factories
  // based on the first token in |challenge|.
  //
  // |host_resolver| is used by the Negotiate authentication handler to perform
  // CNAME lookups to generate a Kerberos SPN for the server. If the "negotiate"
  // scheme is used and the factory was created with
  // |negotiate_disable_cname_lookup| false, |host_resolver| must not be null,
  // and it must remain valid for the lifetime of the created |handler|.
  int CreateAuthHandler(
      HttpAuthChallengeTokenizer* challenge,
      HttpAuth::Target target,
      const SSLInfo& ssl_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::SchemeHostPort& scheme_host_port,
      CreateReason reason,
      int digest_nonce_count,
      const NetLogWithSource& net_log,
      HostResolver* host_resolver,
      std::unique_ptr<HttpAuthHandler>* handler) override;

#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_POSIX)
  std::optional<std::string> GetNegotiateLibraryNameForTesting() const;
#endif

  // Returns true if the scheme is allowed to be used for all origins. An auth
  // handler may still be created for an origin if that origin is allowed by
  // policy to use all supported auth handlers.
  bool IsSchemeAllowedForTesting(const std::string& scheme) const;

 private:
  bool IsSchemeAllowed(const std::string& scheme) const;

  // Retrieve the factory for the specified |scheme|. If no factory exists
  // for the |scheme|, nullptr is returned. The returned factory must not be
  // deleted by the caller, and it is guaranteed to be valid until either
  // a new factory is registered for the same scheme, or until this
  // registry factory is destroyed.
  HttpAuthHandlerFactory* GetSchemeFactory(const std::string& scheme) const;

  using FactoryMap =
      std::map<std::string, std::unique_ptr<HttpAuthHandlerFactory>>;
  std::set<std::string> default_auth_schemes_ {
    kBasicAuthScheme, kDigestAuthScheme,
#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
        kNegotiateAuthScheme,
#endif
        kNtlmAuthScheme
  };
  FactoryMap factory_map_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_FACTORY_H_
