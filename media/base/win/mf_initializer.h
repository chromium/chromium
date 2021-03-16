// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_INITIALIZER_H_
#define MEDIA_BASE_WIN_MF_INITIALIZER_H_

#include <mfapi.h>

#include <memory>

#include "base/compiler_specific.h"
#include "media/base/win/mf_initializer_export.h"

namespace media {

// Handy-dandy wrapper struct that kills MediaFoundation on destruction.
struct MF_INITIALIZER_EXPORT MFSession {
  ~MFSession();
};

using MFSessionLifetime = std::unique_ptr<MFSession>;

// Make sure that MFShutdown is called for each MFStartup that is successful.
// The public documentation stating that it needs to have a corresponding
// shutdown for all startups (even failed ones) is wrong.
MF_INITIALIZER_EXPORT MFSessionLifetime InitializeMediaFoundation()
    WARN_UNUSED_RESULT;

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_INITIALIZER_H_
