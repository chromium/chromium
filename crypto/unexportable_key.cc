// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include "base/check.h"
#include "build/build_config.h"

namespace crypto {

UnexportableSigningKey::~UnexportableSigningKey() = default;
UnexportableKeyProvider::~UnexportableKeyProvider() = default;

#if BUILDFLAG(IS_WIN)
std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderWin();
#endif

static std::unique_ptr<UnexportableKeyProvider> (*g_mock_provider)() = nullptr;

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProvider() {
  if (g_mock_provider) {
    return g_mock_provider();
  }

#if BUILDFLAG(IS_WIN)
  return GetUnexportableKeyProviderWin();
#else
  return nullptr;
#endif
}

namespace internal {

void SetUnexportableKeyProviderForTesting(
    std::unique_ptr<UnexportableKeyProvider> (*func)()) {
  if (g_mock_provider) {
    // Nesting ScopedMockUnexportableSigningKeyForTesting is not supported.
    CHECK(!func);
    g_mock_provider = nullptr;
  } else {
    g_mock_provider = func;
  }
}

}  // namespace internal
}  // namespace crypto
