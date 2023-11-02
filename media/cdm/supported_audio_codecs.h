// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_SUPPORTED_AUDIO_CODECS_H_
#define MEDIA_CDM_SUPPORTED_AUDIO_CODECS_H_

#include "base/containers/flat_set.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"

namespace media {

// Returns a set of audio codecs that are supported for decoding by the
// browser after a CDM has decrypted the stream. This will be used by
// CDMs that only support decryption of audio content.
// Note that this should only be used on desktop CDMs. On other platforms
// (e.g. Android) we should query the system for (encrypted) audio codec
// support.
MEDIA_EXPORT const base::flat_set<AudioCodec> GetCdmSupportedAudioCodecs();

}  //  namespace media

#endif  // MEDIA_CDM_SUPPORTED_AUDIO_CODECS_H_
