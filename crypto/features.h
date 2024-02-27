// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_FEATURES_H_
#define CRYPTO_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

#if BUILDFLAG(IS_MAC)
// Enable the macOS implementation of unexportable keys.
CRYPTO_EXPORT BASE_DECLARE_FEATURE(kEnableMacUnexportableKeys);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace crypto

#endif  // CRYPTO_FEATURES_H_
