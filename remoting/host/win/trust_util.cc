// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/trust_util.h"

#include <windows.h>

#include <softpub.h>
#include <wintrust.h>

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"

#if defined(OFFICIAL_BUILD)
#include <wincrypt.h>
#endif

namespace remoting {

namespace {

#if defined(OFFICIAL_BUILD)
constexpr wchar_t kGooglePublisherName[] = L"Google LLC";

bool IsBinarySignedByGoogle(HANDLE verify_trust_state_data) {
  CRYPT_PROVIDER_DATA* prov_data =
      WTHelperProvDataFromStateData(verify_trust_state_data);
  if (!prov_data || prov_data->csSigners == 0) {
    return false;
  }

  CRYPT_PROVIDER_SGNR* signer = WTHelperGetProvSignerFromChain(
      prov_data, /*idxSigner=*/0, /*fCounterSigner=*/false,
      /*idxCounterSigner=*/0);
  if (!signer || !signer->pChainContext) {
    return false;
  }

  const CERT_CHAIN_CONTEXT* cert_chain_context = signer->pChainContext;
  if (cert_chain_context->cChain == 0 || !cert_chain_context->rgpChain[0]) {
    return false;
  }

  CERT_SIMPLE_CHAIN* simple_chain = cert_chain_context->rgpChain[0];
  if (simple_chain->cElement == 0 || !simple_chain->rgpElement[0]) {
    return false;
  }

  const CERT_CONTEXT* cert_context = simple_chain->rgpElement[0]->pCertContext;
  if (!cert_context) {
    return false;
  }

  // Get the subject. First ask how long the name is, including null
  // terminator.
  size_t length = CertGetNameStringW(
      cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, /*dwFlags=*/0,
      /*pvTypePara=*/nullptr, /*pszNameString=*/nullptr, /*cchNameString=*/0);
  if (length <= 1) {
    return false;
  }

  std::wstring subject_name;
  CertGetNameStringW(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, /*dwFlags=*/0,
                     /*pvTypePara=*/nullptr,
                     base::WriteInto(&subject_name, length), length);

  VLOG(1) << "Subject name: " << subject_name;

  return subject_name == kGooglePublisherName;
}
#endif  // defined(OFFICIAL_BUILD)

}  // namespace

bool IsBinaryTrusted(const base::FilePath& binary_path) {
#if defined(OFFICIAL_BUILD)
  // This function uses the WinVerifyTrust function to validate the signature
  // for the provided |binary_path|. More information on the structures and
  // function used here can be found at:
  // https://docs.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust

  WINTRUST_FILE_INFO file_info = {0};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = binary_path.value().c_str();
  file_info.hFile = NULL;
  file_info.pgKnownSubject = NULL;

  WINTRUST_DATA wintrust_data = {0};
  wintrust_data.cbStruct = sizeof(wintrust_data);
  wintrust_data.pPolicyCallbackData = NULL;
  wintrust_data.pSIPClientData = NULL;
  wintrust_data.dwUIChoice = WTD_UI_NONE;
  wintrust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrust_data.dwUnionChoice = WTD_CHOICE_FILE;
  wintrust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  wintrust_data.hWVTStateData = NULL;
  wintrust_data.pwszURLReference = NULL;
  wintrust_data.dwUIContext = WTD_UICONTEXT_EXECUTE;
  wintrust_data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
  wintrust_data.pFile = &file_info;

  GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  LONG trust_status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                                     &policy_guid, &wintrust_data);

  // Capture the error code immediately after the verify action as subsequent
  // calls (like the cleanup call below) can reset it.
  DWORD last_error = GetLastError();

  bool is_trusted = false;
  if (trust_status == ERROR_SUCCESS) {
    is_trusted = IsBinarySignedByGoogle(wintrust_data.hWVTStateData);
  }

  // Free the provider data if verify action was performed.
  // We check hWVTStateData as it's allocated by the trust provider.
  if (wintrust_data.hWVTStateData != NULL) {
    wintrust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policy_guid,
                   &wintrust_data);
  }

  if (is_trusted) {
    // Indicates that the binary is trusted:
    //   - The hash that represents the subject is trusted
    //   - The publisher is Google
    //   - No verification or time stamp chain errors
    LOG(INFO) << "Verified signature and publisher for " << binary_path.value();
    return true;
  }

  if (trust_status == ERROR_SUCCESS) {
    LOG(ERROR) << "Binary is signed but not by Google: " << binary_path.value();
    return false;
  }

  switch (trust_status) {
    case TRUST_E_NOSIGNATURE:
      // The file was not signed or had a signature that was not valid.
      // The reason for this status is retrieved via GetLastError(). Note that
      // the last error is a DWORD but the expected values set by this function
      // are HRESULTS so we need to cast.
      switch (static_cast<HRESULT>(last_error)) {
        case TRUST_E_NOSIGNATURE:
          LOG(ERROR) << "No signature found for " << binary_path.value()
                     << ". ErrorReason: 0x" << std::hex << last_error;
          break;
        case TRUST_E_SUBJECT_FORM_UNKNOWN:
          LOG(ERROR) << "The trust provider does not support the form "
                     << "specified for the subject for " << binary_path.value()
                     << ". ErrorReason: 0x" << std::hex << last_error;
          break;
        case TRUST_E_PROVIDER_UNKNOWN:
          LOG(ERROR) << "The trust provider is not recognized on this system "
                     << "for " << binary_path.value() << ". ErrorReason: 0x"
                     << std::hex << last_error;
          break;
        default:
          // The signature was not valid or there was an error opening the file.
          LOG(ERROR) << "Could not verify signature for " << binary_path.value()
                     << ". ErrorReason: " << last_error << ", 0x" << std::hex
                     << last_error;
          break;
      }
      return false;

    case TRUST_E_EXPLICIT_DISTRUST:
      // The hash that represents the subject or the publisher is not allowed by
      // the admin or user.
      LOG(ERROR) << "Signature for " << binary_path.value() << " is present, "
                 << "but is explicitly distrusted.";
      return false;

    case TRUST_E_SUBJECT_NOT_TRUSTED:
      LOG(ERROR) << "Signature for " << binary_path.value() << " is present, "
                 << "but not trusted.";
      return false;

    case CRYPT_E_SECURITY_SETTINGS:
      LOG(ERROR) << "Verification failed for " << binary_path.value() << ". "
                 << "The hash representing the subject or the publisher wasn't "
                 << "explicitly trusted by the admin and admin policy has "
                 << "disabled user trust. No signature, publisher or timestamp "
                 << "errors.";
      return false;

    default:
      LOG(ERROR) << "Signature verification error for " << binary_path.value()
                 << ": 0x" << std::hex << trust_status;
      return false;
  }
#else
  // Binaries are only signed in official builds so running the code above for
  // local builds won't work w/o setting up a test certificate and signing the
  // binaries using it. To simplify local development, bypass the signature
  // checks.
  return true;
#endif
}

}  // namespace remoting
