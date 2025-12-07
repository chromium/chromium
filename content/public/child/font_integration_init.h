// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_FONT_INTEGRATION_INIT_H_
#define CONTENT_PUBLIC_CHILD_FONT_INTEGRATION_INIT_H_

#include "content/common/content_export.h"

namespace content {

// Initializes the current child process' font integration stack, which can be
// DWriteFontProxy or FontDataManager depending on platform and experiment
// state.
CONTENT_EXPORT void InitializeFontIntegration();

// Uninitialize the current child process' font integration stack. This is safe
// to call even if it has not been initialized. After this, calls to load fonts
// may fail.
CONTENT_EXPORT void UninitializeFontIntegration();

}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_FONT_INTEGRATION_INIT_H_
