// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_INITIALIZER_H_
#define MEDIA_BASE_WIN_MF_INITIALIZER_H_

#include "media/base/media_export.h"

namespace media {

// Must be called before any code that needs MediaFoundation.
[[nodiscard]] MEDIA_EXPORT bool InitializeMediaFoundation();

// Preloads DLLs required for MediaFoundation; returns false if DLLs fail to
// load. InitializeMediaFoundation() will also return false if load fails.
MEDIA_EXPORT bool PreSandboxMediaFoundationInitialization();

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_INITIALIZER_H_
