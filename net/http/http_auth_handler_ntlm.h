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
#define SECURITY_WIN32 1
#include <windows.h>
#include <security.h>
#include "net/http/http_auth_sspi_win.h"
#elif defined(NTLM_PORTABLE)
#include "net/ntlm/ntlm_client.h"
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
  // A function that returns the time as the number of 100 nanosecond ticks
  // since Jan 1, 1601 (UTC).
  typedef uint64_t (*GetMSTimeProc)();

  // A function that generates n random bytes in the output buffer.
  typedef void (*GenerateRandomProc)(uint8_t* output, size_t n);

  // A function that returns the local host name. Returns an empty string if
  // the local host name is not available.
  typedef std::string (*HostNameProc)();

  // For unit tests to override and restore the GenerateRandom and
  // GetHostName functions.
  class ScopedProcSetter {
   public:
    ScopedProcSetter(GetMSTimeProc ms_time_proc,
                     GenerateRandomProc random_proc,
                     HostNameProc host_name_proc) {
      old_ms_time_proc_ = SetGetMSTimeProc(ms_time_proc);
      old_random_proc_ = SetGenerateRandomProc(random_proc);
      old_host_name_proc_ = SetHostNameProc(host_name_proc);
    }

    ~ScopedProcSetter() {
      SetGetMSTimeProc(old_ms_time_proc_);
      SetGenerateRandomProc(old_random_proc_);
      SetHostNameProc(old_host_name_proc_);
    }

   private:
    GetMSTimeProc old_ms_time_proc_;
    GenerateRandomProc old_random_proc_;
    HostNameProc old_host_name_proc_;

    DISALLOW_COPY_AND_ASSIGN(ScopedProcSetter);
  };
#endif

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
  // This function acquires a credentials handle in the SSPI implementation.
  // It does nothing in the portable implementation.
  int InitializeBeforeFirstChallenge();

  // HttpAuthHandler
  bool Init(HttpAuthChallengeTokenizer* tok, const SSLInfo& ssl_info) override;
  int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                            const HttpRequestInfo* request,
                            CompletionOnceCallback callback,
                            std::string* auth_token) override;
  HttpAuth::AuthorizationResult HandleAnotherChallengeImpl(
      HttpAuthChallengeTokenizer* challenge) override;

 private:
  ~HttpAuthHandlerNTLM() override;

#if defined(NTLM_PORTABLE)
  // For unit tests to override the GetMSTime, GenerateRandom and GetHostName
  // functions. Returns the old function.
  static GetMSTimeProc SetGetMSTimeProc(GetMSTimeProc proc);
  static GenerateRandomProc SetGenerateRandomProc(GenerateRandomProc proc);
  static HostNameProc SetHostNameProc(HostNameProc proc);

  // Given an input token received from the server, generate the next output
  // token to be sent to the server.
  std::vector<uint8_t> GetNextToken(base::span<const uint8_t> in_token);
#endif

  // Parse the challenge, saving the results into this instance.
  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuthChallengeTokenizer* tok, bool initial_challenge);

  // Create an NTLM SPN to identify the |origin| server.
  static std::string CreateSPN(const GURL& origin);

#if defined(NTLM_SSPI)
  HttpAuthSSPI auth_sspi_;
#elif defined(NTLM_PORTABLE)
  ntlm::NtlmClient ntlm_client_;
#endif

#if defined(NTLM_PORTABLE)
  static GetMSTimeProc get_ms_time_proc_;
  static GenerateRandomProc generate_random_proc_;
  static HostNameProc get_host_name_proc_;
#endif

  base::string16 domain_;
  AuthCredentials credentials_;
  std::string channel_bindings_;

  // Decoded authentication token that the server returned as part of an NTLM
  // challenge.
  std::string challenge_token_;

#if defined(NTLM_SSPI)
  const HttpAuthPreferences* http_auth_preferences_;
#endif

  DISALLOW_COPY_AND_ASSIGN(HttpAuthHandlerNTLM);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_
