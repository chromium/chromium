// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_FEATURES_H_
#define CRYPTO_FEATURES_H_

#include "base/feature_list.h"
#include "crypto/crypto_export.h"

namespace crypto::features {

// Enable encryption for process bound strings, if supported by the platform.
CRYPTO_EXPORT BASE_DECLARE_FEATURE(kProcessBoundStringEncryption);

// Assign a label to unexportable keys on Windows instead of relying on
// exporting wrapped keys.
CRYPTO_EXPORT BASE_DECLARE_FEATURE(kLabelWindowsUnexportableKeys);

}  // namespace crypto::features

#endif  // CRYPTO_FEATURES_H_
