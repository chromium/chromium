// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/rand_util.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/thunk/thunk.h"

// The crypto interface doesn't have a normal C -> C++ thunk since it doesn't
// actually have any proxy wrapping or associated objects; it's just a call
// into base. So we implement the entire interface here, using the thunk
// namespace so it magically gets hooked up in the proper places.

namespace ppapi {

namespace {

// TODO(tsepez): this should be delcared UNSAFE_BUFFER_USAGE.
void GetRandomBytes(char* buffer, uint32_t num_bytes) {
  base::RandBytes(base::as_writable_bytes(
      // SAFETY: The caller is required to give a valid pointer and size pair.
      UNSAFE_BUFFERS(base::span(buffer, num_bytes))));
}

const PPB_Crypto_Dev crypto_interface = {&GetRandomBytes};

}  // namespace

namespace thunk {

PPAPI_THUNK_EXPORT const PPB_Crypto_Dev_0_1* GetPPB_Crypto_Dev_0_1_Thunk() {
  return &crypto_interface;
}

}  // namespace thunk

}  // namespace ppapi
