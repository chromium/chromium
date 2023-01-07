// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_FEATURE_CHECKS_H_
#define MEDIA_BASE_WIN_MF_FEATURE_CHECKS_H_

#include "media/base/media_export.h"

namespace media {

MEDIA_EXPORT bool SupportMediaFoundationPlayback();
MEDIA_EXPORT bool SupportMediaFoundationClearPlayback();
MEDIA_EXPORT bool SupportMediaFoundationEncryptedPlayback();

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_FEATURE_CHECKS_H_
