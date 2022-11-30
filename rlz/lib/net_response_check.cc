// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "rlz/lib/net_response_check.h"

#include "base/strings/string_util.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/crc32.h"
#include "rlz/lib/string_utils.h"

// Checksum validation convenience call for RLZ responses.
namespace rlz_lib {

bool IsPingResponseValid(const char* response, int* checksum_idx) {
  if (!response || !response[0])
    return false;

  if (checksum_idx)
    *checksum_idx = -1;

  if (strlen(response) > kMaxPingResponseLength) {
    ASSERT_STRING("IsPingResponseValid: response is too long to parse.");
    return false;
  }

  // Find the checksum line.
  std::string response_string(response);

  std::string checksum_param("\ncrc32: ");
  int calculated_crc;
  int checksum_index = response_string.find(checksum_param);
  if (checksum_index >= 0) {
    // Calculate checksum of message preceeding checksum line.
    // (+ 1 to include the \n)
    std::string message(response_string.substr(0, checksum_index + 1));
    if (!Crc32(message.c_str(), &calculated_crc))
      return false;
  } else {
    checksum_param = "crc32: ";  // Empty response case.
    if (!base::StartsWith(response_string, checksum_param,
                          base::CompareCase::SENSITIVE))
      return false;

    checksum_index = 0;
    if (!Crc32("", &calculated_crc))
      return false;
  }

  // Find the checksum value on the response.
  int checksum_end = response_string.find("\n", checksum_index + 1);
  if (checksum_end < 0)
    checksum_end = response_string.size();

  int checksum_begin = checksum_index + checksum_param.size();
  std::string checksum =
      response_string.substr(checksum_begin, checksum_end - checksum_begin + 1);
  base::TrimWhitespaceASCII(checksum, base::TRIM_ALL, &checksum);

  if (checksum_idx)
    *checksum_idx = checksum_index;

  return calculated_crc == HexStringToInteger(checksum.c_str());
}

}  // namespace rlz_lib
