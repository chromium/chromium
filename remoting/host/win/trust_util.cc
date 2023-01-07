// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/trust_util.h"

#include <windows.h>

#include <softpub.h>
#include <wintrust.h>

#include "base/logging.h"

namespace remoting {

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

  DWORD dwLastError;
  switch (trust_status) {
    case ERROR_SUCCESS:
      // Indicates that the binary is trusted:
      //   - The hash that represents the subject is trusted
      //   - The publisher is trusted
      //   - No verification or time stamp chain errors
      LOG(INFO) << "Signature verified for " << binary_path.value();
      return true;

    case TRUST_E_NOSIGNATURE:
      // The file was not signed or had a signature that was not valid.
      // The reason for this status is retrieved via GetLastError(). Note that
      // the last error is a DWORD but the expected values set by this function
      // are HRESULTS so we need to cast.
      dwLastError = GetLastError();
      switch (static_cast<HRESULT>(dwLastError)) {
        case TRUST_E_NOSIGNATURE:
          LOG(ERROR) << "No signature found for " << binary_path.value()
                     << ". ErrorReason: 0x" << std::hex << dwLastError;
          break;
        case TRUST_E_SUBJECT_FORM_UNKNOWN:
          LOG(ERROR) << "The trust provider does not support the form "
                     << "specified for the subject for " << binary_path.value()
                     << ". ErrorReason: 0x" << std::hex << dwLastError;
          break;
        case TRUST_E_PROVIDER_UNKNOWN:
          LOG(ERROR) << "The trust provider is not recognized on this system "
                     << "for " << binary_path.value() << ". ErrorReason: 0x"
                     << std::hex << dwLastError;
          break;
        default:
          // The signature was not valid or there was an error opening the file.
          LOG(ERROR) << "Could not verify signature for " << binary_path.value()
                     << ". ErrorReason: " << dwLastError << ", 0x" << std::hex
                     << dwLastError;
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
