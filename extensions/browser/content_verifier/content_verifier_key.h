// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_KEY_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_KEY_H_

#include "base/containers/span.h"

namespace extensions {

using ContentVerifierKey = base::span<const uint8_t>;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_KEY_H_
