// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_OVERLAY_MOJO_FACTORY_H_
#define MEDIA_BASE_ANDROID_OVERLAY_MOJO_FACTORY_H_

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "media/base/android_overlay_config.h"

namespace media {

// Note that this compiles on non-android too.
using AndroidOverlayMojoFactoryCB =
    base::RepeatingCallback<std::unique_ptr<AndroidOverlay>(
        const base::UnguessableToken&,
        AndroidOverlayConfig)>;

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_OVERLAY_MOJO_FACTORY_H_
