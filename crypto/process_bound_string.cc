// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/process_bound_string.h"

#include "base/containers/span.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dpapi.h>

#include "base/process/memory.h"
#else
#include "third_party/boringssl/src/include/openssl/mem.h"
#endif  // BUILDFLAG(IS_WIN)

namespace crypto::internal {

#if BUILDFLAG(IS_WIN)
static_assert(CRYPTPROTECTMEMORY_BLOCK_SIZE > 0 &&
                  (CRYPTPROTECTMEMORY_BLOCK_SIZE &
                   (CRYPTPROTECTMEMORY_BLOCK_SIZE - 1)) == 0,
              "CRYPTPROTECTMEMORY_BLOCK_SIZE must be a power of two");
#endif  // BUILDFLAG(IS_WIN)

size_t MaybeRoundUp(size_t size) {
#if BUILDFLAG(IS_WIN)
  return (size + CRYPTPROTECTMEMORY_BLOCK_SIZE - 1u) &
         ~(CRYPTPROTECTMEMORY_BLOCK_SIZE - 1u);
#else
  return size;
#endif  // BUILDFLAG(IS_WIN)
}

bool MaybeEncryptBuffer(base::span<uint8_t> buffer) {
#if BUILDFLAG(IS_WIN)
  if (::CryptProtectMemory(buffer.data(), buffer.size(),
                           CRYPTPROTECTMEMORY_SAME_PROCESS)) {
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)
  return false;
}

bool MaybeDecryptBuffer(base::span<uint8_t> buffer) {
#if BUILDFLAG(IS_WIN)
  if (::CryptUnprotectMemory(buffer.data(), buffer.size(),
                             CRYPTPROTECTMEMORY_SAME_PROCESS)) {
    return true;
  }
  if (::GetLastError() == ERROR_WORKING_SET_QUOTA) {
    base::TerminateBecauseOutOfMemory(0);
  }
#endif  // BUILDFLAG(IS_WIN)
  return false;
}

void SecureZeroBuffer(base::span<uint8_t> buffer) {
#if BUILDFLAG(IS_WIN)
  ::SecureZeroMemory(buffer.data(), buffer.size());
#else
  OPENSSL_cleanse(buffer.data(), buffer.size());
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace crypto::internal
