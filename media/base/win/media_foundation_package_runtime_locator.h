// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MEDIA_FOUNDATION_PACKAGE_RUNTIME_LOCATOR_H_
#define MEDIA_BASE_WIN_MEDIA_FOUNDATION_PACKAGE_RUNTIME_LOCATOR_H_

#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"

namespace media {

MEDIA_EXPORT bool LoadMediaFoundationPackageDecoder(AudioCodec codec);
MEDIA_EXPORT bool FindMediaFoundationPackageDecoder(AudioCodec codec);

}  // namespace media

#endif  // MEDIA_BASE_WIN_MEDIA_FOUNDATION_PACKAGE_RUNTIME_LOCATOR_H_
