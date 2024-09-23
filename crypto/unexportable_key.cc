// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include "base/check.h"
#include "base/functional/bind.h"

namespace crypto {

namespace {
std::unique_ptr<UnexportableKeyProvider> (*g_mock_provider)() = nullptr;
}  // namespace

UnexportableSigningKey::~UnexportableSigningKey() = default;
UnexportableKeyProvider::~UnexportableKeyProvider() = default;
VirtualUnexportableSigningKey::~VirtualUnexportableSigningKey() = default;
VirtualUnexportableKeyProvider::~VirtualUnexportableKeyProvider() = default;

bool UnexportableSigningKey::IsHardwareBacked() const {
  return false;
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderWin();
std::unique_ptr<VirtualUnexportableKeyProvider>
GetVirtualUnexportableKeyProviderWin();
#elif BUILDFLAG(IS_MAC)
std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderMac(
    UnexportableKeyProvider::Config config);
#endif

// Implemented in unexportable_key_software_unsecure.cc.
std::unique_ptr<UnexportableKeyProvider>
GetUnexportableKeyProviderSoftwareUnsecure();

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProvider(
    UnexportableKeyProvider::Config config) {
  if (g_mock_provider) {
    return g_mock_provider();
  }

#if BUILDFLAG(IS_WIN)
  return GetUnexportableKeyProviderWin();
#elif BUILDFLAG(IS_MAC)
  return GetUnexportableKeyProviderMac(std::move(config));
#else
  return nullptr;
#endif
}

std::unique_ptr<VirtualUnexportableKeyProvider>
GetVirtualUnexportableKeyProvider_DO_NOT_USE_METRICS_ONLY() {
#if BUILDFLAG(IS_WIN)
  return GetVirtualUnexportableKeyProviderWin();
#else
  return nullptr;
#endif
}

namespace internal {

bool HasScopedUnexportableKeyProvider() {
  return g_mock_provider != nullptr;
}

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
