// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/hex_utils.h"

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "net/third_party/quiche/src/quiche/common/quiche_text_utils.h"

namespace net {

std::string HexDecode(std::string_view hex) {
  std::string output;
  const bool success = base::HexStringToString(hex, &output);
  DCHECK(success);
  return output;
}

std::string HexDump(std::string_view input) {
  return quiche::QuicheTextUtils::HexDump(input);
}

}  // namespace net
