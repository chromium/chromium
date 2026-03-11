// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_certificate_chain.h"  // nogncheck

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace content {

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  SignedExchangeCertificateChain::Parse(data, nullptr);
  return 0;
}

}  // namespace content
