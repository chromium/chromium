// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_policy/origin_policy.h"  // nogncheck

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_piece.h"

#include <stddef.h>
#include <stdint.h>

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

// Initialize ICU, which is required by the URL parser.
IcuEnvironment* env = new IcuEnvironment();

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  blink::OriginPolicy::From(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  return 0;
}

}  // namespace content
