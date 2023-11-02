// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/usage_stats_consent.h"

#include <windows.h>
#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "remoting/host/win/omaha.h"

namespace {

// The following strings are used to construct the registry key names where
// we record whether the user has consented to crash dump collection.
// the user's consent to collect crash dumps is recorded.
const wchar_t kOmahaClientStateKeyFormat[] =
    L"Software\\Google\\Update\\%ls\\%ls";
const wchar_t kOmahaClientState[] = L"ClientState";
const wchar_t kOmahaClientStateMedium[] = L"ClientStateMedium";
const wchar_t kOmahaUsagestatsValue[] = L"usagestats";

LONG ReadUsageStatsValue(const wchar_t* state_key, DWORD* usagestats_out) {
  // presubmit: allow wstring
  std::wstring client_state = base::StringPrintf(kOmahaClientStateKeyFormat,
                                                 state_key,
                                                 remoting::kHostOmahaAppid);
  base::win::RegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, client_state.c_str(), KEY_READ);
  if (result != ERROR_SUCCESS) {
    return result;
  }

  return key.ReadValueDW(kOmahaUsagestatsValue, usagestats_out);
}

}  // namespace

namespace remoting {

bool GetUsageStatsConsent(bool* allowed, bool* set_by_policy) {
  // TODO(alexeypa): report whether the consent is set by policy once
  // supported.
  *set_by_policy = false;

  // The user's consent to collect crash dumps is recorded as the "usagestats"
  // value in the ClientState or ClientStateMedium key. Probe the
  // ClientStateMedium key first.
  DWORD value = 0;
  if (ReadUsageStatsValue(kOmahaClientStateMedium, &value) == ERROR_SUCCESS) {
    *allowed = value != 0;
    return true;
  }
  if (ReadUsageStatsValue(kOmahaClientState, &value) == ERROR_SUCCESS) {
    *allowed = value != 0;
    return true;
  }

  // We do not log the error code here because the logging hasn't been
  // initialized yet.
  return false;
}

bool IsUsageStatsAllowed() {
  bool allowed;
  bool set_by_policy;
  return GetUsageStatsConsent(&allowed, &set_by_policy) && allowed;
}

bool SetUsageStatsConsent(bool allowed) {
  DWORD value = allowed;
  // presubmit: allow wstring
  std::wstring client_state = base::StringPrintf(kOmahaClientStateKeyFormat,
                                                 kOmahaClientStateMedium,
                                                 kHostOmahaAppid);
  base::win::RegKey key;
  LONG result = key.Create(HKEY_LOCAL_MACHINE, client_state.c_str(),
                           KEY_SET_VALUE);
  if (result == ERROR_SUCCESS) {
    result = key.WriteValue(kOmahaUsagestatsValue, value);
    if (result == ERROR_SUCCESS) {
      return true;
    }
  }

  PLOG(ERROR) << "Failed to record the user's consent to crash dump reporting";
  return false;
}

}  // namespace remoting
