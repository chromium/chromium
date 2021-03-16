// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_initializer.h"

#include <mfapi.h>

#include "base/logging.h"

#include "base/optional.h"

namespace media {

MFSessionLifetime InitializeMediaFoundation() {
  if (MFStartup(MF_VERSION, MFSTARTUP_LITE) == S_OK)
    return std::make_unique<MFSession>();
  DVLOG(1) << "Media Foundation unavailable or it failed to initialize";
  return nullptr;
}

MFSession::~MFSession() {
  MFShutdown();
}

}  // namespace media
