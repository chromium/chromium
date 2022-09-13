// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
#define CRYPTO_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_

namespace crypto {

// ScopedMockUnexportableKeyProvider causes
// |GetUnexportableKeyProvider| to return a mock, software-based
// implementation of |UnexportableKeyProvider| while it is in scope.
//
// This needs you to link against the test_support target.
class ScopedMockUnexportableKeyProvider {
 public:
  ScopedMockUnexportableKeyProvider();
  ~ScopedMockUnexportableKeyProvider();
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
