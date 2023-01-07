// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/capi_util.h"

#include <stddef.h>
#include <stdlib.h>

namespace crypto {

void* WINAPI CryptAlloc(size_t size) {
  return malloc(size);
}

void WINAPI CryptFree(void* p) {
  free(p);
}

}  // namespace crypto
