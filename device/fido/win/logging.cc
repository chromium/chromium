// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/win/logging.h"

#include <string>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/device_event_log/device_event_log.h"

namespace {

constexpr char kSep[] = ", ";

// Quoted wraps |in| in double quotes and backslash-escapes all other double
// quote characters.
std::string Quoted(std::string_view in) {
  std::string result;
  base::ReplaceChars(in, "\\", "\\\\", &result);
  base::ReplaceChars(result, "\"", "\\\"", &result);
  return "\"" + result + "\"";
}

std::wstring Quoted(std::wstring_view in) {
  std::wstring result;
  base::ReplaceChars(in, L"\\", L"\\\\", &result);
  base::ReplaceChars(result, L"\"", L"\\\"", &result);
  return L"\"" + result + L"\"";
}

std::wstring Quoted(const wchar_t* in) {
  return Quoted(std::wstring_view(in ? in : L""));
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_RP_ENTITY_INFORMATION& in) {
  return out << "{" << in.dwVersion << kSep << Quoted(in.pwszId) << kSep
             << Quoted(in.pwszName) << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_USER_ENTITY_INFORMATION& in) {
  return out << "{" << in.dwVersion << kSep << base::HexEncode(in.pbId, in.cbId)
             << kSep << Quoted(in.pwszName) << kSep
             << Quoted(in.pwszDisplayName) << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_COSE_CREDENTIAL_PARAMETER& in) {
  return out << "{" << in.dwVersion << kSep << Quoted(in.pwszCredentialType)
             << kSep << in.lAlg << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_COSE_CREDENTIAL_PARAMETERS& in) {
  out << "{" << in.cCredentialParameters << ", &[";
  for (size_t i = 0; i < in.cCredentialParameters; ++i) {
    out << (i ? kSep : "") << in.pCredentialParameters[i];
  }
  return out << "]}";
}

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CLIENT_DATA& in) {
  return out << "{" << in.dwVersion << kSep
             << Quoted({reinterpret_cast<char*>(in.pbClientDataJSON),
                        in.cbClientDataJSON})
             << kSep << Quoted(in.pwszHashAlgId) << "}";
}

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CREDENTIAL& in) {
  return out << "{" << in.dwVersion << kSep << base::HexEncode(in.pbId, in.cbId)
             << kSep << Quoted(in.pwszCredentialType) << "}";
}

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CREDENTIALS& in) {
  out << "{" << in.cCredentials << ", &[";
  for (size_t i = 0; i < in.cCredentials; ++i) {
    out << (i ? kSep : "") << in.pCredentials[i];
  }
  return out << "]}";
}

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_CREDENTIAL_EX& in) {
  return out << "{" << in.dwVersion << kSep << base::HexEncode(in.pbId, in.cbId)
             << kSep << Quoted(in.pwszCredentialType) << kSep << in.dwTransports
             << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_CREDENTIAL_LIST& in) {
  out << "{" << in.cCredentials << ", &[";
  for (size_t i = 0; i < in.cCredentials; ++i) {
    out << (i ? kSep : "") << "&" << *in.ppCredentials[i];
  }
  return out << "]}";
}

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_EXTENSION& in) {
  return out << "{" << Quoted(in.pwszExtensionIdentifier) << kSep
             << base::HexEncode(in.pvExtension, in.cbExtension) << "}";
}

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_EXTENSIONS& in) {
  out << "{" << in.cExtensions << ", &[";
  for (size_t i = 0; i < in.cExtensions; ++i) {
    out << (i ? kSep : "") << in.pExtensions[i];
  }
  return out << "]}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_HMAC_SECRET_SALT& in) {
  // The salts may be considered sensitive, and this structure is also reused
  // for the outputs, so only the lengths are logged.
  return out << "{[" << in.cbFirst << "]" << kSep << "[" << in.cbSecond << "]}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_CRED_WITH_HMAC_SECRET_SALT& in) {
  return out << "{" << base::HexEncode(in.pbCredID, in.cbCredID) << kSep << "&"
             << *in.pHmacSecretSalt << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_HMAC_SECRET_SALT_VALUES& in) {
  out << "{";
  if (in.pGlobalHmacSalt) {
    out << "&" << *in.pGlobalHmacSalt;
  } else {
    out << "(null)";
  }
  out << kSep << "[";
  for (DWORD i = 0; i < in.cCredWithHmacSecretSaltList; i++) {
    out << (i ? kSep : "") << in.pCredWithHmacSecretSaltList[i];
  }
  return out << "]}";
}

std::ostream& operator<<(
    std::ostream& out,
    const WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS& in) {
  out << "{" << in.dwVersion << kSep << in.dwTimeoutMilliseconds << kSep
      << in.CredentialList << kSep << in.Extensions << kSep
      << in.dwAuthenticatorAttachment << kSep
      << in.dwUserVerificationRequirement << kSep << in.dwFlags;
  if (in.dwVersion < WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_2) {
    return out << "}";
  }
  out << kSep << Quoted(in.pwszU2fAppId);
  if (in.pbU2fAppId) {
    out << ", &" << *in.pbU2fAppId;
  } else {
    out << ", (null)";
  }
  if (in.dwVersion < WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_3) {
    return out << "}";
  }
  if (in.pAllowCredentialList) {
    out << ", &" << *in.pAllowCredentialList;
  } else {
    out << ", (null)";
  }
  if (in.dwVersion < WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_5) {
    return out << "}";
  }
  out << kSep << in.dwCredLargeBlobOperation << "(";
  if (in.cbCredLargeBlob) {
    out << base::HexEncode(in.pbCredLargeBlob, in.cbCredLargeBlob) << ")";
  } else {
    out << "null)";
  }
  if (in.dwVersion < WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_6) {
    return out << "}";
  }
  if (in.pHmacSecretSaltValues) {
    out << ", &" << *in.pHmacSecretSaltValues;
  } else {
    out << ", (null)";
  }
  return out << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS& in) {
  out << "{" << in.dwVersion << kSep << in.dwTimeoutMilliseconds << kSep
      << in.CredentialList << kSep << in.Extensions << kSep
      << in.dwAuthenticatorAttachment << kSep << in.bRequireResidentKey << kSep
      << in.dwUserVerificationRequirement << kSep
      << in.dwAttestationConveyancePreference << kSep << in.dwFlags;
  if (in.dwVersion < WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_2) {
    return out << "}";
  }
  out << kSep << in.pCancellationId;
  if (in.dwVersion < WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_3) {
    return out << "}";
  }
  if (in.pExcludeCredentialList) {
    out << ", &" << *in.pExcludeCredentialList;
  } else {
    out << ", (null)";
  }
  if (in.dwVersion < WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_4) {
    return out << "}";
  }
  out << kSep << in.dwLargeBlobSupport;
  return out << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_CREDENTIAL_ATTESTATION& in) {
  out << "{" << in.dwVersion << kSep << Quoted(in.pwszFormatType) << kSep
      << base::HexEncode(in.pbAuthenticatorData, in.cbAuthenticatorData) << kSep
      << base::HexEncode(in.pbAttestation, in.cbAttestation) << kSep
      << in.dwAttestationDecodeType << kSep
      << base::HexEncode(in.pbAttestationObject, in.cbAttestationObject) << kSep
      << base::HexEncode(in.pbCredentialId, in.cbCredentialId);
  if (in.dwVersion < WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_2) {
    return out << "}";
  }
  out << kSep << in.Extensions;
  if (in.dwVersion < WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3) {
    return out << "}";
  }
  out << kSep << in.dwUsedTransport;
  if (in.dwVersion < WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_4) {
    return out << "}";
  }
  out << kSep << in.bLargeBlobSupported;
  return out << "}";
}

std::ostream& operator<<(std::ostream& out, const WEBAUTHN_ASSERTION& in) {
  out << "{" << in.dwVersion << kSep
      << base::HexEncode(in.pbAuthenticatorData, in.cbAuthenticatorData) << kSep
      << base::HexEncode(in.pbSignature, in.cbSignature) << kSep
      << in.Credential << kSep << base::HexEncode(in.pbUserId, in.cbUserId);
  if (in.dwVersion < WEBAUTHN_ASSERTION_VERSION_2) {
    return out << "}";
  }
  out << in.Extensions;
  out << kSep << in.dwCredLargeBlobStatus << " (";
  if (in.pbCredLargeBlob) {
    out << base::HexEncode(in.pbCredLargeBlob, in.cbCredLargeBlob) << ")";
  } else {
    out << "null)";
  }
  if (in.dwVersion < WEBAUTHN_ASSERTION_VERSION_3) {
    return out << "}";
  }
  if (in.pHmacSecret) {
    out << kSep << "&" << *in.pHmacSecret;
  } else {
    out << ", (null)";
  }
  return out << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const WEBAUTHN_GET_CREDENTIALS_OPTIONS& in) {
  out << "{" << in.dwVersion << kSep;
  if (in.pwszRpId) {
    out << in.pwszRpId;
  } else {
    out << "(null)";
  }
  return out << kSep << in.bBrowserInPrivateMode << "}";
}
