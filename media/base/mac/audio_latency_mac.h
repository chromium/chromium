// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_AUDIO_LATENCY_MAC_H_
#define MEDIA_BASE_MAC_AUDIO_LATENCY_MAC_H_

#include "media/base/media_shmem_export.h"

namespace media {

MEDIA_SHMEM_EXPORT int GetMinAudioBufferSizeMacOS(int min_buffer_size,
                                                  int sample_rate);

}  // namespace media

#endif  // MEDIA_BASE_MAC_AUDIO_LATENCY_MAC_H_
