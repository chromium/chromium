// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_INITIALIZER_H_
#define MEDIA_BASE_WIN_MF_INITIALIZER_H_

#include "media/base/win/mf_util_export.h"

namespace media {

// Must be called before any code that needs MediaFoundation.
[[nodiscard]] MF_UTIL_EXPORT bool InitializeMediaFoundation();

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_INITIALIZER_H_
