// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"

// This contains the portable and the SSPI implementations for NTLM.
// We use NTLM_SSPI for Windows, and NTLM_PORTABLE for other platforms.
#if defined(OS_WIN)
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

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

namespace net {

class HttpAuthPreferences;

// Code for handling HTTP NTLM authentication.
class NET_EXPORT_PRIVATE HttpAuthHandlerNTLM : public HttpAuthHandler {
 public:
  class Factory : public HttpAuthHandlerFactory {
   public:
    Factory();
    ~Factory() override;

    int CreateAuthHandler(HttpAuthChallengeTokenizer* challenge,
                          HttpAuth::Target target,
                          const SSLInfo& ssl_info,
                          const NetworkIsolationKey& network_isolation_key,
                          const GURL& origin,
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

    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

#if defined(NTLM_PORTABLE)
  explicit HttpAuthHandlerNTLM(
      const HttpAuthPreferences* http_auth_preferences);
#endif
#if defined(NTLM_SSPI)
  HttpAuthHandlerNTLM(SSPILibrary* sspi_library,
                      const HttpAuthPreferences* http_auth_preferences);
#endif

  // HttpAuthHandler
  bool NeedsIdentity() override;
  bool AllowsDefaultCredentials() override;

 protected:
  // HttpAuthHandler
  bool Init(HttpAuthChallengeTokenizer* tok,
            const SSLInfo& ssl_info,
            const NetworkIsolationKey& network_isolation_key) override;
  int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                            const HttpRequestInfo* request,
                            CompletionOnceCallback callback,
                            std::string* auth_token) override;
  HttpAuth::AuthorizationResult HandleAnotherChallengeImpl(
      HttpAuthChallengeTokenizer* challenge) override;

 private:
  ~HttpAuthHandlerNTLM() override;

  // Parse the challenge, saving the results into this instance.
  HttpAuth::AuthorizationResult ParseChallenge(HttpAuthChallengeTokenizer* tok);

  // Create an NTLM SPN to identify the |origin| server.
  static std::string CreateSPN(const GURL& origin);

#if defined(NTLM_SSPI)
  HttpAuthSSPI mechanism_;
  const HttpAuthPreferences* http_auth_preferences_;
#elif defined(NTLM_PORTABLE)
  HttpAuthNtlmMechanism mechanism_;
#endif

  std::string channel_bindings_;

  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandlerNTLM);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_
