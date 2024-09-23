// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/web_package/signed_exchange_certificate_chain.h"  // nogncheck


namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  SignedExchangeCertificateChain::Parse(base::make_span(data, size), nullptr);
  return 0;
}

}  // namespace content
