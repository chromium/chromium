// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_CALLBACK_H_
#define PDF_PPAPI_MIGRATION_CALLBACK_H_

#include <stdint.h>

#include "base/callback_forward.h"

namespace chrome_pdf {

// A `base::OnceCallback` compatible with `pp::CompletionCallback`. Accepts an
// `int32_t` result.
using ResultCallback = base::OnceCallback<void(int32_t)>;

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_CALLBACK_H_
