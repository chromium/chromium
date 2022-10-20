// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_NEGOTIATE_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_NEGOTIATE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_mechanism.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/http_auth_negotiate_android.h"
#elif BUILDFLAG(IS_WIN)
#include "net/http/http_auth_sspi_win.h"
#elif BUILDFLAG(IS_POSIX)
#include "net/http/http_auth_gssapi_posix.h"
#endif

namespace url {
class SchemeHostPort;
}

namespace net {

class HttpAuthPreferences;

// Handler for WWW-Authenticate: Negotiate protocol.
//
// See http://tools.ietf.org/html/rfc4178 and http://tools.ietf.org/html/rfc4559
// for more information about the protocol.

class NET_EXPORT_PRIVATE HttpAuthHandlerNegotiate : public HttpAuthHandler {
 public:
#if BUILDFLAG(IS_WIN)
  typedef SSPILibrary AuthLibrary;
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  typedef GSSAPILibrary AuthLibrary;
#endif

  class NET_EXPORT_PRIVATE Factory : public HttpAuthHandlerFactory {
   public:
    explicit Factory(HttpAuthMechanismFactory negotiate_auth_system_factory);
    ~Factory() override;

#if !BUILDFLAG(IS_ANDROID)
    // Sets the system library to use, thereby assuming ownership of
    // |auth_library|.
    void set_library(std::unique_ptr<AuthLibrary> auth_provider) {
      auth_library_ = std::move(auth_provider);
    }

#if BUILDFLAG(IS_POSIX)
    const std::string& GetLibraryNameForTesting() const;
#endif  // BUILDFLAG(IS_POSIX)
#endif  // !BUILDFLAG(IS_ANDROID)

    // HttpAuthHandlerFactory overrides
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

   private:
    HttpAuthMechanismFactory negotiate_auth_system_factory_;
    bool is_unsupported_ = false;
#if !BUILDFLAG(IS_ANDROID)
    std::unique_ptr<AuthLibrary> auth_library_;
#endif  // !BUILDFLAG(IS_ANDROID)
  };

  HttpAuthHandlerNegotiate(std::unique_ptr<HttpAuthMechanism> auth_system,
                           const HttpAuthPreferences* prefs,
                           HostResolver* host_resolver);

  ~HttpAuthHandlerNegotiate() override;

  // HttpAuthHandler
  bool NeedsIdentity() override;
  bool AllowsDefaultCredentials() override;
  bool AllowsExplicitCredentials() override;

  const std::string& spn_for_testing() const { return spn_; }

 protected:
  // HttpAuthHandler
  bool Init(HttpAuthChallengeTokenizer* challenge,
            const SSLInfo& ssl_info,
            const NetworkAnonymizationKey& network_anonymization_key) override;
  int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                            const HttpRequestInfo* request,
                            CompletionOnceCallback callback,
                            std::string* auth_token) override;
  HttpAuth::AuthorizationResult HandleAnotherChallengeImpl(
      HttpAuthChallengeTokenizer* challenge) override;

 private:
  enum State {
    STATE_RESOLVE_CANONICAL_NAME,
    STATE_RESOLVE_CANONICAL_NAME_COMPLETE,
    STATE_GENERATE_AUTH_TOKEN,
    STATE_GENERATE_AUTH_TOKEN_COMPLETE,
    STATE_NONE,
  };

  std::string CreateSPN(const std::string& server,
                        const url::SchemeHostPort& scheme_host_port);

  void OnIOComplete(int result);
  void DoCallback(int result);
  int DoLoop(int result);

  int DoResolveCanonicalName();
  int DoResolveCanonicalNameComplete(int rv);
  int DoGenerateAuthToken();
  int DoGenerateAuthTokenComplete(int rv);
  HttpAuth::DelegationType GetDelegationType() const;

  std::unique_ptr<HttpAuthMechanism> auth_system_;
  const raw_ptr<HostResolver> resolver_;

  NetworkAnonymizationKey network_anonymization_key_;

  // Members which are needed for DNS lookup + SPN.
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;

  // Things which should be consistent after first call to GenerateAuthToken.
  bool already_called_ = false;
  bool has_credentials_ = false;
  AuthCredentials credentials_;
  std::string spn_;
  std::string channel_bindings_;

  // Things which vary each round.
  CompletionOnceCallback callback_;
  raw_ptr<std::string> auth_token_ = nullptr;

  State next_state_ = STATE_NONE;

  raw_ptr<const HttpAuthPreferences> http_auth_preferences_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_NEGOTIATE_H_
