// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp.h"

using net::ct::DecodeSignedCertificateTimestamp;
using net::ct::SignedCertificateTimestamp;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  scoped_refptr<SignedCertificateTimestamp> sct;
  std::string_view buffer(reinterpret_cast<const char*>(data), size);
  DecodeSignedCertificateTimestamp(&buffer, &sct);
  return 0;
}
