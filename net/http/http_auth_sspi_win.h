// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains common routines used by NTLM and Negotiate authentication
// using the SSPI API on Windows.

#ifndef NET_HTTP_HTTP_AUTH_SSPI_WIN_H_
#define NET_HTTP_HTTP_AUTH_SSPI_WIN_H_

#include <windows.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

// security.h needs to be included for CredHandle. Unfortunately CredHandle
// is a typedef and can't be forward declared.
#define SECURITY_WIN32 1
#include <security.h>

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_mechanism.h"

namespace net {

class HttpAuthChallengeTokenizer;

// SSPILibrary is introduced so unit tests can mock the calls to Windows' SSPI
// implementation. The default implementation simply passes the arguments on to
// the SSPI implementation provided by Secur32.dll.
//
// A single SSPILibrary can only be used with a single security package. Hence
// the package is bound at construction time. Overridable SSPI methods exclude
// the security package parameter since it is implicit.
class NET_EXPORT_PRIVATE SSPILibrary
    : public base::RefCountedThreadSafe<SSPILibrary> {
 public:
  explicit SSPILibrary(const wchar_t* package);

  // Determines the maximum token length in bytes for a particular SSPI package.
  //
  // |library| and |max_token_length| must be non-nullptr pointers to valid
  // objects.
  //
  // If the return value is OK, |*max_token_length| contains the maximum token
  // length in bytes.
  //
  // If the return value is ERR_UNSUPPORTED_AUTH_SCHEME, |package| is not an
  // known SSPI authentication scheme on this system. |*max_token_length| is not
  // changed.
  //
  // If the return value is ERR_UNEXPECTED, there was an unanticipated problem
  // in the underlying SSPI call. The details are logged, and
  // |*max_token_length| is not changed.
  Error DetermineMaxTokenLength(ULONG* max_token_length);

  virtual SECURITY_STATUS AcquireCredentialsHandle(LPWSTR pszPrincipal,
                                                   unsigned long fCredentialUse,
                                                   void* pvLogonId,
                                                   void* pvAuthData,
                                                   SEC_GET_KEY_FN pGetKeyFn,
                                                   void* pvGetKeyArgument,
                                                   PCredHandle phCredential,
                                                   PTimeStamp ptsExpiry) = 0;

  virtual SECURITY_STATUS InitializeSecurityContext(PCredHandle phCredential,
                                                    PCtxtHandle phContext,
                                                    SEC_WCHAR* pszTargetName,
                                                    unsigned long fContextReq,
                                                    unsigned long Reserved1,
                                                    unsigned long TargetDataRep,
                                                    PSecBufferDesc pInput,
                                                    unsigned long Reserved2,
                                                    PCtxtHandle phNewContext,
                                                    PSecBufferDesc pOutput,
                                                    unsigned long* contextAttr,
                                                    PTimeStamp ptsExpiry) = 0;

  virtual SECURITY_STATUS QueryContextAttributesEx(PCtxtHandle phContext,
                                                   ULONG ulAttribute,
                                                   PVOID pBuffer,
                                                   ULONG cbBuffer) = 0;

  virtual SECURITY_STATUS QuerySecurityPackageInfo(PSecPkgInfoW* pkgInfo) = 0;

  virtual SECURITY_STATUS FreeCredentialsHandle(PCredHandle phCredential) = 0;

  virtual SECURITY_STATUS DeleteSecurityContext(PCtxtHandle phContext) = 0;

  virtual SECURITY_STATUS FreeContextBuffer(PVOID pvContextBuffer) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SSPILibrary>;
  virtual ~SSPILibrary();

  // Security package used with DetermineMaxTokenLength(),
  // QuerySecurityPackageInfo(), AcquireCredentialsHandle(). All of these must
  // be consistent.
  const std::wstring package_name_;

  // Written once by DetermineMaxTokenLength(), which may run concurrently on
  // the per-handler SSPI sequences.
  base::Lock lock_;
  ULONG max_token_length_ GUARDED_BY(lock_) = 0;
  bool is_supported_ GUARDED_BY(lock_) = true;
};

class SSPILibraryDefault : public SSPILibrary {
 public:
  explicit SSPILibraryDefault(const wchar_t* package);

  SECURITY_STATUS AcquireCredentialsHandle(LPWSTR pszPrincipal,
                                           unsigned long fCredentialUse,
                                           void* pvLogonId,
                                           void* pvAuthData,
                                           SEC_GET_KEY_FN pGetKeyFn,
                                           void* pvGetKeyArgument,
                                           PCredHandle phCredential,
                                           PTimeStamp ptsExpiry) override;

  SECURITY_STATUS InitializeSecurityContext(PCredHandle phCredential,
                                            PCtxtHandle phContext,
                                            SEC_WCHAR* pszTargetName,
                                            unsigned long fContextReq,
                                            unsigned long Reserved1,
                                            unsigned long TargetDataRep,
                                            PSecBufferDesc pInput,
                                            unsigned long Reserved2,
                                            PCtxtHandle phNewContext,
                                            PSecBufferDesc pOutput,
                                            unsigned long* contextAttr,
                                            PTimeStamp ptsExpiry) override;

  SECURITY_STATUS QueryContextAttributesEx(PCtxtHandle phContext,
                                           ULONG ulAttribute,
                                           PVOID pBuffer,
                                           ULONG cbBuffer) override;

  SECURITY_STATUS QuerySecurityPackageInfo(PSecPkgInfoW* pkgInfo) override;

  SECURITY_STATUS FreeCredentialsHandle(PCredHandle phCredential) override;

  SECURITY_STATUS DeleteSecurityContext(PCtxtHandle phContext) override;

  SECURITY_STATUS FreeContextBuffer(PVOID pvContextBuffer) override;

 protected:
  ~SSPILibraryDefault() override;
};

class NET_EXPORT_PRIVATE HttpAuthSSPI : public HttpAuthMechanism {
 public:
  HttpAuthSSPI(scoped_refptr<SSPILibrary> sspi_library,
               HttpAuth::Scheme scheme);
  ~HttpAuthSSPI() override;

  // HttpAuthMechanism implementation:
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
  class SspiContext;

  struct TokenResult {
    int net_error = OK;
    std::string auth_token;
    bool has_security_context = false;
  };

  void OnTokenGenerated(TokenResult result);

  // Created on first use; most handlers never generate a token.
  base::SequenceBound<SspiContext> context_;
  scoped_refptr<SSPILibrary> library_;
  HttpAuth::Scheme scheme_;
  std::string decoded_server_auth_token_;
  HttpAuth::DelegationType delegation_type_;

  // Mirrors the validity of the CtxtHandle owned by `context_`, which cannot
  // be inspected off the blocking sequence.
  bool has_security_context_ = false;

  // Valid only while a GenerateAuthToken() call is in flight.
  raw_ptr<std::string> auth_token_ = nullptr;
  CompletionOnceCallback callback_;

  base::WeakPtrFactory<HttpAuthSSPI> weak_factory_{this};
};

// Splits |combined| into domain and username.
// If |combined| is of form "FOO\bar", |domain| will contain "FOO" and |user|
// will contain "bar".
// If |combined| is of form "bar", |domain| will be empty and |user| will
// contain "bar".
// |domain| and |user| must be non-nullptr.
NET_EXPORT_PRIVATE void SplitDomainAndUser(const std::u16string& combined,
                                           std::u16string* domain,
                                           std::u16string* user);

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_SSPI_WIN_H_
