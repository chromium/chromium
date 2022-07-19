// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string test_data(reinterpret_cast<const char*>(data), size);
  network::ParseClientHintToDelegatedThirdPartiesHeader(
      test_data, network::MetaCHType::NameAcceptCH);
  network::ParseClientHintToDelegatedThirdPartiesHeader(
      test_data, network::MetaCHType::HttpEquivDelegateCH);
  return 0;
}
