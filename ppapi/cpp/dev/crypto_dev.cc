// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/crypto_dev.h"

#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Crypto_Dev_0_1>() {
  return PPB_CRYPTO_DEV_INTERFACE_0_1;
}

}  // namespace

bool Crypto_Dev::GetRandomBytes(char* buffer, uint32_t num_bytes) {
  if (has_interface<PPB_Crypto_Dev_0_1>()) {
    get_interface<PPB_Crypto_Dev_0_1>()->GetRandomBytes(buffer, num_bytes);
    return true;
  }
  return false;
}

}  // namespace pp
