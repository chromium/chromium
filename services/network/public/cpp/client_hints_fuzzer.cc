// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"

// This is a workaround for https://crbug.com/778929.
struct Environment {
  Environment() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

Environment* environment = new Environment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string test_data(reinterpret_cast<const char*>(data), size);
  network::ParseClientHintToDelegatedThirdPartiesHeader(
      test_data, network::MetaCHType::HttpEquivDelegateCH);
  return 0;
}
