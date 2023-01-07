// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

// This contains the portable and the SSPI implementations for NTLM.
// We use NTLM_SSPI for Windows, and NTLM_PORTABLE for other platforms.
#if BUILDFLAG(IS_WIN)
#define NTLM_SSPI
#else
#define NTLM_PORTABLE
#endif

#if defined(NTLM_SSPI)
#include "net/http/http_auth_sspi_win.h"
#elif defined(NTLM_PORTABLE)
#include "net/http/http_auth_ntlm_mechanism.h"
#endif

#include <memory>
#include <string>
#include <vector>

#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

namespace url {
class SchemeHostPort;
}

namespace net {

class HttpAuthPreferences;

// Code for handling HTTP NTLM authentication.
class NET_EXPORT_PRIVATE HttpAuthHandlerNTLM : public HttpAuthHandler {
 public:
  class Factory : public HttpAuthHandlerFactory {
   public:
    Factory();

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    ~Factory() override;

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
#if defined(NTLM_SSPI)
    // Set the SSPILibrary to use. Typically the only callers which need to use
    // this are unit tests which pass in a mocked-out version of the SSPI
    // library.  After the call |sspi_library| will be owned by this Factory and
    // will be destroyed when the Factory is destroyed.
    void set_sspi_library(std::unique_ptr<SSPILibrary> sspi_library) {
      sspi_library_ = std::move(sspi_library);
    }
#endif  // defined(NTLM_SSPI)

   private:
#if defined(NTLM_SSPI)
    std::unique_ptr<SSPILibrary> sspi_library_;
#endif  // defined(NTLM_SSPI)
  };

#if defined(NTLM_PORTABLE)
  explicit HttpAuthHandlerNTLM(
      const HttpAuthPreferences* http_auth_preferences);
#endif
#if defined(NTLM_SSPI)
  HttpAuthHandlerNTLM(SSPILibrary* sspi_library,
                      const HttpAuthPreferences* http_auth_preferences);
#endif

  HttpAuthHandlerNTLM(const HttpAuthHandlerNTLM&) = delete;
  HttpAuthHandlerNTLM& operator=(const HttpAuthHandlerNTLM&) = delete;

  ~HttpAuthHandlerNTLM() override;

  // HttpAuthHandler
  bool NeedsIdentity() override;
  bool AllowsDefaultCredentials() override;

 protected:
  // HttpAuthHandler
  bool Init(HttpAuthChallengeTokenizer* tok,
            const SSLInfo& ssl_info,
            const NetworkAnonymizationKey& network_anonymization_key) override;
  int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                            const HttpRequestInfo* request,
                            CompletionOnceCallback callback,
                            std::string* auth_token) override;
  HttpAuth::AuthorizationResult HandleAnotherChallengeImpl(
      HttpAuthChallengeTokenizer* challenge) override;

 private:
  // Parse the challenge, saving the results into this instance.
  HttpAuth::AuthorizationResult ParseChallenge(HttpAuthChallengeTokenizer* tok);

  // Create an NTLM SPN to identify the |scheme_host_port| server.
  static std::string CreateSPN(const url::SchemeHostPort& scheme_host_port);

#if defined(NTLM_SSPI)
  HttpAuthSSPI mechanism_;
  raw_ptr<const HttpAuthPreferences> http_auth_preferences_;
#elif defined(NTLM_PORTABLE)
  HttpAuthNtlmMechanism mechanism_;
#endif

  std::string channel_bindings_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_
