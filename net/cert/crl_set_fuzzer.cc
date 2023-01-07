// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "net/cert/crl_set.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 32 + 32 + 20)
    return 0;

  FuzzedDataProvider data_provider(data, size);
  std::string spki_hash = data_provider.ConsumeBytesAsString(32);
  std::string issuer_hash = data_provider.ConsumeBytesAsString(32);
  size_t serial_length = data_provider.ConsumeIntegralInRange(4, 19);
  std::string serial = data_provider.ConsumeBytesAsString(serial_length);
  std::string crlset_data = data_provider.ConsumeRemainingBytesAsString();

  scoped_refptr<net::CRLSet> out_crl_set;
  net::CRLSet::Parse(crlset_data, &out_crl_set);

  if (out_crl_set) {
    out_crl_set->CheckSPKI(spki_hash);
    out_crl_set->CheckSerial(serial, issuer_hash);
    out_crl_set->IsExpired();
  }

  return 0;
}
