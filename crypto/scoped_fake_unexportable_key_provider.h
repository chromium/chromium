// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_FAKE_UNEXPORTABLE_KEY_PROVIDER_H_
#define CRYPTO_SCOPED_FAKE_UNEXPORTABLE_KEY_PROVIDER_H_

namespace crypto {

// ScopedFakeUnexportableKeyProvider causes
// |GetUnexportableKeyProvider| to return a fake, software-based
// implementation of |UnexportableKeyProvider| while it is in scope.
//
// This needs you to link against the test_support target.
class ScopedFakeUnexportableKeyProvider {
 public:
  ScopedFakeUnexportableKeyProvider();
  ScopedFakeUnexportableKeyProvider(const ScopedFakeUnexportableKeyProvider&) =
      delete;
  ScopedFakeUnexportableKeyProvider(ScopedFakeUnexportableKeyProvider&&) =
      delete;
  ~ScopedFakeUnexportableKeyProvider();
};

// `ScopedNullUnexportableKeyProvider` causes `GetUnexportableKeyProvider` to
// return a nullptr, emulating the key provider not being supported.
class ScopedNullUnexportableKeyProvider {
 public:
  ScopedNullUnexportableKeyProvider();
  ScopedNullUnexportableKeyProvider(const ScopedNullUnexportableKeyProvider&) =
      delete;
  ScopedNullUnexportableKeyProvider(ScopedNullUnexportableKeyProvider&&) =
      delete;
  ~ScopedNullUnexportableKeyProvider();
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_FAKE_UNEXPORTABLE_KEY_PROVIDER_H_
