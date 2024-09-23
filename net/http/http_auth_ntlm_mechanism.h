// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_NTLM_MECHANISM_H_
#define NET_HTTP_HTTP_AUTH_NTLM_MECHANISM_H_

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "net/base/auth.h"
#include "net/base/net_export.h"
#include "net/http/http_auth_mechanism.h"
#include "net/ntlm/ntlm_client.h"

namespace net {

class NET_EXPORT_PRIVATE HttpAuthNtlmMechanism : public HttpAuthMechanism {
 public:
  explicit HttpAuthNtlmMechanism(const HttpAuthPreferences* preferences);
  ~HttpAuthNtlmMechanism() override;

  HttpAuthNtlmMechanism(const HttpAuthNtlmMechanism&) = delete;
  HttpAuthNtlmMechanism& operator=(const HttpAuthNtlmMechanism&) = delete;

  // A function that returns the time as the number of 100 nanosecond ticks
  // since Jan 1, 1601 (UTC).
  using GetMSTimeProc = uint64_t (*)();

  // A function that generates random bytes into the entire output buffer.
  using GenerateRandomProc = void (*)(base::span<uint8_t> output);

  // A function that returns the local host name. Returns an empty string if
  // the local host name is not available.
  using HostNameProc = std::string (*)();

  // For unit tests to override and restore the GenerateRandom and
  // GetHostName functions.
  class ScopedProcSetter {
   public:
    ScopedProcSetter(GetMSTimeProc ms_time_proc,
                     GenerateRandomProc random_proc,
                     HostNameProc host_name_proc);
    ~ScopedProcSetter();

    ScopedProcSetter(const ScopedProcSetter&) = delete;
    ScopedProcSetter& operator=(const ScopedProcSetter&) = delete;

   private:
    GetMSTimeProc old_ms_time_proc_;
    GenerateRandomProc old_random_proc_;
    HostNameProc old_host_name_proc_;
  };

  // HttpAuthMechanism
  bool Init(const NetLogWithSource& net_log) override;
  bool NeedsIdentity() const override;
  bool AllowsExplicitCredentials() const override;
  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuthChallengeTokenizer* tok) override;
  int GenerateAuthToken(const AuthCredentials* credentials,
                        const std::string& spn,
                        const std::string& channel_bindings,
                        std::string* auth_token,
                        const NetLogWithSource& net_log,
                        CompletionOnceCallback callback) override;
  void SetDelegation(HttpAuth::DelegationType delegation_type) override;

 private:
  ntlm::NtlmClient ntlm_client_;

  // Decoded authentication token that the server returned as part of an NTLM
  // challenge.
  std::string challenge_token_;

  // Keep track of whether we sent the negotiate token. While it is still spec
  // compliant to respond to any challenge without a token with a negotiate
  // token, this mechanism considers it an error to respond to a negotiate token
  // with an empty token.
  bool first_token_sent_ = false;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_NTLM_MECHANISM_H_
