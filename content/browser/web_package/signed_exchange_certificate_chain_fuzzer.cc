// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_certificate_chain.h"  // nogncheck

#include "base/compiler_specific.h"

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // SAFETY: required from fuzzer.
  SignedExchangeCertificateChain::Parse(
      UNSAFE_BUFFERS(base::span(data, size)), nullptr);
  return 0;
}

}  // namespace content
