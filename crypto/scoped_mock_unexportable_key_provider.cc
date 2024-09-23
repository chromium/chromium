// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/scoped_mock_unexportable_key_provider.h"

#include <vector>

#include "crypto/unexportable_key.h"

namespace crypto {

namespace {

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderMock() {
  return GetSoftwareUnsecureUnexportableKeyProvider();
}

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderNull() {
  return nullptr;
}

}  // namespace

ScopedMockUnexportableKeyProvider::ScopedMockUnexportableKeyProvider() {
  internal::SetUnexportableKeyProviderForTesting(
      GetUnexportableKeyProviderMock);
}

ScopedMockUnexportableKeyProvider::~ScopedMockUnexportableKeyProvider() {
  internal::SetUnexportableKeyProviderForTesting(nullptr);
}

ScopedNullUnexportableKeyProvider::ScopedNullUnexportableKeyProvider() {
  internal::SetUnexportableKeyProviderForTesting(
      GetUnexportableKeyProviderNull);
}

ScopedNullUnexportableKeyProvider::~ScopedNullUnexportableKeyProvider() {
  internal::SetUnexportableKeyProviderForTesting(nullptr);
}

}  // namespace crypto
