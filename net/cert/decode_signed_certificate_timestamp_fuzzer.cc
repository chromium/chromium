// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp.h"

using net::ct::DecodeSignedCertificateTimestamp;
using net::ct::SignedCertificateTimestamp;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  scoped_refptr<SignedCertificateTimestamp> sct;
  base::StringPiece buffer(reinterpret_cast<const char*>(data), size);
  DecodeSignedCertificateTimestamp(&buffer, &sct);
  return 0;
}
