// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_player_logging_id.h"

namespace media {

namespace {

// A count of all MediaLogs created in the current process. Used to generate
// unique IDs.
static base::AtomicSequenceNumber g_next_player_id;

}  // namespace

MediaPlayerLoggingID GetNextMediaPlayerLoggingID() {
  return g_next_player_id.GetNext();
}

}  // namespace media
