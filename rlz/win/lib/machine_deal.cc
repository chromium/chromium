// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Library functions related to the OEM Deal Confirmation Code.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/win/lib/machine_deal.h"

#include <windows.h>

#include <stddef.h>

#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/machine_deal_win.h"
#include "rlz/lib/net_response_check.h"
#include "rlz/win/lib/lib_mutex.h"
#include "rlz/win/lib/registry_util.h"
#include "rlz/win/lib/rlz_value_store_registry.h"

namespace {

const wchar_t kDccValueName[]             = L"DCC";

// Current DCC can only uses [a-zA-Z0-9_-!@$*();.<>,:]
// We will be more liberal and allow some additional chars, but not url meta
// chars.
bool IsGoodDccChar(char ch) {
  if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch))
    return true;

  switch (ch) {
    case '_':
    case '-':
    case '!':
    case '@':
    case '$':
    case '*':
    case '(':
    case ')':
    case ';':
    case '.':
    case '<':
    case '>':
    case ',':
    case ':':
      return true;
  }

  return false;
}

// This function will remove bad rlz chars and also limit the max rlz to some
// reasonable size. It also assumes that normalized_dcc is at least
// kMaxDccLength+1 long.
void NormalizeDcc(const char* raw_dcc, char* normalized_dcc) {
  size_t index = 0;
  for (; raw_dcc[index] != 0 && index < rlz_lib::kMaxDccLength; ++index) {
    char current = raw_dcc[index];
    if (IsGoodDccChar(current)) {
      normalized_dcc[index] = current;
    } else {
      normalized_dcc[index] = '.';
    }
  }

  normalized_dcc[index] = 0;
}

bool GetResponseLine(const char* response_text, int response_length,
                     int* search_index, std::string* response_line) {
  if (!response_line || !search_index || *search_index > response_length)
    return false;

  response_line->clear();

  if (*search_index < 0)
    return false;

  int line_begin = *search_index;
  const char* line_end = strchr(response_text + line_begin, '\n');

  if (line_end == NULL || line_end - response_text > response_length) {
    line_end = response_text + response_length;
    *search_index = -1;
  } else {
    *search_index = line_end - response_text + 1;
  }

  response_line->assign(response_text + line_begin,
                        line_end - response_text - line_begin);
  return true;
}

bool GetResponseValue(const std::string& response_line,
                      const std::string& response_key,
                      std::string* value) {
  if (!value)
    return false;

  value->clear();

  if (!base::StartsWith(response_line, response_key,
                        base::CompareCase::SENSITIVE))
    return false;

  std::vector<std::string> tokens = base::SplitString(
      response_line, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 2)
    return false;

  // The first token is the key, the second is the value.  The value is already
  // trimmed for whitespace.
  *value = tokens[1];
  return true;
}

}  // namespace

namespace rlz_lib {

bool MachineDealCode::Set(const char* dcc) {
  LibMutex lock;
  if (lock.failed())
    return false;

  // TODO: if (!ProcessInfo::CanWriteMachineKey()) return false;

  // Validate the new dcc value.
  size_t length = strlen(dcc);
  if (length >  kMaxDccLength) {
    ASSERT_STRING("MachineDealCode::Set: DCC length is exceeds max allowed.");
    return false;
  }

  base::win::RegKey hklm_key(HKEY_LOCAL_MACHINE,
                             RlzValueStoreRegistry::GetWideLibKeyName().c_str(),
                             KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);
  if (!hklm_key.Valid()) {
    ASSERT_STRING("MachineDealCode::Set: Unable to create / open machine key."
                  " Did you call rlz_lib::CreateMachineState()?");
    return false;
  }

  char normalized_dcc[kMaxDccLength + 1];
  NormalizeDcc(dcc, normalized_dcc);
  VERIFY(length == strlen(normalized_dcc));

  // Write the DCC to HKLM.  Note that we need to include the null character
  // when writing the string.
  if (!RegKeyWriteValue(&hklm_key, kDccValueName, normalized_dcc)) {
    ASSERT_STRING("MachineDealCode::Set: Could not write the DCC value");
    return false;
  }

  return true;
}

bool MachineDealCode::GetNewCodeFromPingResponse(const char* response,
    bool* has_new_dcc, char* new_dcc, int new_dcc_size) {
  if (!has_new_dcc || !new_dcc || !new_dcc_size)
    return false;

  *has_new_dcc = false;
  new_dcc[0] = 0;

  int response_length = -1;
  if (!IsPingResponseValid(response, &response_length))
    return false;

  // Get the current DCC value to compare to later)
  char stored_dcc[kMaxDccLength + 1];
  if (!Get(stored_dcc, std::size(stored_dcc)))
    stored_dcc[0] = 0;

  int search_index = 0;
  std::string response_line;
  std::string new_dcc_value;
  bool old_dcc_confirmed = false;
  const std::string dcc_cgi(kDccCgiVariable);
  const std::string dcc_cgi_response(kSetDccResponseVariable);
  while (GetResponseLine(response, response_length, &search_index,
                         &response_line)) {
    std::string value;

    if (!old_dcc_confirmed &&
        GetResponseValue(response_line, dcc_cgi, &value)) {
      // This is the old DCC confirmation - should match value in registry.
      if (value != stored_dcc)
        return false;  // Corrupted DCC - ignore this response.
      else
        old_dcc_confirmed = true;
      continue;
    }

    if (!(*has_new_dcc) &&
        GetResponseValue(response_line, dcc_cgi_response, &value)) {
      // This is the new DCC.
      if (value.size() > kMaxDccLength) continue;  // Too long
      *has_new_dcc = true;
      new_dcc_value = value;
    }
  }

  old_dcc_confirmed |= (NULL == stored_dcc[0]);

  base::strlcpy(new_dcc, new_dcc_value.c_str(), new_dcc_size);
  return old_dcc_confirmed;
}

bool MachineDealCode::SetFromPingResponse(const char* response) {
  bool has_new_dcc = false;
  char new_dcc[kMaxDccLength + 1];

  bool response_valid = GetNewCodeFromPingResponse(response, &has_new_dcc,
                                                   new_dcc, std::size(new_dcc));

  if (response_valid && has_new_dcc)
    return Set(new_dcc);

  return response_valid;
}

bool MachineDealCode::GetAsCgi(char* cgi, int cgi_size) {
  if (!cgi || cgi_size <= 0) {
    ASSERT_STRING("MachineDealCode::GetAsCgi: Invalid buffer");
    return false;
  }

  cgi[0] = 0;

  std::string cgi_arg;
  base::StringAppendF(&cgi_arg, "%s=", kDccCgiVariable);
  int cgi_arg_length = cgi_arg.size();

  if (cgi_arg_length >= cgi_size) {
    ASSERT_STRING("MachineDealCode::GetAsCgi: Insufficient buffer size");
    return false;
  }

  base::strlcpy(cgi, cgi_arg.c_str(), cgi_size);

  if (!Get(cgi + cgi_arg_length, cgi_size - cgi_arg_length)) {
    cgi[0] = 0;
    return false;
  }
  return true;
}

bool MachineDealCode::Get(char* dcc, int dcc_size) {
  LibMutex lock;
  if (lock.failed())
    return false;

  if (!dcc || dcc_size <= 0) {
    ASSERT_STRING("MachineDealCode::Get: Invalid buffer");
    return false;
  }

  dcc[0] = 0;

  base::win::RegKey dcc_key(HKEY_LOCAL_MACHINE,
                            RlzValueStoreRegistry::GetWideLibKeyName().c_str(),
                            KEY_READ | KEY_WOW64_32KEY);
  if (!dcc_key.Valid())
    return false;  // no DCC key.

  size_t size = dcc_size;
  if (!RegKeyReadValue(dcc_key, kDccValueName, dcc, &size)) {
    ASSERT_STRING("MachineDealCode::Get: Insufficient buffer size");
    dcc[0] = 0;
    return false;
  }

  return true;
}

bool MachineDealCode::Clear() {
  base::win::RegKey dcc_key(HKEY_LOCAL_MACHINE,
                            RlzValueStoreRegistry::GetWideLibKeyName().c_str(),
                            KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);
  if (!dcc_key.Valid())
    return false;  // no DCC key.

  dcc_key.DeleteValue(kDccValueName);

  // Verify deletion.
  wchar_t dcc[kMaxDccLength + 1];
  DWORD dcc_size = std::size(dcc);
  if (dcc_key.ReadValue(kDccValueName, dcc, &dcc_size, NULL) == ERROR_SUCCESS) {
    ASSERT_STRING("MachineDealCode::Clear: Could not delete the DCC value.");
    return false;
  }

  return true;
}

}  // namespace rlz_lib
