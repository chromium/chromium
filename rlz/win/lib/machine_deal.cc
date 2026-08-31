// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Library functions related to the OEM Deal Confirmation Code.

#include "rlz/win/lib/machine_deal.h"

#include <windows.h>

#include <stddef.h>

#include <vector>

#include "base/strings/strcat.h"
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
  if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch)) {
    return true;
  }

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
// reasonable size.
std::string NormalizeDcc(std::string_view raw_dcc) {
  std::string_view truncated = raw_dcc.substr(0, rlz_lib::kMaxDccLength);
  std::string normalized;
  normalized.reserve(truncated.size());
  for (char ch : truncated) {
    normalized.push_back(IsGoodDccChar(ch) ? ch : '.');
  }
  return normalized;
}

// TODO(crbug.com/351564777): Refactor GetResponseLine and GetResponseValue to
// return std::string_view slices and avoid intermediate string allocations.
bool GetResponseLine(std::string_view response_text,
                     size_t* search_index,
                     std::string* response_line) {
  if (!response_line || !search_index ||
      *search_index >= response_text.size()) {
    return false;
  }

  response_line->clear();

  size_t line_begin = *search_index;
  size_t line_end = response_text.find('\n', line_begin);
  std::string_view line =
      (line_end == std::string_view::npos)
          ? response_text.substr(line_begin)
          : response_text.substr(line_begin, line_end - line_begin);
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  *response_line = std::string(line);
  *search_index = (line_end == std::string_view::npos) ? response_text.size()
                                                       : line_end + 1;
  return true;
}

bool GetResponseValue(const std::string& response_line,
                      const std::string& response_key,
                      std::string* value) {
  if (!value) {
    return false;
  }

  value->clear();

  if (!base::StartsWith(response_line, response_key,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  std::vector<std::string> tokens = base::SplitString(
      response_line, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != 2) {
    return false;
  }

  // The first token is the key, the second is the value.  The value is already
  // trimmed for whitespace.
  *value = tokens[1];
  return true;
}

}  // namespace

namespace rlz_lib {

bool MachineDealCode::Set(std::string_view dcc) {
  LibMutex lock;
  if (lock.failed()) {
    return false;
  }

  // Validate the new dcc value.
  if (dcc.size() > kMaxDccLength) {
    ASSERT_STRING("MachineDealCode::Set: DCC length exceeds max allowed.");
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

  std::string normalized_dcc = NormalizeDcc(dcc);

  // Write the DCC to HKLM.
  if (!RegKeyWriteValue(&hklm_key, kDccValueName, normalized_dcc)) {
    ASSERT_STRING("MachineDealCode::Set: Could not write the DCC value");
    return false;
  }

  return true;
}

bool MachineDealCode::GetNewCodeFromPingResponse(std::string_view response,
                                                 bool* has_new_dcc,
                                                 std::string* new_dcc) {
  if (!has_new_dcc || !new_dcc) {
    return false;
  }

  *has_new_dcc = false;
  new_dcc->clear();

  int response_length = -1;
  // TODO(crbug.com/351564777): Modernize IsPingResponseValid to accept
  // std::string_view.
  if (!IsPingResponseValid(std::string(response).c_str(), &response_length) ||
      response_length < 0) {
    return false;
  }

  // Get the current DCC value to compare to later.
  std::optional<std::string> stored_dcc = Get();

  size_t search_index = 0;
  std::string_view response_sub =
      response.substr(0, static_cast<size_t>(response_length));
  std::string response_line;
  std::string new_dcc_value;
  bool old_dcc_confirmed = false;
  const std::string dcc_cgi(kDccCgiVariable);
  const std::string dcc_cgi_response(kSetDccResponseVariable);
  while (GetResponseLine(response_sub, &search_index, &response_line)) {
    std::string value;

    if (!old_dcc_confirmed &&
        GetResponseValue(response_line, dcc_cgi, &value)) {
      // This is the old DCC confirmation - should match value in registry.
      if (value != stored_dcc.value_or("")) {
        return false;  // Corrupted DCC - ignore this response.
      } else {
        old_dcc_confirmed = true;
      }
      continue;
    }

    if (!(*has_new_dcc) &&
        GetResponseValue(response_line, dcc_cgi_response, &value)) {
      // This is the new DCC.
      if (value.size() > kMaxDccLength) {
        continue;  // Too long
      }
      *has_new_dcc = true;
      new_dcc_value = value;
    }
  }

  old_dcc_confirmed |= !stored_dcc;

  *new_dcc = new_dcc_value;
  return old_dcc_confirmed;
}

bool MachineDealCode::SetFromPingResponse(std::string_view response) {
  bool has_new_dcc = false;
  std::string new_dcc;

  bool response_valid =
      GetNewCodeFromPingResponse(response, &has_new_dcc, &new_dcc);

  if (response_valid && has_new_dcc) {
    return Set(new_dcc);
  }

  return response_valid;
}

std::optional<std::string> MachineDealCode::GetAsCgi() {
  std::optional<std::string> dcc = Get();
  if (!dcc) {
    return std::nullopt;
  }
  return base::StrCat({kDccCgiVariable, "=", *dcc});
}

std::optional<std::string> MachineDealCode::Get() {
  LibMutex lock;
  if (lock.failed()) {
    return std::nullopt;
  }

  base::win::RegKey dcc_key(HKEY_LOCAL_MACHINE,
                            RlzValueStoreRegistry::GetWideLibKeyName().c_str(),
                            KEY_READ | KEY_WOW64_32KEY);
  if (!dcc_key.Valid()) {
    return std::nullopt;  // no DCC key.
  }

  std::optional<std::string> dcc = RegKeyReadValue(dcc_key, kDccValueName);
  if (!dcc || dcc->empty()) {
    return std::nullopt;
  }

  return dcc;
}

bool MachineDealCode::Clear() {
  base::win::RegKey dcc_key(HKEY_LOCAL_MACHINE,
                            RlzValueStoreRegistry::GetWideLibKeyName().c_str(),
                            KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);
  if (!dcc_key.Valid()) {
    return false;  // no DCC key.
  }

  dcc_key.DeleteValue(kDccValueName);

  // Verify deletion.
  std::wstring dcc;
  if (dcc_key.ReadValue(kDccValueName, &dcc) == ERROR_SUCCESS) {
    ASSERT_STRING("MachineDealCode::Clear: Could not delete the DCC value.");
    return false;
  }

  return true;
}

}  // namespace rlz_lib
