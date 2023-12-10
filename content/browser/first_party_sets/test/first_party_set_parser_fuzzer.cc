// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <cstdint>
#include <memory>
#include <sstream>

#include "base/version.h"

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string string_input(reinterpret_cast<const char*>(data), size);
  std::istringstream stream(string_input);
  FirstPartySetParser::ParseSetsFromStream(stream, base::Version("1.0"), false,
                                           false);

  return 0;
}

}  // namespace content
