// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See "SSPI Sample Application" at
// http://msdn.microsoft.com/en-us/library/aa918273.aspx

#include "net/http/http_auth_sspi_win.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_multi_round_parse.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"

namespace net {
using DelegationType = HttpAuth::DelegationType;

namespace {

base::Value SecurityStatusToValue(Error mapped_error, SECURITY_STATUS status) {
  base::Value params{base::Value::Type::DICTIONARY};
  params.SetIntKey("net_error", mapped_error);
  params.SetIntKey("security_status", status);
  return params;
}

base::Value AcquireCredentialsHandleParams(const base::string16* domain,
                                           const base::string16* user,
                                           Error result,
                                           SECURITY_STATUS status) {
  base::Value params{base::Value::Type::DICTIONARY};
  if (domain && user) {
    params.SetStringKey("domain", base::UTF16ToUTF8(*domain));
    params.SetStringKey("user", base::UTF16ToUTF8(*user));
  }
  params.SetKey("status", SecurityStatusToValue(result, status));
  return params;
}

base::Value ContextFlagsToValue(DWORD flags) {
  base::Value params{base::Value::Type::DICTIONARY};
  params.SetStringKey("value", base::StringPrintf("0x%08lx", flags));
  params.SetBoolKey("delegated",
                    (flags & ISC_RET_DELEGATE) == ISC_RET_DELEGATE);
  params.SetBoolKey("mutual",
                    (flags & ISC_RET_MUTUAL_AUTH) == ISC_RET_MUTUAL_AUTH);
  return params;
}

base::Value ContextAttributesToValue(SSPILibrary* library,
                                     PCtxtHandle handle,
                                     DWORD attributes) {
  base::Value params{base::Value::Type::DICTIONARY};

  SecPkgContext_NativeNames native_names = {0};
  auto qc_result = library->QueryContextAttributesEx(
      handle, SECPKG_ATTR_NATIVE_NAMES, &native_names, sizeof(native_names));
  if (qc_result == SEC_E_OK && native_names.sClientName &&
      native_names.sServerName) {
    params.SetStringKey("source", base::as_u16cstr(native_names.sClientName));
    params.SetStringKey("target", base::as_u16cstr(native_names.sServerName));
  }

  SecPkgContext_NegotiationInfo negotiation_info = {0};
  qc_result = library->QueryContextAttributesEx(
      handle, SECPKG_ATTR_NEGOTIATION_INFO, &negotiation_info,
      sizeof(negotiation_info));
  if (qc_result == SEC_E_OK && negotiation_info.PackageInfo &&
      negotiation_info.PackageInfo->Name) {
    params.SetStringKey("mechanism",
                        base::as_u16cstr(negotiation_info.PackageInfo->Name));
    params.SetBoolKey("open", negotiation_info.NegotiationState !=
                                  SECPKG_NEGOTIATION_COMPLETE);
  }

  SecPkgContext_Authority authority = {0};
  qc_result = library->QueryContextAttributesEx(handle, SECPKG_ATTR_AUTHORITY,
                                                &authority, sizeof(authority));
  if (qc_result == SEC_E_OK && authority.sAuthorityName) {
    params.SetStringKey("authority",
                        base::as_u16cstr(authority.sAuthorityName));
  }

  params.SetKey("flags", ContextFlagsToValue(attributes));
  return params;
}

base::Value InitializeSecurityContextParams(SSPILibrary* library,
                                            PCtxtHandle handle,
                                            Error result,
                                            SECURITY_STATUS status,
                                            DWORD attributes) {
  base::Value params{base::Value::Type::DICTIONARY};
  params.SetKey("status", SecurityStatusToValue(result, status));
  if (result == OK)
    params.SetKey("context",
                  ContextAttributesToValue(library, handle, attributes));
  return params;
}

Error MapAcquireCredentialsStatusToError(SECURITY_STATUS status) {
  switch (status) {
    case SEC_E_OK:
      return OK;
    case SEC_E_INSUFFICIENT_MEMORY:
      return ERR_OUT_OF_MEMORY;
    case SEC_E_INTERNAL_ERROR:
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case SEC_E_NO_CREDENTIALS:
    case SEC_E_NOT_OWNER:
    case SEC_E_UNKNOWN_CREDENTIALS:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case SEC_E_SECPKG_NOT_FOUND:
      // This indicates that the SSPI configuration does not match expectations
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    default:
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

Error AcquireExplicitCredentials(SSPILibrary* library,
                                 const base::string16& domain,
                                 const base::string16& user,
                                 const base::string16& password,
                                 const NetLogWithSource& net_log,
                                 CredHandle* cred) {
  SEC_WINNT_AUTH_IDENTITY identity;
  identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
  identity.User = reinterpret_cast<unsigned short*>(
      const_cast<wchar_t*>(base::as_wcstr(user)));
  identity.UserLength = user.size();
  identity.Domain = reinterpret_cast<unsigned short*>(
      const_cast<wchar_t*>(base::as_wcstr(domain)));
  identity.DomainLength = domain.size();
  identity.Password = reinterpret_cast<unsigned short*>(
      const_cast<wchar_t*>(base::as_wcstr(password)));
  identity.PasswordLength = password.size();

  TimeStamp expiry;

  net_log.BeginEvent(NetLogEventType::AUTH_LIBRARY_ACQUIRE_CREDS);

  // Pass the username/password to get the credentials handle.
  SECURITY_STATUS status = library->AcquireCredentialsHandle(
      nullptr,                          // pszPrincipal
      SECPKG_CRED_OUTBOUND,             // fCredentialUse
      nullptr,                          // pvLogonID
      &identity,                        // pAuthData
      nullptr,                          // pGetKeyFn (not used)
      nullptr,                          // pvGetKeyArgument (not used)
      cred,                             // phCredential
      &expiry);                         // ptsExpiry

  auto result = MapAcquireCredentialsStatusToError(status);
  net_log.EndEvent(NetLogEventType::AUTH_LIBRARY_ACQUIRE_CREDS, [&] {
    return AcquireCredentialsHandleParams(&domain, &user, result, status);
  });
  return result;
}

Error AcquireDefaultCredentials(SSPILibrary* library,
                                const NetLogWithSource& net_log,
                                CredHandle* cred) {
  TimeStamp expiry;
  net_log.BeginEvent(NetLogEventType::AUTH_LIBRARY_ACQUIRE_CREDS);

  // Pass the username/password to get the credentials handle.
  // Note: Since the 5th argument is nullptr, it uses the default
  // cached credentials for the logged in user, which can be used
  // for a single sign-on.
  SECURITY_STATUS status = library->AcquireCredentialsHandle(
      nullptr,                          // pszPrincipal
      SECPKG_CRED_OUTBOUND,             // fCredentialUse
      nullptr,                          // pvLogonID
      nullptr,                          // pAuthData
      nullptr,                          // pGetKeyFn (not used)
      nullptr,                          // pvGetKeyArgument (not used)
      cred,                             // phCredential
      &expiry);                         // ptsExpiry

  auto result = MapAcquireCredentialsStatusToError(status);
  net_log.EndEvent(NetLogEventType::AUTH_LIBRARY_ACQUIRE_CREDS, [&] {
    return AcquireCredentialsHandleParams(nullptr, nullptr, result, status);
  });
  return result;
}

Error MapInitializeSecurityContextStatusToError(SECURITY_STATUS status) {
  switch (status) {
    case SEC_E_OK:
    case SEC_I_CONTINUE_NEEDED:
      return OK;
    case SEC_I_COMPLETE_AND_CONTINUE:
    case SEC_I_COMPLETE_NEEDED:
    case SEC_I_INCOMPLETE_CREDENTIALS:
    case SEC_E_INCOMPLETE_MESSAGE:
    case SEC_E_INTERNAL_ERROR:
      // These are return codes reported by InitializeSecurityContext
      // but not expected by Chrome (for example, INCOMPLETE_CREDENTIALS
      // and INCOMPLETE_MESSAGE are intended for schannel).
      return ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS;
    case SEC_E_INSUFFICIENT_MEMORY:
      return ERR_OUT_OF_MEMORY;
    case SEC_E_UNSUPPORTED_FUNCTION:
      NOTREACHED();
      return ERR_UNEXPECTED;
    case SEC_E_INVALID_HANDLE:
      NOTREACHED();
      return ERR_INVALID_HANDLE;
    case SEC_E_INVALID_TOKEN:
      return ERR_INVALID_RESPONSE;
    case SEC_E_LOGON_DENIED:
      return ERR_ACCESS_DENIED;
    case SEC_E_NO_CREDENTIALS:
    case SEC_E_WRONG_PRINCIPAL:
      return ERR_INVALID_AUTH_CREDENTIALS;
    case SEC_E_NO_AUTHENTICATING_AUTHORITY:
    case SEC_E_TARGET_UNKNOWN:
      return ERR_MISCONFIGURED_AUTH_ENVIRONMENT;
    default:
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

Error MapQuerySecurityPackageInfoStatusToError(SECURITY_STATUS status) {
  switch (status) {
    case SEC_E_OK:
      return OK;
    case SEC_E_SECPKG_NOT_FOUND:
      // This isn't a documented return code, but has been encountered
      // during testing.
      return ERR_UNSUPPORTED_AUTH_SCHEME;
    default:
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

Error MapFreeContextBufferStatusToError(SECURITY_STATUS status) {
  switch (status) {
    case SEC_E_OK:
      return OK;
    default:
      // The documentation at
      // http://msdn.microsoft.com/en-us/library/aa375416(VS.85).aspx
      // only mentions that a non-zero (or non-SEC_E_OK) value is returned
      // if the function fails, and does not indicate what the failure
      // conditions are.
      return ERR_UNDOCUMENTED_SECURITY_LIBRARY_STATUS;
  }
}

}  // anonymous namespace

Error SSPILibrary::DetermineMaxTokenLength(ULONG* max_token_length) {
  if (!is_supported_)
    return ERR_UNSUPPORTED_AUTH_SCHEME;

  if (max_token_length_ != 0) {
    *max_token_length = max_token_length_;
    return OK;
  }

  DCHECK(max_token_length);
  PSecPkgInfo pkg_info = nullptr;
  is_supported_ = false;

  SECURITY_STATUS status = QuerySecurityPackageInfo(&pkg_info);
  Error rv = MapQuerySecurityPackageInfoStatusToError(status);
  if (rv != OK)
    return rv;
  int token_length = pkg_info->cbMaxToken;

  status = FreeContextBuffer(pkg_info);
  rv = MapFreeContextBufferStatusToError(status);
  if (rv != OK)
    return rv;
  *max_token_length = max_token_length_ = token_length;
  is_supported_ = true;
  return OK;
}

SECURITY_STATUS SSPILibraryDefault::AcquireCredentialsHandle(
    LPWSTR pszPrincipal,
    unsigned long fCredentialUse,
    void* pvLogonId,
    void* pvAuthData,
    SEC_GET_KEY_FN pGetKeyFn,
    void* pvGetKeyArgument,
    PCredHandle phCredential,
    PTimeStamp ptsExpiry) {
  return ::AcquireCredentialsHandleW(
      pszPrincipal, const_cast<LPWSTR>(package_name_.c_str()), fCredentialUse,
      pvLogonId, pvAuthData, pGetKeyFn, pvGetKeyArgument, phCredential,
      ptsExpiry);
}

SECURITY_STATUS SSPILibraryDefault::InitializeSecurityContext(
    PCredHandle phCredential,
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
    PTimeStamp ptsExpiry) {
  return ::InitializeSecurityContextW(phCredential, phContext, pszTargetName,
                                      fContextReq, Reserved1, TargetDataRep,
                                      pInput, Reserved2, phNewContext, pOutput,
                                      contextAttr, ptsExpiry);
}

SECURITY_STATUS SSPILibraryDefault::QueryContextAttributesEx(
    PCtxtHandle phContext,
    ULONG ulAttribute,
    PVOID pBuffer,
    ULONG cbBuffer) {
  // TODO(https://crbug.com/992779): QueryContextAttributesExW is not included
  // in Secur32.Lib in 10.0.18362.0 SDK. This symbol requires switching to using
  // Windows SDK API sets in mincore.lib or OneCore.Lib. Switch to using
  // QueryContextAttributesEx when the switch is made.
  return ::QueryContextAttributes(phContext, ulAttribute, pBuffer);
}

SECURITY_STATUS SSPILibraryDefault::QuerySecurityPackageInfo(
    PSecPkgInfoW* pkgInfo) {
  return ::QuerySecurityPackageInfoW(const_cast<LPWSTR>(package_name_.c_str()),
                                     pkgInfo);
}

SECURITY_STATUS SSPILibraryDefault::FreeCredentialsHandle(
    PCredHandle phCredential) {
  return ::FreeCredentialsHandle(phCredential);
}

SECURITY_STATUS SSPILibraryDefault::DeleteSecurityContext(
    PCtxtHandle phContext) {
  return ::DeleteSecurityContext(phContext);
}

SECURITY_STATUS SSPILibraryDefault::FreeContextBuffer(PVOID pvContextBuffer) {
  return ::FreeContextBuffer(pvContextBuffer);
}

HttpAuthSSPI::HttpAuthSSPI(SSPILibrary* library, HttpAuth::Scheme scheme)
    : library_(library),
      scheme_(scheme),
      delegation_type_(DelegationType::kNone) {
  DCHECK(library_);
  DCHECK(scheme_ == HttpAuth::AUTH_SCHEME_NEGOTIATE ||
         scheme_ == HttpAuth::AUTH_SCHEME_NTLM);
  SecInvalidateHandle(&cred_);
  SecInvalidateHandle(&ctxt_);
}

HttpAuthSSPI::~HttpAuthSSPI() {
  ResetSecurityContext();
  if (SecIsValidHandle(&cred_)) {
    library_->FreeCredentialsHandle(&cred_);
    SecInvalidateHandle(&cred_);
  }
}

bool HttpAuthSSPI::Init(const NetLogWithSource&) {
  return true;
}

bool HttpAuthSSPI::NeedsIdentity() const {
  return decoded_server_auth_token_.empty();
}

bool HttpAuthSSPI::AllowsExplicitCredentials() const {
  return true;
}

void HttpAuthSSPI::SetDelegation(DelegationType delegation_type) {
  delegation_type_ = delegation_type;
}

void HttpAuthSSPI::ResetSecurityContext() {
  if (SecIsValidHandle(&ctxt_)) {
    library_->DeleteSecurityContext(&ctxt_);
    SecInvalidateHandle(&ctxt_);
  }
}

HttpAuth::AuthorizationResult HttpAuthSSPI::ParseChallenge(
    HttpAuthChallengeTokenizer* tok) {
  if (!SecIsValidHandle(&ctxt_)) {
    return net::ParseFirstRoundChallenge(scheme_, tok);
  }
  std::string encoded_auth_token;
  return net::ParseLaterRoundChallenge(scheme_, tok, &encoded_auth_token,
                                       &decoded_server_auth_token_);
}

int HttpAuthSSPI::GenerateAuthToken(const AuthCredentials* credentials,
                                    const std::string& spn,
                                    const std::string& channel_bindings,
                                    std::string* auth_token,
                                    const NetLogWithSource& net_log,
                                    CompletionOnceCallback /*callback*/) {
  // Initial challenge.
  if (!SecIsValidHandle(&cred_)) {
    // ParseChallenge fails early if a non-empty token is received on the first
    // challenge.
    DCHECK(decoded_server_auth_token_.empty());
    int rv = OnFirstRound(credentials, net_log);
    if (rv != OK)
      return rv;
  }

  DCHECK(SecIsValidHandle(&cred_));
  void* out_buf;
  int out_buf_len;
  int rv = GetNextSecurityToken(
      spn, channel_bindings,
      static_cast<void*>(const_cast<char*>(decoded_server_auth_token_.c_str())),
      decoded_server_auth_token_.length(), net_log, &out_buf, &out_buf_len);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend the scheme.
  std::string encode_input(static_cast<char*>(out_buf), out_buf_len);
  std::string encode_output;
  base::Base64Encode(encode_input, &encode_output);
  // OK, we are done with |out_buf|
  free(out_buf);
  if (scheme_ == HttpAuth::AUTH_SCHEME_NEGOTIATE) {
    *auth_token = "Negotiate " + encode_output;
  } else {
    *auth_token = "NTLM " + encode_output;
  }
  return OK;
}

int HttpAuthSSPI::OnFirstRound(const AuthCredentials* credentials,
                               const NetLogWithSource& net_log) {
  DCHECK(!SecIsValidHandle(&cred_));
  int rv = OK;
  if (credentials) {
    base::string16 domain;
    base::string16 user;
    SplitDomainAndUser(credentials->username(), &domain, &user);
    rv = AcquireExplicitCredentials(library_, domain, user,
                                    credentials->password(), net_log, &cred_);
    if (rv != OK)
      return rv;
  } else {
    rv = AcquireDefaultCredentials(library_, net_log, &cred_);
    if (rv != OK)
      return rv;
  }

  return rv;
}

int HttpAuthSSPI::GetNextSecurityToken(const std::string& spn,
                                       const std::string& channel_bindings,
                                       const void* in_token,
                                       int in_token_len,
                                       const NetLogWithSource& net_log,
                                       void** out_token,
                                       int* out_token_len) {
  ULONG max_token_length = 0;
  // Microsoft SDKs have a loose relationship with const.
  Error rv = library_->DetermineMaxTokenLength(&max_token_length);
  if (rv != OK)
    return rv;

  CtxtHandle* ctxt_ptr = nullptr;
  SecBufferDesc in_buffer_desc, out_buffer_desc;
  SecBufferDesc* in_buffer_desc_ptr = nullptr;
  SecBuffer in_buffers[2], out_buffer;

  in_buffer_desc.ulVersion = SECBUFFER_VERSION;
  in_buffer_desc.cBuffers = 0;
  in_buffer_desc.pBuffers = in_buffers;
  if (in_token_len > 0) {
    // Prepare input buffer.
    SecBuffer& sec_buffer = in_buffers[in_buffer_desc.cBuffers++];
    sec_buffer.BufferType = SECBUFFER_TOKEN;
    sec_buffer.cbBuffer = in_token_len;
    sec_buffer.pvBuffer = const_cast<void*>(in_token);
    ctxt_ptr = &ctxt_;
  } else {
    // If there is no input token, then we are starting a new authentication
    // sequence.  If we have already initialized our security context, then
    // we're incorrectly reusing the auth handler for a new sequence.
    if (SecIsValidHandle(&ctxt_)) {
      NOTREACHED();
      return ERR_UNEXPECTED;
    }
  }

  std::vector<char> sec_channel_bindings_buffer;
  if (!channel_bindings.empty()) {
    sec_channel_bindings_buffer.reserve(sizeof(SEC_CHANNEL_BINDINGS) +
                                        channel_bindings.size());
    sec_channel_bindings_buffer.resize(sizeof(SEC_CHANNEL_BINDINGS));
    SEC_CHANNEL_BINDINGS* bindings_desc =
        reinterpret_cast<SEC_CHANNEL_BINDINGS*>(
            sec_channel_bindings_buffer.data());
    bindings_desc->cbApplicationDataLength = channel_bindings.size();
    bindings_desc->dwApplicationDataOffset = sizeof(SEC_CHANNEL_BINDINGS);
    sec_channel_bindings_buffer.insert(sec_channel_bindings_buffer.end(),
                                       channel_bindings.begin(),
                                       channel_bindings.end());
    DCHECK_EQ(sizeof(SEC_CHANNEL_BINDINGS) + channel_bindings.size(),
              sec_channel_bindings_buffer.size());

    SecBuffer& sec_buffer = in_buffers[in_buffer_desc.cBuffers++];
    sec_buffer.BufferType = SECBUFFER_CHANNEL_BINDINGS;
    sec_buffer.cbBuffer = sec_channel_bindings_buffer.size();
    sec_buffer.pvBuffer = sec_channel_bindings_buffer.data();
  }

  if (in_buffer_desc.cBuffers > 0)
    in_buffer_desc_ptr = &in_buffer_desc;

  // Prepare output buffer.
  out_buffer_desc.ulVersion = SECBUFFER_VERSION;
  out_buffer_desc.cBuffers = 1;
  out_buffer_desc.pBuffers = &out_buffer;
  out_buffer.BufferType = SECBUFFER_TOKEN;
  out_buffer.cbBuffer = max_token_length;
  out_buffer.pvBuffer = malloc(out_buffer.cbBuffer);
  if (!out_buffer.pvBuffer)
    return ERR_OUT_OF_MEMORY;

  DWORD context_flags = 0;
  // Firefox only sets ISC_REQ_DELEGATE, but MSDN documentation indicates that
  // ISC_REQ_MUTUAL_AUTH must also be set. On Windows delegation by KDC policy
  // is always respected.
  if (delegation_type_ != DelegationType::kNone)
    context_flags |= (ISC_REQ_DELEGATE | ISC_REQ_MUTUAL_AUTH);

  net_log.BeginEvent(NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX);

  // This returns a token that is passed to the remote server.
  DWORD context_attributes = 0;
  base::string16 spn16 = base::ASCIIToUTF16(spn);
  SECURITY_STATUS status = library_->InitializeSecurityContext(
      &cred_,                          // phCredential
      ctxt_ptr,                        // phContext
      base::as_writable_wcstr(spn16),  // pszTargetName
      context_flags,                   // fContextReq
      0,                               // Reserved1 (must be 0)
      SECURITY_NATIVE_DREP,            // TargetDataRep
      in_buffer_desc_ptr,              // pInput
      0,                               // Reserved2 (must be 0)
      &ctxt_,                          // phNewContext
      &out_buffer_desc,                // pOutput
      &context_attributes,             // pfContextAttr
      nullptr);                        // ptsExpiry
  rv = MapInitializeSecurityContextStatusToError(status);
  net_log.EndEvent(NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX, [&] {
    return InitializeSecurityContextParams(library_, &ctxt_, rv, status,
                                           context_attributes);
  });

  if (rv != OK) {
    ResetSecurityContext();
    free(out_buffer.pvBuffer);
    return rv;
  }
  if (!out_buffer.cbBuffer) {
    free(out_buffer.pvBuffer);
    out_buffer.pvBuffer = nullptr;
  }
  *out_token = out_buffer.pvBuffer;
  *out_token_len = out_buffer.cbBuffer;
  return OK;
}

void SplitDomainAndUser(const base::string16& combined,
                        base::string16* domain,
                        base::string16* user) {
  // |combined| may be in the form "user" or "DOMAIN\user".
  // Separate the two parts if they exist.
  // TODO(cbentzel): I believe user@domain is also a valid form.
  size_t backslash_idx = combined.find(L'\\');
  if (backslash_idx == base::string16::npos) {
    domain->clear();
    *user = combined;
  } else {
    *domain = combined.substr(0, backslash_idx);
    *user = combined.substr(backslash_idx + 1);
  }
}

}  // namespace net
