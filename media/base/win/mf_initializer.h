// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_INITIALIZER_H_
#define MEDIA_BASE_WIN_MF_INITIALIZER_H_

#include "base/compiler_specific.h"
#include "media/base/win/mf_initializer_export.h"

namespace media {

// Must be called before any code that needs MediaFoundation.
MF_INITIALIZER_EXPORT bool InitializeMediaFoundation() WARN_UNUSED_RESULT;

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_INITIALIZER_H_
