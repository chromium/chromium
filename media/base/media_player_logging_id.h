// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_PLAYER_LOGGING_ID_H_
#define MEDIA_BASE_MEDIA_PLAYER_LOGGING_ID_H_

#include "base/atomic_sequence_num.h"
#include "media/base/media_export.h"

namespace media {

// alias the name for readability
using MediaPlayerLoggingID = int32_t;

MEDIA_EXPORT MediaPlayerLoggingID GetNextMediaPlayerLoggingID();

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_PLAYER_LOGGING_ID_H_
